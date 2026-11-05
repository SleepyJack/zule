#ifndef ZULE_SCHEDULE_H
#define ZULE_SCHEDULE_H

#include "zule.h"

#include <stddef.h>
#include <stdint.h>

typedef void (*zule_task_function_t)(void);

typedef struct {
    zule_task_function_t task_function;
    uint32_t start_slot;
    uint32_t budget_slots;
} zule_window_t;

typedef struct {
    uint32_t slot_us;
    uint32_t cycle_slots;
    const zule_window_t *windows;
    size_t window_count;
} zule_schedule_t;

/* Return OK for a nonempty schedule with nonzero slot period and cycle length, callable tasks,
 * positive budgets, and ordered, nonoverlapping windows within the cycle.
 * Otherwise return INVALID_SCHEDULE, including for null schedule/windows.
 * Non-null pointers must address readable storage for the declared array size;
 * validation cannot establish storage validity or task execution-time bounds.
 * The configuration must remain unchanged during validation and subsequent use. */
zule_result_t zule_schedule_validate(const zule_schedule_t *schedule);

#endif
