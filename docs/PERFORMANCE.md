# Prepared execution

TurboTUV runs a column in one of two ways, with bit-identical results:

* **Generic** (`TT_EXECUTION_GENERIC`): the direct translation of TUV-x. Every
  run calls the public kernels in sequence: each spectrum at every level and
  wavelength, mixing, the Delta-Eddington or discrete-ordinate solver, and each
  rate as a sum over every wavelength.
* **Prepared** (`TT_EXECUTION_PREPARED`, the default): work that cannot change
  between columns is done once, when the model is built, and the rest runs in
  kernels arranged for the hardware. Every output value is the generic value,
  bit for bit.

The baseline is the reviewed generic implementation, preserved as branch
`baseline` and tag `v0.1.0-baseline`. Generic execution in the current library
is that same code.

## Results

`bench/turbotuv_bench.c` builds one model and evaluates 300 columns. Each column
changes the temperature (±10 K offset plus a ±8 K wave), the air and O2 profiles
(±5%), the O3 profile (±30%), the solar zenith angle (0–95°) and the radiation
scale (±1.7%) through the host-update API, so no column-dependent work can be
reused. `tools/bench_compare.py` ran the baseline and prepared programs in five
rotating rounds on an idle Apple M5 Pro (macOS 26.7.1, AC power), single
threaded, CMake Release (`-O3 -DNDEBUG -ffp-contract=off`). Each run printed a
hash of every radiation field and rate of every column. Within each compiler,
the baseline and prepared hashes were equal: the results are bit-identical.

Median time per column (range over the five rounds):

| Configuration | Compiler | Baseline | Prepared | Speedup |
| --- | --- | ---: | ---: | ---: |
| tuv_5_4: 156 wavelengths, 121 levels, 109 photolysis + 28 dose | Clang 23.1.3 | 5456 µs (5426–5554) | 1054 µs (1048–1059) | **5.18x** |
| | GCC 16.2.0 | 8091 µs (8076–8097) | 1064 µs (1056–1071) | **7.61x** |
| ts1_tsmlt: 102 wavelengths, 121 levels, 73 photolysis + 4 heating | Clang 23.1.3 | 4089 µs (4068–4123) | 946 µs (945–966) | **4.32x** |
| | GCC 16.2.0 | 5170 µs (5162–5233) | 1097 µs (1090–1106) | **4.71x** |

Building a model takes 3.5 ms longer for tuv_5_4 (16.5 ms instead of 13.0 ms)
and 1.8 ms longer for ts1_tsmlt; this cost is paid once per model. Peak memory
of the benchmark grows from 12.5 to 17.1 MB (tuv_5_4) and from 10.5 to 13.6 MB
(ts1_tsmlt), mostly the per-run rows of the varying spectra.

Reproduce with:

```sh
git worktree add ../TurboTUV-baseline baseline
cmake -S ../TurboTUV-baseline -B ../TurboTUV-baseline/build -DCMAKE_BUILD_TYPE=Release
cmake --build ../TurboTUV-baseline/build -j
python3 tools/bench_compare.py baseline-llvm=../TurboTUV-baseline/build/turbotuv_bench \
    prepared-llvm=build/release/turbotuv_bench \
    --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --rounds 5
```

`turbotuv_bench --generic` runs the generic path of the current build.

## Where the time went

Sampled profiles of the baseline showed that radiative transfer was not the
main cost. For tuv_5_4, about 78% of a column went to evaluating cross sections
and quantum yields, including their `exp`, `log` and `pow` calls. Integrating
the rates took 6.5%, and Delta-Eddington transfer with its slant paths and
tridiagonal solves took 8.5%. For ts1_tsmlt, the 20 temperature-parameterized
cross sections alone took 42% of a column. Each one was re-evaluated at every
level, with a conserving regrid per level.

Yet most of that work could not change between columns. Only 5.7% (tuv_5_4) and
8.2% (ts1_tsmlt) of the cross-section and yield values vary with the
atmospheric state. 61 of 109 and 30 of 73 reactions have spectra that are
entirely independent of it.

Prepared execution, per stage (instrumented, Clang):

| Stage | tuv_5_4 | ts1_tsmlt |
| --- | ---: | ---: |
| Delta-Eddington transfer and mixing | 409 µs | 283 µs |
| Varying reaction spectra | 250 µs | 442 µs |
| Rate integrals | 263 µs | 103 µs |
| O2 band optics (radiator and reactions) | 77 µs | 77 µs |
| Geometry, gas optics, actinic flux | 38 µs | 29 µs |

## Design

The design adapts the TurboMICM reference solution (`../TurboMICM/fable`) to
the structure of TUV-x. That solution has four parts: vector-ordered data with
lanes processed in lock step, specialized kernels prepared from the mechanism,
a fingerprint so that stale kernels cannot run, and the generic kernels kept as
the fallback and test reference. Here they become:

