# REAPER results

Implementation is in progress on `reaper`. No additional 2x result or
impossibility conclusion has been established. The denominator is prepared
`main` at `9e149851af5bc48fc7ce64b5fbe50c6959ff2d51`, not the older baseline.

## R0: controls and measurement

The current machine is Apple M5, ARM64 macOS 27.0, with GCC/GFortran 15.2.0 and
Clang/Flang 21.1.5. This differs from the historical performance record.
Release flags are `-O3 -DNDEBUG -ffp-contract=off`, without native CPU flags or
fast math. The LLVM linker cannot read macOS 27's new SDK architecture tags;
both C controls therefore use the installed macOS 26.5 SDK. GFortran and Flang
use compiler-matched NetCDF Fortran installations. Clang compiles the original
reference's C++ dependencies for both families because GCC 15 cannot compile
the new Mach SDK assertions. This does not affect the C or Fortran numerical
compiler choices. No x86-64 performance result is claimed.

The pinned control source is an isolated Git archive. Original TUV-x
`ebe5006cbdb74b9040173c97b93323e48a83b464` was recovered into another isolated
checkout; the user's sibling checkouts were not changed. Both families pass
all 39 configured C/reference CTests and all 88 original reference tests.
The prepared catalogue contains 912 spectra, 550 transfer atmospheres, 26
models, 364 perturbed model columns, and 124,035,553 exact value comparisons.
All 190 embedded datasets (4,547,069 compiled bytes) pass regeneration and
byte-for-byte audit against that revision.

The new diagnostics build passes 22 CTests per compiler. Four full-output
benchmark gates compare 165,382,800 values to frozen main, all bit-identical.
The comparator has independent tests for tiny-rate sensitivity, zeros,
nonfinite values and ULP accounting. Execution counters distinguish actual
prepared runs and generic fallbacks; optional stage timers are disabled in
all acceptance binaries. See [tool documentation](../tools/reaper/README.md).

Evidence, including all commands, test inventories, hashes, numerical
maxima, timing samples and profiling records, is under
[`docs/evidence/reaper/baseline`](evidence/reaper/baseline/).

Median uninstrumented time, ten paired rounds of 300 varied columns repeated
five times per run (at least one second of timed work):

| Configuration | Compiler | Frozen main | R0 counters |
| --- | --- | ---: | ---: |
| tuv_5_4 | gcc | 1078.9 µs | 1081.3 µs |
| tuv_5_4 | clang | 1080.5 µs | 1079.8 µs |
| ts1_tsmlt | gcc | 1108.8 µs | 1109.9 µs |
| ts1_tsmlt | clang | 963.4 µs | 963.1 µs |

The stage timers add 0.4–0.6% to median column time in the separate five-round
overhead comparison. All instrumented hashes match their uninstrumented controls.
No prepared declines or generic fallbacks occurred in the benchmark.

Instrumented stages (µs/column; transfer contains its listed substages):

| Stage | tuv GCC | tuv Clang | ts1 GCC | ts1 Clang |
| --- | ---: | ---: | ---: | ---: |
| geometry | 14.3 | 10.7 | 14.2 | 10.7 |
| gas | 13.6 | 8.6 | 9.6 | 6.7 |
| oxygen_depth | 65.1 | 40.6 | 64.7 | 40.2 |
| transfer | 456.4 | 424.6 | 314.2 | 292.9 |
| mix | 142.8 | 141.5 | 97.8 | 96.5 |
| slant | 106.5 | 97.3 | 73.6 | 67.8 |
| coefficients | 107.2 | 81.9 | 74.7 | 57.4 |
| solve | 68.5 | 70.3 | 47.6 | 49.2 |
| output | 30.6 | 32.7 | 19.8 | 21.2 |
| flux | 29.7 | 19.4 | 17.6 | 12.7 |
| spectra_direct | 254.8 | 223.9 | 95.5 | 86.6 |
| spectra_yield | 37.6 | 37.3 | 20.6 | 20.3 |
| spectra_parameterized | 0.0 | 0.0 | 443.9 | 350.5 |
| oxygen_cross | 56.1 | 32.8 | 55.8 | 32.3 |
| photolysis | 121.6 | 206.8 | 55.8 | 98.2 |
| heating | 0.0 | 0.0 | 3.6 | 6.2 |
| dose | 29.6 | 66.8 | 0.0 | 0.0 |

