/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 *
 * Prepared spectra. Each kind below restates the generic kernel in spectra.c,
 * yields.c or temperature.c for one wavelength at many levels. Subexpressions
 * of wavelength alone are computed when the plan is built, those of level
 * alone once per evaluation, and the remaining operations keep the generic
 * order, so each value is identical to the generic kernel's. Wavelengths are
 * classified as constant only where the kernel's branches or data make the
 * value independent of temperature and air density; the generic kernel
 * evaluates those once, and a sweep of reference columns guards the
 * classification. */
#include "spectrum_plan.h"
#include "reaper_internal.h"
#include "reaper_math.h"
#include "reaper_writer.h"
#include "spectral_validation.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Constant classifications that rely on a finite exponential hold for all
 * temperatures up to this limit; hotter columns use the generic path. */
#define TEMPERATURE_LIMIT 1.0e4
/* exp and pow(10, x) stay finite below these arguments. */
#define EXP_LIMIT 700.0
#define POW10_LIMIT 300.0

typedef struct {
    size_t point;
    double dx, left, right, half_width, weight_a, weight_b;
} overlap_segment;
struct tt_spectrum_plan {
    const tt_spectrum *s;
    int cs, qy, kind, generic;
    size_t nw, count, stride, tables, required;
    double *constant, *coef, maximum_temperature;
    size_t *index;
    /* Parameterized cross sections: padded raw grid and values, powers of
     * the exponential form, varying padded points and target bins. */
    size_t raw, first_bin, end_bin;
    int interpolation_bounded;
    unsigned char *interpolation_mode;
    double *x, *y, *powers;
    size_t *overlap_offsets;
    overlap_segment *overlaps;
    size_t fixed_count;
    uint64_t *fixed_masks;
    double *fixed_rows;
    double *clamped, clamp_low, clamp_high;
};

static double clamp(double t, double lo, double hi) {
    return fmax(lo, fmin(t, hi));
}
static double sq(double x) {
    return x * x;
}
/* Both generic files use this repeated-squaring power. */
static double integer_power(double x, size_t n) {
    double y = 1;
    while (n) {
        if (n & 1)
            y *= x;
        n >>= 1;
        if (n)
            x *= x;
    }
    return y;
}
static double parameter(const tt_spectral_table *p, size_t w, size_t c) {
    return p->values[c * p->rows + w];
}
/* Temperature-table interpolation index and fraction, as in spectra.c. */
static void bracket(const tt_spectral_table *q, double t, size_t *j, double *f) {
    size_t k = 0;
    while (k + 2 < q->temperature_count && t > q->temperatures[k + 1])
        ++k;
    *j = k;
    *f = (t - q->temperatures[k]) / (q->temperatures[k + 1] - q->temperatures[k]);
}
/* True when every interpolation column at w holds the same finite value, so
 * interpolation returns that value for any fraction. */
static int flat(const tt_spectral_table *q, size_t w) {
    double v = parameter(q, w, 0);
    if (!isfinite(v))
        return 0;
    for (size_t c = 1; c < q->temperature_count; ++c)
        if (parameter(q, w, c) != v || !isfinite(parameter(q, w, c)))
            return 0;
    return 1;
}
/* Largest |T - offset| for temperatures in (0, TEMPERATURE_LIMIT]. */
static double offset_bound(double offset) {
    return fmax(fabs(offset), fabs(TEMPERATURE_LIMIT - offset));
}

/* ---- Cross sections ---------------------------------------------------- */

enum { CS_STRIDE = 4 };

/* Fill coefficients for wavelength w and return whether its value varies. */
static int cs_prepare(tt_spectrum_plan *p, size_t w, double *c) {
    const tt_spectrum *s = p->s;
    const tt_spectral_table *t = s->tables;
    double lambda = s->wavelength.midpoints[w], edge = s->wavelength.edges[w];
    double base = p->required && p->kind != TT_CS_HCFC ? parameter(t, w, 0) : 0, coeff;
    switch (p->kind) {
    case TT_CS_ACETONE:
    case TT_CS_CH2O:
    case TT_CS_CLONO2:
        c[0] = base;
        for (size_t k = 1; k < (p->kind == TT_CS_ACETONE ? 4u
                                : p->kind == TT_CS_CH2O  ? 2u
                                                         : 3u);
             ++k)
            c[k] = parameter(t, w, k);
        return 1;
    case TT_CS_CCL4:
    case TT_CS_CHCL3:
        if (p->kind == TT_CS_CCL4 ? !(lambda > 194 && lambda < 250)
                                  : !(lambda > 190 && lambda < 240))
            return 0;
        coeff =
            p->kind == TT_CS_CCL4
                ? 1.0739 +
                      lambda * (-1.6275e-2 +
                                lambda * (8.8141e-5 + lambda * (-1.9811e-7 + 1.5022e-10 * lambda)))
                : 3.7973 +
                      lambda * (-7.0913e-2 +
                                lambda * (4.9397e-4 + lambda * (-1.5226e-6 + lambda * 1.7555e-9)));
        /* t is clamped to [-85, 5]: a zero base stays zero. */
        if (base == 0 && fabs(coeff) * 85 < POW10_LIMIT)
            return 0;
        c[0] = base;
        c[1] = coeff;
        return 1;
    case TT_CS_CFC11:
        c[0] = base;
        c[1] = lambda - 184.9;
        return !(base == 0 && fabs(c[1]) * 1.e-4 * offset_bound(298) < EXP_LIMIT);
    case TT_CS_CH3ONO2:
    case TT_CS_HNO3:
    case TT_CS_RONO2:
        c[0] = base;
        c[1] = parameter(t, w, 1);
        if (c[1] == 0 || (base == 0 && fabs(c[1]) * offset_bound(298) < EXP_LIMIT))
            return 0;
        return 1;
    case TT_CS_CHBR3:
        if (!(lambda > 290 && lambda < 340))
            return 0;
        c[0] = .06183 - .000241 * lambda;
        c[1] = 2.376 + .14757 * lambda;
        c[2] = base;
        return 1;
    case TT_CS_CL2:
        c[0] = sq(log(329.5 / lambda));
        c[1] = sq(log(406.5 / lambda));
        return 1;
    case TT_CS_H2O2:
        if (!(edge >= 260 && edge < 350))
            return 0;
        c[0] = ((((((1.5534675e-13 * lambda - 2.652014e-10) * lambda + 1.6878206e-7) * lambda -
                   4.035101e-5) *
                      lambda -
                  4.4589016e-3) *
                     lambda +
                 4.535649) *
                    lambda -
                9.2170972e2) *
                   lambda +
               6.4761e4;
        c[1] = (((-1.0924e-7 * lambda - 3.0493e-5) * lambda + 1.1522e-1) * lambda - 5.1351e1) *
                   lambda +
               6.8123e3;
        return 1;
    case TT_CS_HCFC:
        if (!(lambda >= 190 && lambda <= t[0].temperatures[1]))
            return 0;
        for (size_t k = 0; k < t[0].rows; ++k)
            c[k] = integer_power(lambda - 206.214, k);
        return 1;
    case TT_CS_N2O:
        if (!(lambda >= 173 && lambda <= 240))
            return 0;
        c[0] = (((2.520672e-7 * lambda - 1.777846e-4) * lambda + 4.301146e-2) * lambda - 4.071805) *
                   lambda +
               68.21023;
        c[1] = exp(((-1.881058e-5 * lambda + 1.111572e-2) * lambda - 2.116255) * lambda + 123.4014);
        return 1;
    case TT_CS_N2O5:
        c[0] = base;
        c[1] = 1000 * parameter(t + 1, w, 0);
        /* (300 - t) / (300 t) lies in [0, 1/1043] for t clamped to [233, 300]. */
        return !(c[1] == 0 || (base == 0 && fabs(c[1]) / 1043 < POW10_LIMIT));
    case TT_CS_NO2:
    case TT_CS_TINT:
        for (size_t k = 0; k < p->tables; ++k)
            if (!flat(t + k, w))
                return 1;
        return 0;
    case TT_CS_O3_JPL06:
        if (!(edge >= 196.078 && s->wavelength.edges[w + 1] <= 342.5))
            return 0;
        c[0] = parameter(t + 1, w, 0);
        c[1] = (parameter(t + 2, w, 0) - c[0]) / (298 - 218);
        c[2] = parameter(t + 2, w, 0);
        return 1;
    case TT_CS_O3: {
        double v185 = 185 * tt_air_refractive_index(185, 2.45e19);
        double v195 = 195 * tt_air_refractive_index(195, 2.45e19);
        double v345 = 345 * tt_air_refractive_index(345, 2.45e19);
        size_t table = edge < v185 ? 2 : edge < v195 ? 3 : edge < v345 ? 1 : 0;
        c[0] = (double)table;
        return table && !flat(t + table, w);
    }
    case TT_CS_OCLO:
        return 1;
    default:
        return 0;
    }
}

