# Generated rate layout screening after 501c331

This assessment keeps the published version-2 rate artifacts unchanged. It
screens smaller helpers (8/16 terms), aligned wavelength traversal within
interleaved output groups, and deterministic greedy grouping by wavelength
set overlap. Every output's terms retain ascending wavelength order within
its accumulator. All candidates pass both 48-column primary numerical gates
and confirm attached generated execution before timing.

The initial screens use three rotating rounds of 300 columns repeated three
times: 32 gates and 108 timing samples across both compilers. Independent
five-round alternating confirmation of the best small candidates uses eight
repeats, retaining all 40 samples above one second.

| Candidate | Compiler | TUV gain over selected | TS1 gain over selected | Decision |
| --- | --- | ---: | ---: | --- |
| 16 terms, 4 accumulators | GCC | 1.00007x | 1.00040x | No useful confirmed gain; not selected. |
| Aligned, overlap-grouped, 4 outputs, 32 terms | Clang | 1.00410x | 1.00753x | Small gain only; retain as a candidate for combination, not a selected artifact. |

Most grouped GCC variants regress, and 8-term helpers regress on both
compilers. The first confirmation attempt used six repeats and stopped when
a sample was shorter than one second; its failure log is retained. The
complete confirmation was rerun with eight repeats. These measurements do
not replace the four primary ten-round acceptance timings.

`prototype-source.tar.gz` contains the exact generators, copied model-plan
validator (extended only to allow 8/16-term experimental recipes), preparation
and sweep scripts, and emitted sources/plans for every variant. Experimental
alignment/grouping is identified by source hashes and explicit report fields;
the private version-2 descriptors do not encode those new choices. They must
not be published as production artifacts. Retention would require supported
recipe flags, independent attachment validation and full generated-kernel
validation.

`spectrum-profile/` records an independent, single-thread attribution run
using a private timing wrapper. CL2 remains the largest individual direct
spectrum cost, about 40 us per TUV column and 26 us per TS1 column. Aggregate
exponential and Harwood parameterizations dominate TS1's spectral work. The
wrapper adds instrumentation overhead and supplies no acceptance timing.

Reproduce from repository checkpoint `501c331`: extract the archive at the
repository root, preserve its `r3c-selected-*` base builds and R0 frozen
controls, then run `rate-layout-sweep.py clang`, `rate-layout-sweep.py gcc`,
`rate-layout-confirm.py` and `spectrum-cost-profile.py` serially. Sweep scripts
need the scientific Python environment recorded in the preceding checkpoint.
The preparation script is provenance; using it with newer source defines a
new experiment. `manifest.json` hashes every retained evidence file.