This cost budget rejects a rate-only strategy for the full 2x target. It does
not bound a combined rewrite. R1 begins with layout, fixed overlap schedules,
and shared O2 work; R2–R3 must also address transfer and spectra.

Sampling and vectorization reports are retained beside the timings. They are
attribution evidence, not speedup measurements. The compiler reports identify
early exits/reductions in mixing and dependent array accesses in transfer as
vectorization obstacles. This gives R3 a concrete portable-C target. Libm
calls remain visible in the spectrum and O2 samples; allocation is not the
largest sampled cost. R0 does not establish a hardware lower bound.

## R1: portable overhead experiments

`TT_EXECUTION_REAPER` is a new opt-in selector (value 2). The existing values
0 and 1 and the prepared default are unchanged. REAPER declines to the exact
prepared executor, then the generic sequence, when an input is unsupported.
The first phase retains the original operation order throughout:

* Rate-loop tiles 1, 2, 4, 8, 16, 32 and a full-level loop were measured on
  each compiler/configuration. Clang selects the full-level loop, GCC 16
  levels. These are register/cache layout choices; no ISA or vector runtime
  is required. Smaller tiles were often much slower.
* Temperature-parameterized spectra store their fixed overlap segments and
  coordinate differences. Evaluation retains the original slope division,
  endpoint interpolation, accumulation order, padding and range selection.
* The O2 radiator and reactions share a single optics call when both need
  the column. Scratch is model-owned; the public allocating wrapper remains
  available. Depth-only, cross-only and combined calls are checked against
  it on the existing O2 catalogue. Matching per-level logarithms are reused.
* Flux formation also checks its final stored values for finiteness, avoiding
  a separate pass. Its isolated effect is small; in combination it helps GCC
  and is approximately neutral for Clang.

The isolated sweeps and five-round interaction measurements are recorded in
[`evidence/reaper/r1`](evidence/reaper/r1/). Every variant matched the frozen
control's full-output hashes for all 300 changing columns. The selected CMake
binaries additionally pass four full-value gates (165,382,800 values, zero
absolute/relative/ULP error). All 43 C/reference CTests pass on both compiler
families, including two new direct REAPER/Fortran model and host-update tests.
The separate REAPER catalogue preserves the original exact prepared tests and
asserts actual execution for ordinary successful columns. All 23 portable
CTest suites also pass under Apple Clang address/undefined-behavior sanitizers.

The first improvements do not reach 2x. Rate generation and relaxed
accumulation experiments continue in R2; transfer and further spectral work
remain necessary in R3. These results do not establish impossibility.

Selected R1 binaries, ten alternating paired rounds with full-value gates:

| Configuration | Compiler | Frozen main | R1 | Speedup | 95% lower bound |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | gcc | 1076.7 µs | 1006.2 µs | 1.070x | 1.066x |
| tuv_5_4 | clang | 1077.0 µs | 918.7 µs | 1.172x | 1.171x |
| ts1_tsmlt | gcc | 1106.5 µs | 956.6 µs | 1.157x | 1.156x |
| ts1_tsmlt | clang | 963.6 µs | 801.8 µs | 1.202x | 1.193x |

The minimum median gain is 1.070x (lower bound 1.066x). Every timed column
ran the REAPER executor, with zero prepared or generic fallback calls.
These are combined measurements, not multiplied isolated gains.

## R2: generated rate kernels

The optional emitter consumes the canonical C model plan. It emits bounded
C11 functions with literal offsets, hexadecimal constants, and explicit
operation order. It does not reinterpret the scientific configuration. SHA-256
fingerprints cover the immutable builder state and term lists. An optional
static function table attaches matching photolysis/heating/dose families;
uncovered or declined families remain interpreted. No runtime compiler,
Python embedding, dynamic loader, or global mutable numerical workspace is used.

