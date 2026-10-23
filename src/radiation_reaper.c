/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 *
 * REAPER Delta-Eddington: contiguous mixing plus wavelength blocks. Each lane
 * performs exactly the arithmetic of tt_mix_radiators (one moment) and
 * tt_delta_eddington for its wavelength; the lanes of a block share the ray
 * geometry and loop structure, so the compiler can vectorize across them.
 * The solver also evaluates each exp(-slant) once per level, where the
 * generic code evaluates the same value up to four times. */
#include "model_plan.h"
#include "diagnostics_internal.h"
#include "reaper_internal.h"
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <string.h>

enum {
    BLOCK = TT_REAPER_BLOCK,
    ROWS = TT_REAPER_COMPACT ? 12 : 17,
    EXTRA = TT_REAPER_COMPACT ? 7 : 2
};
#define R_RESTRICT restrict

typedef struct {
    double *g, *om, *taun, *slant, *ex, *e1, *e2, *e3, *e4, *cup, *cdn, *cuptn, *cdntn, *cp, *u;
} block_work;

size_t tt_reaper_delta_eddington_work(size_t n, size_t nw) {
    if (n > (SIZE_MAX / BLOCK - EXTRA) / ROWS || (n && nw > SIZE_MAX / n / 3))
        return 0;
    size_t block = (ROWS * n + EXTRA) * BLOCK, mix = 3 * n * nw;
    return mix > SIZE_MAX - block ? 0 : block + mix;
}

static void partition(size_t n, double *work, block_work *s) {
#define TAKE(field, count)                                                                         \
    do {                                                                                           \
        s->field = work;                                                                           \
        work += (count) * BLOCK;                                                                   \
    } while (0)
    TAKE(g, n);
    TAKE(om, n);
    TAKE(taun, n);
    TAKE(slant, n + 1);
    TAKE(ex, n + 1);
    if (TT_REAPER_COMPACT) {
        s->e1 = s->g;
        s->e2 = s->om;
        s->e3 = s->taun;
    } else {
        TAKE(e1, n);
        TAKE(e2, n);
        TAKE(e3, n);
    }
    TAKE(e4, n);
    TAKE(cup, TT_REAPER_COMPACT ? 3 : n);
    TAKE(cdn, TT_REAPER_COMPACT ? 2 : n);
    TAKE(cuptn, n);
    TAKE(cdntn, n);
    TAKE(cp, 2 * n);
    TAKE(u, 2 * n);
#undef TAKE
}

/* Validate contiguous input arrays, then sum in the original radiator order.
 * Separating the guard removes early exits from the arithmetic loop. The
 * model owns disjoint workspace; caller-supplied radiator roles stay dynamic. */
static tt_status mix_layers(size_t nr, const tt_radiator *r, size_t count, double *restrict scatter,
                            double *restrict absorb, double *restrict moment,
                            const unsigned char *roles) {
    memset(scatter, 0, count * sizeof(double));
    memset(absorb, 0, count * sizeof(double));
    memset(moment, 0, count * sizeof(double));
    for (size_t j = 0; j < nr; ++j) {
        const double *restrict tau = r[j].optical_depth;
        const double *restrict ssa = r[j].single_scattering_albedo;
        const double *restrict g = r[j].asymmetry;
        if (!tau || !ssa || !g)
            return TT_INVALID;
        if (roles) {
            unsigned kind = roles[j] & 7;
            if (roles[j] & 8) {
                int valid = 1;
                for (size_t k = 0; k < count; ++k)
                    valid &= (tau[k] >= 0) & (tau[k] <= DBL_MAX);
                if (!valid)
                    return TT_INVALID;
            }
            if (kind == 1)
                continue;
            if (kind == 2) {
                for (size_t k = 0; k < count; ++k)
                    absorb[k] = absorb[k] + tau[k];
                continue;
            }
            if (kind == 3) {
                for (size_t k = 0; k < count; ++k)
                    scatter[k] = scatter[k] + tau[k];
                continue;
            }
        }
        int valid = 1;
        if (!roles)
            for (size_t k = 0; k < count; ++k) {
                valid &= (tau[k] >= 0) & (tau[k] <= DBL_MAX) & (ssa[k] >= 0) & (ssa[k] <= 1) &
                         (fabs(g[k]) <= 1);
            }
        if (!valid)
            return TT_INVALID;
        for (size_t k = 0; k < count; ++k) {
            double ds = tau[k] * ssa[k];
            scatter[k] = scatter[k] + ds;
            absorb[k] = absorb[k] + tau[k] * (1.0 - ssa[k]);
            moment[k] = moment[k] + g[k] * ds;
        }
    }
    return TT_OK;
}

