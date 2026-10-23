# Machine-specific R3 reproduction commands; run from the repository root.
# Restore selected R2 generated C sources from the R2 evidence if needed.
# Run profiles/resources only after all builds and tests finish.

# r3-selected-build.sh
#!/bin/sh
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for F in gcc clang; do
  if [ "$F" = gcc ]; then
    CC_BIN=/opt/homebrew/bin/gcc-15
    FC_BIN=/opt/homebrew/bin/gfortran-15
    REF_BUILD="$PWD/build/reference-gcc"
    NF_PREFIX=/opt/homebrew/opt/netcdf-fortran
    VARIANT=small-acc
  else
    CC_BIN=/opt/homebrew/opt/llvm/bin/clang
    FC_BIN=/opt/homebrew/bin/flang
    REF_BUILD="$PWD/build/reference-llvm"
    NF_PREFIX="$PWD/build/reaper/netcdf-flang"
    VARIANT=interleave-fma
  fi
  SOURCES="$PWD/build/reaper/r2-combinations-$F/$VARIANT/kernel-0.c;$PWD/build/reaper/r2-combinations-$F/$VARIANT/kernel-1.c;$PWD/build/reaper/r2-combinations-$F/$VARIANT/registry.c"
  cmake -S . -B "build/reaper/r3-selected-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_Fortran_COMPILER="$FC_BIN" "-DCMAKE_Fortran_FLAGS=-ffp-contract=off -isysroot $SDK" \
    -DTURBOTUV_REFERENCE_BUILD="$REF_BUILD" -DTURBOTUV_REFERENCE_SOURCE="$PWD/build/reaper/tuvx-source" \
    -DTURBOTUV_REFERENCE_PREFIX="$NF_PREFIX" -DTURBOTUV_NETCDF_PREFIX=/opt/homebrew/opt/netcdf \
    -DPython3_EXECUTABLE="$PY" "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "docs/evidence/reaper/r3/configure-$F.log" 2>&1
  cmake --build "build/reaper/r3-selected-$F" -j4 > "docs/evidence/reaper/r3/build-$F.log" 2>&1
  ctest --test-dir "build/reaper/r3-selected-$F" --output-on-failure \
    > "docs/evidence/reaper/r3/ctest-$F.log" 2>&1
 done


# r3-sanitize.sh
#!/bin/sh
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
SOURCES="$PWD/build/reaper/r2-combinations-clang/interleave-fma/kernel-0.c;$PWD/build/reaper/r2-combinations-clang/interleave-fma/kernel-1.c;$PWD/build/reaper/r2-combinations-clang/interleave-fma/registry.c"
cmake -S . -B build/reaper/r3-sanitize -DCMAKE_C_COMPILER=/usr/bin/clang \
 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_SYSROOT="$SDK" -DTURBOTUV_SANITIZE=ON \
 "-DTURBOTUV_REAPER_SOURCES=$SOURCES" > docs/evidence/reaper/r3/configure-sanitize.log 2>&1
cmake --build build/reaper/r3-sanitize -j4 > docs/evidence/reaper/r3/build-sanitize.log 2>&1
ctest --test-dir build/reaper/r3-sanitize --output-on-failure > docs/evidence/reaper/r3/ctest-sanitize.log 2>&1


# r3-gates.sh
#!/bin/sh
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for F in gcc clang; do
 if [ "$F" = gcc ]; then CC=/opt/homebrew/bin/gcc-15; else CC=/opt/homebrew/opt/llvm/bin/clang; fi
 for CONFIG in tuv_5_4 ts1_tsmlt; do
  "$PY" tools/reaper/compare.py "build/reaper/main-$F/turbotuv_bench" \
   "build/reaper/r3-selected-$F/turbotuv_bench:--reaper" --config "examples/$CONFIG.json" \
   --columns 300 --json "docs/evidence/reaper/r3/gate-$F-$CONFIG.json" \
   > "docs/evidence/reaper/r3/gate-$F-$CONFIG.log" 2>&1
 done
 "$PY" tools/reaper/heldout.py --base "build/reaper/r3-selected-$F" \
  --primary "build/reaper/r3-selected-$F/turbotuv_bench" --cc "$CC" --sysroot "$SDK" \
  --work "build/reaper/r3-heldout-$F" --evidence "docs/evidence/reaper/r3/heldout-$F" \
  > "docs/evidence/reaper/r3/heldout-$F.log" 2>&1
 done


