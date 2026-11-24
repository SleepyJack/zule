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

/* The application's tasks, indexed by task ID. */
typedef struct {
    const zule_task_t *entries;
    size_t count;
} zule_task_table_t;

/* One schedule: a repeating cycle of windows over the task table. */
typedef struct {
    uint32_t cycle_slots;
    const zule_window_t *windows;
    size_t window_count;
} zule_schedule_t;

/* The generated schedule definition: the slot length and task table, which
 * every schedule shares, so the tick and task IDs mean the same in each, and the
 * schedules, indexed by schedule ID as the generated zule_schedule_ids.h numbers
 * them. */
typedef struct {
    uint32_t slot_us;
    zule_task_table_t tasks;
    const zule_schedule_t *schedules;
    size_t schedule_count;
} zule_schedule_definition_t;

/* Return OK for a nonzero slot period, a nonempty task table of callable tasks
 * whose IDs equal their index, and at least one schedule, each with a nonzero
 * cycle length and windows that reference entries in the task table, have
 * positive budgets, and are ordered, nonoverlapping, and within the cycle.
 * Otherwise return INVALID_SCHEDULE, including for a null definition, task
 * table, schedules, or windows. Non-null pointers must address readable storage
 * for the declared array size; task names, when present and non-null, must point
 * to valid immutable strings. Validation cannot establish storage validity or
 * task execution-time bounds. The definition must remain unchanged during
 * validation and subsequent use. */
zule_result_t zule_schedule_definition_validate(const zule_schedule_definition_t *definition);

#endif