/* Transpose the accumulated values and apply the original delta scaling.
 * These are raw sums, not a normalized mixture that would be mixed again. */
static tt_status scale_layers(size_t n, size_t w0, size_t count, size_t total,
                              const double *restrict sums, block_work *s) {
    double *restrict gp = s->g, *restrict omp = s->om, *restrict taup = s->taun;
    int valid = 1;
    for (size_t i = 0; i < n; ++i)
        for (size_t c = 0; c < BLOCK; ++c) {
            size_t k = (w0 + (c < count ? c : count - 1)) * n + n - 1 - i, q = i * BLOCK + c;
            double scatter = fmax(sums[k], 1.0 / 1.e36),
                   absorb = fmax(sums[total + k], 1.0 / 1.e36);
            double tau = scatter + absorb;
            double om = scatter == 1.0 / 1.e36 ? 1.0 / 1.e36 : scatter / tau;
            double g = sums[2 * total + k] / scatter;
            valid &= (tau >= 0) & (tau <= DBL_MAX) & (om >= 0) & (om <= 1) & (fabs(g) < 1);
            gp[q] = g;
            omp[q] = om;
            taup[q] = tau;
        }
    if (!valid)
        return TT_INVALID;
    for (size_t q = 0; q < n * BLOCK; ++q) {
        double gu = gp[q], omu = omp[q], tau = taup[q], f = gu * gu;
        gp[q] = (gu - f) / (1.0 - f);
        omp[q] = (1.0 - f) * omu / (1.0 - omu * f);
        taup[q] = (1.0 - omu * f) * tau;
    }
    return TT_OK;
}

/* Slant optical depth at each level, as slant_depth in radiation.c. */
static void slant_depths(size_t n, const int *nid, const double *path, double mu, block_work *s) {
    const double *R_RESTRICT b_taun = s->taun;
    double *R_RESTRICT b_slant = s->slant;
    double *R_RESTRICT b_ex = s->ex;

    for (size_t l = 0; l <= n; ++l) {
        double *slant = b_slant + l * BLOCK;
        int crossed = nid[l];
        if (l == 0 && mu >= 0) {
            for (size_t c = 0; c < BLOCK; ++c)
                slant[c] = 0.0;
            continue;
        }
        if (crossed < 0) {
            for (size_t c = 0; c < BLOCK; ++c)
                slant[c] = (double)1.e36f; /* reference literal has default real kind */
            continue;
        }
        const double *p = path + l * n;
        size_t single = (size_t)crossed < l ? (size_t)crossed : l;
        double acc[BLOCK];
        for (size_t c = 0; c < BLOCK; ++c)
            acc[c] = 0.0;
        for (size_t j = 0; j < single; ++j) {
            const double *t = b_taun + j * BLOCK;
            for (size_t c = 0; c < BLOCK; ++c)
                acc[c] = acc[c] + t[c] * p[j];
        }
        for (size_t j = single; j < (size_t)crossed; ++j) {
            const double *t = b_taun + j * BLOCK;
            for (size_t c = 0; c < BLOCK; ++c)
                acc[c] = acc[c] + 2.0 * t[c] * p[j];
        }
        memcpy(slant, acc, sizeof acc);
    }
    for (size_t l = 0; l <= n; ++l)
        for (size_t c = 0; c < BLOCK; ++c)
            b_ex[l * BLOCK + c] = exp(-b_slant[l * BLOCK + c]);
}

