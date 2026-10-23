#include "turbotuv_model.h"
#include "turbotuv_diagnostics.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "REAPER updates:%d: %s\n", __LINE__, #x);                              \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static tt_model *create(const char *path, tt_execution execution) {
    tt_config *config = NULL;
    tt_model *model = NULL;
    tt_error error;
    CHECK(tt_config_read_file(path, &config, &error) == TT_OK);
    CHECK(tt_model_create(tt_config_root(config), NULL, &model, &error) == TT_OK);
    CHECK(tt_model_set_execution(model, execution) == TT_OK);
    tt_config_destroy(config);
    return model;
}
static void compare(const tt_model_result *a, const tt_model_result *b, tt_model_dimensions s) {
    const double *aa[] = {a->radiation.direct_flux,
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
    const double *bb[] = {b->radiation.direct_flux,
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
    size_t n = s.levels * s.wavelengths;
    size_t count[] = {
        n, n, n, n, n, n, n, n, s.levels * s.photolysis, s.levels * s.heating, s.levels * s.dose};
    for (size_t f = 0; f < 11; ++f)
        for (size_t i = 0; i < count[f]; ++i) {
            double x = aa[f][i], y = bb[f][i], norm = fmax(fabs(x), fabs(y));
            if (f < 6)
                norm = fmax(norm, 1);
            CHECK(isfinite(x) && isfinite(y) && (x == 0) == (y == 0));
            CHECK(!(y >= 0 && x < 0));
            CHECK(fabs(x - y) <= 64 * DBL_EPSILON * norm);
        }
}
int main(int argc, char **argv) {
    CHECK(argc > 1);
    for (int c = 1; c < argc; ++c) {
        tt_model *fast = create(argv[c], TT_EXECUTION_REAPER);
        tt_model *ref = create(argv[c], TT_EXECUTION_GENERIC);
        tt_model_dimensions s = tt_model_shape(fast);
        size_t n = (s.levels - 1) * s.wavelengths;
        double *data = calloc(3 * n, sizeof(double));
        CHECK(data);
        const char *names[] = {"air", "O3", "O2", "aerosols"};
        const double angles[] = {0, 88, 93};
        const tt_model_result *a, *b;
        tt_error error;
        CHECK(tt_model_run(fast, 30, 1, &a, &error) == TT_OK); /* Populate caches. */
        for (size_t name = 0; name < 4; ++name)
            for (size_t kind = 0; kind < 5; ++kind) {
                for (size_t i = 0; i < n; ++i) {
                    data[i] = kind == 0 || kind == 4 ? 0 : .001 + (i % 19) * .0001;
                    data[n + i] = kind == 1 ? 0 : kind == 2 ? 1 : .65;
                    data[2 * n + i] = kind == 2 ? 0 : .3;
                }
                tt_named_radiator r = {names[name],
                                       s.levels - 1,
                                       s.wavelengths,
                                       {data, data + n, data + 2 * n, kind == 2}};
                CHECK(tt_model_set_radiator(fast, &r) == TT_OK);
                CHECK(tt_model_set_radiator(ref, &r) == TT_OK);
                /* Rejected updates must preserve the newly cached state. */
                CHECK(tt_model_run(fast, 30, 1, &a, &error) == TT_OK);
                data[n] = NAN;
                CHECK(tt_model_set_radiator(fast, &r) == TT_INVALID);
                CHECK(tt_model_set_radiator(ref, &r) == TT_INVALID);
                for (size_t k = 0; k < 3; ++k) {
                    CHECK(tt_model_run(fast, angles[k], .97, &a, &error) == TT_OK);
                    CHECK(tt_model_run(ref, angles[k], .97, &b, &error) == TT_OK);
                    compare(a, b, s);
                }
            }
        tt_diagnostics d;
        CHECK(tt_model_diagnostics(fast, &d) == TT_OK && d.reaper_calls && !d.generic_calls);
        free(data);
        tt_model_destroy(fast);
        tt_model_destroy(ref);
    }
    puts("REAPER radiator replacements: zero, absorption, scattering, general and rejected updates "
         "pass");
    return 0;
}