# r3-profile-build.sh
#!/bin/sh
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for F in gcc clang; do
  if [ "$F" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; VARIANT=small-acc;
  else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; VARIANT=interleave-fma; fi
  SOURCES="$PWD/build/reaper/r2-combinations-$F/$VARIANT/kernel-0.c;$PWD/build/reaper/r2-combinations-$F/$VARIANT/kernel-1.c;$PWD/build/reaper/r2-combinations-$F/$VARIANT/registry.c"
  cmake -S . -B "build/reaper/r3-profile-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DBUILD_TESTING=OFF \
    -DTURBOTUV_PROFILE=ON "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "docs/evidence/reaper/r3/configure-profile-$F.log" 2>&1
  cmake --build "build/reaper/r3-profile-$F" -j3 > "docs/evidence/reaper/r3/build-profile-$F.log" 2>&1
done


# r3-oracle.sh
#!/bin/sh
set -eu
for F in gcc clang; do
 if [ "$F" = clang ]; then CC=/opt/homebrew/opt/llvm/bin/clang; else CC=/opt/homebrew/bin/gcc-15; fi
 "$CC" -std=c11 -O3 -ffp-contract=off -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
 -Isrc -I/opt/homebrew/include -L/opt/homebrew/lib tools/reaper/check_math.c -lmpfr -lgmp -lm \
 -o "build/reaper/check-math-$F"
 "build/reaper/check-math-$F" 1000000 0 > "docs/evidence/reaper/r3/oracle-exp-$F.json"
 "build/reaper/check-math-$F" 1000000 1 > "docs/evidence/reaper/r3/oracle-pow10-$F.json"
 done


# r3-timings.sh
#!/bin/sh
set -eu
python3 tools/bench_compare.py \
  main-gcc=build/reaper/main-gcc/turbotuv_bench:--repeat,10 \
  reaper-gcc=build/reaper/r3-selected-gcc/turbotuv_bench:--repeat,10,--reaper \
  main-clang=build/reaper/main-clang/turbotuv_bench:--repeat,10 \
  reaper-clang=build/reaper/r3-selected-clang/turbotuv_bench:--repeat,10,--reaper \
  --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --columns 300 --rounds 10 \
  --minimum-seconds 1 --gate docs/evidence/reaper/r3/gate-gcc-tuv_5_4.json \
  --gate docs/evidence/reaper/r3/gate-gcc-ts1_tsmlt.json \
  --gate docs/evidence/reaper/r3/gate-clang-tuv_5_4.json \
  --gate docs/evidence/reaper/r3/gate-clang-ts1_tsmlt.json \
  --allow-differences reaper-gcc --allow-differences reaper-clang \
  --csv docs/evidence/reaper/r3/timings.csv --json docs/evidence/reaper/r3/timings.json


PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
for F in gcc clang; do
 "$PY" tools/reaper/profile.py "build/reaper/r3-profile-$F/turbotuv_bench" --reaper \
  --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json \
  --json "docs/evidence/reaper/r3/profile-$F.json"
 for CONFIG in tuv_5_4 ts1_tsmlt; do
  /usr/bin/time -l "build/reaper/r3-selected-$F/turbotuv_bench" "examples/$CONFIG.json" \
   --reaper --detail --columns 300 --repeat 3 \
   > "docs/evidence/reaper/r3/resources-$F-$CONFIG.log" 2>&1
 done
done
python3 tools/reaper/check_math_bounds.py --json docs/evidence/reaper/r3/math-bound.json
python3 tools/reaper/provenance.py --build build/reaper/r3-selected-gcc \
 --build build/reaper/r3-selected-clang --build build/reaper/r3-sanitize \
 --reference build/reaper/tuvx-source --json docs/evidence/reaper/r3/provenance.json
