# R3 experiment ledger

All timings in the subdirectories are short screening measurements. Only
`timings.json` at this directory's root uses the final combined CMake binaries,
fresh 300-column gates, ten rotating paired rounds, and at least one second of
timed work per sample. Do not multiply isolated gains or use screening samples
as acceptance evidence. Some early `aliases`, `tables`, and Clang `slants`
samples were noisy; the slant experiment overlapped compilation and is not
evidence of a performance win.

| Evidence directory | Experiment | Decision |
| --- | --- | --- |
| `cache` | Fixed temperature-range signatures and direct-formula clamp rows | Keep fixed-range reuse and caches for expensive clamped formulas. Caching cheap formulas adds packing/scattering overhead. |
| `transfer` | Contiguous mixing, wavelength blocks 2/4/8/16/32 | Keep block 16. Small blocks lose instruction parallelism; block 32 increases work and register pressure. |
| `aliases` | Disjoint workspace aliases, contiguous mixing, per-column role classification | Keep disjoint aliases and contiguous mixing as the basis for later guard/role changes. Per-column role scans are too costly. |
| `guards` | Boolean reductions without short-circuit exits | Keep bitwise validation reductions; compilers can vectorize these while preserving invalid-input checks. |
| `math` | Degree-13 reduced exponential in spectra or transfer | Spectral gates pass, but the long polynomial gives little or negative benefit. Transfer variants fail numerical gates and are rejected before timing. |
| `table-math` | 64-entry, degree-5 bounded exp/pow10 | Keep only in spectra, with documented domain guards and independent error analysis. Transfer trials again fail zeros/sign/cancellation checks. |
| `tables` | Two-temperature table specialization and level-contiguous interpolation | Keep together with bounded spectral math. Operation order within each output is unchanged. |
| `slants` | Interleave 1/2/4 slant rows, blocks 8/16 | Keep the original row traversal. No reliable combined improvement justifies the extra loops. |
| `parameter-tiles` | Conserving interpolation across 2/4/8/16/32 levels | Advance to the combined parameterization/interpolation experiment. |
| `parameter-batch` | Parameterization and interpolation across independent levels | Keep 4 levels for Clang and 8 for GCC. The final build additionally exposes the constant exp/pow10 choice to the compiler. |
| `coefficients` | Cache immutable radiator roles; isolate coefficient branches; skip trivial exponentials | Keep role caching and independent coefficient loops. Reject exp-zero shortcuts. Successful radiator replacement invalidates the role cache. |
| `roots` | Checked nonnegative sqrt loop, blocks 8/16/32 | Keep the checked loop for GCC and the original sqrt expression for Clang; block 16 remains the combined default. |
| `oxygen` | Ordered Chebyshev recurrences across independent levels | Keep 32 levels for GCC, 64 for Clang. Public/generic O2 evaluation is unchanged. |
| `symmetric` | Zero-moment mixture shortcut, blocks 8/16/32 | Reject: no primary-workload gain, and the usual aerosols have a nonzero moment. |
| `kinds` | Instrument individual direct/parameterized spectrum kinds | Attribution only. Remaining direct formula/libm and parameterized exponential work motivates further specialization. |

Each sweep JSON retains exact compile commands, source/binary hashes, output
gates, code sizes where measured, and every timing sample. Gzipped source files
preserve the experimental implementations, including rejected variants removed
from the shipping library. `prototype-source.tar.gz` preserves the pre-cleanup
source context. Historical sweep flags must be used with their archived source
and the base archive named in the report; several controls no longer exist in
the cleaned implementation. The final selected source is the R3 commit, with
individual hashes in `provenance.json`.

The full GCC and Clang suites each pass 49 tests, including direct Fortran
spectrum/model/host comparisons. The sanitizer suite passes all 28 tests;
after a final O2 predicate correction, both full compiler suites and the six
affected sanitizer tests were rerun. The four final full-value gates compare
165,382,800 values. Six held-out models per compiler pass ordered and roundoff
generated-rate gates, exact ordered replay against the REAPER interpreter,
and mismatched-primary-artifact fallback. These held-out grids and rate lists
are not used to select tuning parameters.

`REAPER_MATH.md` supplies a uniform error bound for the spectrum-only math.
The independent MPFR oracle checks 4,832,736 exp inputs and 4,824,406 pow10
inputs per compiler, including every reduction boundary and its neighbors.
All four runs stay below 1.905 epsilon relative to the correctly rounded
oracle. MPFR, GMP, and Python are development tools, not runtime dependencies.

The target remains **not achieved; feasibility unresolved**. Neither rejected
arithmetic variants nor the measured residual cost proves impossibility.
Further generated spectrum specialization, transfer producer/consumer fusion,
and other roundoff-safe transformations remain available to investigate.
