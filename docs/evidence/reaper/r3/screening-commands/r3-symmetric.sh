#!/bin/sh
set -eu
for FAMILY in clang gcc; do
 if [ "$FAMILY" = clang ]; then CC=/opt/homebrew/opt/llvm/bin/clang; else CC=/opt/homebrew/bin/gcc-15; fi
 /Users/fillmore/miniforge3/envs/earth-eval/bin/python tools/reaper/transfer_sweep.py \
 --family "$FAMILY" --cc "$CC" --base "build/reaper/r3-roles-$FAMILY" \
 --control "build/reaper/r3-tables-$FAMILY/combined/turbotuv_bench" \
 --sysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
 --work "build/reaper/r3-symmetric-$FAMILY" --evidence docs/evidence/reaper/r3/symmetric \
 --variants symmetric --rounds 2 > "build/reaper/r3-symmetric-$FAMILY.log" 2>&1
 done
