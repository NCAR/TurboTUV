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

* All 21 quantum-yield calculators and 13 spectral weighting functions, with
  temperature and density sweeps, adjacent floating-point values at cutoffs,
  all acetone branches, and the original Taylor-series constructor. The
  1,295,363 comparisons per compiler set have a largest relative error of
  13.5 epsilon-scaled units (32-unit ceiling). Independent complementary-branch
  and normalization tests pass, including under both sanitizers.

* All four temperature parameterizations (exponential polynomial, Taylor,
  Burkholder, Harwood), with a shared range/slice representation and the full
  temperature-dependent cross-section regridding path. All 417,727 comparisons
  are bit-for-bit identical on both compiler sets. Tests include overlapping
  inclusive ranges, fixed temperatures, both logarithm bases, inverted
  offsets, interior slices, and zero padding. Sanitizer checks pass.

* Actinic-flux and irradiance conversion, photolysis, heating, and dose
  integration. The 53,821 comparisons against original rate constructors and
  calculators are bit-for-bit identical on both compilers. They cover up to
  1,023 bands and 81 levels, three wavelength/temperature-dependent reaction
  spectra, variable thresholds/scaling, negative-total clipping, and darkness.
  Integrations traverse contiguous spectra and preserve each level's original
  summation order. Independent single-band and sanitizer checks pass.

* O2 Lyman-alpha and Schumann-Runge cross sections and optical depths, using
  compiled Chebyshev coefficients. All 318,240 comparisons are bit-for-bit
  identical on both compilers, including the 680 coefficients parsed by the
  original constructor, column cutoffs, nearly equal columns, extreme slant
  columns, temperature variation, and untouched neighboring bands. Sanitizers
  pass. The C code defines the all-thin-column limit using the reference's
  tabulated values; the original indexes outside its array in that case.

* Dense and packed-band LU factorization, reciprocal-condition estimation,
  and normal/transposed solves share one matrix representation and elimination
  path. All 753,290 factor/pivot/condition/solution comparisons are bit-for-bit
  identical on both compilers. Tests cover varied bandwidths and pivoting,
  sizes 1 through 81, extreme scales, independent normwise backward-error
  checks, and singular/invalid inputs. Sanitizers pass.

* Discrete-ordinate real eigenproblem, Gauss quadrature, and Legendre helpers.
  All 100,460 comparisons are bit-for-bit identical on both compilers, with
  independent eigen-equation residuals and polynomial quadrature integrals.
  Eigen tests include symmetric, similarity-scaled asymmetric, triangular,
  and isolated-block matrices through order 32; quadrature runs through order
  256. Private Fortran helpers are extracted verbatim into a test-only module
  with source provenance, leaving the original library unchanged. Sanitizers
  pass; a zero 2x2 matrix receives defined identity eigenvectors in C.

* Complete pseudo-spherical discrete-ordinate solver, specialized to TUV-x's
  fixed flux-only, delta-M, Lambertian-surface configuration. All 16 even
  stream counts from 2 through 32 pass 4,400 atmosphere cases and 16,368,400
  comparisons per compiler, within 46 epsilon-scaled units (64-unit ceiling).
  Cases cover 1/2/5/16/81 layers, 11 solar angles, thin/thick/vacuum optics,
  nearly conservative scattering, Rayleigh mixing, surface reflection and
  absorption-depth truncation. Both sanitizers pass. Layer scratch storage is
  reused across wavelengths. The band solver also covers bandwidths larger
  than matrix order, as needed for single-layer boundary systems.

* Shared owned grid/profile structures, equal-interval and supplied grids,
  compiled text-table parsing, air/O2 logarithmic profiles, O3 extension and
  column rescaling, solar position/distance profiles, solar-spectrum merging,
  and host updates. All 697 cases and 541,850 comparisons pass on both compiler
  sets within 5.85 epsilon-scaled units (32-unit ceiling), including zero-cell
  profiles, 240-layer profiles, upper columns, host midpoint overrides, and
  adjacent floating-point values at all solar splice boundaries. Sanitizers
  pass. The original solar conversion's default-REAL literal and opposite
  time-zone signs for zenith/distance are preserved explicitly.