GCC selects 32-term helpers, eight accumulators and one output at a time.
Clang selects 64-term helpers, four accumulators, four interleaved outputs,
and explicit standard `fma`. The generator also retains its ordered mode.
Both configurations result from size/accumulator/interleave/FMA/tile sweeps,
followed by combination tests. Large functions and product reuse generally
cost more than they save; the complete ledger includes these rejected timings.

All 45 C/Fortran CTests pass with each selected compiler build. All 25 portable
suites pass under address/undefined-behavior sanitizers, including concurrent
independent REAPER models. The original exact prepared tests remain intact;
the separate REAPER catalogue applies the documented field-specific gates.
Its inherited extreme standalone cases preserve identical nonfinite bit
patterns and statuses; the primary complete-model gates independently require
all outputs to be finite.

The four 300-column gates compare 165,382,800 values. Maximum rate error is
9.913 relative epsilon units; radiation and spectra remain unchanged in R2.
Six held-out model descriptions per compiler change layer counts (including
one and two), wavelength grids, rate order/scales, and transfer method.
Ordered generated output is bit-identical; relaxed output passes the gates.
The original primary artifacts correctly decline attachment to these altered
models, including a same-shape reordered rate list. SHA test vectors, export
determinism, mutation-stable identity, mismatch preservation, partial coverage,
family decline/overwrite and detach tests also pass.

Selected R2 CMake binaries, ten alternating paired rounds of 300 columns
repeated five times (each run exceeds one second of timed work):

| Configuration | Compiler | Frozen main | R2 | Speedup | 95% lower bound |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | gcc | 1074.0 µs | 960.6 µs | 1.118x | 1.116x |
| tuv_5_4 | clang | 1077.1 µs | 852.0 µs | 1.264x | 1.262x |
| ts1_tsmlt | gcc | 1104.7 µs | 938.7 µs | 1.175x | 1.174x |
| ts1_tsmlt | clang | 959.8 µs | 778.5 µs | 1.233x | 1.229x |

Every timed column ran REAPER, with two successful generated family calls
and no family, prepared, or generic fallback. Output hashes remain stable
within each binary. The target is **not achieved; feasibility remains unresolved**.
These improvements do not establish an impossibility bound.

Fresh instrumented residual costs, in µs/column:

| Stage | tuv GCC | tuv Clang | ts1 GCC | ts1 Clang |
| --- | ---: | ---: | ---: | ---: |
| transfer | 456.7 | 426.0 | 317.3 | 293.7 |
| mix | 143.0 | 141.5 | 99.1 | 96.4 |
| slant | 105.2 | 97.8 | 73.9 | 68.2 |
| coefficients | 107.7 | 82.4 | 75.4 | 57.5 |
| solve | 68.8 | 70.6 | 48.3 | 49.5 |
| spectra_direct | 255.7 | 227.4 | 96.6 | 87.7 |
| spectra_parameterized | 0.0 | 0.0 | 366.2 | 273.5 |
| photolysis | 84.0 | 71.1 | 41.5 | 36.9 |
| heating | 0.0 | 0.0 | 3.1 | 2.5 |
| dose | 21.2 | 15.3 | 0.0 | 0.0 |

Transfer includes its substages. Even zero-cost remaining rate integration
would leave these totals above half of frozen main. R3 therefore targets
mixing, transfer layout and spectrum evaluation; isolated gains will not be
multiplied to predict the combined result.

Static attachment/fingerprinting adds about 5–15 ms in the recorded runs.
Generated source emission and compilation took 8.66 s for the selected GCC
prototype and 2.45 s for Clang, before linking. Runtime maximum RSS was
14.8–18.9 MB. Cold-build and short-job break-even measurements remain part
of R4/R6. This is an opt-in static prototype, not a runtime JIT/cache claim.

Evidence is in [`evidence/reaper/r2`](evidence/reaper/r2/): source/plan archives,
compile commands, object/binary sizes, all screening and held-out gates,
reference/sanitizer logs, resource measurements, profiles, and paired samples.

## R3: spectra, O2 lanes, and transfer layout

