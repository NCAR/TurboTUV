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
