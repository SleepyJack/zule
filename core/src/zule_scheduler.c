#include "zule_scheduler.h"
#include "zule_port.h"
#include "zule_schedule_definition.h"

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
    uint32_t cycle_count;
    size_t active_window_ind;
    zule_task_t task;
    bool initialized;
    zule_fault_handler_t on_fault;
} zule_scheduler_t;

static zule_scheduler_t scheduler;

zule_result_t zule_scheduler_init(zule_fault_handler_t on_fault) {
    scheduler.initialized = false;
    scheduler.schedule = zule_schedule_get();
    const zule_result_t result = zule_schedule_validate(scheduler.schedule);
    if (result != ZULE_RESULT_OK) {
        return result;
    }
    scheduler.slot = 0U;
    scheduler.cycle_count = 0U;
    scheduler.active_window_ind = 0U;
    scheduler.task.function = NULL;
    scheduler.task.state = ZULE_TASK_IDLE;
    scheduler.task.fault = ZULE_RESULT_OK;
    scheduler.on_fault = on_fault;
    scheduler.initialized = true;
    return ZULE_RESULT_OK;
}

static void latch_fault(zule_result_t reason) {
    if (scheduler.task.fault != ZULE_RESULT_OK) {
        return;
    }
    scheduler.task.fault = reason;
    if (scheduler.on_fault != NULL) {
        const zule_fault_info_t fault = {
            .reason = reason,
            .window_index = scheduler.active_window_ind,
            .slot = scheduler.slot,
            .cycle_count = scheduler.cycle_count,
        };
        scheduler.on_fault(&fault);
    }
}

zule_result_t zule_scheduler_tick(void) {
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
            latch_fault(ZULE_RESULT_DISPATCH_MISSED);
        } else if (scheduler.task.state == ZULE_TASK_RUNNING) {
            latch_fault(ZULE_RESULT_EXECUTION_OVERRUN);
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
        if (scheduler.slot == 0U) {
            ++scheduler.cycle_count;
        }
    }
    return scheduler.task.fault;
}

zule_result_t zule_scheduler_run(void) {
    zule_result_t result = ZULE_RESULT_OK;
    if (!scheduler.initialized) {
        result = ZULE_RESULT_NOT_INITIALIZED;
    } else {
        zule_port_critical_state_t critical_state = zule_port_critical_enter();
        // Dispatch the active task if it is pending and there is no active fault
        if ((scheduler.task.fault == ZULE_RESULT_OK) &&
            (scheduler.task.state == ZULE_TASK_PENDING)) {
            scheduler.task.state = ZULE_TASK_RUNNING;
            zule_task_function_t task_function = scheduler.task.function;
            zule_port_critical_exit(critical_state);
            task_function();
            critical_state = zule_port_critical_enter();
            if (scheduler.task.state == ZULE_TASK_RUNNING) {
                scheduler.task.state = ZULE_TASK_COMPLETED;
            }
        }
        result = scheduler.task.fault;
        zule_port_critical_exit(critical_state);
    }
    return result;
}
