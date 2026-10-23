#ifndef TT_DIAGNOSTICS_INTERNAL_H
#define TT_DIAGNOSTICS_INTERNAL_H
#include "turbotuv_diagnostics.h"
#ifdef TT_PROFILE
#include <time.h>
static inline double tt_profile_now(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + 1.e-9 * (double)t.tv_nsec;
}
#define TT_TIME_START(name) double name = tt_profile_now()
#define TT_TIME_END(diag, stage, name)                                                             \
    do {                                                                                           \
        if (diag) {                                                                                \
            (diag)->seconds[stage] += tt_profile_now() - (name);                                   \
            ++(diag)->calls[stage];                                                                \
        }                                                                                          \
    } while (0)
#else
#define TT_TIME_START(name) ((void)0)
#define TT_TIME_END(diag, stage, name) ((void)(diag))
#endif
#endif
