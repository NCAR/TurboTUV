#!/bin/sh
# Run from the repository root at this checkpoint. Prerequisites:
# R0 frozen controls/reference checkout, R4 generated artifacts, and the
# selected 0872e28 builds reproduced by continuation/commands.sh.
set -eu
PY=${REAPER_PYTHON:-/Users/fillmore/miniforge3/envs/earth-eval/bin/python}
tar -xzf docs/evidence/reaper/r6/secondary-source.tar.gz -C .
sh build/reaper/r6-deploy.sh > docs/evidence/reaper/r6/deploy-all.log 2>&1
"$PY" build/reaper/r6-audits.py
mkdir -p build/reaper/r6-host-source
python3 build/reaper/prepare-r6-host.py > docs/evidence/reaper/r6/heldout/host-build.log 2>&1
# Serial measurements: do not run other compilation or benchmarks here.
"$PY" build/reaper/r6-heldout.py > docs/evidence/reaper/r6/heldout/run.log 2>&1
# Docker lifecycle attempts are historical availability evidence, not a
# prerequisite or an instruction to start/stop the user's applications.
