/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TURBOTUV_DIAGNOSTICS_H
#define TURBOTUV_DIAGNOSTICS_H
#include "turbotuv_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Per-model counters. A failed fast-path attempt can increment both a decline
 * and the fallback executor's count. Reset outside tt_model_run. No globals. */
typedef enum {
    TT_STAGE_GEOMETRY,
    TT_STAGE_GAS,
    TT_STAGE_OXYGEN_DEPTH,
    TT_STAGE_TRANSFER,
    TT_STAGE_FLUX,
    TT_STAGE_SPECTRA_DIRECT,
    TT_STAGE_SPECTRA_YIELD,
    TT_STAGE_SPECTRA_PARAMETERIZED,
    TT_STAGE_OXYGEN_CROSS,
    TT_STAGE_PHOTOLYSIS,
    TT_STAGE_HEATING,
    TT_STAGE_DOSE,
    TT_STAGE_MIX,
    TT_STAGE_SLANT,
    TT_STAGE_COEFFICIENTS,
    TT_STAGE_SOLVE,
    TT_STAGE_OUTPUT,
    TT_STAGE_COUNT
} tt_stage;
typedef struct {
    size_t prepared_calls, generic_calls, prepared_declines;
    size_t reaper_calls, reaper_declines, generated_calls, generated_declines;
    size_t calls[TT_STAGE_COUNT];
    double seconds[TT_STAGE_COUNT];
    int profiling; /* nonzero only in TURBOTUV_PROFILE builds */
} tt_diagnostics;
tt_status tt_model_diagnostics(const tt_model *model, tt_diagnostics *out);
tt_status tt_model_reset_diagnostics(tt_model *model);
const char *tt_stage_name(tt_stage stage);
#ifdef __cplusplus
}
#endif
#endif
