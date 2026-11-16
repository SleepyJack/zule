#ifndef ZULE_MODULES_H
#define ZULE_MODULES_H

/* Zule modules: optional hook consumers shipped with Zule, each enabled by a
 * ZULE_CONFIG_MODULE_... setting. The core includes this header and knows
 * modules only through the conditions and entry points below; it never names an
 * individual module. No modules exist yet. */
#include "zule.h"

#include <stddef.h>
#include <stdint.h>

/* For each hook: 1 when any enabled module consumes it. A module adds its
 * setting to the conditions of the hooks it needs. */
#define ZULE_MODULES_HOOK_TICK_START 0
#define ZULE_MODULES_HOOK_TICK_END 0
#define ZULE_MODULES_HOOK_TICK_TASK_RELEASE 0
#define ZULE_MODULES_HOOK_TASK_START 0
#define ZULE_MODULES_HOOK_TASK_END 0

/* Every hook passes a timestamp, so modules require the timebase. */
#if (ZULE_MODULES_HOOK_TICK_START || ZULE_MODULES_HOOK_TICK_END ||                                 \
     ZULE_MODULES_HOOK_TICK_TASK_RELEASE || ZULE_MODULES_HOOK_TASK_START ||                        \
     ZULE_MODULES_HOOK_TASK_END) &&                                                                \
    !ZULE_CONFIG_TIMEBASE
#error "Enabled modules require ZULE_CONFIG_TIMEBASE"
#endif

/* Entry points, defined in zule_modules.c for hooks that a module consumes. Each
 * calls the enabled modules in a fixed order, with the hook's guarantees (see
 * zule_hooks.h). */
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
