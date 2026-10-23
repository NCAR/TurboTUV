#include "turbotuv_reaper.h"
#include "turbotuv_diagnostics.h"
#include "reaper_writer.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "artifact:%d: %s\n", __LINE__, #x);                                    \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static void hash_check(const char *message, const char *expected) {
    tt_sha256 s;
    unsigned char bytes[32];
    char hex[65];
    tt_sha256_init(&s);
    /* Feed across arbitrary streaming boundaries, including zero-sized input. */
    tt_sha256_update(&s, message, 0);
    for (size_t i = 0; i < strlen(message); ++i)
        tt_sha256_update(&s, message + i, 1);
    tt_sha256_final(&s, bytes);
    for (size_t i = 0; i < 32; ++i)
        snprintf(hex + 2 * i, 3, "%02x", bytes[i]);
    CHECK(!strcmp(hex, expected));
}
static tt_model *create(const char *path) {
    tt_config *c = NULL;
    tt_model *m = NULL;
    tt_error e;
    CHECK(tt_config_read_file(path, &c, &e) == TT_OK);
    CHECK(tt_model_create(tt_config_root(c), NULL, &m, &e) == TT_OK);
    tt_config_destroy(c);
    return m;
}
static char *export_text(tt_model *m, unsigned char digest[32]) {
    FILE *f = tmpfile();
    CHECK(f && tt_model_export_reaper(m, f, digest) == TT_OK);
    CHECK(!fseek(f, 0, SEEK_END));
    long length = ftell(f);
    CHECK(length > 0 && !fseek(f, 0, SEEK_SET));
    char *text = malloc((size_t)length + 1);
    CHECK(text && fread(text, 1, (size_t)length, f) == (size_t)length);
    text[length] = 0;
    CHECK(!fclose(f));
    tt_sha256 s;
    unsigned char checked[32];
    tt_sha256_init(&s);
    tt_sha256_update(&s, text, (size_t)length);
    tt_sha256_final(&s, checked);
    CHECK(!memcmp(digest, checked, 32));
    return text;
}
static int declines(const tt_reaper_view *v) {
    /* A failed family may have partial output; interpreter must overwrite it. */
    v->rates[0] = NAN;
    return 0;
}
int main(int argc, char **argv) {
    CHECK(argc == 3);
    hash_check("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    hash_check("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    hash_check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
               "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    hash_check("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklm"
               "nopqrlmnopqrsmnopqrstnopqrstu",
               "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    unsigned char a[32], b[32];
    CHECK(tt_model_reaper_identity(NULL, a) == TT_INVALID);
    for (int config = 1; config < argc; ++config) {
        tt_model *m = create(argv[config]), *other = create(argv[config]);
        tt_reaper_info info;
        CHECK(tt_model_reaper_info(m, &info) == TT_OK && info.selection == TT_REAPER_UNCHECKED);
        char *first = export_text(m, a), *second = export_text(other, b);
        CHECK(!strcmp(first, second) && !memcmp(a, b, 32));
        CHECK(tt_model_set_execution(other, TT_EXECUTION_REAPER) == TT_OK);
        CHECK(tt_model_reaper_info(other, &info) == TT_OK &&
              (info.selection == TT_REAPER_ATTACHED || info.selection == TT_REAPER_NO_ARTIFACTS ||
               info.selection == TT_REAPER_NO_MATCH));
        CHECK((info.selection == TT_REAPER_ATTACHED) == (info.capabilities != 0));
        tt_model_destroy(other);
        free(second);
        CHECK(tt_model_reaper_identity(m, b) == TT_OK && !memcmp(a, b, 32));
        tt_config *description = NULL;
        tt_error e;
        CHECK(tt_config_parse(first, strlen(first), &description, &e) == TT_OK);
        size_t slots = tt_config_size(tt_config_get(tt_config_root(description), "slots")) + 1;
        tt_config_node entries = tt_config_get(tt_config_root(description), "slots");
        for (size_t i = 0; i + 1 < slots; ++i) {
            tt_config_node s = tt_config_get(tt_config_at(entries, i), "spectrum");
            int parameterized, kind;
            CHECK(tt_config_integer(tt_config_get(s, "parameterized"), &parameterized) == TT_OK);
            CHECK(tt_config_integer(tt_config_get(s, "kind"), &kind) == TT_OK);
            if (parameterized)
                CHECK(kind == -1); /* Stable signed sentinel, including on 32-bit size_t. */
        }
        tt_config_destroy(description);
        free(first);
        tt_model_dimensions shape = tt_model_shape(m);
        tt_reaper_kernels k = {.abi = TT_REAPER_ABI,
                               .schema = TT_REAPER_SCHEMA,
                               .implementation = TT_REAPER_IMPLEMENTATION,
                               .generator = TT_REAPER_GENERATOR,
                               .numerics = TT_REAPER_ORDERED,
                               .levels = shape.levels,
                               .wavelengths = shape.wavelengths,
                               .slots = slots,
                               .photolysis = shape.photolysis,
                               .heating = shape.heating,
                               .dose = shape.dose,
                               .term_budget = 64,
                               .accumulators = 1,
                               .interleave = 1,
                               .photolysis_kernel = declines};
        memcpy(k.model_sha256, a, 32);
        CHECK(tt_reaper_recipe_identity(&k, k.recipe_sha256) == TT_OK);
        CHECK(tt_model_attach_reaper(m, &k, &e) == TT_OK);
        CHECK(tt_model_reaper_capabilities(m) == 1);
        CHECK(tt_model_reaper_info(m, &info) == TT_OK && info.selection == TT_REAPER_ATTACHED &&
              !memcmp(info.recipe_sha256, k.recipe_sha256, 32));
        tt_reaper_kernels bad = k;
        bad.abi++;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "ABI"));
        bad = k;
        bad.levels++;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "dimensions"));
        bad = k;
        bad.model_sha256[31] ^= 1;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID &&
              strstr(e.message, "fingerprint"));
        bad = k;
        bad.numerics = (tt_reaper_numerics)99;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "numerical"));
        bad = k;
        bad.generator++;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "generator"));
        bad = k;
        bad.features = TT_REAPER_FMA;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "numerical"));
        bad = k;
        bad.interleave = 3;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "layout"));
        bad = k;
        bad.level_tile = 3;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "layout"));
        bad = k;
        bad.features = 0x80u;
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "layout"));
        bad = k;
        bad.features = TT_REAPER_REUSE_PRODUCTS; /* Valid choice, wrong recipe digest. */
        CHECK(tt_model_attach_reaper(m, &bad, &e) == TT_INVALID && strstr(e.message, "recipe"));
        CHECK(tt_model_reaper_info(m, &info) == TT_OK && info.selection == TT_REAPER_ATTACHED &&
              !memcmp(info.recipe_sha256, k.recipe_sha256, 32));
        CHECK(tt_model_reaper_capabilities(m) == 1);
        size_t count = shape.levels * (shape.photolysis + shape.heating + shape.dose);
        double *expected = malloc(count * sizeof(double));
        double *temperature = malloc(shape.levels * sizeof(double));
        CHECK(expected && temperature);
        const tt_profile *profile = tt_model_profile(m, "temperature", "K");
        for (size_t z = 0; z < shape.levels; ++z)
            temperature[z] = profile->edges[z] + 0.25;
        CHECK(tt_model_update_profile(m, "temperature", "K", shape.levels - 1, temperature, NULL,
                                      NULL, NULL, NULL) == TT_OK);
        CHECK(tt_model_reaper_identity(m, b) == TT_OK && !memcmp(a, b, 32));
        second = export_text(m, b);
        CHECK(!memcmp(a, b, 32));
        free(second);
        const tt_model_result *r;
        CHECK(tt_model_run(m, 89.9, 0.97, &r, &e) == TT_OK);
        memcpy(expected, r->photolysis, count * sizeof(double));
        CHECK(tt_model_set_execution(m, TT_EXECUTION_REAPER) == TT_OK);
        CHECK(tt_model_reset_diagnostics(m) == TT_OK);
        CHECK(tt_model_reaper_info(m, &info) == TT_OK && info.selection == TT_REAPER_ATTACHED &&
              !info.preparation_failures && (info.optimizations & TT_REAPER_TRANSFER_WORKSPACE) &&
              info.transfer_workspace_bytes);
        CHECK(tt_model_run(m, 89.9, 0.97, &r, &e) == TT_OK);
        /* A declined generated family is overwritten by the interpreter.
         * REAPER's spectrum inputs may now differ within its roundoff contract. */
        for (size_t i = 0; i < count; ++i) {
            double actual = r->photolysis[i], norm = fmax(fabs(actual), fabs(expected[i]));
            CHECK(isfinite(actual) && (actual == 0) == (expected[i] == 0));
            CHECK(!(expected[i] >= 0 && actual < 0));
            CHECK(fabs(actual - expected[i]) <= 64 * DBL_EPSILON * norm);
        }
        tt_diagnostics d;
        CHECK(tt_model_diagnostics(m, &d) == TT_OK && d.reaper_calls == 1 && !d.generated_calls &&
              d.generated_declines == 1);
        CHECK(tt_model_run(m, NAN, 1, &r, &e) == TT_INVALID);
        CHECK(tt_model_attach_reaper(m, NULL, &e) == TT_OK && !tt_model_reaper_capabilities(m));
        CHECK(tt_model_set_execution(m, TT_EXECUTION_PREPARED) == TT_OK);
        CHECK(tt_model_set_execution(m, TT_EXECUTION_REAPER) == TT_OK);
        CHECK(tt_model_reaper_info(m, &info) == TT_OK && info.selection == TT_REAPER_DETACHED);
        CHECK(tt_model_run(m, 30, 1, &r, &e) == TT_OK);
        free(expected);
        free(temperature);
        tt_model_destroy(m);
    }
    tt_model *m = create(argv[1]), *other = create(argv[2]);
    CHECK(tt_model_reaper_identity(m, a) == TT_OK && tt_model_reaper_identity(other, b) == TT_OK);
    CHECK(memcmp(a, b, 32));
    tt_model_destroy(m);
    tt_model_destroy(other);
    puts("SHA-256 vectors, deterministic export, runtime updates, mismatch/partial/decline/detach: "
         "passed");
    return 0;
}
