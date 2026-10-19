# TurboTUV

C11 translation of NCAR TUV-x, with compiled-in scientific data and a direct
Fortran comparison suite. The port is in progress; the current library covers
interpolation, tridiagonal systems, spherical geometry, solar position,
radiator mixing, and the Delta-Eddington radiation solver.

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

The production library requires only a C compiler and the system math library.
All 190 TUV-x data files are embedded in the library. NetCDF values are stored
as exact hexadecimal doubles; text tables retain their original bytes. The
public API is in [include/turbotuv.h](include/turbotuv.h). Units, dimensions,
array ordering, ownership, and errors are documented there.

Reference testing uses the sibling `../TUV-x` checkout at revision
`ebe5006` (0.17.0). `tools/reference.py gcc` and `tools/reference.py llvm` build
and run its existing tests using the Homebrew compiler sets. `--prefix` and
`--netcdf` select compiler-matched NetCDF Fortran and NetCDF C installations.
The reference Python analyses need numpy, scipy, and netCDF4.

To enable direct numerical comparisons, configure with
`TURBOTUV_REFERENCE_BUILD`, `TURBOTUV_REFERENCE_PREFIX`,
`TURBOTUV_NETCDF_PREFIX`, and the matching `CMAKE_Fortran_COMPILER`.
The test-only bridge links the original Fortran library; production code does
not call Fortran. Fast math and floating-point contraction are disabled to
preserve the reference's arithmetic.

`tools/embed_data.py --check` checks regeneration, and
`tests/verify_data.py build/release/tests/dump_data` audits every compiled value
and text byte against the original scientific files. Only these development
tools need Python/netCDF4 and the original input files. Source hashes and
upstream revision are recorded in [data/manifest.json](data/manifest.json).

See [docs/PORTING.md](docs/PORTING.md) for scope and validation progress.
The upstream Apache-2.0 license and UCAR attribution apply to translated code
and scientific data.
