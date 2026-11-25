#ifndef ZULE_MODULES_CONFIG_DEFAULTS_H
#define ZULE_MODULES_CONFIG_DEFAULTS_H

/* The module settings and their defaults. Included by
 * zule_modules_config_apply.h, after any zule_user_config.h; not included on
 * its own. Each module is enabled by a ZULE_CONFIG_MODULE_... setting, defaulting
 * to 0. */

/* Tick timing: worst tick deviations and longest tick handler; see
 * zule_tick_timing.h. */
#ifndef ZULE_CONFIG_MODULE_TICK_TIMING
#define ZULE_CONFIG_MODULE_TICK_TIMING 0
#endif

/* Task statistics: execution times and dispatch latency for each task, and
 * totals for each schedule; see zule_task_stats.h. */
#ifndef ZULE_CONFIG_MODULE_TASK_STATS
#define ZULE_CONFIG_MODULE_TASK_STATS 0
#endif

#endif