The selected combination reuses exact fixed-temperature range results and
expensive direct-formula clamp rows; evaluates tables and parameterized
spectra across independent levels; and batches the ordered O2 recurrence.
The separate REAPER transfer implementation uses contiguous mixing, cached
immutable radiator roles, vectorizable validity reductions, and independent
coefficient loops. Successful radiator replacement invalidates the role cache.
Transfer arithmetic and its libm exponentials retain the original order.

Selected spectrum formulas use a portable bounded exp/pow10 implementation.
Its uniform error argument is in [REAPER_MATH.md](REAPER_MATH.md): less than
5 epsilon relative to the real function over the guarded normal domains.
Independent MPFR checks cover every reduction boundary and adjacent binary64
inputs, domain endpoints, dense residuals, and seeded random values. Each
compiler passes 4,832,736 exp and 4,824,406 pow10 samples with maxima below
1.905 epsilon. Exceptional arrays, other rounding modes, and unsupported
floating-point representations use libm. Approximate transfer exponentials
fail the physical-zero/cancellation gates and were rejected.

The full 49-test C/Fortran suite passes on each compiler. This now includes
direct REAPER spectrum/Fortran comparisons and radiator-role mutation tests.
All 28 sanitizer tests pass; a final O2 boundary-predicate correction was
followed by both complete compiler suites and six affected sanitizer tests.
The original exact prepared tests remain unchanged. Four final 300-column
gates compare 165,382,800 values; the largest rate difference is 9.913 epsilon.
Six held-out models per compiler pass both ordered and roundoff generated-rate
tests, exact ordered replay against REAPER's interpreter, and mismatched
artifact fallback. Ordered rate kernels now receive REAPER spectrum inputs,
so their bit-identity claim applies to the same REAPER inputs.

Final uninstrumented binaries, ten rotating paired rounds, 300 changing
columns repeated ten times per run (every run exceeds one second):

| Configuration | Compiler | Frozen main | R3 | Paired speedup | 95% lower bound |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | gcc | 1079.8 µs | 745.8 µs | 1.448x | 1.446x |
| tuv_5_4 | clang | 1078.7 µs | 685.7 µs | 1.574x | 1.572x |
| ts1_tsmlt | gcc | 1107.6 µs | 590.2 µs | 1.878x | 1.876x |
| ts1_tsmlt | clang | 963.4 µs | 535.5 µs | 1.799x | 1.795x |

Every timed column ran REAPER with two generated family calls and no family,
prepared, or generic fallbacks. Hashes are stable within each binary. These
combined measurements establish useful gains; the four-case 2x objective is
**not achieved; feasibility unresolved**. Further transfer fusion and spectrum
specialization remain unexplored, so these measurements cannot prove a bound.

Fresh instrumented residual costs, in µs/column (transfer includes its listed
substages; these attribution samples are separate from acceptance timings):

| Stage | tuv GCC | tuv Clang | ts1 GCC | ts1 Clang |
| --- | ---: | ---: | ---: | ---: |
| transfer | 352.1 | 325.9 | 241.6 | 222.7 |
| mix | 50.3 | 56.8 | 32.9 | 37.1 |
| slant | 103.2 | 99.3 | 71.4 | 68.5 |
| coefficients | 98.3 | 64.8 | 68.6 | 45.3 |
| solve | 68.4 | 70.6 | 47.9 | 49.6 |
| output | 31.0 | 33.4 | 20.3 | 21.6 |
| spectra_direct | 171.3 | 169.6 | 67.7 | 65.7 |
| spectra_yield | 36.5 | 36.7 | 20.1 | 19.8 |
| spectra_parameterized | 0.0 | 0.0 | 145.1 | 127.9 |
| oxygen_depth | 25.8 | 26.4 | 25.7 | 26.4 |
| photolysis | 83.7 | 71.3 | 41.6 | 37.2 |
| heating | 0.0 | 0.0 | 3.0 | 2.5 |
| dose | 21.2 | 15.3 | 0.0 | 0.0 |

The separate resource runs report 17.5–18.9 ms model construction, 4.7–14.2 ms
REAPER selection/identity preparation, and 15.2–19.4 MB maximum RSS. Detailed
profiles put host updates near 1.1–1.3 µs/column, so removing host overhead
alone cannot close the remaining gap. Static rate source/compile costs remain
those recorded in R2; R4 will harden attachment and deployment metadata.

