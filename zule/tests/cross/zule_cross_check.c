/* Link check for cross-compiled builds; see CMakeLists.txt. Provides the task
 * the schedule names, the user hooks when enabled, and an entry point that
 * reaches the public API, so every enabled part of Zule is linked. */
#include "zule.h"

#if ZULE_CONFIG_MODULE_TICK_TIMING
#include "zule_tick_timing.h"
#endif
#if ZULE_CONFIG_MODULE_TASK_STATS
#include "zule_task_stats.h"
#endif

#include <stddef.h>
#include <stdint.h>

/* Written by the task and hooks, so their bodies are not optimised away. */
static volatile uint32_t sink;

void zule_task_check(void) {
#if ZULE_CONFIG_MODULE_TICK_TIMING
    zule_tick_timing_t timing;
    zule_tick_timing_get(&timing);
    sink = timing.ticks;
    zule_tick_timing_reset();
#endif
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_t stats;
    if (zule_task_stats_get(0U, &stats)) {
        sink = stats.max_execution_counts;
    }
    zule_task_stats_summary_t summary;
    zule_task_stats_summary_get(&summary);
    sink = summary.max_cycle_busy_counts;
    zule_task_stats_reset();
#endif
    ++sink;
}

#if ZULE_CONFIG_USER_HOOK_TICK_START
void zule_user_hook_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
    sink = time_counts + slot + cycle_count;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_END
void zule_user_hook_tick_end(uint32_t time_counts) {
    sink = time_counts;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
void zule_user_hook_tick_task_release(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    sink = time_counts + task_id + (uint32_t)window_index;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_START
void zule_user_hook_task_start(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    sink = time_counts + task_id + (uint32_t)window_index;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_END
void zule_user_hook_task_end(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    sink = time_counts + task_id + (uint32_t)window_index;
}
#endif

static void handle_fault(const zule_fault_info_t *fault) {
    sink = (uint32_t)fault->reason;
}

/* The linker's default entry point; there is no C runtime to call main(). */
void _start(void);

void _start(void) {
    const zule_config_t config = {.on_fault = handle_fault};
    (void)zule_configure(&config);
    sink = (zule_task_name_get(0U) != NULL) ? 1U : 0U;
#if ZULE_CONFIG_TIMEBASE
    sink = zule_timebase_hz();
#endif
    (void)zule_run();
    for (;;) {
    }
}