/* Eliminate a pair of system rows as soon as the next layer is ready.
 * Passing each coefficient base preserves the enclosing restrict association,
 * including when its storage previously held a scaling input. */
static inline tt_status forward_row(size_t row, const double *restrict b_e1,
                                    const double *restrict b_e2, const double *restrict b_e3,
                                    const double *restrict b_e4, const double *restrict b_cup,
                                    const double *restrict b_cdn, const double *restrict b_cuptn,
                                    const double *restrict b_cdntn, double *restrict cp,
                                    double *restrict u) {
    if (!row) {
        for (size_t c = 0; c < BLOCK; ++c) {
            double b = b_e1[c], d = -b_e2[c], r = -b_cdn[c];
            if (b == 0.0)
                return TT_SINGULAR;
            cp[c] = d / b;
            u[c] = r / b;
        }
        return TT_OK;
    }
    size_t i = row - 1;
    const double *e1 = b_e1 + i * BLOCK, *e2 = b_e2 + i * BLOCK, *e3 = b_e3 + i * BLOCK,
                 *e4 = b_e4 + i * BLOCK, *n1 = e1 + BLOCK, *n2 = e2 + BLOCK, *n3 = e3 + BLOCK,
                 *n4 = e4 + BLOCK;
    const double *cup1 = b_cup + (TT_REAPER_COMPACT ? 1 + (row & 1) : i + 1) * BLOCK,
                 *cdn1 = b_cdn + (TT_REAPER_COMPACT ? (row & 1) : i + 1) * BLOCK,
                 *cuptn = b_cuptn + i * BLOCK, *cdntn = b_cdntn + i * BLOCK;
    size_t odd = 2 * i + 1, even = 2 * i + 2;
    double pivots[BLOCK];
    for (size_t c = 0; c < BLOCK; ++c) {
        double a = n2[c] * e1[c] - e3[c] * n4[c];
        double b = e2[c] * n2[c] - e4[c] * n4[c];
        double d = n1[c] * n4[c] - n2[c] * n3[c];
        double r = (cup1[c] - cuptn[c]) * n2[c] - (cdn1[c] - cdntn[c]) * n4[c];
        double pivot = b - a * cp[(odd - 1) * BLOCK + c];
        pivots[c] = pivot;
        double denom = 1.0 / pivot;
        cp[odd * BLOCK + c] = d * denom;
        u[odd * BLOCK + c] = (r - a * u[(odd - 1) * BLOCK + c]) * denom;
    }
    for (size_t c = 0; c < BLOCK; ++c)
        if (pivots[c] == 0.0)
            return TT_SINGULAR;
    for (size_t c = 0; c < BLOCK; ++c) {
        double a = e2[c] * e3[c] - e4[c] * e1[c];
        double b = e1[c] * n1[c] - e3[c] * n3[c];
        double d = e3[c] * n4[c] - e1[c] * n2[c];
        double r = e3[c] * (cup1[c] - cuptn[c]) + e1[c] * (cdntn[c] - cdn1[c]);
        double pivot = b - a * cp[odd * BLOCK + c];
        pivots[c] = pivot;
        double denom = 1.0 / pivot;
        cp[even * BLOCK + c] = d * denom;
        u[even * BLOCK + c] = (r - a * u[odd * BLOCK + c]) * denom;
    }
    for (size_t c = 0; c < BLOCK; ++c)
        if (pivots[c] == 0.0)
            return TT_SINGULAR;
    return TT_OK;
}

/* Layer coefficients and particular solutions, followed immediately by
 * forward elimination. Each scalar operation retains its reference order. */
