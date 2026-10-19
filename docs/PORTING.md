# Translation and validation record

Reference: `../TUV-x`, revision `ebe5006` (version 0.17.0).

The acceptance target is the original unit/regression/example coverage plus
new boundary, randomized, and analytic tests, compared directly with Fortran
at double-precision roundoff. Passing the upstream percent-level regression
tolerances alone does not establish acceptance.

## Completed implementation tasks

* C11 build, public numerical API, explicit error returns and caller ownership.
* All 190 upstream scientific/configuration data files embedded, including
  metadata and provenance. Compiled-data audit: 4,547,069 bytes compared exactly
  on both compiler sets; no external runtime data files are required.
* Linear, conserving, fractional-source and fractional-target interpolation,
  including the reference overhang convention; tridiagonal solver; spherical
  ray geometry and air columns; Gregorian date and solar position routines.
* 863,465 direct Fortran/analytic numerical comparisons per compiler set;
  largest error is 2 epsilon-scaled units, with a 32-unit acceptance ceiling.
  Solar-angle error is scaled by 90 degrees to account for cancellation near
  the horizon; other comparisons use relative error. Exact zeros must match.

Compiler versions tested: Homebrew GCC/gfortran 16.2.0 and Clang/Flang 23.1.3.
Both use `-ffp-contract=off`; fast math is not enabled.

* Delta-Eddington solver and optical-property mixing: 275 atmospheres and
  377,125 comparisons per compiler set, including 1/2/5/16/81 layers, 11 solar
  angles from overhead to night, vacuum, thin/thick layers, pure scattering,
  Rayleigh contributions, and surface albedos from zero to one. All six fields
  match the original Fortran within 11.1 epsilon-scaled units (64-unit ceiling,
  normalized incident-beam scale). Independent surface-reflection and direct
  beam boundary checks pass. The C-only invariants also pass Clang address and
  undefined-behavior sanitizers.

* All 26 direct cross-section kernels use a common grid and parameter-table
  representation. Tests exercise 25 temperatures and 1,464 wavelengths,
  including neighboring representable values at wavelength cutoffs. The
  951,643 comparisons per compiler set have a largest relative error of
  0.624 epsilon-scaled units (32-unit ceiling). Embedded column regridding
  also checks conservation against an independently calculated trapezoid
  integral. C-only spectral tests pass both sanitizers.

## Reference portability

`tools/reference.py` creates an isolated source copy under `build/` and applies
test-harness portability corrections: legacy default-real promotion for Flang,
and portable temporary-file replacement for GNU-only `sed -i` commands. The
original checkout and numerical equations are unchanged.

The first unmodified GNU run built and passed 76/88 tests; remaining failures
were the BSD sed incompatibility and missing SciPy. Flang initially failed to
build legacy tests because real-kind promotion was omitted. After correcting
those issues, the new numerical comparisons pass on both compilers. Further
legacy Flang test issues are under investigation and are not counted as passes.
The corrected GNU baseline passes all 88 upstream tests. The Flang baseline
passes all 75 unit/example tests; twelve legacy regressions encounter an input
failure in old TUV, and the existing C++ placeholder comparison segfaults.

Undefined reference behavior is not copied: the C air-column routine defines
the top vertical-column entry, which Fortran leaves unwritten; comparisons
exclude that entry. C validates grids and prevents out-of-bounds access. The
reference conserving interpolator assumes padded source grids, so randomized
reference comparisons use interior target bins.

## Remaining implementation tasks

* Temperature parameterizations, quantum yields and action spectra; complete
  construction of cross-section tables from the model configuration.
* Grid/profile/configuration model and embedded table preparation.
* Discrete-ordinate solver and full radiator/profile construction.
* O2 Lyman-alpha/Schumann-Runge parameterization, photolysis, dose and heating rates.
* Standalone model execution and reproduction of all existing model scenarios.
* Extended reference tests, complete compiler matrix, sanitizer checks and final audit.
