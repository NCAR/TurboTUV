# TurboTUV

C11 translation of NCAR TUV-x, with compiled-in scientific data and a direct
Fortran comparison suite. The port is in progress; the current library covers
interpolation, tridiagonal systems, spherical geometry, solar position,
radiator mixing, both Delta-Eddington and discrete-ordinate radiation solvers, and 26 direct
cross-section calculators, 21 quantum-yield calculators, and 13 spectral
weighting functions, all four temperature parameterizations, and photolysis,
heating, and dose integration, including the O2 band corrections. Grid and
profile constructors use embedded atmospheric and solar spectra; host updates
share the same structures. JSON and YAML parsing use an immutable C interface
in [include/turbotuv_config.h](include/turbotuv_config.h), backed by bundled
[LibYAML](https://github.com/yaml/libyaml) sources with private symbol prefixes.
Prepared spectral models in [include/turbotuv_model.h](include/turbotuv_model.h)
construct cross sections, quantum yields, and weights from the original
configuration formats and compiled datasets. They own their tables and support
repeated evaluation with different atmospheric profiles. The same header
provides grid and profile constructors for the original configuration formats.

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
