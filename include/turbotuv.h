/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TURBOTUV_H
#define TURBOTUV_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* All physical quantities use the TUV-x units: nm, km, K, molecules/cm^3.
 * Arrays are contiguous, owned by the caller unless explicitly documented.
 * No routine terminates the process or holds mutable global state. */
typedef enum {
    TT_OK = 0,
    TT_INVALID,
    TT_RANGE,
    TT_MEMORY,
    TT_SINGULAR,
    TT_NOT_FOUND
} tt_status;
const char *tt_status_string(tt_status status);

typedef enum {
    TT_LINEAR,
    TT_CONSERVING,
    TT_FRACTIONAL_SOURCE,
    TT_FRACTIONAL_TARGET
} tt_interpolation;

/* nx, nt count coordinates; all grids must be strictly increasing.
 * Linear interpolation writes nt values, the other methods nt-1 bin values.
 * Linear interpolation returns zero outside [x[0], x[nx-1]), including the
 * last source point, matching TUV-x. Conserving requires full source coverage.
 * fold applies the reference's historical overhang convention unchanged. */
tt_status tt_interpolate(tt_interpolation method, int fold,
                         size_t nx, const double *x, const double *y,
                         size_t nt, const double *target, double *out);

/* Lower/upper diagonals have n entries; lower[0], upper[n-1] are unused.
 * work has n entries. Inputs and output/work must not overlap. */
tt_status tt_tridiagonal(size_t n, const double *lower, const double *diagonal,
                         const double *upper, const double *rhs,
                         double *solution, double *work);

/* The catalogue contains immutable, compiled-in copies of all upstream data.
 * Shape and storage follow NetCDF C order (last dimension contiguous).
 * Text entries are NUL terminated. Lookup accepts upstream relative paths. */
typedef struct {
    const char *name;
    size_t rank;
    size_t shape[4];
    size_t count;
    const double *values;
    const char *attributes_json;
} tt_data_variable;
typedef struct {
    const char *path;
    const char *sha256;
    size_t variable_count;
    const tt_data_variable *variables;
    const char *text;
    size_t text_length;
} tt_dataset;
size_t tt_dataset_count(void);
const tt_dataset *tt_dataset_at(size_t index);
const tt_dataset *tt_dataset_find(const char *path);
const tt_data_variable *tt_dataset_variable(const tt_dataset *data, const char *name);

/* Geometry uses increasing height edges. nid has n entries. dsdh is an n by
 * (n-1) row-major array, with rows ordered from top to bottom of atmosphere.
 * This is the ray-path convention used by both TUV-x transfer solvers. */
tt_status tt_spherical_geometry(size_t n, const double *height, double zenith,
                                int *nid, double *dsdh);
tt_status tt_air_mass(size_t n, const double *air_column, const int *nid,
                      const double *dsdh, double *vertical, double *slant);

/* Calendar follows the reference's supported range, 1950 through 2050.
 * Solar position returns elevation in degrees; zenith = 90 - elevation. */
int tt_day_of_year(int year, int month, int day);
double tt_earth_sun_distance(int year, int day, double utc_hour);
double tt_solar_elevation(int year, int day, double utc_hour,
                          double latitude, double longitude);

/* Spectral fields use [wavelength][layer/level] order, with height increasing.
 * Moments use [moment][wavelength][layer]; moment 0 is the first Legendre
 * coefficient. All input/output arrays are caller-owned. */
typedef struct {
    const double *optical_depth;
    const double *single_scattering_albedo;
    const double *asymmetry;
    int is_air;
} tt_radiator;
typedef struct {
    size_t layers, wavelengths, moments;
    const double *height;
    const double *optical_depth;
    const double *single_scattering_albedo;
    const double *legendre;
} tt_atmosphere;
typedef struct {
    double *direct_flux, *down_flux, *up_flux;
    double *direct_irradiance, *down_irradiance, *up_irradiance;
} tt_radiation;

tt_status tt_mix_radiators(size_t count, const tt_radiator *radiators,
                           size_t layers, size_t wavelengths, size_t moments,
                           double *optical_depth, double *albedo, double *legendre);
tt_status tt_delta_eddington(const tt_atmosphere *atmosphere, double zenith,
                             const double *surface_albedo, tt_radiation *out);

#ifdef __cplusplus
}
#endif
#endif