[`evidence/reaper/r3`](evidence/reaper/r3/) contains all selected-build gates,
profiles, resource logs, oracle records, provenance and paired samples. Its
[ledger](evidence/reaper/r3/LEDGER.md) records retained and rejected cache,
layout, math, slant, coefficient, O2 and parameterization trials. Historical
screening results are explicitly separated from the final combined timings.

## R4: static attachment and deployment

Version-2 descriptors now include generator version, interleave width,
FMA/product-reuse flags, and a canonical recipe digest in addition to the model
digest. C independently recomputes the recipe identity during attachment.
The spectrum-kind sentinel is serialized as signed `-1`, removing dependence
on `size_t` width. Version-1 exports/descriptors must be regenerated.
Explicit ABI, layout, numerical-policy, recipe or model mismatches preserve
the previous valid attachment and report the reason.

`tt_model_reaper_info` exposes automatic selection, absent/incompatible
artifacts, explicit detach, selected capabilities, and optional-workspace
allocation failures. Allocation failure uses the existing transfer workspace;
an injected-failure test compares every output field and verifies successful
REAPER execution. Selection is cached per model, descriptor storage is copied,
and static code is safely shared by concurrent independent models.

All 50 C/Fortran tests pass on GCC and Clang, and all 29 sanitizer tests pass.
Four 300-column gates are bit-identical to R3, including all 165,382,800 values.
The six altered model descriptions per compiler again pass generated ordered,
roundoff, exact ordered replay, and incompatible-artifact fallback tests. The
fallback evidence now explicitly reports `no-match` and zero generated calls.

Minimal C11/libm builds disable testing, the CLI, the benchmark, generation and
Python discovery. Installed CMake consumers pass for both minimal and generated
libraries, on both compilers and both primary models, with `PATH=/nonexistent`
during execution. Their only dynamic dependency on this machine is libSystem;
Python, a compiler, a dynamic loader adapter and C++ are not runtime dependencies.
This demonstrates the required static route on ARM/macOS. x86-64 execution has
not yet been demonstrated.

Static deployment already provides profitable reuse, so a runtime compiler,
loader and persistent binary cache were not added. Such integration would add
platform and lifecycle work without reducing the measured column arithmetic.
The offline artifact record includes compiler binary/version/target, effective
flags, host compatibility information, all recipe/source/object hashes and a
build identity. It does not claim to validate or load an arbitrary cached binary.

Fresh-model initialization medians from five alternating process runs, and
estimated break-even using R3's gated steady-state savings:

| Configuration | Compiler | Frozen main initialization | R4 initialization | Extra startup | Static break-even |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | gcc | 16.779 ms | 30.951 ms | 14.172 ms | 43 columns |
| tuv_5_4 | clang | 17.576 ms | 27.527 ms | 9.951 ms | 26 columns |
| ts1_tsmlt | gcc | 15.708 ms | 22.575 ms | 6.867 ms | 14 columns |
| ts1_tsmlt | clang | 15.461 ms | 20.135 ms | 4.674 ms | 11 columns |

These are fresh model/process measurements with a warm OS file cache, not a
cold-machine claim. Repeated selection within a model costs about 1.5–1.8 ns
in the 10,000-call probes. Emitting and compiling both primary kernel sets in
isolation takes 8.346 s for GCC and 2.378 s for Clang. Generated object totals
are 1,572,432 and 649,680 bytes respectively. Charging that entire build to a
single configuration yields estimated break-even counts of 16,145–25,033 for
GCC and 5,568–6,077 for Clang; these exclude final linking and other build work.
Already linked deployments pay only the static initialization cost above.
Separate resource runs report maximum RSS of 15.2–19.5 MB.

A three-round R3/R4 regression screen reports paired median ratios of 0.980–1.000
and stable identical hashes. No regression exceeds the plan's 5% investigation
threshold. This screen is not a new ten-round 2x acceptance claim; the primary
target remains **not achieved; feasibility unresolved**.
Evidence is in [`evidence/reaper/r4`](evidence/reaper/r4/).
