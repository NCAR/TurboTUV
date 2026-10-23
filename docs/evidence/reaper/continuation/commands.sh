#!/bin/sh
# Run from the repository root at this checkpoint. Prerequisites: R0 frozen
# controls, compiler-matched Fortran references, and R4 version-2 artifacts.
# Do not run other compilation or benchmarks while measure runs.
set -eu
mkdir -p build/reaper docs/evidence/reaper/continuation/combined

cat > build/reaper/r3b-build.sh <<'REAPER_SCRIPT'
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
  SOURCES="$PWD/build/reaper/r4-artifact-$F/kernel-0.c;$PWD/build/reaper/r4-artifact-$F/kernel-1.c;$PWD/build/reaper/r4-artifact-$F/registry.c"
  cmake -S . -B "build/reaper/r3b-selected-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_Fortran_COMPILER="$FC_BIN" "-DCMAKE_Fortran_FLAGS=-ffp-contract=off -isysroot $SDK" \
    -DTURBOTUV_REFERENCE_BUILD="$REF_BUILD" -DTURBOTUV_REFERENCE_SOURCE="$PWD/build/reaper/tuvx-source" \
    -DTURBOTUV_REFERENCE_PREFIX="$NF_PREFIX" -DTURBOTUV_NETCDF_PREFIX=/opt/homebrew/opt/netcdf \
    -DPython3_EXECUTABLE="$PY" "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "docs/evidence/reaper/continuation/combined/configure-$F.log" 2>&1
  cmake --build "build/reaper/r3b-selected-$F" -j4 > "docs/evidence/reaper/continuation/combined/build-$F.log" 2>&1
  ctest --test-dir "build/reaper/r3b-selected-$F" --output-on-failure \
    > "docs/evidence/reaper/continuation/combined/ctest-$F.log" 2>&1
 done
REAPER_SCRIPT

cat > build/reaper/r3b-sanitize.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
SOURCES="$PWD/build/reaper/r4-artifact-clang/kernel-0.c;$PWD/build/reaper/r4-artifact-clang/kernel-1.c;$PWD/build/reaper/r4-artifact-clang/registry.c"
cmake -S . -B build/reaper/r3b-sanitize -DCMAKE_C_COMPILER=/usr/bin/clang \
 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_SYSROOT="$SDK" -DTURBOTUV_SANITIZE=ON \
 "-DTURBOTUV_REAPER_SOURCES=$SOURCES" > docs/evidence/reaper/continuation/combined/configure-sanitize.log 2>&1
cmake --build build/reaper/r3b-sanitize -j4 > docs/evidence/reaper/continuation/combined/build-sanitize.log 2>&1
ctest --test-dir build/reaper/r3b-sanitize --output-on-failure > docs/evidence/reaper/continuation/combined/ctest-sanitize.log 2>&1
REAPER_SCRIPT

cat > build/reaper/r3b-gates.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for F in gcc clang; do
 if [ "$F" = gcc ]; then CC=/opt/homebrew/bin/gcc-15; else CC=/opt/homebrew/opt/llvm/bin/clang; fi
 for CONFIG in tuv_5_4 ts1_tsmlt; do
  "$PY" tools/reaper/compare.py "build/reaper/main-$F/turbotuv_bench" \
   "build/reaper/r3b-selected-$F/turbotuv_bench:--reaper" --config "examples/$CONFIG.json" \
   --columns 300 --json "docs/evidence/reaper/continuation/combined/gate-$F-$CONFIG.json" \
   > "docs/evidence/reaper/continuation/combined/gate-$F-$CONFIG.log" 2>&1
 done
 "$PY" tools/reaper/heldout.py --base "build/reaper/r3b-selected-$F" \
  --primary "build/reaper/r3b-selected-$F/turbotuv_bench" --cc "$CC" --sysroot "$SDK" \
  --work "build/reaper/r3b-heldout-$F" --evidence "docs/evidence/reaper/continuation/combined/heldout-$F" \
  > "docs/evidence/reaper/continuation/combined/heldout-$F.log" 2>&1
 done
REAPER_SCRIPT

cat > build/reaper/r3b-replay.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
for F in gcc clang; do
 for CONFIG in tuv_5_4 ts1_tsmlt; do
  "$PY" tools/reaper/compare.py "build/reaper/r4-selected-$F/turbotuv_bench:--reaper" "build/reaper/r3b-selected-$F/turbotuv_bench:--reaper" --config "examples/$CONFIG.json" --columns 300 --json "docs/evidence/reaper/continuation/combined/replay-$F-$CONFIG.json" > "docs/evidence/reaper/continuation/combined/replay-$F-$CONFIG.log" 2>&1
 done
done
REAPER_SCRIPT

cat > build/reaper/r3b-oracle.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
for F in gcc clang; do
 if [ "$F" = clang ]; then CC=/opt/homebrew/opt/llvm/bin/clang; else CC=/opt/homebrew/bin/gcc-15; fi
 "$CC" -std=c11 -O3 -ffp-contract=off -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
 -Isrc -I/opt/homebrew/include -L/opt/homebrew/lib tools/reaper/check_math.c -lmpfr -lgmp -lm \
 -o "build/reaper/check-math-r3b-$F"
 "build/reaper/check-math-r3b-$F" 1000000 0 > "docs/evidence/reaper/continuation/combined/oracle-exp-$F.json"
 "build/reaper/check-math-r3b-$F" 1000000 1 > "docs/evidence/reaper/continuation/combined/oracle-pow10-$F.json"
 done
REAPER_SCRIPT

