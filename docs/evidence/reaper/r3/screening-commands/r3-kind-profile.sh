#!/bin/sh
set -eu
mkdir -p docs/evidence/reaper/r3/kinds
for FAMILY in clang gcc; do
 if [ "$FAMILY" = clang ]; then CC=/opt/homebrew/opt/llvm/bin/clang; else CC=/opt/homebrew/bin/gcc-15; fi
 BASE="build/reaper/r3-current-profile-$FAMILY"
 TARGET="build/reaper/r3-kind-profile-$FAMILY"
 mkdir -p "$TARGET"
 "$CC" -std=c11 -O3 -ffp-contract=off -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
 -DTT_REAPER_SPECTRUM_EXP=1 -DTT_REAPER_TABLES=1 -Iinclude -Isrc \
 -c build/reaper/r3-prototype/spectrum-kinds.c -o "$TARGET/spectrum_plan.c.o"
 cp "$BASE/libturbotuv.a" "$TARGET/libturbotuv.a"
 ar r "$TARGET/libturbotuv.a" "$TARGET/spectrum_plan.c.o"
 "$CC" -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
 "$BASE/CMakeFiles/turbotuv_bench.dir/bench/turbotuv_bench.c.o" "$TARGET/libturbotuv.a" -lm -o "$TARGET/turbotuv_bench"
 for CONFIG in tuv_5_4 ts1_tsmlt; do
 "$TARGET/turbotuv_bench" "examples/$CONFIG.json" --reaper --columns 300 --repeat 10 --detail \
 > "docs/evidence/reaper/r3/kinds/$FAMILY-$CONFIG.stdout" 2> "docs/evidence/reaper/r3/kinds/$FAMILY-$CONFIG.stderr"
 done
 done
