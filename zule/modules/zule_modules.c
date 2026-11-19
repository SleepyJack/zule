#include "zule_modules.h"

#include "task_stats/zule_task_stats_hooks.h"
#include "tick_timing/zule_tick_timing_hooks.h"

/* Module entry points, declared in the core's internal zule_modules.h. Each
 * calls the enabled modules that consume it, in a fixed order. Compiled into the
 * Zule library, with the core, so modules can use the core API and internal
 * helpers without a link cycle. */

#if ZULE_MODULES_ANY
/* Initialize each enabled module, returning the first failure. Modules whose
 * init cannot fail come first. Of the others, the first one's result is assigned
 * directly, and each later one runs only if the result so far is OK. */
zule_result_t zule_modules_init(const zule_schedule_t *schedule) {
    zule_result_t result = ZULE_RESULT_OK;
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_init();
#endif
#if ZULE_CONFIG_MODULE_TICK_TIMING
    result = zule_tick_timing_init(schedule);
#else
    (void)schedule;
#endif
    return result;
}
#endif

#if ZULE_MODULES_HOOK_TICK_START
void zule_modules_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
    (void)slot;
#if ZULE_CONFIG_MODULE_TICK_TIMING
    zule_tick_timing_tick_start(time_counts);
#else
    (void)time_counts;
#endif
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_tick_start(cycle_count);
#else
    (void)cycle_count;
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

#if ZULE_MODULES_HOOK_TICK_TASK_RELEASE
void zule_modules_tick_task_release(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)task_id;
    (void)window_index;
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_tick_task_release(time_counts);
#endif
}
#endif

#if ZULE_MODULES_HOOK_TASK_START
void zule_modules_task_start(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)window_index;
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_task_start(time_counts, task_id);
#endif
}
#endif

#if ZULE_MODULES_HOOK_TASK_END
void zule_modules_task_end(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)window_index;
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_task_end(time_counts, task_id);
#endif
}
#endif
