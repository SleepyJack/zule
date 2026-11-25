#ifndef ZULE_MODULES_CONFIG_H
#define ZULE_MODULES_CONFIG_H

/* Module settings, included by zule_config.h after the core settings. Each
 * module is enabled by a ZULE_CONFIG_MODULE_... setting, defaulting to 0.
 *
 * The derived conditions below are what the core reads: whether to initialize
 * modules, and whether to call the module entry point for each hook. The core
 * never names a module. A module adds its setting to ZULE_MODULES_ANY and to the
 * condition of each hook it consumes, to ZULE_MODULES_TASK_DATA if it keeps
 * data for each task, and to ZULE_MODULES_SCHEDULE_DATA if it keeps data for
 * each schedule. */

/* Tick timing: worst tick deviations and longest tick handler; see
 * zule_tick_timing.h. */
#ifndef ZULE_CONFIG_MODULE_TICK_TIMING
#define ZULE_CONFIG_MODULE_TICK_TIMING 0
#endif

#if ZULE_CONFIG_MODULE_TICK_TIMING != 0 && ZULE_CONFIG_MODULE_TICK_TIMING != 1
#error "ZULE_CONFIG_MODULE_TICK_TIMING must be 0 or 1"
#endif

/* Task statistics: execution times and dispatch latency for each task, and
 * totals over all tasks; see zule_task_stats.h. */
#ifndef ZULE_CONFIG_MODULE_TASK_STATS
#define ZULE_CONFIG_MODULE_TASK_STATS 0
#endif

#if ZULE_CONFIG_MODULE_TASK_STATS != 0 && ZULE_CONFIG_MODULE_TASK_STATS != 1
#error "ZULE_CONFIG_MODULE_TASK_STATS must be 0 or 1"
#endif

/* 1 when any module is enabled; the core then calls zule_modules_init(). */
#define ZULE_MODULES_ANY (ZULE_CONFIG_MODULE_TICK_TIMING || ZULE_CONFIG_MODULE_TASK_STATS)

/* For each hook, 1 when any enabled module consumes it. */
#define ZULE_MODULES_HOOK_TICK_START                                                               \
    (ZULE_CONFIG_MODULE_TICK_TIMING || ZULE_CONFIG_MODULE_TASK_STATS)
#define ZULE_MODULES_HOOK_TICK_END (ZULE_CONFIG_MODULE_TICK_TIMING)
#define ZULE_MODULES_HOOK_TICK_TASK_RELEASE (ZULE_CONFIG_MODULE_TASK_STATS)
#define ZULE_MODULES_HOOK_TICK_SCHEDULE_SWITCH (ZULE_CONFIG_MODULE_TASK_STATS)
#define ZULE_MODULES_HOOK_TASK_START (ZULE_CONFIG_MODULE_TASK_STATS)
#define ZULE_MODULES_HOOK_TASK_END (ZULE_CONFIG_MODULE_TASK_STATS)

/* 1 when any enabled module keeps data for each task. The generated schedule
 * then defines that storage, with one entry per task; see
 * zule_modules_task_data.h. */
#define ZULE_MODULES_TASK_DATA (ZULE_CONFIG_MODULE_TASK_STATS)

/* 1 when any enabled module keeps data for each schedule. The generated schedule
 * then defines that storage, with one entry per schedule; see
 * zule_modules_schedule_data.h. */
#define ZULE_MODULES_SCHEDULE_DATA (ZULE_CONFIG_MODULE_TASK_STATS)

/* Modules receive timestamps, so they require the timebase. */
#if ZULE_MODULES_ANY && !ZULE_CONFIG_TIMEBASE
#error "Enabled modules require ZULE_CONFIG_TIMEBASE"
#endif

#endif