/* Level quantities shared by all wavelengths, then rows for each varying one. */
static void cs_evaluate(const tt_spectrum_plan *p, size_t nz, const double *temp, double *z,
                        double *rows, int reaper) {
    const tt_spectral_table *t = p->s->tables;
    size_t nt = p->tables;
    switch (p->kind) {
    case TT_CS_ACETONE:
        for (size_t i = 0; i < nz; ++i)
            z[i] = clamp(temp[i], 235, 298);
        break;
    case TT_CS_CCL4:
    case TT_CS_CHCL3:
        for (size_t i = 0; i < nz; ++i)
            z[i] = clamp(temp[i], 210, 300) - 295;
        break;
    case TT_CS_CFC11:
        for (size_t i = 0; i < nz; ++i)
            z[i] = 1.e-4 * (temp[i] - 298);
        break;
    case TT_CS_CH2O:
    case TT_CS_CH3ONO2:
    case TT_CS_HNO3:
    case TT_CS_RONO2:
        for (size_t i = 0; i < nz; ++i)
            z[i] = temp[i] - 298;
        break;
    case TT_CS_CHBR3:
        for (size_t i = 0; i < nz; ++i)
            z[i] = 273 - temp[i];
        break;
    case TT_CS_CL2:
        for (size_t i = 0; i < nz; ++i) {
            double bb = exp(402.7 / temp[i]), bbsq = bb * bb, alpha = (bbsq - 1) / (bbsq + 1);
            z[i] = -99.0 * alpha;
            z[nz + i] = -91.5 * alpha;
            z[2 * nz + i] = 1.e-20 * sqrt(alpha);
        }
        break;
    case TT_CS_CLONO2:
        for (size_t i = 0; i < nz; ++i)
            z[i] = temp[i] - 296;
        break;
    case TT_CS_H2O2:
        for (size_t i = 0; i < nz; ++i) {
            z[i] = 1 / (1 + exp(-1265 / clamp(temp[i], 200, 400)));
            z[nz + i] = 1 - z[i];
        }
        break;
    case TT_CS_HCFC:
        /* Per level, each polynomial coefficient of the exponent. */
        for (size_t i = 0; i < nz; ++i) {
            double u = clamp(temp[i], 203, 295) - t[0].temperatures[0];
            for (size_t k = 0; k < t[0].rows; ++k)
                z[k * nz + i] =
                    parameter(t, k, 0) + u * (parameter(t, k, 1) + u * parameter(t, k, 2));
        }
        break;
    case TT_CS_N2O:
        for (size_t i = 0; i < nz; ++i)
            z[i] = clamp(temp[i], 194, 320) - 300;
        break;
    case TT_CS_N2O5:
        for (size_t i = 0; i < nz; ++i) {
            double u = clamp(temp[i], 233, 300);
            z[i] = 300 - u;
            z[nz + i] = 300 * u;
        }
        break;
    case TT_CS_NO2:
    case TT_CS_TINT:
        for (size_t k = 0; k < nt; ++k)
            for (size_t i = 0; i < nz; ++i) {
                const tt_spectral_table *q = t + k;
                size_t j;
                bracket(
                    q,
                    clamp(temp[i], q->temperatures[0], q->temperatures[q->temperature_count - 1]),
                    &j, z + (2 * k + 1) * nz + i);
                z[2 * k * nz + i] = (double)j;
            }
        break;
    case TT_CS_O3:
        for (size_t k = 1; k < 4; ++k)
            for (size_t i = 0; i < nz; ++i) {
                const tt_spectral_table *q = t + k;
                size_t j;
                bracket(
                    q,
                    clamp(temp[i], q->temperatures[0], q->temperatures[q->temperature_count - 1]),
                    &j, z + (2 * k + 1) * nz + i);
                z[2 * k * nz + i] = (double)j;
            }
        break;
    case TT_CS_OCLO:
        for (size_t i = 0; i < nz; ++i) {
            double u = temp[i];
            size_t k = 0;
            if (u <= t[0].temperatures[0])
                z[i] = -1;
            else if (u >= t[nt - 1].temperatures[0])
                z[i] = -2;
            else {
                while (k + 2 < nt && t[k + 1].temperatures[0] <= u)
                    ++k;
                z[i] = (double)k;
                z[nz + i] =
                    (u - t[k].temperatures[0]) / (t[k + 1].temperatures[0] - t[k].temperatures[0]);
            }
        }
        break;
    default:
        break;
    }
    for (size_t k = 0; k < p->count; ++k) {
        size_t w = p->index[k];
        const double *c = p->coef + k * p->stride;
        double *out = rows + k * nz;
        switch (p->kind) {
        case TT_CS_ACETONE:
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] * (1.0 + z[i] * (c[1] + z[i] * (c[2] + z[i] * c[3])));
            break;
        case TT_CS_CCL4:
        case TT_CS_CHCL3:
            if (TT_REAPER_SPECTRUM_EXP && reaper && c[0] >= 1.e-40 && c[0] <= 1.e40) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[1] * z[i];
                tt_reaper_pow10_array(nz, out, out);
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[0] * out[i];
                break;
            }
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] * pow(10, c[1] * z[i]);
            break;
        case TT_CS_CFC11:
            if (TT_REAPER_SPECTRUM_EXP && reaper && c[0] >= 1.e-40 && c[0] <= 1.e40) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[1] * z[i];
                tt_reaper_exp_array(nz, out, out);
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[0] * out[i];
                break;
            }
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] * exp(c[1] * z[i]);
            break;
        case TT_CS_CH2O:
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] + c[1] * z[i];
            break;
        case TT_CS_CH3ONO2:
        case TT_CS_HNO3:
        case TT_CS_RONO2:
            if (TT_REAPER_SPECTRUM_EXP && reaper && c[0] >= 1.e-40 && c[0] <= 1.e40) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[1] * z[i];
                tt_reaper_exp_array(nz, out, out);
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[0] * out[i];
                break;
            }
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] * exp(c[1] * z[i]);
            break;
        case TT_CS_CHBR3:
            for (size_t i = 0; i < nz; ++i)
                out[i] = temp[i] > 210 && temp[i] < 300 ? exp(c[0] * z[i] - c[1]) : c[2];
            break;
        case TT_CS_CL2:
            if (TT_REAPER_SPECTRUM_EXP && reaper) {
                double *ex1 = z + 3 * nz, *ex2 = ex1 + nz;
                for (size_t i = 0; i < nz; ++i) {
                    ex1[i] = z[i] * c[0];
                    ex2[i] = z[nz + i] * c[1];
                }
                tt_reaper_exp_array(2 * nz, ex1, ex1);
                for (size_t i = 0; i < nz; ++i)
                    out[i] = z[2 * nz + i] * (27.3 * ex1[i] + .932 * ex2[i]);
                break;
            }
            for (size_t i = 0; i < nz; ++i) {
                double ex1 = 27.3 * exp(z[i] * c[0]), ex2 = .932 * exp(z[nz + i] * c[1]);
                out[i] = z[2 * nz + i] * (ex1 + ex2);
            }
            break;
        case TT_CS_CLONO2:
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] * (1 + z[i] * (c[1] + z[i] * c[2]));
            break;
        case TT_CS_H2O2:
            for (size_t i = 0; i < nz; ++i)
                out[i] = (z[i] * c[0] + z[nz + i] * c[1]) * 1.e-21;
            break;
        case TT_CS_HCFC:
            if (reaper) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = 0;
                for (size_t j = 0; j < t[0].rows; ++j)
                    for (size_t i = 0; i < nz; ++i)
                        out[i] = out[i] + z[j * nz + i] * c[j];
                if (TT_REAPER_SPECTRUM_EXP)
                    tt_reaper_exp_array(nz, out, out);
                else
                    for (size_t i = 0; i < nz; ++i)
                        out[i] = exp(out[i]);
                break;
            }
            for (size_t i = 0; i < nz; ++i) {
                double sigma = 0;
                for (size_t j = 0; j < t[0].rows; ++j)
                    sigma = sigma + z[j * nz + i] * c[j];
                out[i] = TT_REAPER_SPECTRUM_EXP && reaper ? sigma : exp(sigma);
            }
            if (TT_REAPER_SPECTRUM_EXP && reaper)
                tt_reaper_exp_array(nz, out, out);
            break;
        case TT_CS_N2O:
            if (TT_REAPER_SPECTRUM_EXP && reaper) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[0] + z[i] * c[1];
                tt_reaper_exp_array(nz, out, out);
                break;
            }
            for (size_t i = 0; i < nz; ++i)
                out[i] = exp(c[0] + z[i] * c[1]);
            break;
        case TT_CS_N2O5:
            if (TT_REAPER_SPECTRUM_EXP && reaper && c[0] >= 1.e-40 && c[0] <= 1.e40) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[1] * z[i] / z[nz + i];
                tt_reaper_pow10_array(nz, out, out);
                for (size_t i = 0; i < nz; ++i)
                    out[i] = c[0] * out[i];
                break;
            }
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] * pow(10, c[1] * z[i] / z[nz + i]);
            break;
        case TT_CS_NO2:
        case TT_CS_TINT:
            if (TT_REAPER_TABLES && reaper) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = 0;
                for (size_t q = 0; q < nt; ++q) {
                    if (t[q].temperature_count == 2) {
                        double base = parameter(t + q, w, 0);
                        double difference = parameter(t + q, w, 1) - base;
                        for (size_t i = 0; i < nz; ++i)
                            out[i] = out[i] + base + z[(2 * q + 1) * nz + i] * difference;
                    } else
                        for (size_t i = 0; i < nz; ++i) {
                            size_t j = (size_t)z[2 * q * nz + i];
                            double base = parameter(t + q, w, j);
                            out[i] = out[i] + base +
                                     z[(2 * q + 1) * nz + i] * (parameter(t + q, w, j + 1) - base);
                        }
                }
                break;
            }
            for (size_t i = 0; i < nz; ++i) {
                double value = 0;
                for (size_t q = 0; q < nt; ++q) {
                    size_t j = (size_t)z[2 * q * nz + i];
                    double f = z[(2 * q + 1) * nz + i];
                    value = value + parameter(t + q, w, j) +
                            f * (parameter(t + q, w, j + 1) - parameter(t + q, w, j));
                }
                out[i] = value;
            }
            break;
        case TT_CS_O3_JPL06:
            for (size_t i = 0; i < nz; ++i)
                out[i] = temp[i] < 218    ? c[0]
                         : temp[i] <= 298 ? c[0] + c[1] * (temp[i] - 218)
                                          : c[2];
            break;
        case TT_CS_O3: {
            size_t q = (size_t)c[0];
            if (TT_REAPER_TABLES && reaper && t[q].temperature_count == 2) {
                double base = parameter(t + q, w, 0);
                double difference = parameter(t + q, w, 1) - base;
                for (size_t i = 0; i < nz; ++i)
                    out[i] = base + z[(2 * q + 1) * nz + i] * difference;
                break;
            }
            for (size_t i = 0; i < nz; ++i) {
                size_t j = (size_t)z[2 * q * nz + i];
                double f = z[(2 * q + 1) * nz + i];
                out[i] = parameter(t + q, w, j) +
                         f * (parameter(t + q, w, j + 1) - parameter(t + q, w, j));
            }
            break;
        }
        case TT_CS_OCLO:
            for (size_t i = 0; i < nz; ++i) {
                if (z[i] == -1)
                    out[i] = parameter(t, w, 0);
                else if (z[i] == -2)
                    out[i] = parameter(t + nt - 1, w, 0);
                else {
                    size_t q = (size_t)z[i];
                    out[i] = parameter(t + q, w, 0) +
                             z[nz + i] * (parameter(t + q + 1, w, 0) - parameter(t + q, w, 0));
                }
            }
            break;
        default:
            break;
        }
    }
}

