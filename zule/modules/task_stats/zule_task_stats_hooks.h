#ifndef ZULE_TASK_STATS_HOOKS_H
#define ZULE_TASK_STATS_HOOKS_H

/* Internal: the task statistics module's handlers, called by the module entry
 * points in zule_modules.c with the guarantees of the hooks they serve. */
#include "zule_task_stats.h"

#include <stdint.h>

#if ZULE_CONFIG_MODULE_TASK_STATS
/* Start a run: clear the run's measurement state. Cannot fail, and keeps
 * accumulated results. */
void zule_task_stats_init(void);
void zule_task_stats_tick_start(uint32_t cycle_count);
void zule_task_stats_tick_task_release(uint32_t time_counts);
void zule_task_stats_task_start(uint32_t time_counts, uint32_t task_id);
void zule_task_stats_task_end(uint32_t time_counts, uint32_t task_id);
#endif

#endif
