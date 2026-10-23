/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TT_REAPER_INTERNAL_H
#define TT_REAPER_INTERNAL_H
/* Compile-time experiment controls; all settings use ordinary portable C.
 * The selected defaults are recorded with whole-column measurements. */
#ifndef TT_REAPER_RATE_TILE
#ifdef __clang__
#define TT_REAPER_RATE_TILE 0
#else
#define TT_REAPER_RATE_TILE 16
#endif
#endif
#ifndef TT_REAPER_OVERLAP
#define TT_REAPER_OVERLAP 1
#endif
#ifndef TT_REAPER_OXYGEN
#define TT_REAPER_OXYGEN 1
#endif
#ifndef TT_REAPER_FLUX_CHECK
#define TT_REAPER_FLUX_CHECK 1
#endif
#endif
