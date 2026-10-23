# R6 selected-implementation validation

This checkpoint validates numerical source `0872e28`. It adds no numerical
implementation changes. Full C/Fortran, sanitizer, plain-C fallback, math oracle,
primary gates and ten-round measurements are in `../continuation/combined/`;
R4 records artifact compile costs and attachment/lifecycle validation.
`../../../REAPER_RESULTS.md` gives the combined result and limits.

- `commands.sh`: reproducible ARM/macOS commands with named prerequisites.
- `secondary-source.tar.gz`: host benchmark source and builder, exact untuned
  configurations, installation/audit drivers and serial timing driver.
- `heldout/`: 16 fresh-seed 300-column gates and all 96 secondary timing samples.
  Host radiation formation/upload is timed on both sides; hashing is outside.
- `consumer-*`, `dependencies-*`: eight installed executions without tools on
  PATH; minimal and generated libraries on GCC and Clang.
- `*-audit.log`, `reference-regeneration.json`: data, fixtures, C catalogues and
  isolated byte-for-byte Fortran reference regeneration against pinned TUV-x.
- `docker-*.log`: unsuccessful Linux-environment availability attempt and
  restoration of its initial stopped state. No x86-64 result is claimed.
- `provenance.json`, `summary.json`, `manifest.json`: sources, binaries, compiler
  identity, numerical totals and evidence hashes.

Secondary three-round performance screens show no slowdown above 5%; they do
not replace primary ten-round acceptance. The primary four-case >=2x target
is not achieved; feasibility remains unresolved.
