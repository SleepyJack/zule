#ifndef ZULE_H
#define ZULE_H

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

typedef enum {
    ZULE_TASK_IDLE,
    ZULE_TASK_PENDING,
    ZULE_TASK_RUNNING,
    ZULE_TASK_COMPLETED,
} zule_task_state_t;

typedef enum {
    ZULE_TASK_FAULT_NONE,
    ZULE_TASK_FAULT_NOT_DISPATCHED,
    ZULE_TASK_FAULT_EXECUTION_OVERRUN,
} zule_task_fault_t;

typedef struct {
    zule_task_function_t function;
    zule_task_state_t state;
    zule_task_fault_t fault;
} zule_task_t;

typedef struct {
    const zule_schedule_t *schedule;
    uint32_t slot;
    uint32_t active_window_ind;
    zule_task_t task;
} zule_scheduler_t;

zule_scheduler_t * zule_init(void);

void zule_tick(zule_scheduler_t * const scheduler);

void zule_run(zule_scheduler_t * const scheduler);

#endif
