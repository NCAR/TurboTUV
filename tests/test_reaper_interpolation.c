/* Stress positive conserving interpolation, endpoint cancellation, and
 * ordered fallback for signed or extreme inputs. The preserved prepared
 * path also checks the independently assembled generic reference. */
#include "spectrum_plan.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep allocator injection local to this test's copy of the implementation.
 * The optional one-byte-per-bin allocation may fail without losing the
 * ordered executor or changing the model's result. */
static int fail_optional;
static size_t allocation_failures;
static void *interpolation_calloc(size_t count, size_t size) {
    if (fail_optional && size == sizeof(unsigned char)) {
        ++allocation_failures;
        return NULL;
    }
    return calloc(count, size);
}
#define calloc interpolation_calloc
#include "../src/spectrum_plan.c"
#undef calloc
static uint64_t state = UINT64_C(0x4131177293);
static double rnd(void) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return (double)(state >> 11) * 0x1p-53;
}
int main(void) {
    enum { MAX_RAW = 129, MAX_BINS = 17, NZ = 9 };
    size_t compared = 0;
    double worst = 0;
    for (size_t trial = 0; trial < 10000; ++trial) {
        size_t nr = 2 + (size_t)(rnd() * (MAX_RAW - 2)), nw = 1 + (size_t)(rnd() * (MAX_BINS - 1));
        double x[MAX_RAW], y[MAX_RAW] = {0}, a[MAX_RAW], b[MAX_RAW], sigma[MAX_RAW];
        x[0] = 100;
        for (size_t j = 0; j < nr; ++j) {
            if (j)
                x[j] = x[j - 1] + .01 + 2 * rnd();
            a[j] = -200 + 400 * rnd();
            b[j] = -.3 + .6 * rnd();
            sigma[j] = pow(10, -200 + 400 * rnd());
            if (trial % 10 == 0)
                a[j] = (j % 2 ? -300 : 300), b[j] = 0;
            if (trial % 7 == 0)
                a[j] = 0, y[j] = sigma[j] = (j % 2 ? -1 : 1) * sigma[j];
        }
        double edge[MAX_BINS + 1], mid[MAX_BINS], delta[MAX_BINS];
        for (size_t w = 0; w <= nw; ++w)
            edge[w] = x[0] - .1 + (x[nr - 1] - x[0] + .2) * (double)w / (double)nw;
        if (trial % 13 == 0) {
            nr = 2;
            nw = 1;
            x[0] = 100;
            x[1] = 101;
            edge[0] = 101 - 1.e-10;
            edge[1] = 101 - 1.e-11;
        }
        for (size_t w = 0; w < nw; ++w)
            mid[w] = (edge[w] + edge[w + 1]) / 2, delta[w] = edge[w + 1] - edge[w];
        tt_spectrum s = {0};
        s.role = TT_SPECTRUM_CROSS_SECTION;
        s.parameterized = 1;
        s.wavelength = (tt_grid){nw, edge, mid, delta};
        s.raw_count = nr;
        s.raw_wavelength = x;
        s.raw_data = y;
        s.temperature = (tt_temperature_model){0};
        s.temperature.kind = trial % 7 == 0 ? TT_TEMPERATURE_TAYLOR : TT_TEMPERATURE_HARWOOD;
        s.temperature.count = nr;
        s.temperature.terms = 1;
        s.temperature.a = a;
        s.temperature.b = b;
        s.temperature.sigma = sigma;
        tt_spectrum_plan *p = NULL;
        fail_optional = trial % 19 == 0;
        if (tt_spectrum_plan_create(&s, &p)) {
            fprintf(stderr, "create %zu\n", trial);
            return 1;
        }
        const size_t *indices;
        size_t count = tt_spectrum_plan_varying(p, &indices);
        double *scratch = malloc((tt_spectrum_plan_scratch(p, NZ) + 1) * sizeof(double));
        double *rows = calloc((count + 1) * NZ, sizeof(double));
        double ref[MAX_BINS * NZ], prepared[MAX_BINS * NZ], t[NZ], air[NZ];
        for (size_t z = 0; z < NZ; ++z)
            t[z] = 150 + 300 * rnd(), air[z] = 1.e19;
        if (!scratch || !rows || tt_spectrum_calculate(&s, NZ, t, air, ref) ||
            tt_spectrum_plan_evaluate(p, NZ, t, air, scratch, rows))
            return 2;
        const double *fixed = tt_spectrum_plan_constant(p);
        for (size_t w = 0, k = 0; w < nw; ++w) {
            int varies = k < count && indices[k] == w;
            for (size_t z = 0; z < NZ; ++z)
                prepared[w * NZ + z] = varies ? rows[k * NZ + z] : fixed[w];
            if (varies)
                ++k;
        }
        if (memcmp(ref, prepared, nw * NZ * sizeof(double))) {
            fprintf(stderr, "reference harness %zu\n", trial);
            return 3;
        }
        if (tt_spectrum_reaper_evaluate(p, NZ, t, air, scratch, rows))
            return 4;
        for (size_t w = 0, k = 0; w < nw; ++w) {
            int varies = k < count && indices[k] == w;
            for (size_t z = 0; z < NZ; ++z) {
                double actual = varies ? rows[k * NZ + z] : fixed[w], expected = ref[w * NZ + z];
                double scale = fmax(fabs(actual), fabs(expected));
                double error = fabs(actual - expected) / (DBL_EPSILON * scale + DBL_MIN);
                if (!isfinite(actual) || !isfinite(expected) || (actual == 0) != (expected == 0) ||
                    (actual < 0) != (expected < 0) || error > 32) {
                    fprintf(stderr,
                            "trial=%zu field=%zu/%zu actual=%.17g expected=%.17g error=%.9g\n",
                            trial, w, z, actual, expected, error);
                    return 5;
                }
                if (error > worst)
                    worst = error;
                ++compared;
                if ((trial % 7 == 0 || fail_optional) &&
                    memcmp(&actual, &expected, sizeof(double))) {
                    fprintf(stderr, "ordered fallback differs at trial=%zu field=%zu/%zu\n", trial,
                            w, z);
                    return 6;
                }
            }
            if (varies)
                ++k;
        }
        free(scratch);
        free(rows);
        tt_spectrum_plan_destroy(p);
    }
    if (!allocation_failures)
        return 7;
    printf("cases=10000 compared=%zu maximum_epsilon=%.17g allocation_failures=%zu\n", compared,
           worst, allocation_failures);
    return 0;
}