static tt_status coefficients(size_t n, const int *nid, double mu, block_work *s) {
    const double *R_RESTRICT b_slant = s->slant;
    const double *R_RESTRICT b_ex = s->ex;
    double *R_RESTRICT b_e1 = s->e1;
    double *R_RESTRICT b_e2 = s->e2;
    double *R_RESTRICT b_e3 = s->e3;
#if TT_REAPER_COMPACT
    /* All accesses to an overlaid row derive from its writable restricted
     * base. Independent input aliases would violate C restrict semantics. */
    const double *b_g = b_e1, *b_om = b_e2, *b_taun = b_e3;
#else
    const double *R_RESTRICT b_g = s->g;
    const double *R_RESTRICT b_om = s->om;
    const double *R_RESTRICT b_taun = s->taun;
#endif
    double *R_RESTRICT b_e4 = s->e4;
    double *R_RESTRICT b_cup = s->cup;
    double *R_RESTRICT b_cdn = s->cdn;
    double *R_RESTRICT b_cuptn = s->cuptn;
    double *R_RESTRICT b_cdntn = s->cdntn;

    double tauc[BLOCK];
    for (size_t c = 0; c < BLOCK; ++c)
        tauc[c] = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double *gp = b_g + i * BLOCK, *omp = b_om + i * BLOCK, *taun = b_taun + i * BLOCK;
        const double *s0 = b_slant + i * BLOCK, *s1 = s0 + BLOCK, *x0 = b_ex + i * BLOCK,
                     *x1 = x0 + BLOCK;
        double lam[BLOCK], arg[BLOCK], expon[BLOCK], gam1[BLOCK], gam2[BLOCK], gam3[BLOCK],
            gam4[BLOCK], omi[BLOCK], m[BLOCK];
        int valid_root = 1;
        if (nid[i + 1] >= 0)
            for (size_t c = 0; c < BLOCK; ++c) {
                double previous = tauc[c];
                tauc[c] = previous + taun[c];
                int flat = s1[c] == s0[c];
                double mm = (tauc[c] - previous) / (flat ? 1.0 : s1[c] - s0[c]);
                mm = copysign(fmax(fabs(mm), 1.0 / sqrt(1.e36)), mm);
                m[c] = flat ? sqrt(1.e36) : mm;
            }
        else
            for (size_t c = 0; c < BLOCK; ++c) {
                tauc[c] = tauc[c] + taun[c];
                m[c] = 1.0 / sqrt(1.e36);
            }
        for (size_t c = 0; c < BLOCK; ++c) {
            double gi = copysign(fmin(fabs(gp[c]), 1.0 - 1.e-7), gp[c]);
            omi[c] = fmin(omp[c], 1.0 - 1.e-7);
            gam1[c] = (7.0 - omi[c] * (4.0 + 3.0 * gi)) / 4.0;
            gam2[c] = -(1.0 - omi[c] * (4.0 - 3.0 * gi)) / 4.0;
            gam3[c] = (2.0 - 3.0 * gi * mu) / 4.0;
            gam4[c] = 1.0 - gam3[c];
            double disc = gam1[c] * gam1[c] - gam2[c] * gam2[c];
            if (TT_REAPER_COEFF_SQRT) {
                lam[c] = disc;
                valid_root &= disc >= 0;
            } else {
                lam[c] = sqrt(disc);
                arg[c] = -lam[c] * taun[c];
            }
        }
        if (TT_REAPER_COEFF_SQRT) {
            if (valid_root) {
                /* fabs exposes the already checked nonnegative domain to
                 * compilers that otherwise serialize sqrt for errno. */
                for (size_t c = 0; c < BLOCK; ++c) {
                    lam[c] = copysign(sqrt(fabs(lam[c])), lam[c]);
                    arg[c] = -lam[c] * taun[c];
                }
            } else
                for (size_t c = 0; c < BLOCK; ++c) {
                    lam[c] = sqrt(lam[c]);
                    arg[c] = -lam[c] * taun[c];
                }
        }
        for (size_t c = 0; c < BLOCK; ++c)
            expon[c] = exp(arg[c]);
        for (size_t c = 0; c < BLOCK; ++c) {
            size_t q = i * BLOCK + c;
            double bgam = gam2[c] != 0.0 ? (gam1[c] - lam[c]) / gam2[c] : 0.0;
            b_e1[q] = 1.0 + bgam * expon[c];
            b_e2[q] = 1.0 - bgam * expon[c];
            b_e3[q] = bgam + expon[c];
            b_e4[q] = bgam - expon[c];
            double divisor = lam[c] * lam[c] - 1.0 / (m[c] * m[c]);
            divisor = copysign(fmax(1.e-3, fabs(divisor)), divisor);
            double up = omi[c] * ((gam1[c] - 1.0 / m[c]) * gam3[c] + gam4[c] * gam2[c]) / divisor;
            double dn = omi[c] * ((gam1[c] + 1.0 / m[c]) * gam4[c] + gam2[c] * gam3[c]) / divisor;
            b_cup[(TT_REAPER_COMPACT ? 1 + (i & 1) : i) * BLOCK + c] = up * x0[c];
            b_cdn[(TT_REAPER_COMPACT ? (i & 1) : i) * BLOCK + c] = dn * x0[c];
            if (TT_REAPER_COMPACT && !i)
                b_cup[c] = b_cup[BLOCK + c];
            b_cuptn[q] = up * x1[c];
            b_cdntn[q] = dn * x1[c];
        }
        tt_status result =
            forward_row(i, b_e1, b_e2, b_e3, b_e4, b_cup, b_cdn, b_cuptn, b_cdntn, s->cp, s->u);
        if (result)
            return result;
    }
    return TT_OK;
}

