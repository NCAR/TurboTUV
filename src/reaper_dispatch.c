#include "turbotuv_reaper.h"
#ifndef TT_REAPER_BUILTINS
const tt_reaper_kernels *const *tt_reaper_builtin_tables(size_t *count) {
    *count = 0;
    return NULL;
}
#endif
