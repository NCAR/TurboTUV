# R5 grouping assessment

`screen.json` contains all 144 samples and paired throughput summaries.
The comparison includes profile updates, grouped or ordinary model calls,
and full output copies on one thread. It is separate from primary latency.
No grouping width above one improves throughput reliably, so no public API
or production grouping implementation is retained.

The 24 `gate-*.json` files compare every value for 301 changing columns,
including partial groups, to the selected single-column binary. All are
bit-identical. The four `serial-*.json` checks validate the ordinary-call
control with independently owned model states. `test-*.log` covers per-column
errors, fallback, three-column tails and concurrent independent groups on
GCC, Clang, and ASan/UBSan.

`prototype-source.tar.gz` preserves the private C implementation, generated
rate chunks, benchmark, tests and scripts. The prototype checks matching
model/recipe identities and original callbacks before deferring rate work.
Unmatched groups use ordinary calls. Failed individual columns keep their
own statuses while valid columns finish their own generated integrals.
Nothing is added to the installed public API.

Run `commands.sh` from the repository root to reproduce on the recorded
ARM/macOS environment after building checkpoint 0872e28's selected and
sanitizer libraries. Keep timing runs separate from compilation. Compiler
paths, flags and source/binary hashes are recorded in `build-*.json`.
`resources-*.log` records separate setup and maximum-RSS probes. This evidence
does not claim native x86-64 performance or a hardware impossibility bound.
