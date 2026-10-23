# Further screening after checkpoint 0872e28

These are isolated prototype experiments against the selected R3 follow-up.
None changes the production library at this checkpoint. Timing sweeps use
three rotating rounds, 300 changing columns and three repeats; they are
screening measurements rather than primary acceptance evidence.

| Directory | Experiment | Result |
| --- | --- | --- |
| `accurate-transfer` | Compensated range reduction/table reconstruction and a degree-seven exponential | The 4,832,736-input MPFR screen improves local accuracy to below one epsilon, but all slant/coefficient/both integration variants still fail full-output gates, including near-zero signs and downstream flux/rates. Reject before timing. This unselected prototype has no completed independent uniform proof. |
| `spectral-scatter` | Classify cached temperature regions once; gather indices or copy contiguous runs | All catalogue/model gates pass. Runs save roughly 4–5 µs for TUV in the short screen; TS1 results are neutral or compiler-dependent. Retain as a candidate for a later combined comparison, not as a selected change. |
| `spectral-fusion` | Fuse affine exponential inputs/results, combine Cl2 exponentials, and transpose HCFC coefficient loops | All gates pass. Clang's combined TUV screen saves about 9 µs; GCC and TS1 gains are smaller. No new approximation is introduced. Candidate only; not yet selected or subjected to final combined validation. |
| `zero-moment` | Per-layer zero-moment shortcuts in scaling and coefficients | All gates pass. Coefficient shortcuts are slower for Clang; scaling savings are small and inconsistent. Reject this implementation. |
| `wide-slant` | Slant blocks 16/32/64/128 with coefficient/solve blocks fixed at 16 | All gates pass. Wider layouts are slower in the combined transfer loop. Reject. |

The earlier whole-atmosphere symmetry experiment could not use a shortcut in
models containing aerosols. The new zero-moment screen instead checks each
layer: 10,920 of 18,720 primary TUV aerosol cells have zero moment. Even with
that coverage, the measured guards/layout do not provide a useful combined
gain. This observation is not a bound on all possible specializations.

Each directory retains its prototype source, driver, binary hashes, catalogue
result, full-value gates, and available timing samples. Failed numerical
variants are not timed. The selected library remains checkpoint 0872e28;
the four-case 2x target is not achieved and feasibility remains unresolved.