/* Surface boundary row and back substitution after forward elimination. */
static tt_status finish_solve(size_t n, double mu, const double *albedo, block_work *s) {
    const double *R_RESTRICT b_e1 = s->e1;
    const double *R_RESTRICT b_e2 = s->e2;
    const double *R_RESTRICT b_e3 = s->e3;
    const double *R_RESTRICT b_e4 = s->e4;
    const double *R_RESTRICT b_cuptn = s->cuptn;
    const double *R_RESTRICT b_cdntn = s->cdntn;

    double *R_RESTRICT cp = s->cp, *R_RESTRICT u = s->u;
    size_t last = 2 * n - 1, i = n - 1;
    for (size_t c = 0; c < BLOCK; ++c) {
        size_t q = i * BLOCK + c;
        double rs = albedo[c];
        double a = b_e1[q] - rs * b_e3[q];
        double b = b_e2[q] - rs * b_e4[q];
        double ssfc = rs * mu * s->ex[n * BLOCK + c];
        double r = ssfc - b_cuptn[q] + rs * b_cdntn[q];
        double pivot = b - a * cp[(last - 1) * BLOCK + c];
        if (pivot == 0.0)
            return TT_SINGULAR;
        double denom = 1.0 / pivot;
        cp[last * BLOCK + c] = 0.0 * denom;
        u[last * BLOCK + c] = (r - a * u[(last - 1) * BLOCK + c]) * denom;
    }
    for (size_t row = last; row > 0; --row)
        for (size_t c = 0; c < BLOCK; ++c)
            u[(row - 1) * BLOCK + c] =
                u[(row - 1) * BLOCK + c] - cp[(row - 1) * BLOCK + c] * u[row * BLOCK + c];
    return TT_OK;
}

static void output(size_t n, size_t w0, size_t count, double mu, double scale, const block_work *s,
                   tt_radiation *out) {
    const double *R_RESTRICT b_u = s->u;
    const double *R_RESTRICT b_e1 = s->e1;
    const double *R_RESTRICT b_e2 = s->e2;
    const double *R_RESTRICT b_e3 = s->e3;
    const double *R_RESTRICT b_e4 = s->e4;
    const double *R_RESTRICT b_cup = s->cup;
    const double *R_RESTRICT b_cuptn = s->cuptn;
    const double *R_RESTRICT b_cdntn = s->cdntn;
    const double *R_RESTRICT b_ex = s->ex;
    double *R_RESTRICT o_direct_flux = out->direct_flux;
    double *R_RESTRICT o_down_flux = out->down_flux;
    double *R_RESTRICT o_up_flux = out->up_flux;
    double *R_RESTRICT o_direct_irradiance = out->direct_irradiance;
    double *R_RESTRICT o_down_irradiance = out->down_irradiance;
    double *R_RESTRICT o_up_irradiance = out->up_irradiance;

    const double *u = b_u;
    for (size_t lev = 0; lev <= n; ++lev)
        for (size_t c = 0; c < count; ++c) {
            size_t k = (w0 + c) * (n + 1) + n - lev;
            double up, down;
            if (!lev) {
                down = 0.0;
                up = u[c] * b_e3[c] - u[BLOCK + c] * b_e4[c] + b_cup[c];
            } else {
                size_t i = lev - 1, row = 2 * i, q = i * BLOCK + c;
                down =
                    u[row * BLOCK + c] * b_e3[q] + u[(row + 1) * BLOCK + c] * b_e4[q] + b_cdntn[q];
                up = u[row * BLOCK + c] * b_e1[q] + u[(row + 1) * BLOCK + c] * b_e2[q] + b_cuptn[q];
            }
            double direct = b_ex[lev * BLOCK + c];
            o_direct_flux[k] = direct * scale;
            o_direct_irradiance[k] = mu * direct * scale;
            o_down_irradiance[k] = down * scale;
            o_up_irradiance[k] = up * scale;
            o_down_flux[k] = down / 0.5 * scale;
            o_up_flux[k] = up / 0.5 * scale;
        }
}

