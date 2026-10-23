/* Export only the authoritative C model plan. No configuration interpretation
 * is duplicated in the emitter. */
#include "turbotuv_reaper.h"
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Usage: turbotuv_reaper_export CONFIG PLAN.json\n", stderr);
        return 2;
    }
    tt_config *c = NULL;
    tt_model *m = NULL;
    tt_error e;
    if (tt_config_read_file(argv[1], &c, &e) || tt_model_create(tt_config_root(c), NULL, &m, &e)) {
        fprintf(stderr, "%s\n", e.message);
        tt_config_destroy(c);
        return 1;
    }
    tt_config_destroy(c);
    FILE *f = fopen(argv[2], "wb");
    unsigned char hash[32];
    tt_status status = f ? tt_model_export_reaper(m, f, hash) : TT_INVALID;
    if (f && fclose(f))
        status = TT_INVALID;
    tt_model_destroy(m);
    if (status) {
        fprintf(stderr, "Export: %s\n", tt_status_string(status));
        return 1;
    }
    for (size_t i = 0; i < 32; ++i)
        printf("%02x", hash[i]);
    puts("");
    return 0;
}
