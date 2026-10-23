# Spectral conditioning follow-up after R6

These experiments start from selected checkpoint `0872e28`, with version-2
static rate artifacts. Screens use three rotating rounds, 300 changing
columns and three repeats. Only `combined/timings.json` provides the new
primary ten-round result. Source archives retain both successful and rejected
experiments; do not infer a selected optimization by finding prototype code.

| Directory | Experiment | Decision |
| --- | --- | --- |
| `interpolation` | Reciprocal slopes, positive endpoint weights, merged point weights, and FMA | Catalogue/primary screens pass and TS1 improves by about 10–16 microseconds. Unrestricted reassociation does not preserve the desired ordered fallback for signed data. Do not select these prototypes without conditioning guards. |
| `slant-levels` | Share optical-depth loads across two/three/four ordered slant rows, with 8/16 lanes | All gates pass. Changes are small and compiler-dependent; no useful consistent gain justifies selection. |
| `mix-tiles` | Tile ordered radiator mixing in 32/128/512/2048-cell blocks | All gates pass. Small TUV gains are offset by neutral/slower TS1 results. Not selected. |
| `guarded-interpolation` | Scan every intermediate value before using weighted segments | Clang TS1 regresses; stop this version after its Clang screen. Per-column checking costs more than the saved divisions. |
| `checked-interpolation` | Reuse the existing exponential domain result and static raw-data classification | Both compiler catalogues/primary screens pass. A new independent stress test still exposes the pre-existing endpoint-cancellation issue; not selected. |
| `adaptive-interpolation` | Guard each risky overlap and replay the level block with libm and ordered interpolation | New stress cases pass, but the per-overlap guard and replay are too costly. |
| `conditioned-interpolation` | Estimate conditioning across the complete bin, with optional weights | Stress cases pass. A separate conditioning traversal still adds substantial cost in Clang TS1. |
| `fused-interpolation` | Estimate conditioning while accumulating weighted integrals; combine cached-run copies and level-contiguous HCFC loops | Selected after full validation. The pointwise exponential algorithm and its bounds are unchanged. |
| `interpolation-stress` | Fresh deterministic spectra with narrow bins, large contrasts, signed data and extreme exponents | The prior selected implementation reaches 59.726 epsilon against the 32-epsilon standalone gate. The new conditioning fallback fixes this case. Signed-data checks require exact ordered replay; unchecked prototypes do not meet that additional requirement. |

The stress case uses valid numerical inputs with a deliberately very large
spectral contrast; it is not a failed primary atmospheric column. Historical
primary gates remain valid for their recorded inputs. The new regression test
preserves the exact prepared/generic comparison and exercises optional-metadata
allocation failure as well as the roundoff mode.

Simple temperature-translation reuse was considered but not implemented:
primary columns also change the amplitude and phase of a vertical sinusoidal
temperature perturbation. A translation-only cache would rarely apply. This
observation does not rule out more general reuse.

The selected implementation keeps ordered prepared/generic paths, makes no
change to transfer arithmetic, and leaves all rejected layout variants outside
the library. Full validation and ten-round measurements give 1.545/1.650x for
TUV and 1.995/1.831x for TS1 (GCC/Clang); all lower confidence bounds remain
below two. The four-case 2x objective remains unresolved. The new stress test
passes with maximum 12.910 epsilon and 53 optional-metadata allocation failures.
