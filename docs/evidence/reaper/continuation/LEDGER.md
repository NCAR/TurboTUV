# Transfer and spectrum follow-up after R4

This checkpoint continues R3 tuning against the R4 static artifacts. All
subdirectory sweeps use three rotating screening rounds of 300 columns
repeated three times. Only `combined/timings.json` is the final ten-round
comparison with frozen prepared main. Do not add or multiply isolated gains.

| Directory | Experiment | Decision |
| --- | --- | --- |
| `math-lanes` | Independent exponential lanes, Clang vector width/interleave and GCC unroll factors | Keep width 2/interleave 2 for Clang and unroll 4 for GCC. The 64-entry table, degree-five polynomial, range reduction and guards are unchanged. Hints have a plain C fallback and can be disabled. |
| `slant-sums` | Two/four/eight partial sums and explicit FMA | Reject every variant before timing. Small slant changes are amplified by the difference of adjacent slant depths in the transfer coefficients; full-output gates fail, including physical zeros/signs. This is not an impossibility proof. |
| `fusion` | Forward elimination immediately after each layer's coefficients, blocks 8/16/32 | Keep block 16. Top boundary and interior rows retain the scalar operation order. The final pass handles the surface boundary and back substitution. |
| `compact` | Reuse consumed scaling rows for coefficients and alternate two particular-solution rows | Keep for GCC; Clang retains the full workspace because compact storage regresses its timings. The compact layout uses `(12*n+7)*block` doubles instead of `(17*n+2)*block`, plus the unchanged mixing arrays. Restricted accesses to overlaid storage derive from the same writable base. |
| `table-size` | Tables of 256/512/1024/2048 values with shorter polynomials | Not selected. Catalogue and model screening gates pass, but expanded independent boundary-oracle validation was not completed. These prototypes must not be treated as validated math implementations. |
| `flux-fusion` | Form actinic flux and irradiance during radiation output | Reject: no useful combined gain. The proposed private view/API is not part of the library. |
| `small-arguments` | Degree-eight/nine/ten exponential polynomials on small intervals | Reject: slower in both primary models despite passing screening gates. No new polynomial is shipped. |
| `flux-scale` | Reciprocal irradiance scaling, regrouped scale factors, vectorizable checks | Keep only the bitwise finiteness reduction, with each predicate normalized to zero/one as required by portable C. Reciprocal/regrouped arithmetic adds overhead and is rejected. |

The selected solver changes and math hints reproduce R4 output hashes on all
four 300-column gates. The independent math oracle is rerun with the selected
compiler hints; its uniform bound remains the one in `docs/REAPER_MATH.md`.
Existing exact prepared tests remain intact. Full compiler/reference suites,
sanitizers, altered-grid/rate-list tests, hint-disabled builds, source hashes,
and fresh combined performance measurements are recorded under `combined`.

Each sweep records its compile commands, binary hashes, catalogue result,
full-output gates and every timing sample. Gzipped files retain experimental
source, including rejected implementations. Use them with the R4 base library
and version-2 generated artifacts named in the commands. The selected source
is the commit containing this ledger; `combined/provenance.json` identifies
its source and binary hashes.

The four-case 2x target remains open. These experiments establish neither a
2x result nor a quantitative lower bound that rules one out.
