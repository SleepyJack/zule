#ifndef ZULE_SCHEDULE_H
#define ZULE_SCHEDULE_H

#include "zule.h"

#include <stddef.h>
#include <stdint.h>

typedef void (*zule_task_function_t)(void);

typedef struct {
    uint32_t id;
    zule_task_function_t function;
#if ZULE_CONFIG_TASK_NAMES
    const char *name;
#endif
} zule_task_t;

typedef struct {
    const zule_task_t *task;
    uint32_t start_slot;
    uint32_t budget_slots;
} zule_window_t;

/* One schedule: a repeating cycle of windows over the shared task table. */
typedef struct {
    uint32_t cycle_slots;
    const zule_window_t *windows;
    size_t window_count;
} zule_schedule_t;

/* An application's schedules, indexed by schedule ID as the generated
 * zule_schedule_ids.h numbers them, with the slot length and task table they
 * share, so the tick and task IDs mean the same in each. */
typedef struct {
    uint32_t slot_us;
    const zule_task_t *tasks;
    size_t task_count;
    const zule_schedule_t *schedules;
    size_t schedule_count;
} zule_schedules_t;

/* Return OK for a nonzero slot period, a nonempty task table of callable tasks
 * whose IDs equal their task array index, and at least one schedule, each with a
 * nonzero cycle length and windows that reference entries in the task array,
 * have positive budgets, and are ordered, nonoverlapping, and within the cycle.
 * Otherwise return INVALID_SCHEDULE, including for null schedules, windows, or
 * tasks. Non-null pointers must address readable storage for the declared array
 * size; task names, when present and non-null, must point to valid immutable
 * strings. Validation cannot establish storage validity or task execution-time
 * bounds. The schedules must remain unchanged during validation and subsequent
 * use. */
zule_result_t zule_schedules_validate(const zule_schedules_t *schedules);

#endif
