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

## R3 follow-up after R4: transfer fusion and spectral loops

Forward elimination now consumes each layer's coefficients immediately.
The surface boundary and back substitution remain a final pass, with each
scalar operation retaining its previous order. GCC reuses consumed scaling
rows and alternates two particular-solution rows, saving 76,160 bytes of
transfer scratch for each primary model. Clang keeps the full workspace
because the compact layout measured slower there. Independent spectral
exponential lanes use optional guarded compiler hints, and flux finiteness
checks use normalized bitwise predicates. No new approximation is selected.

The full 50-test C/Fortran suites pass on both compilers, as do all 29
sanitizer tests and both 29-test hint-disabled suites. A final portable-C
predicate normalization was followed by 17 affected C/reference tests per
compiler and 12 affected sanitizer/hint-disabled tests. The four 300-column
gates compare 165,382,800 values against frozen main, with maximum difference
9.913 epsilon; another four gates are bit-identical to R4. Hint-disabled
comparisons cover 26,461,248 values with identical hashes. Six altered models
per compiler again pass ordered/roundoff generation, exact ordered replay,
and incompatible-primary-artifact fallback. The independent MPFR checks
repeat all 4,832,736 exp and 4,824,406 pow10 inputs per compiler, retaining
the previous maxima below 1.905 epsilon and the unchanged uniform bound.

Fresh uninstrumented comparisons use ten rotating paired rounds of 300
changing columns repeated ten times, with at least one second of timed work
per sample:

| Configuration | Compiler | Frozen main | Follow-up | Paired speedup | 95% lower bound |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | gcc | 1077.9 µs | 704.3 µs | 1.530x | 1.526x |
| tuv_5_4 | clang | 1078.0 µs | 664.3 µs | 1.624x | 1.621x |
| ts1_tsmlt | gcc | 1108.1 µs | 560.3 µs | 1.979x | 1.975x |
| ts1_tsmlt | clang | 964.1 µs | 519.5 µs | 1.854x | 1.852x |

All timed columns use REAPER and the generated rate families with no declines
or fallbacks. Every sample is retained, including a slow GCC main TS1 round;
no outliers were removed. The four-case 2x goal remains **not achieved;
feasibility unresolved**. A near miss is not an impossibility result.

Separate instrumented residual costs in µs/column are:

| Stage | tuv GCC | tuv Clang | ts1 GCC | ts1 Clang |
| --- | ---: | ---: | ---: | ---: |
| transfer | 312.9 | 298.7 | 214.7 | 204.4 |
| mix | 48.9 | 55.9 | 32.0 | 36.4 |
| slant | 101.1 | 95.9 | 69.8 | 66.9 |
| coefficients and forward elimination | 123.3 | 101.3 | 86.5 | 70.8 |
| final boundary and back substitution | 9.3 | 11.8 | 6.5 | 8.3 |
| output | 29.4 | 32.9 | 19.2 | 21.5 |
| spectra_direct | 160.7 | 163.6 | 63.9 | 62.7 |
| spectra_yield | 35.8 | 36.0 | 19.7 | 19.6 |
| spectra_parameterized | 0.0 | 0.0 | 139.5 | 125.4 |
| oxygen_depth | 25.2 | 25.9 | 25.2 | 26.0 |
| photolysis | 81.9 | 69.7 | 40.5 | 36.6 |
| heating | 0.0 | 0.0 | 3.0 | 2.4 |
| dose | 20.6 | 15.1 | 0.0 | 0.0 |

The coefficient timer now includes forward elimination, so its individual
number cannot be compared directly with the older coefficient timer. Transfer
contains its listed substages. Profile-only model-run times are 688.0/650.7 µs
for TUV GCC/Clang and 546.6/508.4 µs for TS1; host updates add 1.1–1.4 µs.
These attribution runs do not replace the primary paired measurements above.
Separate resource runs report 17.2–18.6 ms construction, 4.4–13.9 ms selection,
and 15.2–19.3 MB maximum RSS.

The [follow-up ledger](evidence/reaper/continuation/LEDGER.md) records eight
families of experiments. Split/FMA slant accumulation fails numerical gates;
flux fusion, reciprocal scaling and small-argument polynomials do not give a
useful combined gain. Larger exponential tables remain unselected prototypes
without complete independent boundary-oracle validation. Remaining avenues
include further spectral specialization and transfer work; this checkpoint
does not supply a lower bound on every allowed rewrite.

