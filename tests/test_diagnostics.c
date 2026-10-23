#include "turbotuv_diagnostics.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "diagnostics: %s\n", #x);                                              \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv) {
    CHECK(argc == 2);
    tt_config *c = NULL;
    tt_model *m = NULL;
    tt_error e;
    tt_diagnostics d;
    const tt_model_result *r;
    CHECK(tt_model_diagnostics(NULL, &d) == TT_INVALID);
    CHECK(tt_model_reset_diagnostics(NULL) == TT_INVALID);
    CHECK(tt_stage_name(TT_STAGE_COUNT) == NULL);
    CHECK(tt_config_read_file(argv[1], &c, &e) == TT_OK);
    CHECK(tt_model_create(tt_config_root(c), NULL, &m, &e) == TT_OK);
    tt_config_destroy(c);
    CHECK(tt_model_run(m, 30, 1, &r, &e) == TT_OK);
    CHECK(tt_model_diagnostics(m, &d) == TT_OK);
    CHECK(d.prepared_calls == 1 && d.generic_calls == 0 && d.prepared_declines == 0);
    CHECK(tt_model_set_execution(m, TT_EXECUTION_GENERIC) == TT_OK);
    CHECK(tt_model_run(m, 30, 1, &r, &e) == TT_OK);
    CHECK(tt_model_diagnostics(m, &d) == TT_OK && d.generic_calls == 1);
    CHECK(tt_model_reset_diagnostics(m) == TT_OK);
    CHECK(tt_model_diagnostics(m, &d) == TT_OK && d.generic_calls == 0 && d.prepared_calls == 0);
    CHECK(tt_model_set_execution(m, TT_EXECUTION_PREPARED) == TT_OK);
    CHECK(tt_model_run(m, NAN, 1, &r, &e) == TT_INVALID);
    CHECK(tt_model_diagnostics(m, &d) == TT_OK);
    CHECK(d.prepared_declines == 1 && d.generic_calls == 1 && d.prepared_calls == 0);
    tt_model_destroy(m);
    return 0;
}