cat > build/reaper/r3b-profile-build.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for F in gcc clang; do
  if [ "$F" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; VARIANT=small-acc;
  else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; VARIANT=interleave-fma; fi
  SOURCES="$PWD/build/reaper/r4-artifact-$F/kernel-0.c;$PWD/build/reaper/r4-artifact-$F/kernel-1.c;$PWD/build/reaper/r4-artifact-$F/registry.c"
  cmake -S . -B "build/reaper/r3b-profile-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DBUILD_TESTING=OFF \
    -DTURBOTUV_PROFILE=ON "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "docs/evidence/reaper/continuation/combined/configure-profile-$F.log" 2>&1
  cmake --build "build/reaper/r3b-profile-$F" -j3 > "docs/evidence/reaper/continuation/combined/build-profile-$F.log" 2>&1
done
REAPER_SCRIPT

cat > build/reaper/r3b-plain.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for F in gcc clang; do
  if [ "$F" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; VARIANT=small-acc;
  else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; VARIANT=interleave-fma; fi
  SOURCES="$PWD/build/reaper/r4-artifact-$F/kernel-0.c;$PWD/build/reaper/r4-artifact-$F/kernel-1.c;$PWD/build/reaper/r4-artifact-$F/registry.c"
  cmake -S . -B "build/reaper/r3b-plain-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DBUILD_TESTING=ON \
    -DTURBOTUV_REAPER_MATH_HINTS=OFF "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "docs/evidence/reaper/continuation/combined/configure-plain-$F.log" 2>&1
  cmake --build "build/reaper/r3b-plain-$F" -j3 > "docs/evidence/reaper/continuation/combined/build-plain-$F.log" 2>&1
  ctest --test-dir "build/reaper/r3b-plain-$F" --output-on-failure > "docs/evidence/reaper/continuation/combined/ctest-plain-$F.log" 2>&1
done
REAPER_SCRIPT

cat > build/reaper/r3b-final-build.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
for NAME in selected-gcc selected-clang plain-gcc plain-clang profile-gcc profile-clang sanitize; do
 cmake --build "build/reaper/r3b-$NAME" -j4 > "docs/evidence/reaper/continuation/combined/final-build-$NAME.log" 2>&1
 case "$NAME" in
 profile-*) ;;
 *) ctest --test-dir "build/reaper/r3b-$NAME" --output-on-failure -R 'reaper|model|execution_diagnostics|prepared_execution' > "docs/evidence/reaper/continuation/combined/final-ctest-$NAME.log" 2>&1 ;;
 esac
done
sh build/reaper/r3b-gates.sh
sh build/reaper/r3b-replay.sh
REAPER_SCRIPT

cat > build/reaper/r3b-timings.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
python3 tools/bench_compare.py \
  main-gcc=build/reaper/main-gcc/turbotuv_bench:--repeat,10 \
  reaper-gcc=build/reaper/r3b-selected-gcc/turbotuv_bench:--repeat,10,--reaper \
  main-clang=build/reaper/main-clang/turbotuv_bench:--repeat,10 \
  reaper-clang=build/reaper/r3b-selected-clang/turbotuv_bench:--repeat,10,--reaper \
  --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --columns 300 --rounds 10 \
  --minimum-seconds 1 --gate docs/evidence/reaper/continuation/combined/gate-gcc-tuv_5_4.json \
  --gate docs/evidence/reaper/continuation/combined/gate-gcc-ts1_tsmlt.json \
  --gate docs/evidence/reaper/continuation/combined/gate-clang-tuv_5_4.json \
  --gate docs/evidence/reaper/continuation/combined/gate-clang-ts1_tsmlt.json \
  --allow-differences reaper-gcc --allow-differences reaper-clang \
  --csv docs/evidence/reaper/continuation/combined/timings.csv --json docs/evidence/reaper/continuation/combined/timings.json
REAPER_SCRIPT

cat > build/reaper/r3b-measure.sh <<'REAPER_SCRIPT'
#!/bin/sh
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
for F in gcc clang; do
 for CONFIG in tuv_5_4 ts1_tsmlt; do
  "$PY" tools/reaper/compare.py "build/reaper/r3b-selected-$F/turbotuv_bench:--reaper" "build/reaper/r3b-plain-$F/turbotuv_bench:--reaper" --config "examples/$CONFIG.json" --columns 48 --json "docs/evidence/reaper/continuation/combined/plain-$F-$CONFIG.json" > "docs/evidence/reaper/continuation/combined/plain-$F-$CONFIG.log" 2>&1
  /usr/bin/time -l "build/reaper/r3b-selected-$F/turbotuv_bench" "examples/$CONFIG.json" --reaper --detail --columns 300 --repeat 3 > "docs/evidence/reaper/continuation/combined/resources-$F-$CONFIG.log" 2>&1
 done
 "$PY" tools/reaper/profile.py "build/reaper/r3b-profile-$F/turbotuv_bench" --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --reaper --json "docs/evidence/reaper/continuation/combined/profile-$F.json"
done
python3 tools/reaper/provenance.py --build build/reaper/r3b-selected-gcc --build build/reaper/r3b-selected-clang --build build/reaper/r3b-sanitize --build build/reaper/r3b-plain-gcc --build build/reaper/r3b-plain-clang --reference build/reaper/tuvx-source --json docs/evidence/reaper/continuation/combined/provenance.json
sh build/reaper/r3b-timings.sh
REAPER_SCRIPT

sh build/reaper/r3b-build.sh

sh build/reaper/r3b-sanitize.sh

sh build/reaper/r3b-profile-build.sh

sh build/reaper/r3b-plain.sh

sh build/reaper/r3b-oracle.sh
python3 tools/reaper/check_math_bounds.py --json docs/evidence/reaper/continuation/combined/math-bound.json

sh build/reaper/r3b-gates.sh

sh build/reaper/r3b-replay.sh

sh build/reaper/r3b-measure.sh
