/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 * Link this small startup probe with the frozen prepared control library. */
#include <turbotuv_model.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
static double now(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + 1.e-9 * (double)t.tv_nsec;
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 1;
    tt_config *config = NULL;
    tt_model *model = NULL;
    tt_error error;
    if (tt_config_read_file(argv[1], &config, &error))
        return 1;
    double begin = now();
    if (tt_model_create(tt_config_root(config), NULL, &model, &error))
        return 1;
    double built = now();
    tt_config_destroy(config);
    if (tt_model_set_execution(model, TT_EXECUTION_PREPARED))
        return 1;
    double selected = now();
    const tt_model_result *result;
    if (tt_model_run(model, 30, 1, &result, &error))
        return 1;
    tt_model_dimensions shape = tt_model_shape(model);
    double sum = 0;
    for (size_t i = 0; i < shape.levels * shape.photolysis; ++i) {
        if (!isfinite(result->photolysis[i]) || result->photolysis[i] < 0)
            return 1;
        sum += result->photolysis[i];
    }
    if (!(sum > 0))
        return 1;
    printf("setup_ms=%.3f preparation_ms=%.3f sum=%.17g\n", 1000 * (built - begin),
           1000 * (selected - built), sum);
    tt_model_destroy(model);
    return 0;
}
