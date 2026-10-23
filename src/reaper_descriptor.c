/* Copyright (C) 2026 University Corporation for Atmospheric Research
 * SPDX-License-Identifier: Apache-2.0 */
#include "turbotuv_reaper.h"
#include "reaper_writer.h"

tt_status tt_reaper_recipe_identity(const tt_reaper_kernels *k, unsigned char digest[32]) {
    if (!k || !digest)
        return TT_INVALID;
    tt_reaper_writer w;
    tt_rw_init(&w, NULL);
    tt_rw_text(&w, "{\"generator\":");
    tt_rw_size(&w, k->generator);
    tt_rw_text(&w, ",\"abi\":");
    tt_rw_size(&w, k->abi);
    tt_rw_text(&w, ",\"schema\":");
    tt_rw_size(&w, k->schema);
    tt_rw_text(&w, ",\"implementation\":");
    tt_rw_size(&w, k->implementation);
    tt_rw_text(&w, ",\"numerics\":");
    tt_rw_int(&w, k->numerics);
    tt_rw_text(&w, ",\"term_budget\":");
    tt_rw_size(&w, k->term_budget);
    tt_rw_text(&w, ",\"accumulators\":");
    tt_rw_size(&w, k->accumulators);
    tt_rw_text(&w, ",\"level_tile\":");
    tt_rw_size(&w, k->level_tile);
    tt_rw_text(&w, ",\"interleave\":");
    tt_rw_size(&w, k->interleave);
    tt_rw_text(&w, ",\"features\":");
    tt_rw_size(&w, k->features);
    tt_rw_text(&w, ",\"model_sha256\":\"");
    for (size_t i = 0; i < 32; ++i) {
        char hex[3];
        snprintf(hex, sizeof hex, "%02x", k->model_sha256[i]);
        tt_rw_text(&w, hex);
    }
    tt_rw_text(&w, "\"}\n");
    tt_sha256_final(&w.sha, digest);
    return TT_OK;
}

const char *tt_reaper_selection_name(tt_reaper_selection state) {
    switch (state) {
    case TT_REAPER_UNCHECKED:
        return "unchecked";
    case TT_REAPER_ATTACHED:
        return "attached";
    case TT_REAPER_DETACHED:
        return "detached";
    case TT_REAPER_NO_ARTIFACTS:
        return "no-artifacts";
    case TT_REAPER_NO_MATCH:
        return "no-match";
    case TT_REAPER_IDENTITY_UNAVAILABLE:
        return "identity-unavailable";
    case TT_REAPER_NO_PLAN:
        return "no-prepared-plan";
    }
    return "unknown";
}
