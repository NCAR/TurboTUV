#!/bin/sh
# Run from the repository root at this checkpoint. Requires the R0 frozen
# main/control references, R4 static artifacts and the compiler/SDK/Python
# paths recorded in combined/provenance.json. Keep timing runs serial.
set -eu
tar -xzf docs/evidence/reaper/mixed-bin/source.tar.gz
mkdir -p docs/evidence/reaper/mixed-bin/combined/untuned
sh build/reaper/r3d-validate.sh
python3 tools/reaper/check_math_bounds.py --json docs/evidence/reaper/mixed-bin/combined/math-bound.json
# The math helper is unchanged from 501c331. For a fresh independent oracle,
# rather than the archived hash-identified reuse, run:
# sh build/reaper/r3d-oracle.sh
