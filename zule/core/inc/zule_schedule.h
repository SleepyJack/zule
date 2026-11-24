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

typedef struct {
    uint32_t slot_us;
    uint32_t cycle_slots;
    const zule_window_t *windows;
    size_t window_count;
    const zule_task_t *tasks;
    size_t task_count;
} zule_schedule_t;

/* An application's schedules, indexed by schedule ID, as the generated
 * zule_schedule_ids.h numbers them. Every schedule shares one slot length and one
 * task table, so tasks and task IDs mean the same in each. */
typedef struct {
    const zule_schedule_t *schedules;
    size_t schedule_count;
} zule_schedule_set_t;

/* Return OK for a nonempty schedule with nonzero slot period and cycle length, callable tasks
 * whose IDs equal their task array index, windows referencing entries in the task array,
 * positive budgets, and ordered, nonoverlapping windows within the cycle.
 * Otherwise return INVALID_SCHEDULE, including for null schedule/windows/tasks.
 * Non-null pointers must address readable storage for the declared array size;
 * Task names, when present and non-null, must point to valid immutable strings.
 * Validation cannot establish storage validity or task execution-time bounds.
 * The configuration must remain unchanged during validation and subsequent use. */
zule_result_t zule_schedule_validate(const zule_schedule_t *schedule);

/* Return OK for a set of at least one schedule, each of which passes
 * zule_schedule_validate(), that all share the first schedule's slot length and
 * task table. Otherwise, including for a null set or schedules array, return
 * INVALID_SCHEDULE. */
zule_result_t zule_schedule_set_validate(const zule_schedule_set_t *set);

#endif
