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
#ifndef TT_REAPER_FIXED_CACHE
#define TT_REAPER_FIXED_CACHE 1
#endif
#ifndef TT_REAPER_DIRECT_CACHE
#define TT_REAPER_DIRECT_CACHE 1
#endif
#ifndef TT_REAPER_BLOCK
#define TT_REAPER_BLOCK 16
#endif
#ifndef TT_REAPER_ROLES
#define TT_REAPER_ROLES 1
#endif
#ifndef TT_REAPER_SPECTRUM_EXP
#define TT_REAPER_SPECTRUM_EXP 1
#endif
#ifndef TT_REAPER_TABLES
#define TT_REAPER_TABLES 1
#endif
#ifndef TT_REAPER_PARAMETER_BATCH
#define TT_REAPER_PARAMETER_BATCH 1
#endif
#ifndef TT_REAPER_PARAMETER_TILE
#ifdef __clang__
#define TT_REAPER_PARAMETER_TILE 4
#else
#define TT_REAPER_PARAMETER_TILE 8
#endif
#endif
#ifndef TT_REAPER_O2_LANES
#ifdef __clang__
#define TT_REAPER_O2_LANES 64
#else
#define TT_REAPER_O2_LANES 32
#endif
#endif
#ifndef TT_REAPER_COEFF_SQRT
#if defined(__GNUC__) && !defined(__clang__)
#define TT_REAPER_COEFF_SQRT 1
#else
#define TT_REAPER_COEFF_SQRT 0
#endif
#endif
#ifndef TT_REAPER_COMPACT
#if defined(__GNUC__) && !defined(__clang__)
#define TT_REAPER_COMPACT 1
#else
#define TT_REAPER_COMPACT 0
#endif
#endif
#ifndef TT_REAPER_MATH_HINTS
#define TT_REAPER_MATH_HINTS 1
#endif
#endif
