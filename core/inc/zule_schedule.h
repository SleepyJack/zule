#ifndef ZULE_SCHEDULE_H
#define ZULE_SCHEDULE_H

#include <stddef.h>
#include <stdint.h>

typedef void (*zule_task_function_t)(void);

typedef struct {
    zule_task_function_t task_function;
    uint32_t start_slot;
    uint32_t budget_slots;
} zule_window_t;

typedef struct {
    uint32_t cycle_slots;
    const zule_window_t *windows;
    size_t window_count;
} zule_schedule_t;

/* Generated configuration must remain alive and immutable throughout use.
 * It must have nonzero cycle/window counts, non-null windows and task pointers,
 * and ordered, nonoverlapping windows contained within the cycle. */
const zule_schedule_t *zule_schedule_get(void);

#endif
