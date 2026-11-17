#ifndef ZULE_MODULES_CONFIG_H
#define ZULE_MODULES_CONFIG_H

/* Module settings, included by zule_config.h after the core settings. Each
 * module is enabled by a ZULE_CONFIG_MODULE_... setting, defaulting to 0.
 *
 * The derived conditions below are what the core reads: whether to initialize
 * modules, and whether to call the module entry point for each hook. The core
 * never names a module. A module adds its setting to ZULE_MODULES_ANY and to the
 * condition of each hook it consumes. */

/* Tick timing: worst tick deviations and longest tick handler; see
 * zule_tick_timing.h. */
#ifndef ZULE_CONFIG_MODULE_TICK_TIMING
#define ZULE_CONFIG_MODULE_TICK_TIMING 0
#endif

#if ZULE_CONFIG_MODULE_TICK_TIMING != 0 && ZULE_CONFIG_MODULE_TICK_TIMING != 1
#error "ZULE_CONFIG_MODULE_TICK_TIMING must be 0 or 1"
#endif

/* 1 when any module is enabled; the core then calls zule_modules_init(). */
#define ZULE_MODULES_ANY (ZULE_CONFIG_MODULE_TICK_TIMING)

/* For each hook, 1 when any enabled module consumes it. */
#define ZULE_MODULES_HOOK_TICK_START (ZULE_CONFIG_MODULE_TICK_TIMING)
#define ZULE_MODULES_HOOK_TICK_END (ZULE_CONFIG_MODULE_TICK_TIMING)
#define ZULE_MODULES_HOOK_TICK_TASK_RELEASE 0
#define ZULE_MODULES_HOOK_TASK_START 0
#define ZULE_MODULES_HOOK_TASK_END 0

/* Modules receive timestamps, so they require the timebase. */
#if ZULE_MODULES_ANY && !ZULE_CONFIG_TIMEBASE
#error "Enabled modules require ZULE_CONFIG_TIMEBASE"
#endif

#endif