## R5: independent-column grouping

A private prototype runs independent model states through the same generated
rate-code chunk before moving to the next chunk. This tests whether reuse of
generated instructions outweighs the larger working set. It groups only models
with matching recipe identities and callbacks, falls back to ordinary calls
for mixed or detached models, and keeps per-column errors and results separate.
There is no new public batch API in the library: the measured prototype is
not profitable.

All six widths, 1/2/4/8/16/32, reproduce the current single-column results
bit for bit on both compilers and both primary configurations. The 301-column
gates cover 995,604,456 values and partial final groups at every width above
one. Separate tests cover invalid columns between valid columns, detached and
generic execution, mixed models, duplicate-handle rejection, a three-column
tail, and concurrent independent groups. GCC, Clang, and address/undefined-
behavior sanitizer runs all pass.

The throughput boundary includes profile packing/updates, execution, and copies
of every returned field and rate. Hashing and file output remain outside it.
Each comparison uses one thread and three rotating screening rounds of 300
columns repeated three times. This is a grouping assessment, not a replacement
for the primary single-column timing protocol or a new 2x acceptance claim.

Median paired throughput relative to repeated calls on one model, with the
same output-copy boundary (values below one mean slower):

| Group width | tuv GCC | tuv Clang | ts1 GCC | ts1 Clang |
| --- | ---: | ---: | ---: | ---: |
| 1 | 1.001x | 1.001x | 1.000x | 1.004x |
| 2 | 0.991x | 0.995x | 0.995x | 0.997x |
| 4 | 0.960x | 0.966x | 0.988x | 0.988x |
| 8 | 0.894x | 0.898x | 0.950x | 0.951x |
| 16 | 0.849x | 0.851x | 0.896x | 0.897x |
| 32 | 0.829x | 0.829x | 0.869x | 0.863x |

Comparisons against ordinary calls on the same number of independent models
also show no useful gain: at width 32, grouping achieves only 0.885–0.923x
their throughput. Groups of two are within roughly one percent of ordinary
calls; wider groups regress progressively. This supports rejecting this
implementation, not ruling out all possible batch designs.

Separate resource runs show construction/selection increasing from 20–31 ms
at width one to 654–963 ms at width 32. Maximum RSS grows from 16–21 MB to
260–410 MB, including the explicit output-copy buffers. Each model owns its
column-dependent spectra, optics, radiation and scratch; sharing another
column's values would violate this experiment's independence requirement.

The [R5 evidence](evidence/reaper/r5/) retains the private source, generated
chunks, build commands/hashes, all numerical gates, concurrency/sanitizer tests,
all 144 timing samples and resource logs. The selected single-column library
and its 1.530–1.979x results remain unchanged. Additional transfer/spectral
screens are recorded separately in the [further ledger](evidence/reaper/further/LEDGER.md).

## R6: combined validation checkpoint

The selected implementation remains `0872e28`; this phase changes no numerical
source. Its two complete 50-test C/Fortran suites, 29 sanitizer tests, plain-C
hint-disabled builds, independent math bounds/oracles, lifecycle/concurrency
checks and primary ten-round timings are recorded in the follow-up evidence
above. R6 adds installation, regeneration and untuned-workload verification
of those same selected binaries. All primary timings, setup/RSS, workspace,
artifact size and break-even results retain their stated measurement scope.

Minimal and generated installed CMake consumers pass with GCC and Clang on
both primary configurations, including destruction of the original
configuration, cached selection, explicit detach and counter checks. All
eight executions run with `PATH=/nonexistent`; the minimal build disables
Python discovery, generation, tests, CLI and benchmark. Dynamic dependencies
remain libSystem only. Scientific-data and fixture audits pass against pinned
TUV-x `ebe5006`, as do all three case-catalogue regeneration checks. The three
Fortran reference adapters regenerate byte for byte in an isolated directory.

Six untuned shapes/rate lists and two changing host-radiation cases pass
300-column gates per compiler with fresh seed 413117: 278,223,000 values,
maximum difference 11.271 epsilon. Secondary performance screens use three
alternating paired rounds, with adaptive repeats keeping all 96 samples above
one second (observed 1.31–3.06 seconds). These screens investigate regressions;
they do not replace the ten-round primary acceptance protocol.

