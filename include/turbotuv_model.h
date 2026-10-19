/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TURBOTUV_MODEL_H
#define TURBOTUV_MODEL_H
#include "turbotuv_config.h"
#ifdef __cplusplus
extern "C" {
#endif

/* A resolver supplies immutable compiled datasets. NULL uses the built-in
 * catalogue. Scientific constructors never open data files. */
typedef const tt_dataset *(*tt_data_resolver)(void *context, const char *path);
typedef struct {
    tt_data_resolver resolve;
    void *context;
} tt_data_source;
tt_status tt_grid_from_config(tt_config_node config, const tt_data_source *source, tt_grid *out,
                              tt_error *error);

/* Named grid views let profiles choose their configured axis without a
 * mutable global warehouse. Names and units are matched exactly.
 * Extraterrestrial-flux profiles populate midpoints; their other fields are
 * defined as zero. All other profile constructors use the usual edge fields. */
typedef struct {
    const char *name, *units;
    const tt_grid *grid;
} tt_named_grid;
tt_status tt_profile_from_config(tt_config_node config, size_t grid_count,
                                 const tt_named_grid *grids, const tt_data_source *source,
                                 tt_profile *out, tt_error *error);

typedef enum {
    TT_SPECTRUM_CROSS_SECTION,
    TT_SPECTRUM_QUANTUM_YIELD,
    TT_SPECTRUM_WEIGHT
} tt_spectrum_role;
typedef struct tt_spectrum tt_spectrum;
/* Prepare tables once on a fixed wavelength grid, using upstream JSON/YAML
 * configuration keys. The result owns its grid, tables and options; neither
 * config nor source must remain alive. Output is unchanged on failure. */
tt_status tt_spectrum_create(tt_spectrum_role role, tt_config_node config,
                             const tt_grid *wavelength, const tt_data_source *source,
                             tt_spectrum **out, tt_error *error);
void tt_spectrum_destroy(tt_spectrum *spectrum);
/* Cross sections/yields write [wavelength][level]. Supply temperature and air
 * arrays for these roles. Weights write wavelength entries and ignore levels
 * and both profile arguments. Prepared objects support concurrent readers. */
tt_status tt_spectrum_calculate(const tt_spectrum *spectrum, size_t levels,
                                const double *temperature, const double *air, double *out);
/* Immutable table views, valid until destroy; useful for inspecting the
 * wavelength interpolation. Parameterized cross sections have no tables. */
size_t tt_spectrum_table_count(const tt_spectrum *spectrum);
const tt_spectral_table *tt_spectrum_table(const tt_spectrum *spectrum, size_t index);
#ifdef __cplusplus
}
#endif
#endif
