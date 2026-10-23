# Conditioning checks for bins containing zero padding

This checkpoint corrects interpolation bins that combine ordinary segments
with zero-valued padding outside the guarded geometry. The ordinary segments
now retain positive weights and their conditioning check. A nonzero unguarded
segment still triggers libm and ordered replay for the active level block.
The exponential helper, generated rate artifacts, transfer code and default
prepared/generic arithmetic are unchanged.

Expanding the preceding implementation's stress test exposes a 33.307-epsilon
failure at trial 1338. The standard regression test now covers 10,000 spectra,
710,370 values and 527 injected allocation failures, with maximum 6.383 epsilon.
A separate 100,000-spectrum screen compares 7,135,407 values with maximum
9.162 epsilon and 5,264 allocation failures. Historical failures and the
larger-table candidates that revealed the omission are in
`../math-table-followup/diagnostic/`; no new math approximation is selected.

`combined/` retains full scientific/compiler, sanitizer, hint-disabled,
scalar-fallback, installed-consumer and numerical-gate results; separate
profiles/resources; and all primary and secondary timing samples. The
unchanged math helper is identified by SHA-256 and its preceding independent
bound/oracle evidence. `commands.sh` and `source.tar.gz` retain the serial
validation drivers. `summary.json` and `manifest.json` record verified totals,
dispatch counts, current binary identities and evidence hashes.

The four-case 2x objective remains unresolved. These tests establish neither
cross-architecture execution nor a lower bound ruling out further portable
improvements. See `../../../REAPER_RESULTS.md` for measured timing and resource
scope.
