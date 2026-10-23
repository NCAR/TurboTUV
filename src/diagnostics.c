#include "model_internal.h"
#include <string.h>
tt_status tt_model_diagnostics(const tt_model *m, tt_diagnostics *out) {
    if (!m || !out)
        return TT_INVALID;
    *out = m->diagnostics;
#ifdef TT_PROFILE
    out->profiling = 1;
#endif
    return TT_OK;
}
tt_status tt_model_reset_diagnostics(tt_model *m) {
    if (!m)
        return TT_INVALID;
    memset(&m->diagnostics, 0, sizeof m->diagnostics);
    return TT_OK;
}
const char *tt_stage_name(tt_stage stage) {
    static const char *const names[] = {"geometry",      "gas",
                                        "oxygen_depth",  "transfer",
                                        "flux",          "spectra_direct",
                                        "spectra_yield", "spectra_parameterized",
                                        "oxygen_cross",  "photolysis",
                                        "heating",       "dose",
                                        "mix",           "slant",
                                        "coefficients",  "solve",
                                        "output"};
    return stage >= 0 && stage < TT_STAGE_COUNT ? names[stage] : NULL;
}
