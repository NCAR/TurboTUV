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
The range-reduction integer requires `INT_MAX >= 1772784`; narrower integer
implementations use libm. Optional guarded Clang/GCC loop hints schedule
independent array elements without changing the arithmetic. Set
`TURBOTUV_REAPER_MATH_HINTS=OFF` to build without these hints.

## Construction

Let `N = 2048` and `h = log(2)/N`. Choose integer `k` by rounding `x/h` (exp)
or `x*log(10)/h` (pow10). The rounded reciprocal need not select the ideal
integer at a reduction boundary: either neighboring integer leaves the
residual inside the proved interval. Split h into
`H = 0x1.62e42fec00000p-12` and `L = 0x1.d1cf79abc9e3bp-43`.
H has 31 significant bits. With `abs(k) <= K = 1772784`, the product `k*H`
uses at most 52 significant bits and is exact.

For exp, form `C = x - k*H` and `r = fma(-k,L,C)`. The subtraction is exact
by Sterbenz's lemma, or it subtracts zero. For pow10, first compensate the
product with the split logarithm
`LH = 0x1.26bb1bbb00000p+1`, `LL = 0x1.554560b752b6bp-33`:

```
A = round(x*LH)
a = fma(x,LH,-A)
B = fma(x,LL,a)
C = A - k*H
D = round(C+B)
r = fma(-k,L,D)
```

The FMA recovers the high-product error, with minimum-subnormal allowances
included below. `C` is exact. Simply evaluating exp(x*log(10)) would not
provide this bound.

Evaluate the cubic Taylor polynomial with three explicit FMAs and correctly
rounded hexadecimal coefficients. Multiply by the rounded table entry
`2^(j/N)`, where j is k modulo N, and by an exact normal power of two. A
positive integer bias `1024*N` implements the quotient and remainder without
right-shifting a negative signed integer. The table occupies 16,384 bytes.
It was generated with Decimal at 100 decimal digits; independent validation
uses Decimal at 120 digits and an MPFR exp2 oracle with an exact `j/N` input
and a 53-bit destination, avoiding double rounding of the table constants.

## Conservative bound

Write `u = 2^-53`, `epsilon = 2u`, and `gamma_3 = 3u/(1-3u)`.
Rounded reciprocal multiplication and integer selection give
`abs(r_exact) <= h*(1/2 + 4u*(K+1))`. Including residual roundoff, use
`a = h/2 + 1e-12 < 0.000169226` for the polynomial interval.

The independent checker verifies the Sterbenz ratios using the actual split
constants. The limiting lower ratio is k=1: its argument is at most 1.5h,
so selection error is bounded locally by 12u, rather than the full-domain K
bound. For k>=2 the lower ratio is at least 0.75 with ample margin. The upper
ratio remains below two. This applies to both signs and to the compensated
pow10 high product.

The checker separately bounds the split-constant errors, each rounding in
`B`, `D` and the final FMA, and minimum-subnormal terms. Its bounds on absolute
residual error are 1.888e-20 for exp and 3.774e-20 for pow10. It also verifies
that these errors fit within the selected interval. The old 64-entry helper's
looser reduction allowance is not reused as an assumption.

Conservative relative-error contributions in epsilon units are:

| Source | Bound |
| --- | ---: |
| Reduction, through exp | 0.000170 |
| Three Horner FMAs, `gamma_3*exp(2a)/epsilon` | 1.501 |
| Rounded polynomial coefficients, `u*exp(2a)/epsilon` | 0.501 |
| Taylor remainder, `exp(2a)*a^4/(24*epsilon)` | 0.154 |
| Rounded table value | 0.500 |
| Product with the table value | 0.500 |
| Exact normal power-of-two scaling | 0 |

Combining the terms multiplicatively, including their cross terms, gives
3.154790 epsilon, below **3.16 epsilon** relative to the exact real function
throughout the guarded domains. This is tighter than the preceding helper's
5-epsilon bound. An 8-epsilon development gate retains margin for comparison
with a rounded reference. C11 does not specify a universal libm error bound;
comparison with the platform's libm is measured separately.

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
of 6.818 epsilon and exercises 527 optional-metadata allocation failures.
These are measured regression-test results, not a uniform bound on every
possible interpolated spectrum.

The check also applies when a bin combines ordinary overlaps with zero-valued
padding outside the guarded geometry. Zero padding contributes exactly zero;
the ordinary segments still use their positive weights and conditioning sum.
Any nonzero unguarded segment forces ordered replay. Previously, one invalid
geometry flag could send the entire bin through ordered interpolation without
checking cancellation in its ordinary segments. Extending the stress test
exposed a 33.307-epsilon failure at trial 1338. At the preceding mixed-bin
checkpoint, a separate 100,000-spectrum screen of the corrected 64-entry
implementation compared 7,135,407 values, with maximum 9.162 epsilon and 5,264
injected allocation failures. The 2048-entry prototype passed the same screen
at 8.858 epsilon. Those diagnostic results are retained in
[`evidence/reaper/math-table-followup`](evidence/reaper/math-table-followup/).

The selected 2048-entry helper is independently checked against 200-bit MPFR
computations at every integer and half-integer reduction boundary, their eight
binary64 neighbors in each direction, domain endpoints, dense residuals and
one million seeded random inputs. Per compiler, exp compares 121,614,882 inputs
and pow10 compares 121,348,084 inputs. Both GCC and Clang stay below 1.995 epsilon
against the rounded MPFR values and the platform libm. These are boundary and
sample checks, not an exhaustive enumeration of all binary64 inputs. The
uniform bound above supplies the separate full-domain argument.

The initial oracle used an archived prototype header; preprocessing-token
comparison under both compilers verifies that the selected header and current
oracle source produce identical translation units after standard adjacent
string concatenation. The only changes were formatting and the table-size
macro. Independent MPFR exp2 checks with a 53-bit destination also verify all
2,048 stored table entries. Commands, hashes, bound calculations and oracle
reports are in [`evidence/reaper/spectral-next`](evidence/reaper/spectral-next/).
