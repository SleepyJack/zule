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
    const zule_task_t *task;
    zule_task_state_t state;
    zule_result_t fault;
} zule_task_execution_t;

typedef struct {
    const zule_schedule_t *schedule;
    uint32_t slot;
    uint32_t cycle_count;
    size_t active_window_ind;
    zule_task_execution_t exec;
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
    scheduler.exec.task = NULL;
    scheduler.exec.state = ZULE_TASK_IDLE;
    scheduler.exec.fault = ZULE_RESULT_OK;
    scheduler.on_fault = on_fault;
    scheduler.initialized = true;
    return ZULE_RESULT_OK;
}

static void raise_fault(zule_result_t reason) {
    if (scheduler.exec.fault != ZULE_RESULT_OK) {
        return;
    }
    scheduler.exec.fault = reason;
    if (scheduler.on_fault != NULL) {
        const zule_fault_info_t fault = {
            .reason = reason,
            .window_index = scheduler.active_window_ind,
            .slot = scheduler.slot,
            .cycle_count = scheduler.cycle_count,
            .task_id = scheduler.exec.task->id,
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
        if (scheduler.exec.state == ZULE_TASK_COMPLETED) {
            // Note that even if task completion is late we will return to Idle. Overrun fault
            // remains latched in that case.
            scheduler.exec.state = ZULE_TASK_IDLE;
        } else if (scheduler.exec.state == ZULE_TASK_PENDING) {
            raise_fault(ZULE_RESULT_DISPATCH_MISSED);
        } else if (scheduler.exec.state == ZULE_TASK_RUNNING) {
            raise_fault(ZULE_RESULT_EXECUTION_OVERRUN);
        } else {
            // Idle - do nothing
        }

        // Prevent window advance in case of active faults
        if (scheduler.exec.fault == ZULE_RESULT_OK) {
            scheduler.active_window_ind =
                (scheduler.active_window_ind + 1u) % scheduler.schedule->window_count;
            active_window = &scheduler.schedule->windows[scheduler.active_window_ind];
        }
    }

    // Prevent new task dispatch in case of active faults
    if (scheduler.exec.fault == ZULE_RESULT_OK) {
        // Check for start of the new active window
        if (scheduler.slot == active_window->start_slot) {
            if (scheduler.exec.state == ZULE_TASK_IDLE) {
                scheduler.exec.task = active_window->task;
                scheduler.exec.state = ZULE_TASK_PENDING;
            } else {
                // This should be unreachable - overrun already detected at end of the previous
                // window and fault raised. Below call to raise_fault is defensive only.
                raise_fault(ZULE_RESULT_EXECUTION_OVERRUN);
            }
        }
    }

    // Halt the scheduler in case of active faults
    if (scheduler.exec.fault == ZULE_RESULT_OK) {
        // Advance the slot
        scheduler.slot = (scheduler.slot + 1u) % scheduler.schedule->cycle_slots;
        if (scheduler.slot == 0U) {
            ++scheduler.cycle_count;
        }
    }
    return scheduler.exec.fault;
}

zule_result_t zule_scheduler_run(void) {
    zule_result_t result = ZULE_RESULT_OK;
    if (!scheduler.initialized) {
        result = ZULE_RESULT_NOT_INITIALIZED;
    } else {
        zule_port_critical_state_t critical_state = zule_port_critical_enter();
        // Dispatch the active task if it is pending and there is no active fault
        if ((scheduler.exec.fault == ZULE_RESULT_OK) &&
            (scheduler.exec.state == ZULE_TASK_PENDING)) {
            scheduler.exec.state = ZULE_TASK_RUNNING;
            zule_task_function_t task_function = scheduler.exec.task->function;
            zule_port_critical_exit(critical_state);
            task_function();
            critical_state = zule_port_critical_enter();
            if (scheduler.exec.state == ZULE_TASK_RUNNING) {
                scheduler.exec.state = ZULE_TASK_COMPLETED;
            }
        }
        result = scheduler.exec.fault;
        zule_port_critical_exit(critical_state);
    }
    return result;
}

zule_result_t zule_scheduler_idle(void) {
    if (!scheduler.initialized) {
        return ZULE_RESULT_NOT_INITIALIZED;
    }
    zule_result_t result = ZULE_RESULT_OK;
    const zule_port_critical_state_t critical_state = zule_port_critical_enter();
    /* Only tick context makes a task pending or latches a fault, and it cannot run between
     * this check and the port's atomic wait. */
    if (scheduler.exec.fault != ZULE_RESULT_OK) {
        result = scheduler.exec.fault;
    } else if (scheduler.exec.state != ZULE_TASK_PENDING &&
               zule_port_idle() != ZULE_PORT_RESULT_OK) {
        result = ZULE_RESULT_PORT_ERROR;
    }
    zule_port_critical_exit(critical_state);
    return result;
}