/* Cache only complete clamp domains. Unclamped levels still execute the
 * original formula, packed into a temporary level set. No temperature bins
 * or approximations enter this reuse. */
static size_t cs_scratch(const tt_spectrum_plan *p, size_t nz) {
    return p->kind == TT_CS_HCFC ? p->s->tables[0].rows * nz : (8 + 2 * p->tables) * nz;
}
static void direct_cache_prepare(tt_spectrum_plan *p) {
    if (!TT_REAPER_DIRECT_CACHE || !p->cs || p->generic || !p->count)
        return;
    double lo, hi;
    switch (p->kind) {
    case TT_CS_CCL4:
    case TT_CS_CHCL3:
        lo = 210;
        hi = 300;
        break;
    case TT_CS_HCFC:
        lo = 203;
        hi = 295;
        break;
    case TT_CS_N2O:
        lo = 194;
        hi = 320;
        break;
    case TT_CS_N2O5:
        lo = 233;
        hi = 300;
        break;
#if TT_REAPER_DIRECT_CACHE > 1
    case TT_CS_ACETONE:
        lo = 235;
        hi = 298;
        break;
    case TT_CS_H2O2:
        lo = 200;
        hi = 400;
        break;
    case TT_CS_O3_JPL06:
        lo = nextafter(218, -INFINITY);
        hi = nextafter(298, INFINITY);
        break;
    case TT_CS_CHBR3:
        lo = 210;
        hi = 300;
        break;
    case TT_CS_OCLO:
        lo = p->s->tables[0].temperatures[0];
        hi = p->s->tables[p->tables - 1].temperatures[0];
        break;
    case TT_CS_NO2:
    case TT_CS_TINT:
    case TT_CS_O3:
        lo = DBL_MAX;
        hi = -DBL_MAX;
        for (size_t q = p->kind == TT_CS_O3 ? 1 : 0; q < p->tables; ++q) {
            const tt_spectral_table *t = p->s->tables + q;
            lo = fmin(lo, t->temperatures[0]);
            hi = fmax(hi, t->temperatures[t->temperature_count - 1]);
        }
        break;
#endif
    default:
        return;
    }
    p->clamped = malloc(2 * p->count * sizeof(double));
    double *scratch = malloc(cs_scratch(p, 2) * sizeof(double));
    if (!p->clamped || !scratch) {
        free(p->clamped);
        p->clamped = NULL;
        free(scratch);
        return;
    }
    double temp[] = {lo, hi};
    p->clamp_low = lo;
    p->clamp_high = hi;
    cs_evaluate(p, 2, temp, scratch, p->clamped, 0);
    free(scratch);
}
static void direct_cached_original(const tt_spectrum_plan *p, size_t nz, const double *temperature,
                                   double *scratch, double *rows) {
    double *packed = scratch, *work = packed + nz, *values = work + cs_scratch(p, nz);
    size_t active = 0;
    for (size_t z = 0; z < nz; ++z)
        if (!(temperature[z] <= p->clamp_low || temperature[z] >= p->clamp_high))
            packed[active++] = temperature[z];
    if (active == nz) {
        cs_evaluate(p, nz, temperature, scratch, rows, 1);
        return;
    }
    if (active)
        cs_evaluate(p, active, packed, work, values, 1);
    for (size_t k = 0; k < p->count; ++k) {
        double lower = p->clamped[2 * k], upper = p->clamped[2 * k + 1];
        size_t j = 0;
        for (size_t z = 0; z < nz; ++z)
            rows[k * nz + z] = temperature[z] <= p->clamp_low    ? lower
                               : temperature[z] >= p->clamp_high ? upper
                                                                 : values[k * active + j++];
    }
}

/* Classify clamp regions once, then fill cached runs or copy active runs.
 * The packed run endpoints must be exactly representable as doubles. */
static void direct_cached(const tt_spectrum_plan *p, size_t nz, const double *temperature,
                          double *scratch, double *rows) {
    if (FLT_RADIX != 2 || DBL_MANT_DIG < 53 || nz > 0x1p51 || nz > (SIZE_MAX - 2) / 3) {
        direct_cached_original(p, nz, temperature, scratch, rows);
        return;
    }
    double *packed = scratch, *work = packed + nz, *values = work + cs_scratch(p, nz);
    size_t active = 0;
    for (size_t z = 0; z < nz; ++z)
        if (!(temperature[z] <= p->clamp_low || temperature[z] >= p->clamp_high))
            packed[active++] = temperature[z];
    if (active == nz) {
        cs_evaluate(p, nz, temperature, scratch, rows, 1);
        return;
    }
    if (active)
        cs_evaluate(p, active, packed, work, values, 1);
    size_t nr = 0, z = 0;
    while (z < nz) {
        int kind = temperature[z] <= p->clamp_low ? 1 : temperature[z] >= p->clamp_high ? 2 : 0;
        size_t end = z + 1;
        while (end < nz) {
            int next = temperature[end] <= p->clamp_low    ? 1
                       : temperature[end] >= p->clamp_high ? 2
                                                           : 0;
            if (next != kind)
                break;
            ++end;
        }
        scratch[nr++] = (double)(3 * end + (size_t)kind);
        z = end;
    }
    for (size_t k = 0; k < p->count; ++k) {
        size_t pos = 0, j = 0;
        for (size_t r = 0; r < nr; ++r) {
            size_t code = (size_t)scratch[r], end = code / 3, kind = code % 3;
            if (!kind) {
                memcpy(rows + k * nz + pos, values + k * active + j, (end - pos) * sizeof(double));
                j += end - pos;
            } else {
                double value = p->clamped[2 * k + kind - 1];
                for (size_t i = pos; i < end; ++i)
                    rows[k * nz + i] = value;
            }
            pos = end;
        }
    }
}

/* ---- Quantum yields ---------------------------------------------------- */

enum { QY_STRIDE = 5 };

static int qy_prepare(tt_spectrum_plan *p, size_t w, double *c) {
    const tt_spectrum *s = p->s;
    const tt_spectral_table *t = s->tables;
    const tt_yield_options *o = &s->yield;
    double lambda = s->wavelength.midpoints[w];
    double base = p->tables ? parameter(t, w, 0) : 0;
    switch (p->kind) {
    case TT_QY_TINT:
    case TT_QY_NO2:
        for (size_t k = 0; k < p->tables; ++k)
            if (!flat(t + k, w))
                return 1;
        return 0;
    case TT_QY_O3_O1D:
    case TT_QY_O3_O3P:
        if (lambda <= 305 || lambda > 328)
            return 0;
        c[0] = exp(-sq(sq((304.225 - lambda) / 5.576)));
        c[1] = exp(-sq((314.957 - lambda) / 6.601));
        c[2] = exp(-sq((310.737 - lambda) / 2.187));
        return 1;
    case TT_QY_NO3_AQ:
    case TT_QY_CH2CHCHO:
    case TT_QY_MEK:
        return 1;
    case TT_QY_CH2O: {
        double q1 = base, q2 = parameter(t, w, 1), qt = 1 - q1;
        if (!(lambda >= 330 && lambda < 360 && q2 > 0))
            return 0;
        c[0] = (1 - (q1 + q2)) / (2.45e19 * q2 * qt);
        c[1] = .05 * (lambda - 329);
        c[2] = 1 / qt;
        return 1;
    }
    case TT_QY_CH3CHO: {
        double q1 = parameter(t, w, 1), q2 = 1 - base, q = q1 > 0 ? q2 / q1 - 1 : 0;
        c[0] = q1 * (1 + q);
        c[1] = q;
        /* With q zero the denominator is exactly one for any air density. */
        return q != 0;
    }
    case TT_QY_C2H5CHO:
        if (base < 10.0 / 1.e36)
            return 0;
        c[0] = 1 / base - 1;
        return c[0] != 0;
    case TT_QY_MVK:
        c[0] = exp(-.055 * (lambda - 308));
        return 1;
    case TT_QY_ACETONE:
        if (lambda < 279 || lambda > 327)
            return 0;
        c[0] = lambda - 248;
        c[1] = 1.e7 / lambda;
        c[2] = c[1] - 33113;
        c[3] = c[1] - 30488;
        c[4] = lambda < 302;
        return 1;
    case TT_QY_CH3COCHO: {
        double phi = clamp(1 - (lambda - 380) / 60, 0, 1);
        if (!(phi > 0 && lambda >= 380 && lambda <= 440))
            return 0;
        c[0] = phi;
        c[1] = 1.36e8 * exp(-8793 / lambda);
        return 1;
    }
    case TT_QY_H2SO4:
        if (!(o->collision_rates[w] >= 0))
            return 0;
        c[0] = o->collision_rates[w];
        return 1;
    default:
        return 0;
    }
}

