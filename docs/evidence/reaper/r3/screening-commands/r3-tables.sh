#!/bin/sh
set -eu
for FAMILY in clang gcc; do
 if [ "$FAMILY" = clang ]; then CC=/opt/homebrew/opt/llvm/bin/clang; else CC=/opt/homebrew/bin/gcc-15; fi
 /Users/fillmore/miniforge3/envs/earth-eval/bin/python tools/reaper/math_sweep.py \
 --family "$FAMILY" --cc "$CC" --base "build/reaper/r3-$FAMILY" \
 --control "build/reaper/r3-cache-$FAMILY/both-expensive/turbotuv_bench" \
 --sysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
 --work "build/reaper/r3-tables-$FAMILY" --evidence docs/evidence/reaper/r3/tables \
 --rounds 2 --variants tables > "build/reaper/r3-tables-$FAMILY.log" 2>&1
 done
