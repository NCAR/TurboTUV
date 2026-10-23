/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef TT_REAPER_WRITER_H
#define TT_REAPER_WRITER_H
#include <stdint.h>
#include <stdio.h>
typedef struct {
    uint32_t h[8];
    uint64_t bytes;
    size_t used;
    unsigned char block[64];
} tt_sha256;
void tt_sha256_init(tt_sha256 *s);
void tt_sha256_update(tt_sha256 *s, const void *data, size_t n);
void tt_sha256_final(tt_sha256 *s, unsigned char digest[32]);
typedef struct {
    FILE *file;
    tt_sha256 sha;
    int failed;
} tt_reaper_writer;
void tt_rw_init(tt_reaper_writer *w, FILE *file);
void tt_rw_text(tt_reaper_writer *w, const char *s);
void tt_rw_size(tt_reaper_writer *w, size_t n);
void tt_rw_double(tt_reaper_writer *w, double x);
void tt_rw_array(tt_reaper_writer *w, const double *x, size_t n);
void tt_rw_string(tt_reaper_writer *w, const char *s);
#endif