static void qy_evaluate(const tt_spectrum_plan *p, size_t nz, const double *temp, const double *m,
                        double *z, double *rows, int reaper) {
    const tt_spectral_table *t = p->s->tables;
    const tt_yield_options *o = &p->s->yield;
    size_t nt = p->tables;
    switch (p->kind) {
    case TT_QY_TINT:
    case TT_QY_NO2:
        for (size_t k = 0; k < nt; ++k)
            for (size_t i = 0; i < nz; ++i) {
                const tt_spectral_table *q = t + k;
                double u = p->kind == TT_QY_TINT ? clamp(temp[i], q->temperatures[0],
                                                         q->temperatures[q->temperature_count - 1])
                                                 : temp[i];
                size_t j;
                bracket(q, u, &j, z + (2 * k + 1) * nz + i);
                z[2 * k * nz + i] = (double)j;
            }
        break;
    case TT_QY_O3_O1D:
    case TT_QY_O3_O3P:
        for (size_t i = 0; i < nz; ++i) {
            double kt = .695 * temp[i], q2 = exp(-825.518 / kt), qfac1 = 1 / (1 + q2),
                   qfac2 = q2 / (1 + q2), t300 = temp[i] / 300;
            z[i] = .8036 * qfac1;
            z[nz + i] = 8.9061 * t300 * t300 * qfac2;
            z[2 * nz + i] = .1192 * pow(t300, 1.5);
        }
        break;
    case TT_QY_NO3_AQ:
        for (size_t i = 0; i < nz; ++i)
            z[i] = exp(-2400 / temp[i] + 3.6);
        break;
    case TT_QY_CH2O:
        for (size_t i = 0; i < nz; ++i)
            z[i] = (300 - temp[i]) / 80;
        break;
    case TT_QY_CH3CHO:
        for (size_t i = 0; i < nz; ++i)
            z[i] = m[i] / 2.465e19;
        break;
    case TT_QY_C2H5CHO:
        for (size_t i = 0; i < nz; ++i)
            z[i] = m[i] / 2.45e19;
        break;
    case TT_QY_CH2CHCHO:
        for (size_t i = 0; i < nz; ++i) {
            if (m[i] > 2.6e19)
                z[i] = .004;
            else {
                double phi = m[i] <= 8.e17 ? .086 + 1.613e-17 * 8.e17 : .086 + 1.613e-17 * m[i];
                z[i] = .004 + 1 / phi;
            }
        }
        break;
    case TT_QY_MVK:
        for (size_t i = 0; i < nz; ++i)
            z[i] = 5.5 + 9.2e-19 * m[i];
        break;
    case TT_QY_ACETONE:
        for (size_t i = 0; i < nz; ++i) {
            double u = clamp(temp[i], o->minimum_temperature, o->maximum_temperature) / 295;
            double a0 = .350 * pow(u, -1.28);
            z[i] = a0;
            z[nz + i] = 1 - a0;
            z[2 * nz + i] = .068 * pow(u, -2.65);
            z[3 * nz + i] = 1.600e-19 * pow(u, -2.38);
            z[4 * nz + i] = .55e-3 * pow(u, -3.19);
            z[5 * nz + i] = 1.62e-17 * pow(u, -10.03);
            z[6 * nz + i] = 1.79e-3 * pow(u, -1.364);
            z[7 * nz + i] = 26.29 * pow(u, -6.59);
            z[8 * nz + i] = 5.72e-7 * pow(u, -2.93);
            z[9 * nz + i] = 30006 * pow(u, -.064);
            z[10 * nz + i] = 1.67e-15 * pow(u, -7.25);
            z[11 * nz + i] = 2.08e-3 * pow(u, -1.16);
        }
        break;
    case TT_QY_MEK:
        for (size_t i = 0; i < nz; ++i) {
            double ptorr = 760 * m[i] / 2.69e19;
            z[i] = fmin(1 / (.96 + 2.22e-3 * ptorr), 1);
        }
        break;
    case TT_QY_CH3COCHO:
        for (size_t i = 0; i < nz; ++i)
            z[i] = m[i] * 760 / 2.456e19;
        break;
    case TT_QY_H2SO4: {
        double pi = 3.1415926535898;
        for (size_t i = 0; i < nz; ++i)
            z[i] = sqrt(16.0 * 8.31446261815324 * temp[i] / (pi * o->molecular_weight)) *
                   (pi * sq(o->molecular_diameter) * m[i] * 1.e6);
        break;
    }
    default:
        break;
    }
    for (size_t k = 0; k < p->count; ++k) {
        size_t w = p->index[k];
        const double *c = p->coef + k * p->stride;
        double *out = rows + k * nz;
        switch (p->kind) {
        case TT_QY_TINT:
        case TT_QY_NO2:
            if (TT_REAPER_TABLES && reaper) {
                for (size_t i = 0; i < nz; ++i)
                    out[i] = 0;
                for (size_t q = 0; q < nt; ++q) {
                    if (t[q].temperature_count == 2) {
                        double base = parameter(t + q, w, 0);
                        double difference = parameter(t + q, w, 1) - base;
                        for (size_t i = 0; i < nz; ++i)
                            out[i] = out[i] + base + z[(2 * q + 1) * nz + i] * difference;
                    } else
                        for (size_t i = 0; i < nz; ++i) {
                            size_t j = (size_t)z[2 * q * nz + i];
                            double base = parameter(t + q, w, j);
                            out[i] = out[i] + base +
                                     z[(2 * q + 1) * nz + i] * (parameter(t + q, w, j + 1) - base);
                        }
                }
                if (p->kind == TT_QY_NO2)
                    for (size_t i = 0; i < nz; ++i)
                        out[i] = fmax(out[i], 0);
                break;
            }
            for (size_t i = 0; i < nz; ++i) {
                double value = 0;
                for (size_t q = 0; q < nt; ++q) {
                    size_t j = (size_t)z[2 * q * nz + i];
                    double f = z[(2 * q + 1) * nz + i];
                    value = value + parameter(t + q, w, j) +
                            f * (parameter(t + q, w, j + 1) - parameter(t + q, w, j));
                }
                out[i] = p->kind == TT_QY_NO2 ? fmax(value, 0) : value;
            }
            break;
        case TT_QY_O3_O1D:
        case TT_QY_O3_O3P:
            for (size_t i = 0; i < nz; ++i) {
                double ozone = .0765 + z[i] * c[0] + z[nz + i] * c[1] + z[2 * nz + i] * c[2];
                out[i] = p->kind == TT_QY_O3_O1D ? ozone : 1 - ozone;
            }
            break;
        case TT_QY_NO3_AQ:
        case TT_QY_CH2CHCHO:
        case TT_QY_MEK:
            memcpy(out, z, nz * sizeof(double));
            break;
        case TT_QY_CH2O:
            for (size_t i = 0; i < nz; ++i) {
                double q = c[0] * (1 + c[1] * z[i]);
                out[i] = 1 / (c[2] + q * m[i]);
            }
            break;
        case TT_QY_CH3CHO:
            for (size_t i = 0; i < nz; ++i)
                out[i] = clamp(c[0] / (1 + c[1] * z[i]), 0, 1);
            break;
        case TT_QY_C2H5CHO:
            for (size_t i = 0; i < nz; ++i)
                out[i] = fmin(1, 1 / (1 + c[0] * z[i]));
            break;
        case TT_QY_MVK:
            for (size_t i = 0; i < nz; ++i)
                out[i] = fmin(c[0] / z[i], 1);
            break;
        case TT_QY_ACETONE:
            for (size_t i = 0; i < nz; ++i) {
                double arg = z[2 * nz + i] * c[0];
                double ca0 = arg > 80 ? 5.e34 : exp(arg) * z[i] / z[nz + i], fco = 1 / (1 + ca0),
                       fac;
                if (c[4] != 0) {
                    double ca1 = z[3 * nz + i] * exp(-z[4 * nz + i] * c[2]);
                    fac = (1 - fco) / (1 + ca1 * m[i]);
                } else {
                    double ca2 = z[5 * nz + i] * exp(-z[6 * nz + i] * c[3]);
                    double ca3 = z[7 * nz + i] * exp(-z[8 * nz + i] * sq(c[1] - z[9 * nz + i]));
                    double ca4 = z[10 * nz + i] * exp(-z[11 * nz + i] * c[3]);
                    fac = (1 - fco) * (1 + ca3 + ca4 * m[i]) /
                          ((1 + ca3 + ca2 * m[i]) * (1 + ca4 * m[i]));
                }
                double qy = 0;
                if (o->acetone_branches & 1)
                    qy = qy + fco;
                if (o->acetone_branches & 2)
                    qy = qy + fac;
                out[i] = qy;
            }
            break;
        case TT_QY_CH3COCHO:
            for (size_t i = 0; i < nz; ++i)
                out[i] = c[0] / (c[0] + c[1] * z[i]);
            break;
        case TT_QY_H2SO4:
            for (size_t i = 0; i < nz; ++i)
                out[i] = i + 1 == nz ? 1 : c[0] / (c[0] + z[i]);
            break;
        default:
            break;
        }
    }
}

/* ---- Temperature-parameterized cross sections -------------------------- */

/* The parameterization as in temperature.c, on the padded raw values y. */
static tt_status apply_impl(const tt_spectrum_plan *p, double temperature, double *y,
                            double *math) {
    const tt_temperature_model *m = &p->s->temperature;
    size_t nr = m->range_count ? m->range_count : 1;
    for (size_t r = 0; r < nr; ++r) {
        double t = temperature;
        if (m->range_count) {
            const tt_temperature_range *range = m->ranges + r;
            if (t < range->minimum || t > range->maximum)
                continue;
            if (range->is_fixed)
                t = range->fixed_value;
        }
        t = t - m->base_temperature;
        if (m->kind == TT_TEMPERATURE_EXPONENTIAL && m->invert_temperature)
            t = -t;
        if ((m->kind == TT_TEMPERATURE_HARWOOD || m->kind == TT_TEMPERATURE_BURKHOLDER) && t == 0)
            return TT_RANGE;
        double *cs = y + 2 + m->first;
        switch (m->kind) {
        case TT_TEMPERATURE_EXPONENTIAL: {
            double factor[64];
            for (size_t j = 0; j < m->terms; ++j)
                factor[j] = m->a[j] + t * m->b[j];
            for (size_t i = 0; i < m->count; ++i) {
                double value = 0;
                for (size_t j = 0; j < m->terms; ++j)
                    value = value + factor[j] * p->powers[j * m->count + i];
                if (TT_REAPER_SPECTRUM_EXP && math)
                    math[i] = value;
                else
                    cs[i] = cs[i] + (m->base_10 ? pow(10, value) : exp(value));
            }
            if (TT_REAPER_SPECTRUM_EXP && math) {
                int positive = 1;
                for (size_t i = 0; i < m->count; ++i)
                    positive &= cs[i] >= 0;
                if (!positive) {
                    for (size_t i = 0; i < m->count; ++i)
                        math[i] = m->base_10 ? pow(10, math[i]) : exp(math[i]);
                } else if (m->base_10)
                    tt_reaper_pow10_array(m->count, math, math);
                else
                    tt_reaper_exp_array(m->count, math, math);
                for (size_t i = 0; i < m->count; ++i)
                    cs[i] = cs[i] + math[i];
            }
            break;
        }
        case TT_TEMPERATURE_TAYLOR: {
            double tp[64];
            for (size_t j = 0; j < m->terms; ++j)
                tp[j] = integer_power(t, j + 1);
            for (size_t i = 0; i < m->count; ++i) {
                double value = 1;
                for (size_t j = 0; j < m->terms; ++j)
                    value = value + m->a[j * m->count + i] * tp[j];
                cs[i] = value * m->sigma[i];
            }
            break;
        }
        case TT_TEMPERATURE_BURKHOLDER: {
            if (m->burkholder_b == 0)
                return TT_INVALID;
            double q = 1 + exp(m->burkholder_a / (m->burkholder_b * t));
            for (size_t i = 0; i < m->count; ++i)
                cs[i] = (m->a[i] / q + m->b[i] * (1 - 1 / q)) * 1.e-20;
            break;
        }
        case TT_TEMPERATURE_HARWOOD:
            if (TT_REAPER_SPECTRUM_EXP && math) {
                for (size_t i = 0; i < m->count; ++i)
                    cs[i] = m->a[i] + m->b[i] / t;
                tt_reaper_pow10_array(m->count, cs, cs);
            } else
                for (size_t i = 0; i < m->count; ++i)
                    cs[i] = pow(10, m->a[i] + m->b[i] / t);
            break;
        }
    }
    return TT_OK;
}

