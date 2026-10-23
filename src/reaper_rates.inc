/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 * Ordered rate experiments; private to model_plan.c. Zero tile means one
 * full-level pass. No reduction order or product grouping changes. */
static void reaper_integrate_tile(const integral *g, const double *restrict flux, size_t nz,
                                  size_t z0, size_t width, double *restrict out) {
#if TT_REAPER_RATE_TILE == 0
    double *restrict acc = out + z0;
    memset(acc, 0, width * sizeof(double));
#else
    double acc[TT_REAPER_RATE_TILE] = {0};
#endif
    for (size_t t = 0; t < g->count; ++t) {
        const term *x = g->terms + t;
        const double *f = flux + x->w * nz + z0;
        const double *c = x->cross ? x->cross + z0 : NULL, *y = x->yield ? x->yield + z0 : NULL;
        double v = x->value, e = x->energy;
        if (!g->heating)
            switch (x->kind) {
            case TERM_CONSTANT:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * v;
                break;
            case TERM_CROSS:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (c[z] * v);
                break;
            case TERM_YIELD:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (v * y[z]);
                break;
            case TERM_BOTH:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (c[z] * y[z]);
                break;
            }
        else
            switch (x->kind) {
            case TERM_CONSTANT:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * v;
                break;
            case TERM_CROSS:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (e * (c[z] * v));
                break;
            case TERM_YIELD:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (e * (v * y[z]));
                break;
            case TERM_BOTH:
                for (size_t z = 0; z < width; ++z)
                    acc[z] = acc[z] + f[z] * (e * (c[z] * y[z]));
                break;
            }
    }
#if TT_REAPER_RATE_TILE != 0
    if (g->dose)
        memcpy(out + z0, acc, width * sizeof(double));
#endif
    if (!g->dose)
        for (size_t z = 0; z < width; ++z)
            out[z0 + z] = acc[z] * g->scale;
}
static void reaper_integrate(const integral *g, const double *flux, size_t nz, double *out) {
#if TT_REAPER_RATE_TILE == 0
    reaper_integrate_tile(g, flux, nz, 0, nz, out);
#else
    size_t z0 = 0;
    for (; z0 + TT_REAPER_RATE_TILE <= nz; z0 += TT_REAPER_RATE_TILE)
        reaper_integrate_tile(g, flux, nz, z0, TT_REAPER_RATE_TILE, out);
    if (z0 < nz)
        reaper_integrate_tile(g, flux, nz, z0, nz - z0, out);
#endif
}
