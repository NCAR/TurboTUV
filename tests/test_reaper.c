/* R1 is ordered arithmetic: reuse the entire exact catalogue as a separate
 * executor test. Keep the original prepared executable and its memcmp gates. */
#include "model_plan.h"
#include "spectrum_plan.h"
#define TT_TEST_REAPER 1
#define TT_EXECUTION_PREPARED TT_EXECUTION_REAPER
#define tt_spectrum_plan_evaluate tt_spectrum_reaper_evaluate
#include "test_prepared.c"
