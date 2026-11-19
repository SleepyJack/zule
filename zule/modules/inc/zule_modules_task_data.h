#ifndef ZULE_MODULES_TASK_DATA_H
#define ZULE_MODULES_TASK_DATA_H

/* Internal: per-task storage for modules, the contract between the modules layer
 * and the generated schedule. The generated schedule defines the storage, with
 * one entry per task, so its size always matches the schedule; the modules own
 * its contents. Visible only to the Zule library, its tests, and generated
 * schedules.
 *
 * Each entry combines what every enabled module keeps for a task. It exists
 * only when ZULE_MODULES_TASK_DATA, derived in zule_modules_config.h. A module
 * that keeps per-task data adds a member here, and its setting to
 * ZULE_MODULES_TASK_DATA. */
#include "zule.h"
#include "zule_task_stats.h"

#if ZULE_MODULES_TASK_DATA

typedef struct {
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_t task_stats;
#endif
} zule_modules_task_data_t;

/* Defined by the generated schedule: the storage, indexed by task ID, with one
 * entry for each of the schedule's tasks. Static and zero-initialized, so it is
 * valid at any time, including before the first run. */
zule_modules_task_data_t *zule_modules_task_data_get(void);

#endif

#endif
