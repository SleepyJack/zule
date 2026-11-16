#ifndef ZULE_HOOKS_H
#define ZULE_HOOKS_H

#include "zule_config.h"

#include <stddef.h>
#include <stdint.h>

/* User hooks. Each is enabled by its ZULE_CONFIG_USER_HOOK_... setting and then
 * must be defined by the application. Enabled modules receive the same events
 * first. Every hook receives time_counts, a timebase count (see
 * zule_timebase_hz()) read once per event, so timestamps never decrease in call
 * order.
 *
 * Hooks never run concurrently with each other or with the tick: tick_ hooks
 * run in tick context, and the others run in application context inside the
 * scheduler's critical section. They must be bounded and nonblocking, and must
 * not call Zule APIs or port functions; their run time delays the tick.
 *
 * Within each tick, tick_start comes first and tick_end last. A window's
 * tick_task_release comes before its task_start, which comes before its
 * task_end; ticks can occur between task_start and task_end. A task that is
 * released but never started was missed, and a fault follows. Hooks continue
 * for ticks after a fault. Task IDs are task indexes, as in zule_task_ids.h. */

#if ZULE_CONFIG_USER_HOOK_TICK_START
/* Tick context, before the tick processes slot (in cycle cycle_count). */
void zule_user_hook_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count);
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_END
/* Tick context, after the tick's processing. */
void zule_user_hook_tick_end(uint32_t time_counts);
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
/* Tick context, when a window starts and its task becomes pending. */
void zule_user_hook_tick_task_release(uint32_t time_counts, uint32_t task_id, size_t window_index);
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_START
/* Application context, immediately before the task function is called. */
void zule_user_hook_task_start(uint32_t time_counts, uint32_t task_id, size_t window_index);
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_END
/* Application context, immediately after the task function returns, including
 * after an execution overrun. */
void zule_user_hook_task_end(uint32_t time_counts, uint32_t task_id, size_t window_index);
#endif

#endif
