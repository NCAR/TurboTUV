/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#include <turbotuv_reaper.h>
#include <turbotuv_diagnostics.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static double now(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + 1.e-9 * (double)t.tv_nsec;
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "installed consumer:%d: %s\n", __LINE__, #x);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 3);
    const int generated = !strcmp(argv[2], "attached");
    tt_config *config = NULL;
    tt_model *model = NULL;
    tt_error error;
    CHECK(tt_config_read_file(argv[1], &config, &error) == TT_OK);
    double begin = now();
    CHECK(tt_model_create(tt_config_root(config), NULL, &model, &error) == TT_OK);
    double built = now();
    tt_config_destroy(config);
    CHECK(tt_model_set_execution(model, TT_EXECUTION_REAPER) == TT_OK);
    double selected = now();
    tt_reaper_info info;
    CHECK(tt_model_reaper_info(model, &info) == TT_OK);
    CHECK(info.selection == (generated ? TT_REAPER_ATTACHED : TT_REAPER_NO_ARTIFACTS));
    CHECK((info.capabilities != 0) == generated && !info.preparation_failures);
    for (size_t i = 0; i < 10000; ++i)
        CHECK(tt_model_set_execution(model, TT_EXECUTION_REAPER) == TT_OK);
    double reused = now();
    const tt_model_result *result;
    CHECK(tt_model_run(model, 30, 1, &result, &error) == TT_OK);
    tt_model_dimensions shape = tt_model_shape(model);
    double sum = 0;
    for (size_t i = 0; i < shape.levels * shape.photolysis; ++i) {
        CHECK(isfinite(result->photolysis[i]) && result->photolysis[i] >= 0);
        sum += result->photolysis[i];
    }
    CHECK(sum > 0);
    tt_diagnostics d;
    CHECK(tt_model_diagnostics(model, &d) == TT_OK && d.reaper_calls == 1 && !d.generic_calls);
    CHECK((d.generated_calls != 0) == generated);
    printf("selection=%s capabilities=%u setup_ms=%.3f preparation_ms=%.3f "
           "reuse_ns=%.3f sum=%.17g\n",
           tt_reaper_selection_name(info.selection), info.capabilities, 1000 * (built - begin),
           1000 * (selected - built), 1.e5 * (reused - selected), sum);
    CHECK(tt_model_attach_reaper(model, NULL, &error) == TT_OK);
    CHECK(tt_model_set_execution(model, TT_EXECUTION_REAPER) == TT_OK);
    CHECK(tt_model_reaper_info(model, &info) == TT_OK && info.selection == TT_REAPER_DETACHED);
    tt_model_destroy(model);
    return 0;
}
