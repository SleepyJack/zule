#ifndef ZULE_MODULES_SCHEDULE_DATA_H
#define ZULE_MODULES_SCHEDULE_DATA_H

/* Internal: per-schedule storage for modules, the contract between the modules
 * layer and the generated schedule, like the per-task storage in
 * zule_modules_task_data.h. The generated schedule defines the storage, with one
 * entry per schedule; the modules own its contents.
 *
 * Each entry combines what every enabled module keeps for a schedule. It exists
 * only when ZULE_MODULES_SCHEDULE_DATA, derived in zule_modules_config.h. A
 * module that keeps per-schedule data adds a member here, and its setting to
 * ZULE_MODULES_SCHEDULE_DATA. */
#include "zule.h"
#include "zule_task_stats.h"

#if ZULE_MODULES_SCHEDULE_DATA

typedef struct {
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_summary_t task_stats_summary;
#endif
} zule_modules_schedule_data_t;

/* Defined by the generated schedule: the storage, indexed by schedule ID, with
 * one entry for each schedule. Static and zero-initialized, so it is valid at
 * any time, including before the first run. */
zule_modules_schedule_data_t *zule_modules_schedule_data_get(void);

#endif

#endif