| Untuned case | GCC paired speedup | Clang paired speedup |
| --- | ---: | ---: |
| one layer | 1.136x | 1.007x |
| two layers | 1.217x | 1.133x |
| odd level count | 1.722x | 1.473x |
| odd layer count | 1.815x | 1.560x |
| discrete ordinate | 1.088x | 1.052x |
| reordered rate list | 1.438x | 1.476x |
| changing host radiation, TUV | 1.509x | 1.693x |
| changing host radiation, TS1 | 2.136x | 2.018x |

None exceeds the 5% regression threshold. The six altered descriptions use
the REAPER interpreter and report `no-match` for the primary generated
artifacts; the host cases attach matching rate artifacts and execute two
generated families per column. Every timed column uses REAPER with zero
generic fallbacks. Hashes are stable within each binary. The host harness
adds formation and upload of all six changing radiation arrays to the timed
interval on both sides; it is archived separately from the primary benchmark.
Its greater-than-2x TS1 results do not establish the primary transfer workload's
2x target.

The available Docker Desktop Linux environment did not start within its
45-second timeout. A force-stop restored its initial stopped state. No x86-64
Linux execution or performance result is therefore demonstrated. Portable
source and two ARM/macOS compiler builds are the verified portability scope.
The [R6 evidence](evidence/reaper/r6/) includes commands, source/configuration
archive, installed-consumer logs, regeneration records, all secondary samples,
numerical gates, provenance and file hashes.

The combined primary result remains 1.530/1.624x for TUV and 1.979/1.854x
for TS1 (GCC/Clang), with lower confidence bounds below 2 in every case.
**Not achieved; feasibility unresolved.** No bound rules out the remaining
portable spectral and transfer transformations; optimization work continues.

## Spectral conditioning follow-up after R6

The next selected checkpoint changes only spectral evaluation and its tests.
HCFC coefficient sums traverse contiguous independent levels while preserving
each sum's order. Direct spectra classify temperature-clamp runs once, then
copy active runs or fill cached values. Parameterized exponential/Harwood
spectra use prepared positive interpolation weights when their data and
geometry satisfy the guarded domain. Sensitive partial bins accumulate a
conditioning estimate and replay libm plus ordered interpolation when needed.
Optional-metadata allocation failure also preserves ordered replay. Prepared
and generic arithmetic, transfer arithmetic, rate artifacts and the public
ABI are unchanged.

This is also a correctness fix. A deterministic stress test found a narrow
partial bin with a very large spectral contrast where the preceding selected
implementation differed from the generic reference by 59.726 epsilon, above
the standalone 32-epsilon limit. Historical atmospheric-column gates remain
valid for their recorded inputs. The new test checks 1,000 spectra and 69,264
values, including nine-level tails, signed data, extreme exponents and 53
optional-allocation failures. Its maximum is now 12.910 epsilon. Prepared
results remain bit-identical to the independently assembled generic reference;
signed data and allocation failures retain exact ordered replay. The spectral
exponential's pointwise algorithm and conservative bound are unchanged. See
[REAPER_MATH.md](REAPER_MATH.md) for the interpolation conditioning argument
and its limits.

All 51 C/Fortran CTests pass with GCC and Clang. All 30 ASan/UBSan tests and
30 tests in each compiler's hint-disabled build pass. Focused checks after
adding allocation injection and the final portability guard pass again. A
scalar-parameter, spectral-approximation-disabled build passes the catalogue
and stress tests. The final run-encoding guard changes no native benchmark or
spectral object bytes in the six selected, hint-disabled and profiling builds;
the before/after hashes are archived. Four 300-column primary gates compare
165,382,800 values with maximum difference 9.913 epsilon. Four 48-column
hint-disabled gates compare another 26,461,248 values exactly. Six held-out
configurations per compiler pass ordered, roundoff, replay and incompatible
artifact checks. The MPFR checks remain at 4,832,736 exp and 4,824,406 pow10
arguments per compiler, with maxima 1.904956 and 1.904958 epsilon respectively.

