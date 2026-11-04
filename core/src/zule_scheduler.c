#include "zule.h"
#include "zule_schedule.h"

#include <stdbool.h>

typedef enum {
    ZULE_TASK_IDLE,
    ZULE_TASK_PENDING,
    ZULE_TASK_RUNNING,
    ZULE_TASK_COMPLETED,
} zule_task_state_t;

typedef struct {
    zule_task_function_t function;
    zule_task_state_t state;
    zule_result_t fault;
} zule_task_t;

typedef struct {
    const zule_schedule_t *schedule;
    uint32_t slot;
    size_t active_window_ind;
    zule_task_t task;
    bool initialized;
} zule_scheduler_t;

static zule_scheduler_t scheduler;

zule_result_t zule_init(void) {
    scheduler.initialized = false;
    scheduler.schedule = zule_schedule_get();
    // TODO: Validate configuration before enabling operation; return
    // ZULE_RESULT_INVALID_SCHEDULE on failure, leaving initialized false.
    scheduler.slot = 0U;
    scheduler.active_window_ind = 0U;
    scheduler.task.function = NULL;
    scheduler.task.state = ZULE_TASK_IDLE;
    scheduler.task.fault = ZULE_RESULT_OK;
    scheduler.initialized = true;
    return ZULE_RESULT_OK;
}

zule_result_t zule_tick(void) {
    if (!scheduler.initialized) {
        return ZULE_RESULT_NOT_INITIALIZED;
    }
    const zule_window_t *active_window = &scheduler.schedule->windows[scheduler.active_window_ind];

    // Check for end of the current active window
    const uint32_t active_window_end_slot =
        (active_window->start_slot + active_window->budget_slots) % scheduler.schedule->cycle_slots;
    if (scheduler.slot == active_window_end_slot) {
        if (scheduler.task.state == ZULE_TASK_COMPLETED) {
            scheduler.task.state = ZULE_TASK_IDLE;
            // TODO: Consider fault recovery here
        } else if (scheduler.task.state == ZULE_TASK_PENDING) {
            scheduler.task.fault = ZULE_RESULT_DISPATCH_MISSED;
            // TODO: Critical error handling
        } else if (scheduler.task.state == ZULE_TASK_RUNNING) {
            // Preserve a missed-dispatch fault if the task eventually starts.
            scheduler.task.fault = (scheduler.task.fault == ZULE_RESULT_OK)
                                       ? ZULE_RESULT_EXECUTION_OVERRUN
                                       : scheduler.task.fault;
            // TODO: Critical error handling
        } else {
            // Idle - do nothing
        }

        // Prevent window advance in case of active faults
        if (scheduler.task.fault == ZULE_RESULT_OK) {
            scheduler.active_window_ind =
                (scheduler.active_window_ind + 1u) % scheduler.schedule->window_count;
            active_window = &scheduler.schedule->windows[scheduler.active_window_ind];
        }
    }

    // Halt the scheduler in case of active faults
    if (scheduler.task.fault == ZULE_RESULT_OK) {

        // Check for start of the new active window
        if (scheduler.slot == active_window->start_slot) {
            if (scheduler.task.state == ZULE_TASK_IDLE) {
                scheduler.task.function = active_window->task_function;
                scheduler.task.state = ZULE_TASK_PENDING;
            } else {
                // Handle overrun
            }
        }

        // Advance the slot
        scheduler.slot = (scheduler.slot + 1u) % scheduler.schedule->cycle_slots;
    }
    return scheduler.task.fault;
}

zule_result_t zule_run(void) {
    if (!scheduler.initialized) {
        return ZULE_RESULT_NOT_INITIALIZED;
    }
    // Dispatch the active task if it is pending and there is no active fault
    if ((scheduler.task.fault == ZULE_RESULT_OK) && (scheduler.task.state == ZULE_TASK_PENDING)) {
        scheduler.task.state = ZULE_TASK_RUNNING;
        scheduler.task.function();
        if (scheduler.task.state == ZULE_TASK_RUNNING) {
            scheduler.task.state = ZULE_TASK_COMPLETED;
        }
    }
    return scheduler.task.fault;
}