* **Plans instead of generated code.** TUV-x's work is dense arrays over
  wavelength and level, not a sparse system, so literal indices in generated C
  would save little. Instead a plan is built from the model's own prepared
  spectra when the model is created. Because it is derived from the model
  itself, it cannot go stale, which is what the fingerprint guarded against.
  * `spectrum_plan.c` classifies every wavelength of every cross section and
    quantum yield. A wavelength is constant where the kernel's own branches and
    table data make the value independent of temperature and air density: base
    tables, out-of-band branches, zero bases under a bounded exponential,
    temperature tables whose columns are equal, and so on. The generic kernel
    evaluates constant wavelengths once.
  * Varying wavelengths are evaluated with the kernel's expressions, rearranged
    only by hoisting subexpressions of wavelength alone (such as `log(329.5/λ)`
    for Cl2 or `pow(λ - λ0, p)` for the exponential parameterization) and of
    level alone (such as the acetone yield's eleven `pow` calls). The
    remaining operations keep the generic order.
  * A guard re-evaluates the generic kernel on reference columns that span every
    clamp and branch. A wavelength classified as constant must be constant
    there. If the guard fails, that spectrum falls back to generic evaluation at
    every wavelength. The tests require that this never happens.
  * Temperature-parameterized cross sections recompute only the target bins
    that touch parameterized points, with the same conserving arithmetic.
* **Rates as precomputed terms.** `model_plan.c` turns each photolysis, heating
  and dose rate into terms in ascending wavelength order. Where both factors
  are constant, a term carries the product `σφ` exactly as the generic rate
  forms it. Terms whose product is zero are omitted: adding `+0` or `-0` to a
  sum that starts at `+0` cannot change it, provided the flux is finite, which
  every run checks. Equal spectra are evaluated once and shared, for example
  the O3 cross section used by two reactions. Sums are tiled over levels so
  that partial sums stay in registers.
* **Lanes of wavelengths.** `radiation_block.c` solves Delta-Eddington transfer
  for blocks of 16 wavelengths in lock step. This is the analog of TurboMICM's
  blocks of cells, with each wavelength an independent problem sharing the ray
  geometry. Radiator mixing is fused into the first stage. The tridiagonal rows
  are assembled inside the elimination. `exp(-τ_slant)` is evaluated once per
  level instead of up to four times. Lanes past the end of the grid repeat the
  last wavelength. The compiler vectorizes across lanes from the data layout
  alone, with no intrinsics or pragmas.
* **The generic path stays.** `tt_model_set_execution` selects either path.
  When a prepared run meets an input outside its assumptions (invalid values,
  temperatures above 10⁴ K, or nonfinite radiation), it declines, and the
  generic sequence runs instead. Results and error statuses are therefore those
  of the generic path for every input. Discrete-ordinate transfer uses the
  generic solver; its spectra and rates are prepared.

TurboMICM also offered a "fast" mode, which changed results at roundoff
(reassociated sums and reciprocal multiplies). That was not needed here: every
transformation above preserves each floating-point operation and its order. No
fused multiply-add is introduced, and no vectorized math library is used. The
exponentials and powers are the platform's scalar libm calls, as in the
generic path.

## Validation

* `tests/test_prepared.c` checks the prepared path against the generic path,
  bit for bit:
  * every spectrum plan against its generic kernel, for all 912 cross-section
    and quantum-yield configurations of the spectrum test catalogue, including
    every kind and all four parameterizations. Each is evaluated at 41 levels
    with randomized and branch-edge temperatures and densities;
  * the blocked Delta-Eddington solver against mixing plus the generic solver,
    on 550 atmospheres: 1–81 layers, zenith 0–180°, vacuum, thin and thick
    layers, pure scattering and surface albedos 0–1, with partial and multiple
    blocks;
  * all 26 runnable model configurations on 14 perturbed columns each. These
    include daylight, twilight, night, a level beyond the plans' temperature
    limit and an invalid level, and compare every field, rate and radiator view
    together with run statuses and error messages.
  In all, 124 million values are compared per toolchain.
* The reference suites now run the prepared path by default and still match
  the original Fortran: bit-identical with GCC/GFortran, within one relative
  epsilon with Clang/Flang, as before. The host-update suite runs every column
  both ways.
* All tests pass with GCC 16.2.0 and Clang 23.1.3 and under Clang's address,
  undefined-behavior and leak sanitizers. The concurrent-model test passes
  ThreadSanitizer.
* The benchmark hashes above compare the prepared path with the preserved
  baseline build over 300 columns of every output.

## Further opportunities

The prepared path is now bound by irreducible scalar `exp`, `pow` and `log`
calls, and by work that is inherently O(levels²): the slant optical depths of
the spherical geometry. Remaining measured opportunities, none needed for the
present speedups:

* register blocking of the rate integrals across reactions;
* computing the Schumann-Runge cross sections once for both the O2 radiator
  and the O2 photolysis reaction (77 µs per column today);
* sharing parameterized-cross-section levels that fall in the same fixed
  temperature range (most ts1 parameterizations fix T below 200–280 K);
* threads: models are independent, so a host can run one per thread, as
  `tests/test_parallel.c` does.
