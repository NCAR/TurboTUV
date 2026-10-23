# REAPER session handoff — 2026-10-23

The session goal is complete: TS1/GCC achieves 2.030x over frozen prepared main,
with a 95% lower speedup bound of 2.025x. The user clarified that one passing
case is sufficient and requested a graceful closeout. Do not continue tuning
in this session. Work on the other three cases belongs in follow-on sessions.

## Final state

- Source branch: `reaper`. Frozen control: `9e149851af5bc48fc7ce64b5fbe50c6959ff2d51`.
- R0–R6 and follow-ups are committed by phase; the latest source selects OCLO
  bracket reuse, a 2048-entry cubic exp/pow10 helper, and scalar direct spectra
  below four active levels. Prepared/generic arithmetic and transfer libm remain.
- Static rate artifacts remain version 2: GCC 32-term/eight-accumulator kernels;
  Clang 64-term/four-accumulator/four-output kernels with explicit FMA.
- Final builds: `build/reaper/r3f-selected-{gcc,clang}`. The preceding `r3e`
  builds omit the tiny-column cutoff and are historical evidence.
- Final evidence: [`evidence/reaper/spectral-next/final`](evidence/reaper/spectral-next/final/).
  [`summary.json`](evidence/reaper/spectral-next/summary.json) verifies totals and identities;
  [`commands.sh`](evidence/reaper/spectral-next/commands.sh) and the source archive reproduce them.
- Wiki: [REAPER](https://github.com/NCAR/TurboTUV/wiki/REAPER). The page records
  the published source commit. No merge to `main` is part of this closeout.

## Results and remaining cases

| Configuration | Compiler | Main median | REAPER median | Paired speedup | Lower 95% |
| --- | --- | ---: | ---: | ---: | ---: |
| tuv_5_4 | gcc | 1080.7 us | 682.1 us | 1.583x | 1.580x |
| tuv_5_4 | clang | 1081.0 us | 635.9 us | 1.700x | 1.698x |
| ts1_tsmlt | gcc | 1110.3 us | 546.8 us | 2.030x | 2.025x |
| ts1_tsmlt | clang | 965.5 us | 513.7 us | 1.881x | 1.877x |

Measurements use Apple M5 ARM64/macOS, GCC 15.2 and Clang 21.1.5,
one thread and the existing whole-column API. Each case has ten rotating
paired rounds of 300 varied columns repeated ten times. Every sample exceeds
one second; no samples are discarded. Scope is the current prepared main
control, not the much older generic baseline or the Fortran zenith sweep.

## Verification

- 51 C/Fortran CTests per compiler; 30 sanitizer tests; 30 hint-disabled tests
  per compiler; scalar fallback, concurrent-model, lifecycle and installed
  consumer checks pass. Installed execution runs with `PATH=/nonexistent`.
- Four primary gates compare 165,382,800 values; maximum 9.913 epsilon.
  Six held-out configurations per compiler and 16 untuned gates pass; the latter
  compare 278,223,000 values at maximum 11.271 epsilon.
- The 10,000-spectrum stress test compares 710,370 values at maximum 6.818
  epsilon and injects 527 optional-allocation failures.
- Independent exp/pow10 bounds are below 3.16 epsilon. Each compiler passes
  121,614,882 exp and 121,348,084 pow10 MPFR comparisons, all below 1.995 epsilon.
  All 2048 table constants match an independent 53-bit MPFR destination.
- All 96 final secondary timing samples exceed one second; no case regresses
  by more than 5%. The tiny-Clang regression and its correction remain archived.
- x86-64 Linux execution remains unverified. Docker was returned to its initial
  stopped state in the earlier R6 work; no container work is needed for closeout.

## Resuming later

1. Read `REAPER.md`, `docs/REAPER_RESULTS.md` and the final evidence; verify the
   branch and working tree before editing. Keep the current exact controls.
2. Start from final profiles, not historical stage sums. Transfer remains the
   largest TUV cost; direct spectra, parameterized TS1 spectra and rate
   integration still leave possible avenues. Stage timings are not lower bounds.
3. Consult the ledgers before repeating trials: transfer exponential/slant
   reassociation failed cancellation/zero gates; aggressive grouping was
   unhelpful. The Clang grouped rate candidate in `rate-layout/` is unselected
   and needs recipe/identity support before any production use.
4. Retain C11/libm numerical code and Python-to-generated-C preparation.
   Do not require C++, ISA intrinsics, runtime compilation or Python embedding.
   Preserve all 32/64-epsilon gates, structural zeros, statuses and metadata.
5. Reproduce commands using the recorded SDK (macOS 26.5), real GCC 15 and
   LLVM 21 paths. Scientific Python is the `earth-eval` environment. MPFR/GMP
   are development-only oracle dependencies. Keep compilation and performance
   runs serial, and verify the exact binaries used for any new timing claim.
