#!/bin/sh
# Commands used for R0 on Apple M5/macOS 27. Paths are intentionally explicit.
# Run from the repository root. Disposable sources/binaries belong in build/.
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
mkdir -p build/reaper/main-src
git archive 9e149851af5bc48fc7ce64b5fbe50c6959ff2d51 | tar -x -C build/reaper/main-src
# Isolated original reference (do not alter ../TUV-x).
# git clone --no-checkout ssh://git@github.com/NCAR/TUV-x build/reaper/tuvx-source
# git -C build/reaper/tuvx-source checkout --detach ebe5006cbdb74b9040173c97b93323e48a83b464
# NetCDF Fortran must match its compiler. The LLVM copy in build/reaper/netcdf-flang
# is the local Flang 21 installation with its pkg-config prefix relocated.
"$PY" tools/reference.py gcc --source build/reaper/tuvx-source \
  --prefix /opt/homebrew/opt/netcdf-fortran --netcdf /opt/homebrew/opt/netcdf \
  --cc /opt/homebrew/bin/gcc-15 --cxx /opt/homebrew/opt/llvm/bin/clang++ \
  --fc /opt/homebrew/bin/gfortran-15 --sysroot "$SDK" --jobs 6
"$PY" tools/reference.py llvm --source build/reaper/tuvx-source \
  --prefix "$PWD/build/reaper/netcdf-flang" --netcdf /opt/homebrew/opt/netcdf \
  --cc /opt/homebrew/opt/llvm/bin/clang --cxx /opt/homebrew/opt/llvm/bin/clang++ \
  --fc /opt/homebrew/bin/flang --sysroot "$SDK" --jobs 6
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
  cmake -S build/reaper/main-src -B "build/reaper/main-$FAMILY" \
    -DCMAKE_C_COMPILER="$CC_BIN" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_Fortran_COMPILER="$FC_BIN" "-DCMAKE_Fortran_FLAGS=-ffp-contract=off -isysroot $SDK" \
    -DTURBOTUV_REFERENCE_BUILD="$REF_BUILD" -DTURBOTUV_REFERENCE_SOURCE="$PWD/build/reaper/tuvx-source" \
    -DTURBOTUV_REFERENCE_PREFIX="$NF_PREFIX" -DTURBOTUV_NETCDF_PREFIX=/opt/homebrew/opt/netcdf \
    -DPython3_EXECUTABLE="$PY"
  cmake --build "build/reaper/main-$FAMILY" -j6
  ctest --test-dir "build/reaper/main-$FAMILY" --output-on-failure
  for VARIANT in r0 profile; do
    PROFILE=OFF
    if [ "$VARIANT" = profile ]; then PROFILE=ON; fi
    cmake -S . -B "build/reaper/$VARIANT-$FAMILY" -DCMAKE_C_COMPILER="$CC_BIN" \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DTURBOTUV_PROFILE="$PROFILE"
    cmake --build "build/reaper/$VARIANT-$FAMILY" -j6
  done
  ctest --test-dir "build/reaper/r0-$FAMILY" --output-on-failure
  for CONFIG in tuv_5_4 ts1_tsmlt; do
    "$PY" tools/reaper/compare.py "build/reaper/main-$FAMILY/turbotuv_bench" \
      "build/reaper/r0-$FAMILY/turbotuv_bench" --config "examples/$CONFIG.json" --columns 300 \
      --json "docs/evidence/reaper/baseline/gate-$FAMILY-$CONFIG.json"
  done
  "$PY" tools/reaper/profile.py "build/reaper/profile-$FAMILY/turbotuv_bench" \
    --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json \
    --json "docs/evidence/reaper/baseline/profile-$FAMILY.json"
done
"$PY" tools/bench_compare.py \
  main-gcc=build/reaper/main-gcc/turbotuv_bench:--repeat,5 \
  r0-gcc=build/reaper/r0-gcc/turbotuv_bench:--repeat,5 \
  main-clang=build/reaper/main-clang/turbotuv_bench:--repeat,5 \
  r0-clang=build/reaper/r0-clang/turbotuv_bench:--repeat,5 \
  --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --columns 300 --rounds 10 \
  --minimum-seconds 1 \
  --gate docs/evidence/reaper/baseline/gate-gcc-tuv_5_4.json \
  --gate docs/evidence/reaper/baseline/gate-gcc-ts1_tsmlt.json \
  --gate docs/evidence/reaper/baseline/gate-clang-tuv_5_4.json \
  --gate docs/evidence/reaper/baseline/gate-clang-ts1_tsmlt.json \
  --csv docs/evidence/reaper/baseline/timings.csv --json docs/evidence/reaper/baseline/timings.json
"$PY" tools/reaper/provenance.py --build build/reaper/main-gcc --build build/reaper/main-clang \
  --build build/reaper/r0-gcc --build build/reaper/r0-clang --build build/reaper/profile-gcc \
  --build build/reaper/profile-clang --reference build/reaper/tuvx-source \
  --json docs/evidence/reaper/baseline/provenance.json
"$PY" tools/embed_data.py --source build/reaper/tuvx-source --check
"$PY" tests/verify_data.py build/reaper/main-gcc/tests/dump_data --source build/reaper/tuvx-source
"$PY" tests/test_reaper_compare.py

# Sample separately from all timings.
for FAMILY in gcc clang; do
  for CONFIG in tuv_5_4 ts1_tsmlt; do
    python3 tools/reaper/sample.py "build/reaper/main-$FAMILY/turbotuv_bench" \
      --config "examples/$CONFIG.json" \
      --output "docs/evidence/reaper/baseline/sample-$FAMILY-$CONFIG.txt"
  done
done
# Compile-only vectorization diagnostics. Assembly stays under build/.
mkdir -p build/reaper/assembly docs/evidence/reaper/baseline/compiler-reports
for UNIT in model_plan spectrum_plan radiation_block oxygen; do
  /opt/homebrew/opt/llvm/bin/clang -std=c11 -O3 -DNDEBUG -ffp-contract=off \
    -isysroot "$SDK" -Iinclude -S "src/$UNIT.c" -o "build/reaper/assembly/$UNIT-clang.s" \
    -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize \
    2> "docs/evidence/reaper/baseline/compiler-reports/$UNIT-clang.txt"
  /opt/homebrew/bin/gcc-15 -std=c11 -O3 -DNDEBUG -ffp-contract=off \
    -isysroot "$SDK" -Iinclude -S "src/$UNIT.c" -o "build/reaper/assembly/$UNIT-gcc.s" \
    "-fopt-info-vec-all=docs/evidence/reaper/baseline/compiler-reports/$UNIT-gcc.txt"
done
