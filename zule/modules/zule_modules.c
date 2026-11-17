#include "zule_modules.h"

#include "tick_timing/zule_tick_timing_hooks.h"

/* Module entry points, declared in the core's internal zule_modules.h. Each
 * calls the enabled modules that consume it, in a fixed order. Compiled into the
 * Zule library, with the core, so modules can use the core API and internal
 * helpers without a link cycle. */

#if ZULE_MODULES_ANY
/* Initialize each enabled module in order, stopping at the first failure: the
 * first module's result is assigned directly, and each later module runs only if
 * the result so far is OK. */
zule_result_t zule_modules_init(const zule_schedule_t *schedule) {
    zule_result_t result = ZULE_RESULT_OK;
#if ZULE_CONFIG_MODULE_TICK_TIMING
    result = zule_tick_timing_init(schedule);
#endif
    return result;
}
#endif

#if ZULE_MODULES_HOOK_TICK_START
void zule_modules_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
    (void)slot;
    (void)cycle_count;
#if ZULE_CONFIG_MODULE_TICK_TIMING
    zule_tick_timing_tick_start(time_counts);
#endif
}
#endif

#if ZULE_MODULES_HOOK_TICK_END
void zule_modules_tick_end(uint32_t time_counts) {
#if ZULE_CONFIG_MODULE_TICK_TIMING
    zule_tick_timing_tick_end(time_counts);
#endif
}
#endif
