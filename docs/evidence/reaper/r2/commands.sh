#!/bin/sh
# R2 reproduction: R0 pinned controls and R1 references must already exist.
set -eu
PY=/Users/fillmore/miniforge3/envs/earth-eval/bin/python
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
for FAMILY in gcc clang; do
  if [ "$FAMILY" = gcc ]; then CC_BIN=/opt/homebrew/bin/gcc-15; else CC_BIN=/opt/homebrew/opt/llvm/bin/clang; fi
  cmake -S . -B "build/reaper/r2-$FAMILY" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK"
  cmake --build "build/reaper/r2-$FAMILY" -j6
  ctest --test-dir "build/reaper/r2-$FAMILY" --output-on-failure
  "$PY" tools/reaper/rate_sweep.py --family "$FAMILY" --cc "$CC_BIN" --sysroot "$SDK" \
    --base "build/reaper/r2-$FAMILY" --work "build/reaper/r2-sweep-$FAMILY" \
    --evidence docs/evidence/reaper/r2/screen \
    --variants ordered-32 ordered-64 ordered-128 ordered-256 acc-2 acc-4 acc-8 \
    interleave-2 interleave-4 interleave-8 fma tile-8 tile-16 tile-32 reuse-products
  "$PY" tools/reaper/rate_sweep.py --family "$FAMILY" --cc "$CC_BIN" --sysroot "$SDK" \
    --base "build/reaper/r2-$FAMILY" --work "build/reaper/r2-combinations-$FAMILY" \
    --evidence docs/evidence/reaper/r2/combinations \
    --variants small-acc small-interleave large-interleave interleave-fma reuse-products
 done

# Final selected builds, reference/held-out/sanitizer gates, paired timing.
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
  cmake -S . -B "build/reaper/r2-selected-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_Fortran_COMPILER="$FC_BIN" "-DCMAKE_Fortran_FLAGS=-ffp-contract=off -isysroot $SDK" \
    -DTURBOTUV_REFERENCE_BUILD="$REF_BUILD" -DTURBOTUV_REFERENCE_SOURCE="$PWD/build/reaper/tuvx-source" \
    -DTURBOTUV_REFERENCE_PREFIX="$NF_PREFIX" -DTURBOTUV_NETCDF_PREFIX=/opt/homebrew/opt/netcdf \
    -DPython3_EXECUTABLE="$PY" "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "docs/evidence/reaper/r2/configure-$F.log" 2>&1
  cmake --build "build/reaper/r2-selected-$F" -j6 > "docs/evidence/reaper/r2/build-$F.log" 2>&1
  ctest --test-dir "build/reaper/r2-selected-$F" --output-on-failure \
    > "docs/evidence/reaper/r2/ctest-$F.log" 2>&1
  cmake -S . -B "build/reaper/r2-profile-$F" -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" -DBUILD_TESTING=OFF \
    -DTURBOTUV_PROFILE=ON "-DTURBOTUV_REAPER_SOURCES=$SOURCES" \
    > "build/reaper/r2-profile-$F-configure.log" 2>&1
  cmake --build "build/reaper/r2-profile-$F" -j6 > "build/reaper/r2-profile-$F-build.log" 2>&1
  "$PY" tools/reaper/profile.py "build/reaper/r2-profile-$F/turbotuv_bench" --reaper \
    --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json \
    --json "docs/evidence/reaper/r2/profile-$F.json"
  for CONFIG in tuv_5_4 ts1_tsmlt; do
    "$PY" tools/reaper/compare.py "build/reaper/main-$F/turbotuv_bench" \
      "build/reaper/r2-selected-$F/turbotuv_bench:--reaper" --config "examples/$CONFIG.json" \
      --columns 300 --json "docs/evidence/reaper/r2/gate-$F-$CONFIG.json"
    /usr/bin/time -l "build/reaper/r2-selected-$F/turbotuv_bench" "examples/$CONFIG.json" \
      --reaper --detail --columns 300 --repeat 3 \
      > "docs/evidence/reaper/r2/resources-$F-$CONFIG.log" 2>&1
  done
  "$PY" tools/reaper/heldout.py --base "build/reaper/r2-selected-$F" \
    --primary "build/reaper/r2-selected-$F/turbotuv_bench" --cc "$CC_BIN" --sysroot "$SDK" \
    --work "build/reaper/r2-heldout-$F" --evidence "docs/evidence/reaper/r2/heldout-$F"
done
SOURCES="$PWD/build/reaper/r2-combinations-clang/interleave-fma/kernel-0.c;$PWD/build/reaper/r2-combinations-clang/interleave-fma/kernel-1.c;$PWD/build/reaper/r2-combinations-clang/interleave-fma/registry.c"
cmake -S . -B build/reaper/r2-sanitize -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_SYSROOT="$SDK" -DTURBOTUV_SANITIZE=ON \
  "-DTURBOTUV_REAPER_SOURCES=$SOURCES" > docs/evidence/reaper/r2/configure-sanitize.log 2>&1
cmake --build build/reaper/r2-sanitize -j6 > docs/evidence/reaper/r2/build-sanitize.log 2>&1
ctest --test-dir build/reaper/r2-sanitize --output-on-failure \
  > docs/evidence/reaper/r2/ctest-sanitize.log 2>&1
python3 tools/bench_compare.py \
  main-gcc=build/reaper/main-gcc/turbotuv_bench:--repeat,5 \
  reaper-gcc=build/reaper/r2-selected-gcc/turbotuv_bench:--repeat,5,--reaper \
  main-clang=build/reaper/main-clang/turbotuv_bench:--repeat,5 \
  reaper-clang=build/reaper/r2-selected-clang/turbotuv_bench:--repeat,5,--reaper \
  --config examples/tuv_5_4.json --config examples/ts1_tsmlt.json --columns 300 --rounds 10 \
  --minimum-seconds 1 --gate docs/evidence/reaper/r2/gate-gcc-tuv_5_4.json \
  --gate docs/evidence/reaper/r2/gate-gcc-ts1_tsmlt.json \
  --gate docs/evidence/reaper/r2/gate-clang-tuv_5_4.json \
  --gate docs/evidence/reaper/r2/gate-clang-ts1_tsmlt.json \
  --allow-differences reaper-gcc --allow-differences reaper-clang \
  --csv docs/evidence/reaper/r2/timings.csv --json docs/evidence/reaper/r2/timings.json
python3 tools/reaper/provenance.py --build build/reaper/r2-selected-gcc --build build/reaper/r2-selected-clang \
  --build build/reaper/r2-sanitize --reference build/reaper/tuvx-source \
  --json docs/evidence/reaper/r2/provenance.json
