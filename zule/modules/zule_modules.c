#include "zule_module_hooks.h"

#include "tick_timing/zule_tick_timing_hooks.h"

/* Module entry points, declared in the core's internal zule_module_hooks.h. Each
 * calls the enabled modules that consume it, in a fixed order. Compiled into the
 * core library, with the modules, so modules can use the core API and internal
 * helpers without a link cycle. */

#if ZULE_MODULES_ANY
zule_result_t zule_modules_init(const zule_schedule_t *schedule) {
    zule_result_t result = ZULE_RESULT_OK;
#if ZULE_CONFIG_MODULE_TICK_TIMING
    if (result == ZULE_RESULT_OK) {
        result = zule_tick_timing_init(schedule);
    }
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