Fresh primary timings use ten rotating paired rounds, 300 changing columns
and ten repeats per sample, against the same frozen prepared `main`. All 80
samples exceed one second (minimum 1.579 seconds); slow samples are retained.
Every candidate sample executes 3,000 REAPER columns and 6,000 generated
families, with no declines, preparation failures or generic fallback and with
profiling disabled. Hashes are stable within each binary.

| Configuration | Compiler | Main median | REAPER median | Paired speedup | Lower 95% |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | GCC | 1078.6 us | 698.6 us | 1.545x | 1.541x |
| tuv_5_4 | Clang | 1079.8 us | 655.3 us | 1.650x | 1.646x |
| ts1_tsmlt | GCC | 1109.1 us | 555.9 us | 1.995x | 1.991x |
| ts1_tsmlt | Clang | 965.1 us | 526.8 us | 1.831x | 1.825x |

TUV improves on both compilers and TS1 improves on GCC. Clang TS1 is slightly
slower than the preceding checkpoint with the added conditioning checks.
The TS1 GCC result is a near miss, not a passing 2x result. All four lower
confidence bounds remain below two.

Separate instrumented profiles retain the following costs; these stage times
are diagnostic measurements, not a lower bound on all possible rewrites.

| Stage (us/column) | TUV GCC | TUV Clang | TS1 GCC | TS1 Clang |
| --- | ---: | ---: | ---: | ---: |
| Transfer, total | 321.793 | 305.669 | 225.876 | 209.384 |
| Mixing | 50.203 | 56.823 | 32.793 | 37.156 |
| Ordered slant depths | 103.741 | 98.136 | 74.722 | 68.434 |
| Coefficients and forward elimination | 127.502 | 103.751 | 91.399 | 72.635 |
| Remaining solve | 9.528 | 12.105 | 6.740 | 8.473 |
| Transfer output | 29.836 | 33.947 | 19.582 | 22.020 |
| Direct spectra | 158.186 | 156.643 | 63.167 | 63.619 |
| Quantum yields | 36.684 | 36.444 | 20.991 | 19.610 |
| Parameterized spectra | 0 | 0 | 141.787 | 133.579 |
| Oxygen depth | 26.039 | 26.375 | 27.151 | 26.433 |
| Photolysis integration | 84.137 | 71.572 | 41.546 | 37.240 |
| Heating | 0 | 0 | 3.093 | 2.478 |
| Dose | 21.253 | 15.336 | 0 | 0 |

Ten-process median setup-plus-selection costs are 33.650/29.056 ms for TUV
and 24.764/21.936 ms for TS1 (GCC/Clang). Separate resource probes report
15.204–19.333 MB peak RSS. Transfer workspace remains 634,496/710,656 bytes
for TUV and 478,976/555,136 bytes for TS1. Frozen-main binaries do not report
startup components, so no fresh incremental break-even is inferred from these
numbers. The R4 compile-cost and break-even observations retain their historical
scope; version-2 generated artifacts are unchanged.

All eight minimal/generated installed-consumer executions pass again with
`PATH=/nonexistent`, with only libSystem dynamically linked. Six untuned shapes
or rate lists and two changing host-radiation cases pass another 16 fresh-seed
300-column gates: 278,223,000 values, maximum 11.271 epsilon. Their 96 secondary
timing samples exceed one second; the smallest paired speedup is 1.000x and
none regresses by more than 5%. These three-round screens do not replace the
primary ten-round comparison. Verification remains ARM/macOS with portable
C11/libm source; x86-64 Linux execution is still unverified.

The [new evidence](evidence/reaper/next/) includes the experiment ledger,
archived successful and rejected prototypes, reproduction commands, full test
logs, numerical gates, profiles, all samples, installed consumers, source and
binary provenance, and a file manifest. **Not achieved; feasibility unresolved.**
No defensible lower bound yet rules out further portable improvements.

## Mixed-bin conditioning and further screening

Further rate and math screens uncovered an omission in the preceding
interpolation fix. A bin containing both ordinary segments and zero padding
outside the guarded geometry could bypass the conditioning check on its
ordinary segments. With the existing math helper, expanding the deterministic
stress test fails trial 1338 at 33.307 epsilon. Larger-table candidates fail
another instance at 93.280 epsilon. This is a standalone interpolation issue;
the previous primary atmospheric-column comparisons retain their recorded
scope.

