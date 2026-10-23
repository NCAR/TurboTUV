# Spectral follow-up ledger

Screening starts at `f1512c1`, with the existing version-2 rate artifacts. All
experiments use C11/libm and unchanged scientific gates. Compiler/build work
and performance runs are serial. The four independent MPFR correctness
processes ran concurrently before acceptance timing began.

| Candidate | Result | Decision |
| --- | --- | --- |
| Consecutive OCLO bracket runs | Keeps the original interpolation arithmetic; saves roughly 8–14 us per primary column in short screens | Retain for at least four active levels; run endpoints use existing scratch and exact-integer guards |
| 256-entry degree-four math | Passes catalogue, stress and primary screens; smaller improvement than the combined 2048-entry candidate | Archive as unselected |
| 512-entry degree-four math | Passes catalogue, stress and primary screens; no advantage over the selected combination | Archive as unselected |
| 2048-entry cubic math | Passes independent reduction/error argument, expanded MPFR boundaries, exact table oracle, and full scientific validation | Retain; 16,384-byte constant table, normal-domain guards and libm fallback |
| OCLO plus 2048-entry math | Improves primary times; initial full validation passes, but one-layer Clang timing regresses | Add a general short-column rule before final selection |
| Skip OCLO runs below four levels | Restores near-control tiny-case timing in six rounds | Narrower alternative not selected |
| Scalar direct spectra below four active levels | Passes catalogue, short-shape and primary gates; tiny-case paired gain 1.019x over main | Retain; packed active subsets use the same rule |

The initial combined build, every slow sample, and the tiny-case regression
remain in the evidence. `combined/` is not silently relabeled as the final
build. Final acceptance measurements are in `final/`; only these establish
the reported performance of the shipped source. The exponential helper has
not changed between those builds, so its boundary oracle is reused with source
identity checks rather than rerun after the scalar-dispatch change.
