#ifndef ZULE_HOOK_POINTS_H
#define ZULE_HOOK_POINTS_H

/* Internal. The core calls these wrappers unconditionally at each hook point. A
 * wrapper reads the timebase once, then calls the modules entry point and the
 * user hook, in that order. An unused hook compiles to nothing, including the
 * time read, which is why time is read here rather than passed in. Tick start
 * is the exception: the scheduler reads its time, shared with the tick check. Callers
 * provide the hook's context: tick context, or the scheduler's critical
 * section. */
#include "zule.h"
#include "zule_modules.h"
#include "zule_port.h"

#include <stddef.h>
#include <stdint.h>

/* Whether each hook has a consumer, and whether any hook does. */
#define ZULE_HOOK_TICK_START_USED (ZULE_MODULES_HOOK_TICK_START || ZULE_CONFIG_USER_HOOK_TICK_START)
#define ZULE_HOOK_TICK_END_USED (ZULE_MODULES_HOOK_TICK_END || ZULE_CONFIG_USER_HOOK_TICK_END)
#define ZULE_HOOK_TICK_TASK_RELEASE_USED                                                           \
    (ZULE_MODULES_HOOK_TICK_TASK_RELEASE || ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE)
#define ZULE_HOOK_TASK_START_USED (ZULE_MODULES_HOOK_TASK_START || ZULE_CONFIG_USER_HOOK_TASK_START)
#define ZULE_HOOK_TASK_END_USED (ZULE_MODULES_HOOK_TASK_END || ZULE_CONFIG_USER_HOOK_TASK_END)
#define ZULE_HOOKS_USED                                                                            \
    (ZULE_HOOK_TICK_START_USED || ZULE_HOOK_TICK_END_USED || ZULE_HOOK_TICK_TASK_RELEASE_USED ||   \
     ZULE_HOOK_TASK_START_USED || ZULE_HOOK_TASK_END_USED)

/* The scheduler reads the time for tick start itself, so the tick check can share
 * the same read. It calls this wrapper only when ZULE_HOOK_TICK_START_USED, so
 * the wrapper is defined only then. */
#if ZULE_HOOK_TICK_START_USED
static inline void zule_hook_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
#if ZULE_MODULES_HOOK_TICK_START
    zule_modules_tick_start(time_counts, slot, cycle_count);
#endif
#if ZULE_CONFIG_USER_HOOK_TICK_START
    zule_user_hook_tick_start(time_counts, slot, cycle_count);
#endif
}
#endif

static inline void zule_hook_tick_end(void) {
#if ZULE_HOOK_TICK_END_USED
    const uint32_t time_counts = zule_port_time_get();
#if ZULE_MODULES_HOOK_TICK_END
    zule_modules_tick_end(time_counts);
#endif
#if ZULE_CONFIG_USER_HOOK_TICK_END
    zule_user_hook_tick_end(time_counts);
#endif
#endif
}

static inline void zule_hook_tick_task_release(uint32_t task_id, size_t window_index) {
#if ZULE_HOOK_TICK_TASK_RELEASE_USED
    const uint32_t time_counts = zule_port_time_get();
#if ZULE_MODULES_HOOK_TICK_TASK_RELEASE
    zule_modules_tick_task_release(time_counts, task_id, window_index);
#endif
#if ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
    zule_user_hook_tick_task_release(time_counts, task_id, window_index);
#endif
#else
    (void)task_id;
    (void)window_index;
#endif
}

static inline void zule_hook_task_start(uint32_t task_id, size_t window_index) {
#if ZULE_HOOK_TASK_START_USED
    const uint32_t time_counts = zule_port_time_get();
#if ZULE_MODULES_HOOK_TASK_START
    zule_modules_task_start(time_counts, task_id, window_index);
#endif
#if ZULE_CONFIG_USER_HOOK_TASK_START
    zule_user_hook_task_start(time_counts, task_id, window_index);
#endif
#else
    (void)task_id;
    (void)window_index;
#endif
}

static inline void zule_hook_task_end(uint32_t task_id, size_t window_index) {
#if ZULE_HOOK_TASK_END_USED
    const uint32_t time_counts = zule_port_time_get();
#if ZULE_MODULES_HOOK_TASK_END
    zule_modules_task_end(time_counts, task_id, window_index);
#endif
#if ZULE_CONFIG_USER_HOOK_TASK_END
    zule_user_hook_task_end(time_counts, task_id, window_index);
#endif
#else
    (void)task_id;
    (void)window_index;
#endif
}

#endif
