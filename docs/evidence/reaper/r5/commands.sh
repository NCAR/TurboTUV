#!/bin/sh
# ARM/macOS reproduction, from the repository root. Requires the selected
# and sanitizer libraries from checkpoint 0872e28 (continuation/commands.sh).
set -eu
PY=${REAPER_PYTHON:-/Users/fillmore/miniforge3/envs/earth-eval/bin/python}
tar -xzf docs/evidence/reaper/r5/prototype-source.tar.gz -C .
python3 build/reaper/build-r5.py clang > docs/evidence/reaper/r5/build-clang.log 2>&1
python3 build/reaper/build-r5.py gcc > docs/evidence/reaper/r5/build-gcc.log 2>&1
"$PY" build/reaper/r5-gates.py > docs/evidence/reaper/r5/gates.log 2>&1
sh build/reaper/r5-sanitize.sh > docs/evidence/reaper/r5/build-sanitize.log 2>&1
sh build/reaper/r5-final-checks.sh > docs/evidence/reaper/r5/final-checks.log 2>&1
# No other compilation or benchmarking should run during this command.
python3 build/reaper/r5-timings.py > docs/evidence/reaper/r5/timings.log 2>&1
