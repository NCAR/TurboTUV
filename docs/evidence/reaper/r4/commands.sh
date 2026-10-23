#!/bin/sh
# R4 reproduction, from the repository root at the R4 source revision.
# R0 frozen controls and compiler-matched Fortran/NetCDF references are prerequisites.
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
mkdir -p docs/evidence/reaper/r4
cmake -S . -B build/reaper/r4-base-clang -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK"
cmake --build build/reaper/r4-base-clang -j4
for CONFIG in tuv_5_4 ts1_tsmlt; do
 build/reaper/r4-base-clang/turbotuv_reaper_export "examples/$CONFIG.json" "build/reaper/r4-$CONFIG-plan.json"
done
python3 tools/reaper/artifact.py build/reaper/r4-tuv_5_4-plan.json build/reaper/r4-ts1_tsmlt-plan.json \
 --directory build/reaper/r4-artifact-clang --cc /opt/homebrew/opt/llvm/bin/clang --sysroot "$SDK" \
 --term-budget 64 --accumulators 4 --interleave 4 --fma > docs/evidence/reaper/r4/artifact-clang.json

# r4-build.sh
#!/bin/sh
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
cmake -S . -B build/reaper/r4-base-gcc -DCMAKE_C_COMPILER=/opt/homebrew/bin/gcc-15 \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" > docs/evidence/reaper/r4/configure-base-gcc.log 2>&1
cmake --build build/reaper/r4-base-gcc -j4 > docs/evidence/reaper/r4/build-base-gcc.log 2>&1
for CONFIG in tuv_5_4 ts1_tsmlt; do
 build/reaper/r4-base-gcc/turbotuv_reaper_export "examples/$CONFIG.json" "build/reaper/r4-$CONFIG-gcc-plan.json"
done
python3 tools/reaper/artifact.py build/reaper/r4-tuv_5_4-gcc-plan.json build/reaper/r4-ts1_tsmlt-gcc-plan.json \
 --directory build/reaper/r4-artifact-gcc --cc /opt/homebrew/bin/gcc-15 --sysroot "$SDK" \
 --term-budget 32 --accumulators 8 > docs/evidence/reaper/r4/artifact-gcc.json
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
  cmake -S . -B "build/reaper/r4-selected-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_Fortran_COMPILER="$FC_BIN" "-DCMAKE_Fortran_FLAGS=-ffp-contract=off -isysroot $SDK" \
    -DTURBOTUV_REFERENCE_BUILD="$REF_BUILD" -DTURBOTUV_REFERENCE_SOURCE="$PWD/build/reaper/tuvx-source" \
    -DTURBOTUV_REFERENCE_PREFIX="$NF_PREFIX" -DTURBOTUV_NETCDF_PREFIX=/opt/homebrew/opt/netcdf \
    -DPython3_EXECUTABLE="$PY" "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "docs/evidence/reaper/r4/configure-$F.log" 2>&1
  cmake --build "build/reaper/r4-selected-$F" -j4 > "docs/evidence/reaper/r4/build-$F.log" 2>&1
  ctest --test-dir "build/reaper/r4-selected-$F" --output-on-failure \
    > "docs/evidence/reaper/r4/ctest-$F.log" 2>&1
 done

