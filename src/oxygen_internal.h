/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TT_OXYGEN_INTERNAL_H
#define TT_OXYGEN_INTERNAL_H
#include "turbotuv.h"
/* Same argument validation/arithmetic as the public wrapper. The caller owns
 * at least 20 * nz scratch doubles, disjoint from inputs and outputs. */
tt_status tt_oxygen_optics_work(const tt_oxygen_bands *bands, size_t nw, size_t nz,
                                const double *temperature, const double *vertical,
                                const double *slant, const double *oxygen, double *depth,
                                double *cross, double *work);
#endif
