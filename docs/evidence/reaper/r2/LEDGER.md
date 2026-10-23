# R2 experiment ledger

Every row passed both full-output screening gates before timing. Screening
uses 48 changing columns (seed 2026), then three rotating timing rounds with
300 columns repeated three times. Final acceptance timings are separate.

| Family | Experiment | tuv µs | ts1 µs | Emission/compile s | Object bytes | Selection |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| gcc | screen/ordered-32 | 978.16 | 948.03 | 7.85 | 1513208 | screened |
| gcc | screen/ordered-64 | 1001.73 | 952.56 | 8.33 | 1504768 | screened |
| gcc | screen/ordered-128 | 1028.75 | 960.67 | 9.37 | 1516224 | screened |
| gcc | screen/ordered-256 | 1032.28 | 959.08 | 9.78 | 1514320 | screened |
| gcc | screen/acc-2 | 989.63 | 948.41 | 8.35 | 1504736 | screened |
| gcc | screen/acc-4 | 979.17 | 946.35 | 8.69 | 1513840 | screened |
| gcc | screen/acc-8 | 976.49 | 946.67 | 8.99 | 1531992 | screened |
| gcc | screen/interleave-2 | 1011.08 | 960.60 | 4.38 | 742224 | screened |
| gcc | screen/interleave-4 | 1044.93 | 982.88 | 4.13 | 693856 | screened |
| gcc | screen/interleave-8 | 1019.88 | 963.28 | 4.84 | 784040 | screened |
| gcc | screen/fma | 983.22 | 948.64 | 8.15 | 1379848 | screened |
| gcc | screen/tile-8 | 1014.12 | 964.78 | 15.57 | 1769504 | screened |
| gcc | screen/tile-16 | 1007.25 | 963.66 | 15.59 | 1769392 | screened |
| gcc | screen/tile-32 | 1004.43 | 963.02 | 15.37 | 1809880 | screened |
| gcc | screen/reuse-products | 1048.99 | 978.28 | 4.11 | 693856 | screened |
| clang | screen/ordered-32 | 878.16 | 790.49 | 2.67 | 751328 | screened |
| clang | screen/ordered-64 | 910.83 | 799.88 | 2.79 | 814800 | screened |
| clang | screen/ordered-128 | 946.64 | 804.13 | 2.68 | 783032 | screened |
| clang | screen/ordered-256 | 960.87 | 805.52 | 2.69 | 787272 | screened |
| clang | screen/acc-2 | 889.45 | 795.72 | 3.75 | 936704 | screened |
| clang | screen/acc-4 | 886.55 | 796.70 | 3.84 | 946680 | screened |
| clang | screen/acc-8 | 883.84 | 797.80 | 3.86 | 955848 | screened |
| clang | screen/interleave-2 | 866.45 | 781.33 | 2.57 | 734232 | screened |
| clang | screen/interleave-4 | 862.19 | 785.96 | 2.56 | 738376 | screened |
| clang | screen/interleave-8 | 901.02 | 797.09 | 2.20 | 606680 | screened |
| clang | screen/fma | 884.51 | 794.78 | 2.88 | 738824 | screened |
| clang | screen/tile-8 | 902.52 | 803.31 | 5.04 | 1044200 | screened |
| clang | screen/tile-16 | 901.62 | 807.39 | 5.17 | 1100704 | screened |
| clang | screen/tile-32 | 897.45 | 849.88 | 5.04 | 1104832 | screened |
| gcc | combinations/reuse-products | 1045.20 | 978.15 | 4.13 | 693856 | screened |
| gcc | combinations/small-acc | 964.27 | 942.76 | 8.66 | 1572336 | selected |
| gcc | combinations/small-interleave | 1000.50 | 958.69 | 5.68 | 989616 | screened |
| gcc | combinations/large-interleave | 1022.70 | 964.85 | 3.96 | 641296 | screened |
| gcc | combinations/interleave-fma | 988.04 | 955.40 | 3.78 | 640888 | screened |
| clang | combinations/reuse-products | 862.76 | 784.85 | 2.65 | 738376 | screened |
| clang | combinations/small-acc | 868.26 | 786.71 | 3.15 | 891936 | screened |
| clang | combinations/small-interleave | 866.68 | 788.77 | 3.02 | 828328 | screened |
| clang | combinations/large-interleave | 942.74 | 789.52 | 2.56 | 671608 | screened |
| clang | combinations/interleave-fma | 851.62 | 778.11 | 2.45 | 649600 | selected |

Ordered 32-term functions were competitive and remain available for
diagnosis and deployments that require exact arithmetic. The selected GCC
combination improves on these further. Clang benefits from combining output
interleaving with FMA. No filename-specific runtime rule chooses a kernel:
attachment requires the full immutable model identity.

Large helpers, wider level tiles and most GCC interleaving variants regressed.
Explicit product reuse saves at most 66/2,962 nonconstant products in tuv
and 45/1,885 in ts1 even before restricting reuse to one bounded helper;
its extra code/layout did not beat the selected combinations.

Validation notes: the first separate REAPER tolerance wrapper rejected legacy
standalone cases whose exact reference already overflowed. It was corrected
to preserve identical nonfinite bit patterns; changed nonfinite outputs fail.
The complete-model finite requirement was unchanged. A custom held-out grid
initially violated the pre-existing O2 band requirement; the final fixtures
retain the complete fixed bands and vary the rest of the grid.

Two reporting errors were corrected before the retained final report: screening
overwrote its short config key with the benchmark path, and repeated
`--allow-differences` options kept only the last value. Timing runs were
repeated after the fixes. No failed report supplied the final speedup claim.

Plans and emitted source are gzip archives with deterministic timestamps.
`generated/manifest.json` records their uncompressed SHA-256 and byte counts.
Build objects and executables remain disposable under `build/reaper/`.
