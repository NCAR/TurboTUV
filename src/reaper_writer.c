/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0
 * SHA-256 and a locale-independent canonical JSON stream. No padding bytes,
 * pointers or native-endian byte sequences enter the serialized identity. */
#include "reaper_writer.h"
#include <inttypes.h>
#include <string.h>
static uint32_t rotate(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}
static void transform(tt_sha256 *s) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
        const unsigned char *p = s->block + 4 * i;
        w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t a = w[i - 15], b = w[i - 2];
        w[i] = w[i - 16] + (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3)) + w[i - 7] +
               (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10));
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5],
             g = s->h[6], h = s->h[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t t1 =
            h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        uint32_t t2 =
            (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}
void tt_sha256_init(tt_sha256 *s) {
    static const uint32_t initial[] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memset(s, 0, sizeof *s);
    memcpy(s->h, initial, sizeof initial);
}
void tt_sha256_update(tt_sha256 *s, const void *data, size_t n) {
    const unsigned char *p = data;
    s->bytes += (uint64_t)n;
    while (n) {
        size_t take = 64 - s->used;
        if (take > n)
            take = n;
        memcpy(s->block + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;
        if (s->used == 64) {
            transform(s);
            s->used = 0;
        }
    }
}
void tt_sha256_final(tt_sha256 *s, unsigned char digest[32]) {
    uint64_t bits = s->bytes * 8;
    unsigned char padding[128] = {0x80};
    size_t n = s->used < 56 ? 56 - s->used : 120 - s->used;
    for (size_t i = 0; i < 8; ++i)
        padding[n + i] = (unsigned char)(bits >> (56 - 8 * i));
    tt_sha256_update(s, padding, n + 8);
    for (size_t i = 0; i < 32; ++i)
        digest[i] = (unsigned char)(s->h[i / 4] >> (24 - 8 * (i % 4)));
}
void tt_rw_init(tt_reaper_writer *w, FILE *file) {
    w->file = file;
    w->failed = 0;
    tt_sha256_init(&w->sha);
}
void tt_rw_text(tt_reaper_writer *w, const char *s) {
    size_t n = strlen(s);
    tt_sha256_update(&w->sha, s, n);
    if (w->file && fwrite(s, 1, n, w->file) != n)
        w->failed = 1;
}
void tt_rw_size(tt_reaper_writer *w, size_t n) {
    char buf[32];
    snprintf(buf, sizeof buf, "%zu", n);
    tt_rw_text(w, buf);
}
void tt_rw_int(tt_reaper_writer *w, int n) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d", n);
    tt_rw_text(w, buf);
}
void tt_rw_double(tt_reaper_writer *w, double x) {
    uint64_t bits = 0;
    memcpy(&bits, &x, sizeof bits);
    char buf[20];
    snprintf(buf, sizeof buf, "\"%016" PRIx64 "\"", bits);
    tt_rw_text(w, buf);
}
void tt_rw_array(tt_reaper_writer *w, const double *x, size_t n) {
    if (!x) {
        tt_rw_text(w, "null");
        return;
    }
    tt_rw_text(w, "[");
    for (size_t i = 0; i < n; ++i) {
        if (i)
            tt_rw_text(w, ",");
        tt_rw_double(w, x[i]);
    }
    tt_rw_text(w, "]");
}
void tt_rw_string(tt_reaper_writer *w, const char *s) {
    tt_rw_text(w, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        char buf[8];
        if (*p == '"' || *p == '\\') {
            buf[0] = '\\';
            buf[1] = (char)*p;
            buf[2] = 0;
        } else if (*p < 32)
            snprintf(buf, sizeof buf, "\\u%04x", *p);
        else {
            buf[0] = (char)*p;
            buf[1] = 0;
        }
        tt_rw_text(w, buf);
    }
    tt_rw_text(w, "\"");
}
