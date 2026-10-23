#!/bin/sh
# Run from the repository root after unpacking ../source.tar.gz. The original
# four oracle runs overlapped each other, never acceptance timing. This serial
# recipe checks identical inputs and retains independent stderr progress logs.
set -eu
mkdir -p build/reaper/spectral-oracle
for FAMILY in gcc clang; do
 if [ "$FAMILY" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; fi
 "$CC_BIN" -std=c11 -O3 -ffp-contract=off -DREAPER_ORACLE_TABLE=2048 \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
  -Ibuild/reaper/math-table-followup-clang/table-2048-degree-3 -Isrc \
  -I/opt/homebrew/include -L/opt/homebrew/lib build/reaper/check-expanded-math.c \
  -lmpfr -lgmp -lm -o "build/reaper/spectral-oracle/check-$FAMILY"
 "build/reaper/spectral-oracle/check-$FAMILY" 1000000 0 \
  > "docs/evidence/reaper/spectral-next/oracle/$FAMILY-exp.json" \
  2> "docs/evidence/reaper/spectral-next/oracle/$FAMILY-exp.log"
 "build/reaper/spectral-oracle/check-$FAMILY" 1000000 1 \
  > "docs/evidence/reaper/spectral-next/oracle/$FAMILY-pow10.json" \
  2> "docs/evidence/reaper/spectral-next/oracle/$FAMILY-pow10.log"
done
python3 build/reaper/spectral-oracle-provenance.py
