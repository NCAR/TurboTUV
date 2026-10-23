# REAPER experiment tools

The C11 library and an already built executable do not need Python. These
measurement tools use Python; `compare.py` and `profile.py` also need NumPy.

`compare.py REFERENCE CANDIDATE --config FILE --json REPORT` runs both column
benchmarks with the same seed and all output values enabled. Program arguments
use `PATH:--generic` or, once implemented, `PATH:--reaper`. The comparison reads
labels and grids independently from each build's adjacent `turbotuv` CLI. It
checks exact shapes/metadata and output lengths before comparing each field.
The six radiation fields use 64 epsilon units with a unit floor; actinic flux,
spectral irradiance and rates use a strictly relative 64 epsilon gate. Zeros,
finiteness, and negative departures from nonnegative reference values are
checked separately. No underflow exception is silently introduced. The report
records absolute, relative, epsilon-scaled and ULP maxima with their column,
row (wavelength or rate index), and level. Raw doubles are temporary artifacts.
A passing report identifies both exact binaries by SHA-256.

`../bench_compare.py --gate REPORT ...` requires matching successful numerical
evidence for every timed binary/configuration, including execution selection,
seed, and column count. Repeats may be increased to provide at least one second
of timed work. Use ten or more rotating rounds and `--minimum-seconds 1` for
acceptance. JSON reports retain every sample and per-round speedup, medians,
ranges, and a deterministic one-sided 95% bootstrap lower bound on the paired
median (20,000 resamples of rounds). A 2x claim requires all four lower bounds
to reach 2.0. `--allow-differences` alone is rejected; `--ungated` explicitly
permits historical or exploratory timing, which cannot establish acceptance.

`profile.py` records a `TURBOTUV_PROFILE=ON` build's stage times and counters.
Transfer includes its mixing/slant/coefficient/solve/output substages: do not
sum both the parent and the children. Stage timing uses C11 `timespec_get` and
is for attribution only; the uninstrumented benchmark uses a monotonic clock.
`--detail` adds separate host-update and model-run measurements, with an extra
clock read, and is likewise supplemental to the unchanged primary boundary.
Every benchmark reports actual prepared/generic successes and declines;
counts exclude the untimed warm-up. Profiling-disabled builds have no library
clock calls. Counter state is model-owned, like the numerical workspace.

`provenance.py` captures compiler identities, flags, CPU/OS/power, input and
binary hashes, and actual CTest inventories. R0's machine-specific commands
are in `docs/evidence/reaper/baseline/commands.sh`; paths to compiler-matched
NetCDF Fortran installations must be adjusted on another machine. The original
reference uses C++ only in its upstream dependencies/tests, never in TurboTUV's
numerical library.


`overhead_sweep.py` builds R1's isolated tile/overlap/O2/flux experiments,
then times them serially in rotating rounds. It records compile/link commands,
object and binary sizes, hashes, per-column execution counts and every sample.
`--combined-only` tests the selected tile with both flux-check implementations.
Prototype archives replace exactly `model_plan.c.o` and `spectrum_plan.c.o`;
final retained binaries are rebuilt and validated through CMake. No generated
functions or different floating-point options are involved in this sweep.