tt_status tt_reaper_delta_eddington_profiled(size_t nr, const tt_radiator *radiators, size_t n,
                                             size_t nw, const int *nid, const double *path,
                                             double zenith, const double *surface, double scale,
                                             double *work, tt_radiation *out,
                                             tt_diagnostics *diagnostics,
                                             const unsigned char *roles) {
    if ((nr && !radiators) || !n || !nw || !nid || !path || !surface || !work || !out ||
        !isfinite(zenith))
        return TT_INVALID;
    block_work s;
    partition(n, work, &s);
    double mu = cos(zenith * (3.1415926535898 / 180.0));
    size_t total = n * nw;
    double *sums = work + (ROWS * n + EXTRA) * BLOCK;
    TT_TIME_START(initial_mix_time);
    tt_status mixed = mix_layers(nr, radiators, total, sums, sums + total, sums + 2 * total, roles);
    TT_TIME_END(diagnostics, TT_STAGE_MIX, initial_mix_time);
    if (mixed)
        return mixed;
    for (size_t w0 = 0; w0 < nw; w0 += BLOCK) {
        size_t count = nw - w0 < BLOCK ? nw - w0 : BLOCK;
        double albedo[BLOCK];
        for (size_t c = 0; c < BLOCK; ++c) {
            albedo[c] = surface[w0 + (c < count ? c : count - 1)];
            if (!isfinite(albedo[c]) || albedo[c] < 0 || albedo[c] > 1)
                return TT_INVALID;
        }
        TT_TIME_START(mix_time);
        tt_status status = scale_layers(n, w0, count, total, sums, &s);
        if (status)
            return status;
        TT_TIME_END(diagnostics, TT_STAGE_MIX, mix_time);
        TT_TIME_START(slant_time);
        slant_depths(n, nid, path, mu, &s);
        TT_TIME_END(diagnostics, TT_STAGE_SLANT, slant_time);
        TT_TIME_START(coefficients_time);
        status = coefficients(n, nid, mu, &s);
        if (status)
            return status;
        TT_TIME_END(diagnostics, TT_STAGE_COEFFICIENTS, coefficients_time);
        TT_TIME_START(solve_time);
        status = finish_solve(n, mu, albedo, &s);
        if (status)
            return status;
        TT_TIME_END(diagnostics, TT_STAGE_SOLVE, solve_time);
        TT_TIME_START(output_time);
        output(n, w0, count, mu, scale, &s, out);
        TT_TIME_END(diagnostics, TT_STAGE_OUTPUT, output_time);
    }
    return TT_OK;
}

tt_status tt_reaper_delta_eddington(size_t nr, const tt_radiator *radiators, size_t n, size_t nw,
                                    const int *nid, const double *path, double zenith,
                                    const double *surface, double scale, double *work,
                                    tt_radiation *out) {
    return tt_reaper_delta_eddington_profiled(nr, radiators, n, nw, nid, path, zenith, surface,
                                              scale, work, out, NULL, NULL);
}
