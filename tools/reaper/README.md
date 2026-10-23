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

## Static rate generation

Build a normal library first. Its `turbotuv_reaper_export CONFIG PLAN.json`
program exports the C builder's actual ordered terms, immutable spectrum
state, slot identities, shapes, and bit-exact constants. Python does not
parse the scientific configuration or decide whether a spectrum is constant.
The canonical JSON uses IEEE binary64 hexadecimal bit strings, locale-free
integers, and SHA-256. Export is unavailable on other floating-point formats;
ordinary C execution remains available. Mutable column profiles and pointers
are absent from this identity. Profiles, solar/surface values, geometry,
optical properties, and supplied radiation are read on each call.

For example, run these commands from the repository root (choose the compiler
and platform flags used for the library):

```sh
build/release/turbotuv_reaper_export examples/tuv_5_4.json build/tuv-plan.json
python3 tools/reaper/artifact.py build/tuv-plan.json --directory build/tuv-kernel \
  --cc cc --term-budget 64 --accumulators 1
cmake -S . -B build/generated -DCMAKE_BUILD_TYPE=Release \
  "-DTURBOTUV_REAPER_SOURCES=$PWD/build/tuv-kernel/kernel-0.c;$PWD/build/tuv-kernel/registry.c"
cmake --build build/generated
```

Supply several exported plans to `artifact.py` to create one registry. The
compiler used for export and the one compiling generated code must honor the
same numerical options; `-ffp-contract=off` is used throughout these
experiments. The artifact record preserves compile arguments, plan/source
hashes, source/object sizes, and generation/compile elapsed time. No target
specific instructions or nonstandard vector types are emitted. Already
compiled artifacts require neither Python nor an external compiler.

The default generated reduction preserves the ordered arithmetic. Separate
options select 2/4/8 accumulators, standard `fma`, interleaved outputs, repeated
products, and level tiles. These are experiments under the documented
roundoff gate, not permission to enable `-ffast-math`. Term budgets bound
individual helpers to 32/64/128/256 source terms; large rates retain bounded
chunks, and the level dimension remains a C loop. Constants are emitted as
exact C11 hexadecimal literals. Calling the emitter twice on identical inputs
and options produces identical C.

Selecting `TT_EXECUTION_REAPER` searches an optional static registry once per
model. A matching descriptor is copied into model-owned state. Every family
reads explicit buffer views; code is immutable and may be shared by models.
`tt_model_attach_reaper` also supports an externally linked descriptor. Null
family pointers leave those families interpreted, a kernel that returns zero
has its partial family output overwritten by the interpreter, and a null
descriptor detaches. Code must remain alive until detached; static linking
satisfies this requirement without module reference counts. The default
prepared and generic selectors never invoke these functions.

ABI, schema, implementation, shape, layout options, numerical policy, and
model fingerprint are checked before attachment. An explicit mismatch returns
`TT_INVALID` with a reason and preserves the old attachment. No matching static
artifact leaves ordinary REAPER execution available. Capability bits identify
photolysis/heating/dose coverage; `generated_calls` counts accepted family calls,
not columns. Benchmark `preparation_ms` reports selection/fingerprinting time
separately from ordinary `setup_ms` and the unchanged timed column boundary.
The library never launches processes or opens a persistent binary cache.

`rate_sweep.py` records generated-versus-interpreted experiments, every full
output gate, source/object/binary sizes and hashes, and rotating timing rounds.
Its short screening workloads select candidates; final claims use the full
300-column, ten-round protocol above. Generated artifacts are optional;
`TURBOTUV_REAPER_SOURCES` empty builds all ordinary execution paths without
Python, generation, or dynamic loading.

## Spectrum and transfer experiments

R3's selected implementation and rejected trials are recorded in
[`docs/evidence/reaper/r3/LEDGER.md`](../../docs/evidence/reaper/r3/LEDGER.md).
`spectrum_sweep.py`, `transfer_sweep.py`, `math_sweep.py`, and `oxygen_sweep.py`
build isolated archive-member replacements for screening. Their historical
reports include exact commands and archived experimental source. Use that
source and its matching base archive when reproducing old variants; removed
experiment macros have no effect in the final cleaned implementation.
Final acceptance always uses rebuilt CMake binaries and full-value gates.

The current descriptor/export format is version 2. Regenerate older artifacts;
do not combine a historical generated initializer with a newer public header.
The emitter records every numerical/layout choice and hashes a canonical
recipe that the C attacher independently recomputes. `artifact.py` also records
compiler binary/version/target, effective flags, host information, source/object
hashes, and an offline build identity. This is provenance for static deployment,
not a persistent runtime compilation cache. Historic evidence is reproduced
with the source and tools at its phase commit.

`tests/installed` is a standalone CMake consumer of an installed package. It
checks generated and ordinary static libraries, destroys configuration before
execution, verifies per-model selection reuse and detach, and queries actual
execution counts. R4 runs this consumer with `PATH=/nonexistent` after building
to demonstrate that execution does not locate Python or a compiler. A separate
test compiles a private model-plan allocation shim; no fault-injection state is
present in the shipped library.

`check_math_bounds.py` independently checks the spectral exponential's table
constants and conservative error budget using Decimal arithmetic.
`check_math.c` uses MPFR/GMP as development-only correctly rounded oracles;
it checks every reduction boundary and neighboring binary64 inputs, domain
endpoints, dense residuals, and seeded random samples. See
[`docs/REAPER_MATH.md`](../../docs/REAPER_MATH.md) for the uniform error argument.
The ordinary test suite checks exceptional arrays and rounding-mode fallback
without requiring MPFR. The numerical library still depends only on C11/libm.