static tt_status apply(const tt_spectrum_plan *p, double temperature, double *y) {
    return apply_impl(p, temperature, y, NULL);
}

/* Conserving interpolation of target bins [first, end), as in numerics.c. */
static void conserve(size_t nx, const double *x, const double *y, const double *target,
                     size_t first, size_t end, double *out, size_t stride) {
    size_t j = 0;
    for (size_t i = first; i < end; ++i) {
        double sum = 0.0;
        while (j + 1 < nx && x[j + 1] <= target[i])
            ++j;
        for (size_t k = j; k + 1 < nx && x[k] < target[i + 1]; ++k) {
            double a1 = fmax(x[k], target[i]), a2 = fmin(x[k + 1], target[i + 1]);
            double slope = (y[k + 1] - y[k]) / (x[k + 1] - x[k]);
            double b1 = y[k] + slope * (a1 - x[k]), b2 = y[k] + slope * (a2 - x[k]);
            sum = sum + 0.5 * (a2 - a1) * (b2 + b1);
        }
        out[(i - first) * stride] = sum / (target[i + 1] - target[i]);
    }
}

/* Store ordered geometry and positive integral weights. Exact prepared
 * execution still replays the divisions and endpoint interpolations. REAPER
 * uses weights only with bounded nonnegative spectra; risky partial bins
 * receive an additional conditioning check before their result is retained. */
static tt_status overlap_prepare(tt_spectrum_plan *p) {
    const double *target = p->s->wavelength.edges;
    size_t bins = p->end_bin - p->first_bin, count = 0;
    p->overlap_offsets = calloc(bins + 1, sizeof(size_t));
    if (!p->overlap_offsets)
        return TT_MEMORY;
    for (size_t i = p->first_bin; i < p->end_bin; ++i) {
        p->overlap_offsets[i - p->first_bin] = count;
        for (size_t k = 0; k + 1 < p->raw; ++k)
            if (p->x[k + 1] > target[i] && p->x[k] < target[i + 1])
                ++count;
    }
    p->overlap_offsets[bins] = count;
    if (count > SIZE_MAX / sizeof(overlap_segment))
        return TT_MEMORY;
    p->overlaps = malloc((count ? count : 1) * sizeof(overlap_segment));
    if (!p->overlaps)
        return TT_MEMORY;
    count = 0;
    for (size_t i = p->first_bin; i < p->end_bin; ++i)
        for (size_t k = 0; k + 1 < p->raw; ++k)
            if (p->x[k + 1] > target[i] && p->x[k] < target[i + 1]) {
                double a1 = fmax(p->x[k], target[i]), a2 = fmin(p->x[k + 1], target[i + 1]);
                overlap_segment *segment = p->overlaps + count++;
                *segment = (overlap_segment){
                    k, p->x[k + 1] - p->x[k], a1 - p->x[k], a2 - p->x[k], 0.5 * (a2 - a1), -1, -1};
                double fraction = (segment->left + segment->right) / segment->dx;
                if (segment->dx >= 1.e-20 && segment->dx <= 1.e20 &&
                    segment->half_width >= 1.e-20 && segment->half_width <= 1.e20 &&
                    fraction >= 0 && fraction <= 2) {
                    segment->weight_b = segment->half_width * fraction;
                    segment->weight_a = 2 * segment->half_width - segment->weight_b;
                }
            }
    p->interpolation_mode = calloc(bins + 1, sizeof *p->interpolation_mode);
    if (p->interpolation_mode) {
        for (size_t bin = 0; bin < bins; ++bin) {
            p->interpolation_mode[bin] = 1;
            int sensitive = 0;
            for (size_t j = p->overlap_offsets[bin]; j < p->overlap_offsets[bin + 1]; ++j) {
                const overlap_segment *segment = p->overlaps + j;
                if (segment->weight_a < 0)
                    p->interpolation_mode[bin] = 0;
                sensitive |= segment->weight_a < .5 * segment->half_width ||
                             segment->weight_b < .5 * segment->half_width;
            }
            if (sensitive)
                p->interpolation_mode[bin] |= 2;
        }
    } else
        p->interpolation_bounded = 0;
    return TT_OK;
}

static void conserve_prepared(const tt_spectrum_plan *p, const double *y, double *out,
                              size_t stride) {
    const double *target = p->s->wavelength.edges;
    for (size_t i = p->first_bin; i < p->end_bin; ++i) {
        size_t bin = i - p->first_bin;
        double sum = 0;
        for (size_t j = p->overlap_offsets[bin]; j < p->overlap_offsets[bin + 1]; ++j) {
            const overlap_segment *s = p->overlaps + j;
            double slope = (y[s->point + 1] - y[s->point]) / s->dx;
            double b1 = y[s->point] + slope * s->left, b2 = y[s->point] + slope * s->right;
            sum = sum + s->half_width * (b2 + b1);
        }
        out[bin * stride] = sum / (target[i + 1] - target[i]);
    }
}

static tt_status parameterized_prepare(tt_spectrum_plan *p) {
    const tt_spectrum *s = p->s;
    const tt_temperature_model *m = &s->temperature;
    size_t n = s->raw_count, np = n + 4, nw = p->nw;
    if (m->terms > 64)
        return TT_INVALID;
    p->raw = np;
    p->x = malloc(2 * np * sizeof(double));
    if (!p->x)
        return TT_MEMORY;
    p->y = p->x + np;
    double delta = (double)1.e-5f;
    p->x[0] = 0;
    p->x[1] = (1 - delta) * s->raw_wavelength[0];
    memcpy(p->x + 2, s->raw_wavelength, n * sizeof(double));
    p->x[n + 2] = (1 + delta) * s->raw_wavelength[n - 1];
    p->x[n + 3] = 1.e38;
    p->y[0] = p->y[1] = p->y[n + 2] = p->y[n + 3] = 0;
    memcpy(p->y + 2, s->raw_data, n * sizeof(double));
    /* Bounded exp/pow10 outputs remain normal through the guarded geometry.
     * Fixed raw data must satisfy the same nonnegative-domain requirements. */
    p->interpolation_bounded = m->range_count <= 4096 && (m->kind == TT_TEMPERATURE_EXPONENTIAL ||
                                                          m->kind == TT_TEMPERATURE_HARWOOD);
    for (size_t i = 0; i < np; ++i) {
        double value = p->y[i];
        p->interpolation_bounded &=
            (value >= 0) & (value <= 1.e100) & ((value == 0) | (value >= 1.e-260));
    }
    if (m->kind == TT_TEMPERATURE_EXPONENTIAL) {
        p->powers = malloc(m->terms * m->count * sizeof(double));
        if (!p->powers)
            return TT_MEMORY;
        for (size_t j = 0; j < m->terms; ++j)
            for (size_t i = 0; i < m->count; ++i)
                p->powers[j * m->count + i] =
                    pow(s->raw_wavelength[m->first + i] - m->base_wavelength, m->powers[j]);
    }
    /* A target bin varies when a segment touching a parameterized point overlaps it. */
    size_t lo = m->first + 2, hi = lo + m->count;
    const double *edges = s->wavelength.edges;
    p->first_bin = nw;
    p->end_bin = 0;
    for (size_t i = 0; i < nw; ++i)
        for (size_t k = lo ? lo - 1 : 0; k < hi && k + 1 < np; ++k)
            if (p->x[k + 1] > edges[i] && p->x[k] < edges[i + 1]) {
                if (i < p->first_bin)
                    p->first_bin = i;
                p->end_bin = i + 1;
            }
    if (p->first_bin > p->end_bin)
        p->first_bin = p->end_bin = 0;
    return overlap_prepare(p);
}

static tt_status parameterized_evaluate(const tt_spectrum_plan *p, size_t nz, const double *temp,
                                        double *y, double *rows) {
    for (size_t i = 0; i < nz; ++i) {
        memcpy(y, p->y, p->raw * sizeof(double));
        tt_status status = apply(p, temp[i], y);
        if (status)
            return status;
        conserve(p->raw, p->x, y, p->s->wavelength.edges, p->first_bin, p->end_bin, rows + i, nz);
    }
    return TT_OK;
}

/* Signatures include every inclusive range, including overlaps and gaps. */
static int fixed_signature(const tt_spectrum_plan *p, double temperature, uint64_t *mask) {
    const tt_temperature_model *m = &p->s->temperature;
    if (!m->range_count || m->range_count > 64 || !isfinite(temperature))
        return 0;
    uint64_t bits = 0;
    for (size_t r = 0; r < m->range_count; ++r) {
        const tt_temperature_range *range = m->ranges + r;
        if (temperature < range->minimum || temperature > range->maximum)
            continue;
        if (!range->is_fixed)
            return 0;
        bits |= UINT64_C(1) << r;
    }
    *mask = bits;
    return 1;
}
static void cache_candidate(tt_spectrum_plan *p, double temperature, double *scratch) {
    uint64_t mask;
    if (!fixed_signature(p, temperature, &mask))
        return;
    for (size_t i = 0; i < p->fixed_count; ++i)
        if (p->fixed_masks[i] == mask)
            return;
    memcpy(scratch, p->y, p->raw * sizeof(double));
    if (apply(p, temperature, scratch))
        return;
    conserve_prepared(p, scratch, p->fixed_rows + p->fixed_count * p->count, 1);
    p->fixed_masks[p->fixed_count++] = mask;
}
static void fixed_prepare(tt_spectrum_plan *p) {
    const tt_temperature_model *m = &p->s->temperature;
    if (!TT_REAPER_FIXED_CACHE || !p->count || p->generic || !p->s->parameterized ||
        !m->range_count || m->range_count > 64)
        return;
    size_t capacity = 6 * m->range_count + 1;
    if (p->count > SIZE_MAX / capacity / sizeof(double))
        return;
    p->fixed_rows = malloc(capacity * p->count * sizeof(double));
    p->fixed_masks = malloc(capacity * sizeof(uint64_t));
    double *scratch = malloc(p->raw * sizeof(double));
    if (!(p->fixed_rows && p->fixed_masks && scratch)) {
        free(p->fixed_rows);
        p->fixed_rows = NULL;
        free(p->fixed_masks);
        p->fixed_masks = NULL;
        free(scratch);
        return; /* Optional optimization; ordinary execution stays available. */
    }
    cache_candidate(p, 0, scratch);
    for (size_t r = 0; r < m->range_count; ++r) {
        double boundary[] = {m->ranges[r].minimum, m->ranges[r].maximum};
        for (size_t k = 0; k < 2; ++k) {
            cache_candidate(p, boundary[k], scratch);
            cache_candidate(p, nextafter(boundary[k], -INFINITY), scratch);
            cache_candidate(p, nextafter(boundary[k], INFINITY), scratch);
        }
    }
    free(scratch);
}
static const double *fixed_rows(const tt_spectrum_plan *p, double temperature) {
    uint64_t mask;
    if (!p->fixed_count || !fixed_signature(p, temperature, &mask))
        return NULL;
    for (size_t i = 0; i < p->fixed_count; ++i)
        if (mask == p->fixed_masks[i])
            return p->fixed_rows + i * p->count;
    return NULL;
}

