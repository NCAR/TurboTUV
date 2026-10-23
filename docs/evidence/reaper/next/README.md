# Spectral conditioning checkpoint

The selected follow-up improves spectral loop layout and adds guarded positive
interpolation weights with libm/ordered replay for sensitive inputs. It fixes
a newly found 59.726-epsilon stress case in the preceding selected source.
The four primary paired gains are 1.545/1.650x for TUV and 1.995/1.831x for
TS1 (GCC/Clang). The four-case 2x target remains unresolved.

- `LEDGER.md`: screened candidate families, rejected transformations and limits.
- `commands.sh`: serial reproduction of selected validation and measurement;
  prerequisites are the R0 frozen controls/references and R4 static artifacts.
- `prototype-source.tar.gz`: exact experiment sources, original stress harness,
  preparation scripts and selected-validation drivers. Historical screen sources
  depend on the `0872e28` base archives; do not regenerate them from current code.
- `interpolation*`, `*-interpolation`, `slant-levels`, `mix-tiles`: exploratory
  gates/timings, including numerical failures and rejected prototypes.
- `combined`: 51 C/Fortran tests per compiler, 30 sanitizer tests, 30 tests in
  each hint-disabled build, focused final checks, scalar fallback checks,
  installed consumers, MPFR oracles, primary/held-out/untuned gates, separate
  profiles, resources and all 80 primary and 96 secondary timing samples.
- `combined/timing-provenance.json`: source state at primary measurement.
  The final binary-representation guard is recorded separately; all twelve
  checked benchmark/object hashes are unchanged. Final source provenance is
  in `combined/provenance.json`.
- `summary.json`, `manifest.json`: numerical totals, performance/dispatch checks
  and SHA-256 hashes of every evidence file other than the manifest itself.

The pointwise exponential approximation and version-2 rate artifacts are
unchanged. Standalone interpolation stress results are measured evidence, not
a uniform proof for every possible spectrum. Verification is ARM/macOS;
x86-64 Linux execution remains unverified. See `../../../REAPER_RESULTS.md`
for the full numerical, timing and resource scope.
