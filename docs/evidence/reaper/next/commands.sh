#!/bin/sh
# Run from the repository root at this checkpoint. Requires frozen R0 main
# controls, matched Fortran references, R4 version-2 rate artifacts, MPFR/GMP,
# and the compiler/Python/SDK paths recorded in combined/provenance.json.
# Scripts run serially. Do not overlap compilation with performance runs.
set -eu
tar -xzf docs/evidence/reaper/next/prototype-source.tar.gz
mkdir -p docs/evidence/reaper/next/combined/untuned
sh build/reaper/r3c-build.sh
sh build/reaper/r3c-sanitize.sh
sh build/reaper/r3c-plain.sh
sh build/reaper/r3c-profile-build.sh
sh build/reaper/r3c-deploy.sh
sh build/reaper/r3c-gates.sh
sh build/reaper/r3c-oracle.sh
python3 tools/reaper/check_math_bounds.py --json docs/evidence/reaper/next/combined/math-bound.json
sh build/reaper/r3c-final-checks.sh
/Users/fillmore/miniforge3/envs/earth-eval/bin/python build/reaper/r3c-untuned.py
sh build/reaper/r3c-measure.sh

# Historical screens use archived *-prototype sources and *-sweep.py drivers
# with the matching 0872e28 r3b-selected archives. Preparation scripts are
# preserved as provenance, but rerunning them on newer source changes the
# experiment. The portability-check driver records the final native binary
# identity check; its pre-guard hashes belong to this measurement session.
