/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 * Compile the private model plan with a local allocation-failure shim. The
 * production library has no fault-injection state or allocator override. */
#include <stdlib.h>
static int fail_preparation;
static void *preparation_malloc(size_t n) {
    return fail_preparation ? NULL : malloc(n);
}
static void *preparation_calloc(size_t n, size_t size) {
    return fail_preparation ? NULL : calloc(n, size);
}
#define malloc preparation_malloc
#define calloc preparation_calloc
#include "model_plan.c"
#undef malloc
#undef calloc

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "REAPER preparation:%d: %s\n", __LINE__, #x);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 3);
    for (int c = 1; c < argc; ++c) {
        tt_config *config = NULL;
        tt_model *model = NULL, *reference = NULL;
        tt_error error;
        CHECK(tt_config_read_file(argv[c], &config, &error) == TT_OK);
        CHECK(tt_model_create(tt_config_root(config), NULL, &model, &error) == TT_OK);
        CHECK(tt_model_create(tt_config_root(config), NULL, &reference, &error) == TT_OK);
        tt_config_destroy(config);
        fail_preparation = 1;
        CHECK(tt_model_set_execution(model, TT_EXECUTION_REAPER) == TT_OK);
        fail_preparation = 0;
        tt_reaper_info info;
        CHECK(tt_model_reaper_info(model, &info) == TT_OK);
        CHECK(info.preparation_failures == (TT_REAPER_ROLE_CACHE | TT_REAPER_TRANSFER_WORKSPACE));
        CHECK(!info.optimizations && !info.transfer_workspace_bytes);
        const tt_model_result *a, *b;
        CHECK(tt_model_run(model, 88, .97, &a, &error) == TT_OK);
        CHECK(tt_model_run(reference, 88, .97, &b, &error) == TT_OK);
        tt_model_dimensions shape = tt_model_shape(model);
        const double *actual[] = {a->radiation.direct_flux,
                                  a->radiation.down_flux,
                                  a->radiation.up_flux,
                                  a->radiation.direct_irradiance,
                                  a->radiation.down_irradiance,
                                  a->radiation.up_irradiance,
                                  a->actinic_flux,
                                  a->spectral_irradiance,
                                  a->photolysis,
                                  a->heating,
                                  a->dose};
        const double *expected[] = {b->radiation.direct_flux,
                                    b->radiation.down_flux,
                                    b->radiation.up_flux,
                                    b->radiation.direct_irradiance,
                                    b->radiation.down_irradiance,
                                    b->radiation.up_irradiance,
                                    b->actinic_flux,
                                    b->spectral_irradiance,
                                    b->photolysis,
                                    b->heating,
                                    b->dose};
        for (size_t field = 0; field < 11; ++field) {
            size_t count = shape.levels * (field < 8    ? shape.wavelengths
                                           : field == 8 ? shape.photolysis
                                           : field == 9 ? shape.heating
                                                        : shape.dose);
            for (size_t i = 0; i < count; ++i) {
                double x = actual[field][i], y = expected[field][i];
                double norm = fmax(fabs(x), fabs(y));
                if (field < 6)
                    norm = fmax(norm, 1);
                CHECK(isfinite(x) && isfinite(y) && (x == 0) == (y == 0));
                CHECK(!(y >= 0 && x < 0) && fabs(x - y) <= 64 * DBL_EPSILON * norm);
            }
        }
        tt_diagnostics counters;
        CHECK(tt_model_diagnostics(model, &counters) == TT_OK && counters.reaper_calls == 1 &&
              !counters.generic_calls);
        CHECK(tt_model_set_execution(model, TT_EXECUTION_PREPARED) == TT_OK);
        CHECK(tt_model_set_execution(model, TT_EXECUTION_REAPER) == TT_OK);
        CHECK(tt_model_reaper_info(model, &info) == TT_OK && info.preparation_failures);
        tt_model_destroy(model);
        tt_model_destroy(reference);
    }
    puts("REAPER optional allocation failure: observable, usable, and numerically valid");
    return 0;
}