/* The same range and term order, with independent levels as contiguous lanes.
 * Padding repeats a real lane; it cannot introduce a new status or branch. */
static tt_status apply_lanes(const tt_spectrum_plan *p, const double *temperature, double *y,
                             double *math, int *bounded) {
    enum { TILE = TT_REAPER_PARAMETER_TILE };
    const tt_temperature_model *m = &p->s->temperature;
    size_t nr = m->range_count ? m->range_count : 1;
    for (size_t r = 0; r < nr; ++r) {
        double t[TILE], factor[64 * TILE];
        int active[TILE], any = 0;
        for (size_t z = 0; z < TILE; ++z) {
            double u = temperature[z];
            active[z] = 1;
            if (m->range_count) {
                const tt_temperature_range *range = m->ranges + r;
                active[z] = !(u < range->minimum || u > range->maximum);
                if (range->is_fixed)
                    u = range->fixed_value;
            }
            u = u - m->base_temperature;
            if (m->kind == TT_TEMPERATURE_EXPONENTIAL && m->invert_temperature)
                u = -u;
            if (active[z] &&
                (m->kind == TT_TEMPERATURE_HARWOOD || m->kind == TT_TEMPERATURE_BURKHOLDER) &&
                u == 0)
                return TT_RANGE;
            t[z] = active[z] ? u : 1;
            any |= active[z];
        }
        if (!any)
            continue;
        double *cs = y + (2 + m->first) * TILE;
        switch (m->kind) {
        case TT_TEMPERATURE_EXPONENTIAL:
            for (size_t j = 0; j < m->terms; ++j)
                for (size_t z = 0; z < TILE; ++z)
                    factor[j * TILE + z] = m->a[j] + t[z] * m->b[j];
            for (size_t i = 0; i < m->count; ++i) {
                double value[TILE] = {0};
                for (size_t j = 0; j < m->terms; ++j)
                    for (size_t z = 0; z < TILE; ++z)
                        value[z] = value[z] + factor[j * TILE + z] * p->powers[j * m->count + i];
                for (size_t z = 0; z < TILE; ++z)
                    math[i * TILE + z] = active[z] ? value[z] : 0;
            }
            if (TT_REAPER_SPECTRUM_EXP) {
                int positive = 1;
                for (size_t i = 0; i < m->count * TILE; ++i)
                    positive &= cs[i] >= 0;
                if (positive) {
                    if (m->base_10)
                        *bounded &= tt_reaper_pow10_array(m->count * TILE, math, math);
                    else
                        *bounded &= tt_reaper_exp_array(m->count * TILE, math, math);
                } else {
                    *bounded = 0;
                    for (size_t i = 0; i < m->count * TILE; ++i)
                        math[i] = m->base_10 ? pow(10, math[i]) : exp(math[i]);
                }
            } else {
                *bounded = 0;
                for (size_t i = 0; i < m->count * TILE; ++i)
                    math[i] = m->base_10 ? pow(10, math[i]) : exp(math[i]);
            }
            for (size_t i = 0; i < m->count; ++i)
                for (size_t z = 0; z < TILE; ++z)
                    if (active[z])
                        cs[i * TILE + z] = cs[i * TILE + z] + math[i * TILE + z];
            break;
        case TT_TEMPERATURE_TAYLOR:
            for (size_t j = 0; j < m->terms; ++j)
                for (size_t z = 0; z < TILE; ++z)
                    factor[j * TILE + z] = integer_power(t[z], j + 1);
            for (size_t i = 0; i < m->count; ++i) {
                double value[TILE];
                for (size_t z = 0; z < TILE; ++z)
                    value[z] = 1;
                for (size_t j = 0; j < m->terms; ++j)
                    for (size_t z = 0; z < TILE; ++z)
                        value[z] = value[z] + m->a[j * m->count + i] * factor[j * TILE + z];
                for (size_t z = 0; z < TILE; ++z)
                    if (active[z])
                        cs[i * TILE + z] = value[z] * m->sigma[i];
            }
            break;
        case TT_TEMPERATURE_BURKHOLDER:
            if (m->burkholder_b == 0)
                return TT_INVALID;
            for (size_t z = 0; z < TILE; ++z)
                factor[z] = 1 + exp(m->burkholder_a / (m->burkholder_b * t[z]));
            for (size_t i = 0; i < m->count; ++i)
                for (size_t z = 0; z < TILE; ++z)
                    if (active[z])
                        cs[i * TILE + z] =
                            (m->a[i] / factor[z] + m->b[i] * (1 - 1 / factor[z])) * 1.e-20;
            break;
        case TT_TEMPERATURE_HARWOOD:
            for (size_t i = 0; i < m->count; ++i)
                for (size_t z = 0; z < TILE; ++z)
                    math[i * TILE + z] = active[z] ? m->a[i] + m->b[i] / t[z] : 0;
            if (TT_REAPER_SPECTRUM_EXP)
                *bounded &= tt_reaper_pow10_array(m->count * TILE, math, math);
            else {
                *bounded = 0;
                for (size_t i = 0; i < m->count * TILE; ++i)
                    math[i] = pow(10, math[i]);
            }
            for (size_t i = 0; i < m->count; ++i)
                for (size_t z = 0; z < TILE; ++z)
                    if (active[z])
                        cs[i * TILE + z] = math[i * TILE + z];
            break;
        }
    }
    return TT_OK;
}

/* ---- Construction and evaluation --------------------------------------- */

void tt_spectrum_plan_destroy(tt_spectrum_plan *p) {
    if (!p)
        return;
    free(p->constant);
    free(p->coef);
    free(p->index);
    free(p->x);
    free(p->powers);
    free(p->overlap_offsets);
    free(p->overlaps);
    free(p->interpolation_mode);
    free(p->fixed_masks);
    free(p->fixed_rows);
    free(p->clamped);
    free(p);
}

/* Reference columns for the constant values and the classification guard:
 * temperatures and air densities spanning every branch of the kernels. */
enum { REFERENCE_LEVELS = 12 };
static const double reference_temperature[3][REFERENCE_LEVELS] = {
    {250, 150, 199.5, 205, 212.25, 234.5, 236, 272.73, 296.5, 299.75, 340, 1500},
    {251.37, 120, 180, 210, 233, 260, 280.25, 295, 298, 330, 450, 9000},
    {222.2, 100, 194, 203, 218, 235, 255.5, 290, 300, 305, 800, 3000}};
static const double reference_air[3][REFERENCE_LEVELS] = {
    {2.45e19, 0, 1.e10, 1.e15, 7.e17, 8.e17, 9.e17, 1.e18, 1.e19, 2.6e19, 2.7e19, 1.e20},
    {2.5e19, 3.e19, 1.e17, 2.e18, 5.e18, 2.465e19, 2.69e19, 4.e16, 6.e19, 0, 1.e21, 3.3e18},
    {1.e19, 5.e18, 2.45e19, 1.e14, 2.6e19, 8.e17, 1.e16, 3.e17, 1.e18, 4.e19, 0, 7.e19}};

tt_status tt_spectrum_plan_create(const tt_spectrum *s, tt_spectrum_plan **out) {
    if (!s || !out || s->role == TT_SPECTRUM_WEIGHT)
        return TT_INVALID;
    tt_spectrum_plan *p = calloc(1, sizeof *p);
    if (!p)
        return TT_MEMORY;
    p->s = s;
    p->cs = s->role == TT_SPECTRUM_CROSS_SECTION && !s->parameterized;
    p->qy = s->role == TT_SPECTRUM_QUANTUM_YIELD;
    p->kind = s->parameterized ? -1 : s->kind;
    p->nw = s->wavelength.cells;
    p->tables = s->table_count;
    p->maximum_temperature = TEMPERATURE_LIMIT;
    size_t nw = p->nw, nz = REFERENCE_LEVELS, usable = 0;
    tt_status status = TT_OK;
    if (p->cs)
        status = tt_cross_section_validate(p->kind, nw, p->tables, s->tables, &p->required);
    p->stride = p->cs && p->kind == TT_CS_HCFC ? s->tables[0].rows : p->cs ? CS_STRIDE : QY_STRIDE;
    if (!p->stride)
        p->stride = 1;
    double *reference = malloc(3 * nw * nz * sizeof(double));
    int valid[3] = {0};
    p->constant = malloc(nw * sizeof(double));
    p->coef = calloc(nw * p->stride, sizeof(double));
    p->index = malloc(nw * sizeof(size_t));
    if (!status && !(reference && p->constant && p->coef && p->index))
        status = TT_MEMORY;
    /* Generic values on the reference columns. A parameterization rejects a
     * temperature equal to its base; such a column is simply not used. */
    for (size_t r = 0; r < 3 && !status; ++r) {
        tt_status column = tt_spectrum_calculate(s, nz, reference_temperature[r], reference_air[r],
                                                 reference + r * nw * nz);
        if (column == TT_OK)
            valid[r] = 1, ++usable;
        else if (column != TT_RANGE)
            status = column;
    }
    if (!status && !usable)
        status = TT_RANGE;
    if (!status && s->parameterized)
        status = parameterized_prepare(p);
    size_t first = valid[0] ? 0 : valid[1] ? 1 : 2;
    for (size_t w = 0; w < nw && !status && !p->generic; ++w) {
        double *c = p->coef + p->count * p->stride;
        int varies = s->parameterized ? w >= p->first_bin && w < p->end_bin
                     : p->cs          ? cs_prepare(p, w, c)
                                      : qy_prepare(p, w, c);
        p->constant[w] = reference[first * nw * nz + w * nz];
        /* Guard: a constant wavelength must be constant on every reference.
         * A failure would be a classification error; this spectrum then
         * falls back to the generic kernel at every wavelength. */
        for (size_t r = 0; r < 3 && !varies; ++r)
            for (size_t z = 0; z < nz && valid[r] && !p->generic; ++z)
                if (memcmp(reference + r * nw * nz + w * nz + z, p->constant + w, sizeof(double)))
                    p->generic = 1;
        if (varies)
            p->index[p->count++] = w;
    }
    if (!status && p->generic) {
        p->count = nw;
        for (size_t w = 0; w < nw; ++w)
            p->index[w] = w;
    }
    free(reference);
    if (status) {
        tt_spectrum_plan_destroy(p);
        return status;
    }
    direct_cache_prepare(p);
    fixed_prepare(p);
    *out = p;
    return TT_OK;
}

