# OCLO bracket reuse and a larger bounded spectral exponential

This follow-up starts at `f1512c1`. It selects consecutive OCLO temperature
bracket reuse and a 2048-entry cubic spectral exp/pow10 helper. The independent
uniform bound is below 3.16 epsilon. Prepared and generic execution retain
their original arithmetic; transfer exponentials still use libm.

`sweep-{gcc,clang}.json` records isolated and combined screening against the
preceding selected binary. All 24 primary screening gates, both compiler
catalogues and 10,000-spectrum stress checks pass. Three-round, 300-column
screens are candidate selection evidence, not 2x acceptance.

`oracle/` retains the expanded MPFR comparisons: 121,614,882 exp inputs and
121,348,084 pow10 inputs per compiler, including every reduction integer and
half-integer boundary and neighboring binary64 values. All differences are
below 1.995 epsilon. `provenance.json` in that directory proves equivalent
preprocessed C tokens between the measured prototype and selected oracle
translation units. `combined/math-bound-{gcc,clang}.json` adds independently
derived reduction and rounding bounds and MPFR verification of every table
constant with an exact input and a 53-bit destination. The complete-domain
argument is in `../../../REAPER_MATH.md`; samples alone do not supply it.

`combined/` is the first full validation and ten-round timing checkpoint. Its
TS1/GCC 95% lower speedup bound is 2.027x, but a secondary one-layer Clang
regression requires further investigation. `tiny/` retains six rotating
rounds for nine configurations of the implementation, all numerically gated.
The larger table alone is near the control, while combining it with OCLO run
construction increases short-column overhead. Reusing scalar direct formulas
below four active levels restores a 1.019x paired gain over frozen main.
`tiny-variants/` retains catalogue and shape/primary screening gates for that
rule and a narrower OCLO-only alternative. These are general level-count
rules, not configuration-name shortcuts.

`final/` validates and times the retained combined implementation with the
four-level cutoff. The math helper and expanded MPFR evidence are unchanged;
the final independent-bound records are hash-identified copies. Full compiler,
Fortran, sanitizer, hint-disabled, installed-consumer, held-out and untuned
checks use the final CMake builds. Primary timings use ten rotating paired
rounds, with 300 changing columns repeated ten times and every sample over
one second. Separate profiles and resource probes do not enter those timings.

`commands.sh` and `source.tar.gz` preserve reproduction drivers and exact
screened source. `summary.json` verifies the final gates, dispatch counts,
source/binary identities and timings. `manifest.json` identifies every retained
evidence file. The user clarified the session goal as at least one primary case above 2x.
That goal is achieved; further optimization of the remaining three cases is
deferred to follow-on sessions. No impossibility claim is made.
Execution is verified on ARM/macOS only; x86-64 Linux remains unverified.
