/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TURBOTUV_REAPER_H
#define TURBOTUV_REAPER_H
#include "turbotuv_model.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TT_REAPER_ABI 1u
#define TT_REAPER_SCHEMA 1u
#define TT_REAPER_IMPLEMENTATION 1u
typedef enum { TT_REAPER_ORDERED = 0, TT_REAPER_ROUNDOFF = 1 } tt_reaper_numerics;
/* No private model layout crosses this interface. Buffers are disjoint;
 * spectra contains immutable per-call rows, and rates is writable. All
 * dimensions/slots are checked at attachment. Code must not retain views. */
typedef struct {
    size_t levels, wavelengths, slots;
    const double *actinic, *irradiance;
    const double *const *spectra;
    double *rates;
} tt_reaper_view;
typedef int (*tt_reaper_kernel)(const tt_reaper_view *view);
typedef struct {
    unsigned abi, schema, implementation;
    tt_reaper_numerics numerics;
    size_t levels, wavelengths, slots, photolysis, heating, dose;
    unsigned term_budget, accumulators, level_tile;
    unsigned char model_sha256[32];
    tt_reaper_kernel photolysis_kernel, heating_kernel, dose_kernel;
} tt_reaper_kernels;
/* Canonical C-builder description, with doubles encoded as IEEE binary64 bit
 * strings. No process pointers or mutable column values enter the identity.
 * Export requires binary64; ordinary execution remains available otherwise. */
tt_status tt_model_export_reaper(tt_model *model, FILE *file, unsigned char digest[32]);
tt_status tt_model_reaper_identity(tt_model *model, unsigned char digest[32]);
/* Copies the descriptor. Its code must outlive attachment (static code is
 * the required deployment route). NULL detaches. A mismatch returns
 * TT_INVALID and a diagnostic, preserving the previous attachment and model.
 * Null family slots use the portable interpreter. Call outside a model run. */
tt_status tt_model_attach_reaper(tt_model *model, const tt_reaper_kernels *kernels,
                                 tt_error *error);
unsigned tt_model_reaper_capabilities(const tt_model *model); /* 1 photolysis, 2 heating, 4 dose */
#ifdef __cplusplus
}
#endif
#endif