const double *tt_spectrum_plan_constant(const tt_spectrum_plan *p) {
    return p->constant;
}
size_t tt_spectrum_plan_varying(const tt_spectrum_plan *p, const size_t **indices) {
    if (indices)
        *indices = p->index;
    return p->count;
}
int tt_spectrum_plan_generic(const tt_spectrum_plan *p) {
    return p->generic;
}
double tt_spectrum_plan_maximum_temperature(const tt_spectrum_plan *p) {
    return p->maximum_temperature;
}
size_t tt_spectrum_plan_scratch(const tt_spectrum_plan *p, size_t nz) {
    if (p->clamped)
        return (1 + p->count) * nz + cs_scratch(p, nz);
    if (p->generic)
        return 0;
    if (p->kind < 0)
        return (2 + 2 * TT_REAPER_PARAMETER_TILE) * p->raw;
    if (p->cs && p->kind == TT_CS_HCFC)
        return p->s->tables[0].rows * nz;
    if (p->qy && p->kind == TT_QY_ACETONE)
        return 12 * nz;
    return 8 * nz + 2 * p->tables * nz;
}
tt_status tt_spectrum_plan_evaluate(const tt_spectrum_plan *p, size_t nz, const double *temperature,
                                    const double *air, double *scratch, double *rows) {
    if (!p->count)
        return TT_OK;
    if (p->generic) /* every wavelength varies: rows are the generic layout */
        return tt_spectrum_calculate(p->s, nz, temperature, air, rows);
    if (p->kind < 0)
        return parameterized_evaluate(p, nz, temperature, scratch, rows);
    if (p->cs)
        cs_evaluate(p, nz, temperature, scratch, rows, 0);
    else
        qy_evaluate(p, nz, temperature, air, scratch, rows, 0);
    return TT_OK;
}

tt_status tt_spectrum_reaper_evaluate(const tt_spectrum_plan *p, size_t nz,
                                      const double *temperature, const double *air, double *scratch,
                                      double *rows) {
    if (p->clamped) {
        direct_cached(p, nz, temperature, scratch, rows);
        return TT_OK;
    }
    if ((TT_REAPER_SPECTRUM_EXP || TT_REAPER_TABLES) && p->cs && !p->generic && p->count) {
        cs_evaluate(p, nz, temperature, scratch, rows, 1);
        return TT_OK;
    }
    if (TT_REAPER_TABLES && p->qy && !p->generic && p->count) {
        qy_evaluate(p, nz, temperature, air, scratch, rows, 1);
        return TT_OK;
    }
    if (!TT_REAPER_OVERLAP || p->kind >= 0 || p->generic || !p->count)
        return tt_spectrum_plan_evaluate(p, nz, temperature, air, scratch, rows);
    if (TT_REAPER_PARAMETER_TILE > 1) {
        enum { TILE = TT_REAPER_PARAMETER_TILE };
        double *raw = scratch, *math = raw + p->raw, *lanes = math + p->raw;
        for (size_t first = 0; first < nz; first += TILE) {
            size_t levels[TILE], active = 0;
            double temperatures[TILE];
            size_t count = nz - first < TILE ? nz - first : TILE;
            for (size_t z = first; z < first + count; ++z) {
                const double *cached = fixed_rows(p, temperature[z]);
                if (cached) {
                    for (size_t k = 0; k < p->count; ++k)
                        rows[k * nz + z] = cached[k];
                    continue;
                }
                if (!TT_REAPER_PARAMETER_BATCH) {
                    memcpy(raw, p->y, p->raw * sizeof(double));
                    tt_status status = apply_impl(p, temperature[z], raw, math);
                    if (status)
                        return status;
                }
                for (size_t k = 0; k < p->raw; ++k)
                    lanes[k * TILE + active] = TT_REAPER_PARAMETER_BATCH ? p->y[k] : raw[k];
                temperatures[active] = temperature[z];
                levels[active++] = z;
            }
            if (!active)
                continue;
            for (size_t k = 0; k < p->raw; ++k)
                for (size_t z = active; z < TILE; ++z)
                    lanes[k * TILE + z] = lanes[k * TILE + active - 1];
            int positive = TT_REAPER_PARAMETER_BATCH && p->interpolation_bounded &&
                           tt_reaper_binary64() && fegetround() == FE_TONEAREST;
            if (TT_REAPER_PARAMETER_BATCH) {
                for (size_t z = active; z < TILE; ++z)
                    temperatures[z] = temperatures[active - 1];
                tt_status status =
                    apply_lanes(p, temperatures, lanes, lanes + TILE * p->raw, &positive);
                if (status)
                    return status;
            }
            /* Endpoint cancellation can amplify even a small spectral error.
             * Accumulate the conditioning estimate with the weighted result,
             * then replay libm and ordered interpolation if it is sensitive. */
            int sensitive = !positive;
            int exponential = p->s->temperature.kind == TT_TEMPERATURE_EXPONENTIAL ||
                              p->s->temperature.kind == TT_TEMPERATURE_HARWOOD;
            const double *target = p->s->wavelength.edges;
            if (!sensitive || !exponential)
                for (size_t bin = 0; bin < p->count; ++bin) {
                    double sum[TILE] = {0};
                    unsigned kind = positive ? (unsigned)p->interpolation_mode[bin] : 0;
                    if (kind & 1) {
                        if (kind & 2) {
                            double absolute[TILE] = {0};
                            for (size_t j = p->overlap_offsets[bin];
                                 j < p->overlap_offsets[bin + 1]; ++j) {
                                const overlap_segment *segment = p->overlaps + j;
                                const double *a = lanes + segment->point * TILE, *b = a + TILE;
                                for (size_t z = 0; z < TILE; ++z) {
                                    sum[z] = sum[z] +
                                             (a[z] * segment->weight_a + b[z] * segment->weight_b);
                                    absolute[z] = absolute[z] + segment->half_width * (a[z] + b[z]);
                                }
                            }
                            /* absolute/sum bounds cancellation in the original
                             * two-endpoint interpolation, across the entire bin. */
                            for (size_t z = 0; z < active; ++z)
                                sensitive |= absolute[z] > 4 * sum[z];
                        } else {
                            for (size_t j = p->overlap_offsets[bin];
                                 j < p->overlap_offsets[bin + 1]; ++j) {
                                const overlap_segment *segment = p->overlaps + j;
                                const double *a = lanes + segment->point * TILE, *b = a + TILE;
                                for (size_t z = 0; z < TILE; ++z)
                                    sum[z] = sum[z] +
                                             (a[z] * segment->weight_a + b[z] * segment->weight_b);
                            }
                        }
                    } else if (positive) {
                        /* A bin may mix ordinary overlaps with zero-valued
                         * padding outside the guarded geometry. Keep checking
                         * the ordinary overlaps; a zero padding segment must
                         * not bypass their conditioning test. */
                        double absolute[TILE] = {0};
                        for (size_t j = p->overlap_offsets[bin]; j < p->overlap_offsets[bin + 1];
                             ++j) {
                            const overlap_segment *segment = p->overlaps + j;
                            const double *a = lanes + segment->point * TILE, *b = a + TILE;
                            if (segment->weight_a < 0) {
                                for (size_t z = 0; z < active; ++z)
                                    sensitive |= (a[z] != 0) | (b[z] != 0);
                            } else {
                                for (size_t z = 0; z < TILE; ++z) {
                                    sum[z] = sum[z] +
                                             (a[z] * segment->weight_a + b[z] * segment->weight_b);
                                    absolute[z] = absolute[z] + segment->half_width * (a[z] + b[z]);
                                }
                            }
                        }
                        for (size_t z = 0; z < active; ++z)
                            sensitive |= absolute[z] > 4 * sum[z];
                    } else
                        for (size_t j = p->overlap_offsets[bin]; j < p->overlap_offsets[bin + 1];
                             ++j) {
                            const overlap_segment *segment = p->overlaps + j;
                            const double *a = lanes + segment->point * TILE, *b = a + TILE;
                            for (size_t z = 0; z < TILE; ++z) {
                                double slope = (b[z] - a[z]) / segment->dx;
                                double b1 = a[z] + slope * segment->left;
                                double b2 = a[z] + slope * segment->right;
                                sum[z] = sum[z] + segment->half_width * (b2 + b1);
                            }
                        }
                    double width = target[p->first_bin + bin + 1] - target[p->first_bin + bin];
                    for (size_t z = 0; z < active; ++z)
                        rows[bin * nz + levels[z]] = sum[z] / width;
                }
            if (sensitive && exponential) {
                for (size_t z = 0; z < active; ++z) {
                    memcpy(raw, p->y, p->raw * sizeof(double));
                    tt_status status = apply_impl(p, temperatures[z], raw, NULL);
                    if (status)
                        return status;
                    conserve_prepared(p, raw, rows + levels[z], nz);
                }
            }
        }
        return TT_OK;
    }
    for (size_t z = 0; z < nz; ++z) {
        const double *cached = fixed_rows(p, temperature[z]);
        if (cached) {
            for (size_t k = 0; k < p->count; ++k)
                rows[k * nz + z] = cached[k];
            continue;
        }
        memcpy(scratch, p->y, p->raw * sizeof(double));
        /* Scalar experimental tiles retain libm and ordered interpolation. */
        tt_status status = apply_impl(p, temperature[z], scratch, NULL);
        if (status)
            return status;
        conserve_prepared(p, scratch, rows + z, nz);
    }
    return TT_OK;
}