* Gas and aerosol optical-property construction, plus immutable embedded
  radiator tables. All 432 cases and 2,316,702 comparisons pass on both compiler
  sets within 0.556 relative epsilon-scaled units (32-unit ceiling); sanitizers
  pass. Cases cover 1–120 layers, aerosol source grids of 2–121 points, column
  normalization, folded upper material, empty layers and air/absorber behavior.
  All 36 original NetCDF-radiator assertions are reproduced in C. The original
  single-sample aerosol constructor uses unallocated arrays; C defines it as
  uniform layer depth.
* The 110 modern upstream test input datasets are also compiled, in a separate
  test-only library. Both compiled-data audits compare 1,884,240 bytes exactly.
  Legacy TUV data and the generic NetCDF I/O fixture are excluded from this
  test catalogue; the original Fortran suites still run those tests normally.

* JSON/YAML configuration parsing uses a bundled, namespaced C LibYAML parser
  behind an immutable node API. All 281 documents and 26,385 checks pass on
  both compilers, including exact numeric/array/string/boolean comparisons
  through the original Fortran configuration interface, Unicode, aliases,
  null/string distinctions, file input, malformed/truncated input and bounded
  nesting. Both sanitizers pass. Upstream source hashes and MIT licensing are
  recorded under `third_party/libyaml`; no runtime dependency is introduced.

* Complete spectral constructors are connected to the C configuration API.
  The prepared objects own their fixed wavelength grid, parameter tables and
  options and never open scientific files. An optional resolver selects other
  compiled datasets, including the separate test catalogue. Shared validators
  keep direct-kernel and prepared-model requirements consistent.
  All 970 cases and 7,189,693 comparisons pass with GCC/GFortran (bit-identical)
  and Clang/Flang (worst 0.981 relative epsilon-scaled units, 32-unit ceiling).
  Address and undefined-behavior sanitizers pass. Cases include every spectral
  constructor from both complete examples, all modern spectral unit-test
  inputs, the production reaction/weight catalogues, midpoint evaluation,
  merged files, all interpolators, cutoffs, overrides, point data, extrapolation,
  and all four temperature parameterizations. The case generator records
  original source hashes and normalizes only the stale type labels that the
  original unit tests bypass by directly calling a specific constructor.
  The ozone constructor's replacement of the warm Hartley/Huggins column and
  quantum-yield upper-boundary padding convention are preserved explicitly.
  The CL2 reference allocates an extra undefined row at midpoints; comparisons
  include its defined rows. HNO3 and H2O2 ignore the reference midpoint flag;
  their edge evaluations are compared. C evaluates precisely the caller's
  supplied temperature array without that ambiguity.

## Reference portability

`tools/reference.py` creates an isolated source copy under `build/` and applies
test-harness portability corrections: legacy default-real promotion for Flang,
and portable temporary-file replacement for GNU-only `sed -i` commands. The
original checkout and numerical equations are unchanged.

The first unmodified GNU run built and passed 76/88 tests; remaining failures
were the BSD sed incompatibility and missing SciPy. Flang initially failed to
build legacy tests because real-kind promotion was omitted. After correcting
those issues, the new numerical comparisons pass on both compilers.
Both corrected reference builds now pass all 88 upstream tests. Two additional
test-harness corrections are needed: explicitly rewind the reopened legacy
CH2O input, and route the C++ placeholder test's large structs through pointer
arguments. Instrumentation isolated the Flang crash to the by-value C call on
Darwin/arm64. The pointer shim preserves all original inputs, outputs, and
assertions. No numerical equations or tolerances are changed.

Undefined reference behavior is not copied: the C air-column routine defines
the top vertical-column entry, which Fortran leaves unwritten; comparisons
exclude that entry. C validates grids and prevents out-of-bounds access. The
reference conserving interpolator assumes padded source grids, so randomized
reference comparisons use interior target bins.

## Remaining implementation tasks

* Assemble grids, profiles, radiators, solvers and prepared spectra into the model.
* Connect O2 corrections and rate calculations to the complete model.
* Standalone model execution and reproduction of all existing model scenarios.
* Extended reference tests, complete compiler matrix, sanitizer checks and final audit.