for F in gcc clang; do
 for CONFIG in tuv_5_4 ts1_tsmlt; do
  "$PY" tools/reaper/compare.py "build/reaper/r3-selected-$F/turbotuv_bench:--reaper" \
   "build/reaper/r4-selected-$F/turbotuv_bench:--reaper" --config "examples/$CONFIG.json" \
   --columns 300 --json "docs/evidence/reaper/r4/gate-$F-$CONFIG.json" \
   > "docs/evidence/reaper/r4/gate-$F-$CONFIG.log" 2>&1
 done
 if [ "$F" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; fi
 "$PY" tools/reaper/heldout.py --base "build/reaper/r4-selected-$F" \
  --primary "build/reaper/r4-selected-$F/turbotuv_bench" --cc "$CC_BIN" --sysroot "$SDK" \
  --work "build/reaper/r4-heldout-$F" --evidence "docs/evidence/reaper/r4/heldout-$F" \
  > "docs/evidence/reaper/r4/heldout-$F.log" 2>&1
done


# r4-sanitize.sh
#!/bin/sh
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
SOURCES="$PWD/build/reaper/r4-artifact-clang/kernel-0.c;$PWD/build/reaper/r4-artifact-clang/kernel-1.c;$PWD/build/reaper/r4-artifact-clang/registry.c"
cmake -S . -B build/reaper/r4-sanitize -DCMAKE_C_COMPILER=/usr/bin/clang \
 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_SYSROOT="$SDK" -DTURBOTUV_SANITIZE=ON \
 "-DTURBOTUV_REAPER_SOURCES=$SOURCES" > docs/evidence/reaper/r4/configure-sanitize.log 2>&1
cmake --build build/reaper/r4-sanitize -j4 > docs/evidence/reaper/r4/build-sanitize.log 2>&1
ctest --test-dir build/reaper/r4-sanitize --output-on-failure > docs/evidence/reaper/r4/ctest-sanitize.log 2>&1


# r4-deploy.sh
#!/bin/sh
set -eu
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for F in gcc clang; do
 if [ "$F" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; fi
 cmake -S . -B "build/reaper/r4-minimal-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE \
  -DBUILD_TESTING=OFF -DTURBOTUV_BUILD_CLI=OFF -DTURBOTUV_BUILD_BENCHMARK=OFF \
  -DTURBOTUV_REAPER_SOURCES= > "docs/evidence/reaper/r4/configure-minimal-$F.log" 2>&1
 cmake --build "build/reaper/r4-minimal-$F" -j3 > "docs/evidence/reaper/r4/build-minimal-$F.log" 2>&1
 for MODE in minimal selected; do
  PREFIX="$PWD/build/reaper/r4-install-$MODE-$F"
  cmake --install "build/reaper/r4-$MODE-$F" --prefix "$PREFIX" \
   > "docs/evidence/reaper/r4/install-$MODE-$F.log" 2>&1
  cmake -S tests/installed -B "build/reaper/r4-consumer-$MODE-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
   -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DCMAKE_PREFIX_PATH="$PREFIX" \
   > "docs/evidence/reaper/r4/consumer-configure-$MODE-$F.log" 2>&1
  cmake --build "build/reaper/r4-consumer-$MODE-$F" -j2 \
   > "docs/evidence/reaper/r4/consumer-build-$MODE-$F.log" 2>&1
  if [ "$MODE" = selected ]; then EXPECTED=attached; else EXPECTED=no-artifacts; fi
  for CONFIG in tuv_5_4 ts1_tsmlt; do
   /usr/bin/env PATH=/nonexistent "$PWD/build/reaper/r4-consumer-$MODE-$F/reaper_consumer" \
    "$PWD/examples/$CONFIG.json" "$EXPECTED" > "docs/evidence/reaper/r4/consumer-$MODE-$F-$CONFIG.log" 2>&1
  done
  /usr/bin/otool -L "build/reaper/r4-consumer-$MODE-$F/reaper_consumer" \
   > "docs/evidence/reaper/r4/dependencies-$MODE-$F.log"
 done
done


# Isolated emission/compile timing after the builds/tests complete.
python3 tools/reaper/artifact.py build/reaper/r4-tuv_5_4-gcc-plan.json build/reaper/r4-ts1_tsmlt-gcc-plan.json \
 --directory build/reaper/r4-artifact-gcc --cc /opt/homebrew/bin/gcc-15 --sysroot "$SDK" \
 --term-budget 32 --accumulators 8 > docs/evidence/reaper/r4/artifact-gcc.json
python3 tools/reaper/artifact.py build/reaper/r4-tuv_5_4-plan.json build/reaper/r4-ts1_tsmlt-plan.json \
 --directory build/reaper/r4-artifact-clang --cc /opt/homebrew/opt/llvm/bin/clang --sysroot "$SDK" \
 --term-budget 64 --accumulators 4 --interleave 4 --fma > docs/evidence/reaper/r4/artifact-clang.json
for F in gcc clang; do
 if [ "$F" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; fi
 "$CC_BIN" -std=c11 -O3 -ffp-contract=off -isysroot "$SDK" -Ibuild/reaper/main-src/include \
  tests/installed/prepared_consumer.c "build/reaper/main-$F/libturbotuv.a" -lm -o "build/reaper/main-consumer-$F"
 python3 tools/reaper/startup.py --main "build/reaper/main-consumer-$F" \
  --candidate "build/reaper/r4-consumer-selected-$F/reaper_consumer" \
  --artifact "docs/evidence/reaper/r4/artifact-$F.json" --steady docs/evidence/reaper/r3/timings.json \
  --family "$F" --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json \
  --json "docs/evidence/reaper/r4/startup-$F.json"
 for CONFIG in tuv_5_4 ts1_tsmlt; do
  /usr/bin/time -l "build/reaper/r4-selected-$F/turbotuv_bench" "examples/$CONFIG.json" \
   --reaper --detail --columns 300 --repeat 3 > "docs/evidence/reaper/r4/resources-$F-$CONFIG.log" 2>&1
 done
done
python3 tools/bench_compare.py \
 r3-gcc=build/reaper/r3-selected-gcc/turbotuv_bench:--reaper,--repeat,10 \
 r4-gcc=build/reaper/r4-selected-gcc/turbotuv_bench:--reaper,--repeat,10 \
 r3-clang=build/reaper/r3-selected-clang/turbotuv_bench:--reaper,--repeat,10 \
 r4-clang=build/reaper/r4-selected-clang/turbotuv_bench:--reaper,--repeat,10 \
 --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --columns 300 --rounds 3 \
 --minimum-seconds 1 --gate docs/evidence/reaper/r4/gate-gcc-tuv_5_4.json \
 --gate docs/evidence/reaper/r4/gate-gcc-ts1_tsmlt.json --gate docs/evidence/reaper/r4/gate-clang-tuv_5_4.json \
 --gate docs/evidence/reaper/r4/gate-clang-ts1_tsmlt.json \
 --csv docs/evidence/reaper/r4/regression.csv --json docs/evidence/reaper/r4/regression.json
python3 tools/reaper/provenance.py --build build/reaper/r4-selected-gcc \
 --build build/reaper/r4-selected-clang --build build/reaper/r4-sanitize \
 --reference build/reaper/tuvx-source --json docs/evidence/reaper/r4/provenance.json