static int same(const double *a, const double *b, size_t n) {
    return n == 0 || (a && b && !memcmp(a, b, n * sizeof(double)));
}
static int same_tables(const tt_spectrum *a, const tt_spectrum *b) {
    if (a->table_count != b->table_count)
        return 0;
    for (size_t i = 0; i < a->table_count; ++i) {
        const tt_spectral_table *x = a->tables + i, *y = b->tables + i;
        if (x->rows != y->rows || x->columns != y->columns ||
            x->temperature_count != y->temperature_count ||
            !same(x->values, y->values, x->rows * x->columns) ||
            !same(x->temperatures, y->temperatures, x->temperature_count))
            return 0;
    }
    return 1;
}
static int same_options(const tt_yield_options *a, const tt_yield_options *b, size_t nw) {
    return a->acetone_branches == b->acetone_branches &&
           same(&a->low_wavelength_value, &b->low_wavelength_value, 1) &&
           same(&a->high_wavelength_value, &b->high_wavelength_value, 1) &&
           same(&a->minimum_temperature, &b->minimum_temperature, 1) &&
           same(&a->maximum_temperature, &b->maximum_temperature, 1) &&
           same(&a->molecular_diameter, &b->molecular_diameter, 1) &&
           same(&a->molecular_weight, &b->molecular_weight, 1) &&
           !a->collision_rates == !b->collision_rates &&
           (!a->collision_rates || same(a->collision_rates, b->collision_rates, nw));
}
static int same_ranges(const tt_temperature_model *x, const tt_temperature_model *y) {
    for (size_t r = 0; r < x->range_count; ++r) {
        const tt_temperature_range *a = x->ranges + r, *b = y->ranges + r;
        if (a->is_fixed != b->is_fixed || !same(&a->minimum, &b->minimum, 1) ||
            !same(&a->maximum, &b->maximum, 1) || !same(&a->fixed_value, &b->fixed_value, 1))
            return 0;
    }
    return 1;
}
int tt_spectrum_plan_equal(const tt_spectrum_plan *a, const tt_spectrum_plan *b) {
    if (a == b)
        return 1;
    if (a->cs != b->cs || a->qy != b->qy || a->kind != b->kind || a->generic != b->generic ||
        a->nw != b->nw || a->count != b->count || a->stride != b->stride ||
        !same(a->constant, b->constant, a->nw) || !same(a->coef, b->coef, a->count * a->stride) ||
        (a->count && memcmp(a->index, b->index, a->count * sizeof(size_t))) ||
        !same(a->s->wavelength.edges, b->s->wavelength.edges, a->nw + 1) ||
        !same(a->s->wavelength.midpoints, b->s->wavelength.midpoints, a->nw))
        return 0;
    if (a->kind < 0) {
        const tt_temperature_model *x = &a->s->temperature, *y = &b->s->temperature;
        size_t na = x->kind == TT_TEMPERATURE_TAYLOR        ? x->terms * x->count
                    : x->kind == TT_TEMPERATURE_EXPONENTIAL ? x->terms
                                                            : x->count;
        size_t nb = x->kind == TT_TEMPERATURE_EXPONENTIAL ? x->terms : na;
        return x->kind == y->kind && x->first == y->first && x->count == y->count &&
               x->terms == y->terms && x->base_10 == y->base_10 &&
               x->invert_temperature == y->invert_temperature && x->range_count == y->range_count &&
               a->raw == b->raw && same(&x->base_temperature, &y->base_temperature, 1) &&
               same(&x->base_wavelength, &y->base_wavelength, 1) &&
               same(&x->burkholder_a, &y->burkholder_a, 1) &&
               same(&x->burkholder_b, &y->burkholder_b, 1) && same(x->a, y->a, na) &&
               (x->kind == TT_TEMPERATURE_TAYLOR || same(x->b, y->b, nb)) &&
               (x->kind != TT_TEMPERATURE_TAYLOR || same(x->sigma, y->sigma, x->count)) &&
               same(a->x, b->x, 2 * a->raw) &&
               (!a->powers || same(a->powers, b->powers, x->terms * x->count)) && same_ranges(x, y);
    }
    return same_tables(a->s, b->s) && (!a->qy || same_options(&a->s->yield, &b->s->yield, a->nw));
}

int tt_spectrum_plan_category(const tt_spectrum_plan *p) {
    return p->s->parameterized ? 2 : p->qy ? 1 : 0;
}

/* Canonical immutable spectrum description. Keep this beside the authoritative
 * prepared representation; Python never reconstructs spectrum classifications. */
void tt_spectrum_plan_write(const tt_spectrum_plan *p, tt_reaper_writer *w) {
    const tt_spectrum *s = p->s;
    tt_rw_text(w, "{\"role\":");
    tt_rw_size(w, (size_t)s->role);
    tt_rw_text(w, ",\"kind\":");
    tt_rw_int(w, s->kind);
    tt_rw_text(w, ",\"parameterized\":");
    tt_rw_size(w, (size_t)s->parameterized);
    tt_rw_text(w, ",\"generic\":");
    tt_rw_size(w, (size_t)p->generic);
    tt_rw_text(w, ",\"constant\":");
    tt_rw_array(w, p->constant, p->nw);
    tt_rw_text(w, ",\"coefficients\":");
    tt_rw_array(w, p->coef, p->count * p->stride);
    tt_rw_text(w, ",\"varying\":[");
    for (size_t i = 0; i < p->count; ++i) {
        if (i)
            tt_rw_text(w, ",");
        tt_rw_size(w, p->index[i]);
    }
    tt_rw_text(w, "],\"tables\":[");
    for (size_t i = 0; i < s->table_count; ++i) {
        if (i)
            tt_rw_text(w, ",");
        const tt_spectral_table *t = s->tables + i;
        tt_rw_text(w, "[");
        tt_rw_size(w, t->rows);
        tt_rw_text(w, ",");
        tt_rw_size(w, t->columns);
        tt_rw_text(w, ",");
        tt_rw_array(w, t->values, t->rows * t->columns);
        tt_rw_text(w, ",");
        tt_rw_array(w, t->temperatures, t->temperature_count);
        tt_rw_text(w, "]");
    }
    tt_rw_text(w, "],\"overrides\":[");
    for (size_t i = 0; i < s->override_count; ++i) {
        if (i)
            tt_rw_text(w, ",");
        tt_rw_text(w, "[");
        tt_rw_size(w, s->overrides[i].first);
        tt_rw_text(w, ",");
        tt_rw_size(w, s->overrides[i].end);
        tt_rw_text(w, ",");
        tt_rw_double(w, s->overrides[i].value);
        tt_rw_text(w, "]");
    }
    tt_rw_text(w, "]");
    if (p->qy) {
        const tt_yield_options *q = &s->yield;
        double values[] = {q->low_wavelength_value, q->high_wavelength_value,
                           q->minimum_temperature,  q->maximum_temperature,
                           q->minimum_wavelength,   q->maximum_wavelength,
                           q->molecular_diameter,   q->molecular_weight};
        tt_rw_text(w, ",\"yield\":[");
        tt_rw_size(w, q->acetone_branches);
        tt_rw_text(w, ",");
        tt_rw_array(w, values, sizeof values / sizeof *values);
        tt_rw_text(w, ",");
        tt_rw_array(w, q->coefficients, q->coefficient_count);
        tt_rw_text(w, ",");
        tt_rw_array(w, q->collision_rates, p->nw);
        tt_rw_text(w, "]");
    }
    if (s->parameterized) {
        const tt_temperature_model *t = &s->temperature;
        size_t na = t->kind == TT_TEMPERATURE_EXPONENTIAL ? t->terms
                    : t->kind == TT_TEMPERATURE_TAYLOR    ? t->terms * t->count
                                                          : t->count;
        size_t nb = t->kind == TT_TEMPERATURE_EXPONENTIAL ? t->terms : t->count;
        double values[] = {t->base_temperature, t->base_wavelength, t->burkholder_a,
                           t->burkholder_b};
        tt_rw_text(w, ",\"temperature\":[");
        tt_rw_size(w, t->kind);
        tt_rw_text(w, ",");
        tt_rw_size(w, t->first);
        tt_rw_text(w, ",");
        tt_rw_size(w, t->count);
        tt_rw_text(w, ",");
        tt_rw_size(w, t->terms);
        tt_rw_text(w, ",");
        tt_rw_size(w, t->base_10);
        tt_rw_text(w, ",");
        tt_rw_size(w, t->invert_temperature);
        tt_rw_text(w, ",");
        tt_rw_array(w, values, 4);
        tt_rw_text(w, ",");
        tt_rw_array(w, t->a, na);
        tt_rw_text(w, ",");
        tt_rw_array(w, t->b, nb);
        tt_rw_text(w, ",");
        tt_rw_array(w, t->powers, t->terms);
        tt_rw_text(w, ",");
        tt_rw_array(w, t->sigma, t->count);
        tt_rw_text(w, ",");
        tt_rw_array(w, p->x, p->raw);
        tt_rw_text(w, ",");
        tt_rw_array(w, p->y, p->raw);
        tt_rw_text(w, ",[");
        for (size_t i = 0; i < t->range_count; ++i) {
            if (i)
                tt_rw_text(w, ",");
            const tt_temperature_range *r = t->ranges + i;
            tt_rw_text(w, "[");
            tt_rw_double(w, r->minimum);
            tt_rw_text(w, ",");
            tt_rw_double(w, r->maximum);
            tt_rw_text(w, ",");
            tt_rw_size(w, r->is_fixed);
            tt_rw_text(w, ",");
            tt_rw_double(w, r->fixed_value);
            tt_rw_text(w, "]");
        }
        tt_rw_text(w, "]]");
    }
    tt_rw_text(w, "}");
}
