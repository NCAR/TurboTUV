#!/bin/sh
# R1 reproduction; run from the R1 phase commit after preparing R0 controls/references.
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for FAMILY in gcc clang; do
  if [ "$FAMILY" = gcc ]; then
    CC_BIN=/opt/homebrew/bin/gcc-15
    FC_BIN=/opt/homebrew/bin/gfortran-15
    REF_BUILD="$PWD/build/reference-gcc"
    NF_PREFIX=/opt/homebrew/opt/netcdf-fortran
  else
    CC_BIN=/opt/homebrew/opt/llvm/bin/clang
    FC_BIN=/opt/homebrew/bin/flang
    REF_BUILD="$PWD/build/reference-llvm"
    NF_PREFIX="$PWD/build/reaper/netcdf-flang"
  fi
  cmake -S . -B "build/reaper/r1-$FAMILY" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_Fortran_COMPILER="$FC_BIN" "-DCMAKE_Fortran_FLAGS=-ffp-contract=off -isysroot $SDK" \
    -DTURBOTUV_REFERENCE_BUILD="$REF_BUILD" -DTURBOTUV_REFERENCE_SOURCE="$PWD/build/reaper/tuvx-source" \
    -DTURBOTUV_REFERENCE_PREFIX="$NF_PREFIX" -DTURBOTUV_NETCDF_PREFIX=/opt/homebrew/opt/netcdf \
    -DPython3_EXECUTABLE="$PY"
  cmake --build "build/reaper/r1-$FAMILY" -j6
  ctest --test-dir "build/reaper/r1-$FAMILY" --output-on-failure
  python3 tools/reaper/overhead_sweep.py --family "$FAMILY" --base "build/reaper/r1-$FAMILY" \
    --control "build/reaper/main-$FAMILY/turbotuv_bench" --compiler "$CC_BIN" --sysroot "$SDK" \
    --output "docs/evidence/reaper/r1/sweep-$FAMILY.json" --work "build/reaper/r1-sweep-$FAMILY"
  python3 tools/reaper/overhead_sweep.py --family "$FAMILY" --base "build/reaper/r1-$FAMILY" \
    --control "build/reaper/main-$FAMILY/turbotuv_bench" --compiler "$CC_BIN" --sysroot "$SDK" \
    --output "docs/evidence/reaper/r1/combined-$FAMILY.json" --work "build/reaper/r1-combined-$FAMILY" \
    --combined-only --rounds 5 --repeat 3
  for CONFIG in tuv_5_4 ts1_tsmlt; do
    "$PY" tools/reaper/compare.py "build/reaper/main-$FAMILY/turbotuv_bench" \
      "build/reaper/r1-$FAMILY/turbotuv_bench:--reaper" --config "examples/$CONFIG.json" \
      --columns 300 --json "docs/evidence/reaper/r1/gate-$FAMILY-$CONFIG.json"
  done
  cmake -S . -B "build/reaper/r1-profile-$FAMILY" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DTURBOTUV_PROFILE=ON -DBUILD_TESTING=OFF
  cmake --build "build/reaper/r1-profile-$FAMILY" -j6
  "$PY" tools/reaper/profile.py "build/reaper/r1-profile-$FAMILY/turbotuv_bench" --reaper \
    --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json \
    --json "docs/evidence/reaper/r1/profile-$FAMILY.json"
done
cmake -S . -B build/reaper/r1-sanitize -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_SYSROOT="$SDK" -DTURBOTUV_SANITIZE=ON
cmake --build build/reaper/r1-sanitize -j6
ctest --test-dir build/reaper/r1-sanitize --output-on-failure
python3 tools/bench_compare.py \
  main-gcc=build/reaper/main-gcc/turbotuv_bench:--repeat,5 \
  reaper-gcc=build/reaper/r1-gcc/turbotuv_bench:--repeat,5,--reaper \
  main-clang=build/reaper/main-clang/turbotuv_bench:--repeat,5 \
  reaper-clang=build/reaper/r1-clang/turbotuv_bench:--repeat,5,--reaper \
  --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --columns 300 --rounds 10 \
  --minimum-seconds 1 --gate docs/evidence/reaper/r1/gate-gcc-tuv_5_4.json \
  --gate docs/evidence/reaper/r1/gate-gcc-ts1_tsmlt.json \
  --gate docs/evidence/reaper/r1/gate-clang-tuv_5_4.json \
  --gate docs/evidence/reaper/r1/gate-clang-ts1_tsmlt.json \
  --csv docs/evidence/reaper/r1/timings.csv --json docs/evidence/reaper/r1/timings.json
python3 tools/reaper/provenance.py --build build/reaper/r1-gcc --build build/reaper/r1-clang \
  --build build/reaper/r1-sanitize --reference build/reaper/tuvx-source \
  --json docs/evidence/reaper/r1/provenance.json
