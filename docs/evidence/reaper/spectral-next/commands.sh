#!/bin/sh
# Run from this checkpoint's repository root. Requires the frozen R0 controls,
# R4 version-2 static artifacts, original Fortran reference builds and the
# compiler/SDK/Python paths in final/provenance.json. Keep timing runs serial.
set -eu
tar -xzf docs/evidence/reaper/spectral-next/source.tar.gz
mkdir -p docs/evidence/reaper/spectral-next/final/untuned
sh build/reaper/r3f-validate.sh
sh build/reaper/spectral-table-check.sh docs/evidence/reaper/spectral-next/final
# Reproduce the expanded boundary oracles from the current, token-equivalent
# production source. These checks are intentionally long; finish them before
# performance measurements. The archived original results are in oracle/.
sh build/reaper/r3f-oracle.sh
/Users/fillmore/miniforge3/envs/earth-eval/bin/python build/reaper/r3f-untuned.py
sh build/reaper/r3f-measure.sh
# Historical sweep-{gcc,clang}.json used f1512c1 with r3d base builds and the
# archived prepare-oclo-runs.py / spectral-next-sweep.py sources. To repeat
# those screens, use that revision; running the scripts on selected source
# would apply transformations twice. Exact prototype headers are archived.
# The initial combined/ checkpoint uses the archived r3e-source sources and
# r3e drivers. It intentionally retains the regression before the cutoff.