The correction accumulates positive weights and the conditioning estimate
for ordinary segments even in these mixed bins. Zero-valued padding adds
exactly zero; any nonzero segment outside the guarded geometry forces libm
and ordered replay. The standard test now covers 10,000 spectra, 710,370
values and 527 optional-allocation failures, with maximum 6.383 epsilon. A
separate 100,000-spectrum screen covers 7,135,407 values and 5,264 allocation
failures, with maximum 9.162 epsilon for the selected 64-entry math helper.
Signed cases and optional-allocation failures remain exact ordered replays.

Both 51-test C/Fortran suites, the 30-test sanitizer suite and both 30-test
hint-disabled suites pass. Focused final tests and the scalar parameterization
fallback pass. All eight minimal/generated installed consumers pass with
`PATH=/nonexistent` and only libSystem dynamically linked. Four 300-column
primary gates compare 165,382,800 values at maximum 9.913 epsilon. The six
held-out generated/interpreted configurations per compiler pass, followed by
16 untuned 300-column gates with 278,223,000 values at maximum 11.271 epsilon.
The spectral math header is unchanged; SHA-256 ties its existing independent
bound and MPFR oracle records to this checkpoint. No new rate artifact or
math approximation is selected.

The [rate-layout assessment](evidence/reaper/rate-layout/) screens eight
variants per compiler: 8/16-term helpers, aligned wavelength traversal, and
greedy output grouping by wavelength-set overlap. All 32 screening gates
pass. The three-round screens retain 108 samples; a separate five-round
confirmation retains 40 samples of at least one second. GCC's best small
candidate confirms only 1.00007/1.00040x over the selected implementation
(TUV/TS1), so it is rejected. Clang's grouped 32-term candidate confirms
1.00410/1.00753x and remains an unselected combination candidate. Private
prototype descriptors do not encode the new layout flags and must not be
used as production artifacts.

The [larger-table assessment](evidence/reaper/math-table-followup/) screens
256/512/1024-entry degree-four and 2048-entry degree-three spectral helpers.
The 256/512-entry versions save roughly 2–8 us in short screens; larger
versions are not timed after failing the original stress test. All five math
variants pass the 100,000-spectrum screen after the mixed-bin correction.
None is selected: the expanded independent reduction proof, MPFR boundary
inventory and combined validation remain necessary. The per-spectrum profile
still attributes about 40 us/TUV column and 26 us/TS1 column to CL2; exponential
and Harwood parameterizations dominate TS1's spectral cost. These diagnostic
measurements do not establish a performance lower bound.

Fresh primary results use the same ten rotating paired rounds, 300 changing
columns and ten repeats. All 80 samples are retained; the shortest takes
1.576 seconds. Candidate samples each execute 3,000 REAPER columns and
6,000 generated families, without fallback, declines or preparation failures.
Profiling is disabled and hashes are stable.

| Configuration | Compiler | Main median | REAPER median | Paired speedup | Lower 95% |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | gcc | 1079.4 us | 698.8 us | 1.546x | 1.543x |
| tuv_5_4 | clang | 1080.1 us | 655.0 us | 1.648x | 1.616x |
| ts1_tsmlt | gcc | 1109.3 us | 557.3 us | 1.992x | 1.987x |
| ts1_tsmlt | clang | 965.6 us | 525.6 us | 1.836x | 1.835x |

All four lower confidence bounds remain below two.

The 96 secondary timing samples have a minimum paired speedup of
0.963x against frozen main; no untuned case regresses by more than 5%.
The four 48-column hint-disabled gates compare 26,461,248 values exactly.
Ten-process median setup-plus-selection costs are 33.059, 29.398, 24.572, 22.239 ms
for TUV GCC/Clang and TS1 GCC/Clang respectively. Separate resource probes
report 15.237–19.268 MB peak RSS. Transfer workspace and generated artifacts
are unchanged. Source/binary provenance, separate profiles and exact commands
are archived; x86-64 execution remains unverified.

The [mixed-bin checkpoint](evidence/reaper/mixed-bin/) records the full
validation, reproduction commands, source archives and fresh timing samples.
The four-case objective remains **not achieved; feasibility unresolved**.
