# Bounded exponential experiment

This is an independent numerical argument for the optional REAPER spectrum
experiment in `src/reaper_math.h`. It does not justify using the approximation
inside the transfer solver: those trials failed the complete-output gates and
are rejected. The existing generic and prepared paths continue to use libm.

The implementation uses standard C11, `fma`, integer arithmetic, and `memcpy`.
It selects the approximation only for the binary64 representation checked by
the implementation, round-to-nearest mode, and an entire input array inside
`[-600,600]` for exp or `[-260,260]` for pow(10,x). Other arrays use libm. The
bounded results are normal, finite, and positive; the ordinary libm path
handles overflow, underflow, nonfinite inputs, and other rounding modes.
The range-reduction integer also requires `INT_MAX >= 65535`; platforms with
a narrower `int` use libm. Optional guarded Clang/GCC loop hints schedule
independent array elements without changing the arithmetic. Set
`TURBOTUV_REAPER_MATH_HINTS=OFF` to build without these hints.

## Construction

Let `h = log(2)/64`. Choose the integer `k` by rounding `x/h` (exp) or
`x*log(10)/h` (pow10). The approximate reciprocal used for this choice need
not choose the ideal integer at an exact boundary: either neighboring integer
gives a residual in the proved interval. Split h into
`0x1.62e42fee00000p-7 + 0x1.a39ef35793c76p-39`. The first component has 33
significant bits, and `abs(k) <= 55400`, so its product with k is exact.
The subtraction from x is exact by Sterbenz's lemma (or subtraction of zero).
One FMA subtracts the low component.

For pow10, first form `x*log(10)` as a compensated product. The split is
`0x1.26bb1bbb00000p+1 + 0x1.554560b752b6bp-33`. An FMA recovers the rounding
residual of the high product; another adds the low product. Subtract the
same exact k-times-h-high product before adding the low correction. Simply
computing exp(x*log(10)) would not provide the required bound.

On the residual r, evaluate the degree-five Taylor polynomial for exp with
five explicit FMAs and correctly rounded hexadecimal coefficients. Multiply
by the correctly rounded table entry `2^(j/64)`, where j is k modulo 64, and
then by the exact normal power of two. A positive integer bias implements
the quotient and remainder without right-shifting a negative signed integer.
The table was generated with Python Decimal at 100 decimal digits; the
independent oracle uses MPFR at 200 bits.

## Conservative bound

Write `u = 2^-53`, `epsilon = 2u`, and `gamma_5 = 5u/(1-5u)`.
The rounded reciprocal, multiplication and integer selection imply
`abs(r_exact) <= log(2)/128 + 3u*55401*h < 0.00542`. This includes both
choices at every reduction boundary. High-part products use at most 49
significant bits. The split-constant errors and rounding of the small
residual, including the compensated pow10 product, contribute less than
`epsilon/8` in absolute residual error. Subnormal intermediate products
near zero add at most a few minimum subnormals and satisfy the same bound.

Using `a = 0.00542`, conservative relative-error contributions in epsilon
units are:

| Source | Bound |
| --- | ---: |
| Reduction, through exp | 0.126 |
| Five Horner FMAs, `gamma_5*exp(2a)/epsilon` | 2.528 |
| Rounded polynomial coefficients, `u*exp(2a)/epsilon` | 0.506 |
| Taylor remainder, `exp(2a)*a^6/(720*epsilon)` | 0.161 |
| Rounded table value | 0.500 |
| Product with the table value | 0.500 |
| Final exact normal power-of-two scaling | 0 |

Their sum, including products of the small error terms, is below 5 epsilon
relative to the exact real function. An 8-epsilon development gate leaves
margin for comparison to a correctly rounded binary64 oracle. This argument
is uniform on the stated domains; it is not based on the benchmark profiles.
Comparison to a particular platform's libm is measured separately, since C11
does not specify a universal libm error bound.

## Validation and integration

`tools/reaper/check_math.c` uses MPFR only as a development oracle. It visits
every reduction integer and half-integer boundary throughout each domain,
neighboring binary64 inputs on both sides, the domain endpoints and zero,
dense residual samples, and reproducible random inputs. Ordinary C tests
also cover array tails, in-place operation, exceptional operands, and rounding
mode fallback. Neither MPFR nor Python is a runtime library dependency.

Each use in a spectrum remains subject to its original 32-epsilon test and
the complete model's 64-epsilon gates. Prefactor integrations are restricted
to safe normal-product domains. The additive exponential parameterization
uses the bounded function only with nonnegative existing cross sections, so
subtraction cannot amplify the local approximation error. This local bound
does not excuse cancellation elsewhere: the measured failures in transfer
are retained in the experiment ledger.

### Conserving parameterized spectra

A pointwise exponential bound does not bound the relative error of a later
subtraction. A new stress case with a large spectral contrast exposed this:
the previous selected implementation differed from generic evaluation by
59.7 epsilon in a narrow partial bin, despite satisfying the exponential's
pointwise bound.

REAPER now prepares positive endpoint weights for a segment's integral. If
`h` is half the overlap width and `q = (left + right) / dx`, the weights are
`A = h * (2 - q)` and `B = h * q`. The integral is `A*a + B*b`. Ordered prepared
execution retains its original slope and two endpoint evaluations.

Only nonnegative normal-domain exponential/Harwood inputs use these weights.
The exponential helper reports whether its existing domain checks succeeded;
interpolation reuses that result. Geometry outside the guarded normal range,
signed/extreme raw data, unavailable optional metadata, unsupported arithmetic,
and scalar experimental tiles retain libm and ordered interpolation.

For partial bins with potentially sensitive endpoints, evaluation also sums
`h*(a+b)` while computing the integral. If this sum exceeds four times the
positive bin integral, the active level block is recomputed with libm and the
ordered interpolator. Segments with `q` between 0.5 and 1.5 need no additional
sum because both endpoint weights are at least `h/2`. This conditioning check
addresses amplification by the existing endpoint subtraction; it does not
change the uniform exp/pow10 bound above or replace the spectrum/model gates.

The stress test uses 10,000 deterministic spectra with narrow partial
bins, large contrasts, signed Taylor data, ordinary and extreme exponentials,
and injected optional-allocation failures. It independently reconstructs the
generic output and verifies that the preserved prepared path is identical.
The selected implementation compares 710,370 values with a maximum difference
of 6.383 epsilon and exercises 527 optional-metadata allocation failures.
These are measured regression-test results, not a uniform bound on every
possible interpolated spectrum.

The check also applies when a bin combines ordinary overlaps with zero-valued
padding outside the guarded geometry. Zero padding contributes exactly zero;
the ordinary segments still use their positive weights and conditioning sum.
Any nonzero unguarded segment forces ordered replay. Previously, one invalid
geometry flag could send the entire bin through ordered interpolation without
checking cancellation in its ordinary segments. Extending the stress test
exposed a 33.307-epsilon failure at trial 1338. A separate 100,000-spectrum
screen of the corrected implementation compares 7,135,407 values, with maximum
9.162 epsilon and 5,264 injected allocation failures. Its evidence and the
unselected larger-table experiments that exposed the issue are retained in
[`evidence/reaper/math-table-followup`](evidence/reaper/math-table-followup/).
