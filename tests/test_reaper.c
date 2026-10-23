/* Exercise REAPER against the whole catalogue under its separate roundoff
 * contract. The original prepared executable retains every memcmp gate. */
#include "model_plan.h"
#include "spectrum_plan.h"
#define TT_TEST_REAPER 1
#define TT_TEST_REAPER_ROUNDOFF 1
#define TT_EXECUTION_PREPARED TT_EXECUTION_REAPER
#define tt_spectrum_plan_evaluate tt_spectrum_reaper_evaluate
#include "test_prepared.c"
