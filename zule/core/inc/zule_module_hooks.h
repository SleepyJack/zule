#ifndef ZULE_MODULE_HOOKS_H
#define ZULE_MODULE_HOOKS_H

/* Internal: the core's hook contract for modules. Visible only within the core
 * library, which also builds the modules; applications cannot include it.
 *
 * Each entry point is declared when an enabled module consumes its hook, as
 * derived in zule_modules_config.h, and is defined by the modules layer in
 * zule_modules.c. Entry points take the same arguments and give the same
 * guarantees as the user hooks in zule_hooks.h, and are called first. */
#include "zule.h"
#include "zule_schedule.h"

#include <stddef.h>
#include <stdint.h>

#if ZULE_MODULES_ANY
/* Called on each run from scheduler initialization, in application context
 * before ticks start, after the schedule and the core's own checks pass. Each
 * enabled module validates and prepares, and the first failure is returned,
 * normally INVALID_CONFIG. Modules keep accumulated results unless they
 * document otherwise. */
zule_result_t zule_modules_init(const zule_schedule_t *schedule);
#endif

#if ZULE_MODULES_HOOK_TICK_START
void zule_modules_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count);
#endif

#if ZULE_MODULES_HOOK_TICK_END
void zule_modules_tick_end(uint32_t time_counts);
#endif

#if ZULE_MODULES_HOOK_TICK_TASK_RELEASE
void zule_modules_tick_task_release(uint32_t time_counts, uint32_t task_id, size_t window_index);
#endif

#if ZULE_MODULES_HOOK_TASK_START
void zule_modules_task_start(uint32_t time_counts, uint32_t task_id, size_t window_index);
#endif

#if ZULE_MODULES_HOOK_TASK_END
void zule_modules_task_end(uint32_t time_counts, uint32_t task_id, size_t window_index);
#endif

#endif
