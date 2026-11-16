#ifndef ZULE_HOOK_POINTS_H
#define ZULE_HOOK_POINTS_H

/* Internal. The core calls these wrappers unconditionally at each hook point. A
 * wrapper reads the timebase once, then calls the modules entry point and the
 * user hook, in that order. An unused hook compiles to nothing, including the
 * time read, which is why time is read here rather than passed in. Callers
 * provide the hook's context: tick context, or the scheduler's critical
 * section. */
#include "zule.h"
#include "zule_port.h"

#include <stddef.h>
#include <stdint.h>

static inline void zule_hook_tick_start(uint32_t slot, uint32_t cycle_count) {
#if ZULE_MODULES_HOOK_TICK_START || ZULE_CONFIG_USER_HOOK_TICK_START
    const uint32_t time_counts = zule_port_time_get();
#if ZULE_MODULES_HOOK_TICK_START
    zule_modules_tick_start(time_counts, slot, cycle_count);
#endif
#if ZULE_CONFIG_USER_HOOK_TICK_START
    zule_user_hook_tick_start(time_counts, slot, cycle_count);
#endif
#else
    (void)slot;
    (void)cycle_count;
#endif
}

static inline void zule_hook_tick_end(void) {
#if ZULE_MODULES_HOOK_TICK_END || ZULE_CONFIG_USER_HOOK_TICK_END
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
#if ZULE_MODULES_HOOK_TICK_TASK_RELEASE || ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
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
#if ZULE_MODULES_HOOK_TASK_START || ZULE_CONFIG_USER_HOOK_TASK_START
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
#if ZULE_MODULES_HOOK_TASK_END || ZULE_CONFIG_USER_HOOK_TASK_END
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
