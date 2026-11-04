#include "zule.h"
#include "schedule.h"

zule_scheduler_t scheduler;

zule_scheduler_t * zule_init(void)
{
    scheduler.schedule = zule_schedule_get();
    scheduler.slot = 0U;
    scheduler.active_window_ind = 0U;
    scheduler.task.state = ZULE_TASK_IDLE;
    scheduler.task.fault = ZULE_TASK_FAULT_NONE;
    return &scheduler;
}

void zule_tick(zule_scheduler_t * const scheduler)
{
    const zule_window_t* active_window = &scheduler->schedule->windows[scheduler->active_window_ind];

    // Check for end of the current active window
    const uint32_t active_window_end = (active_window->start_slot + active_window->budget_slots) % scheduler->schedule->cycle_slots;
    if(scheduler->slot == active_window_end )
    {
        if(scheduler->task.state == ZULE_TASK_COMPLETED) {
            scheduler->task.state = ZULE_TASK_IDLE;
            // TODO: Consider fault recovery here
        } else if(scheduler->task.state == ZULE_TASK_PENDING) {
            scheduler->task.fault = ZULE_TASK_FAULT_NOT_DISPATCHED;
            // TODO: Critical error handling
        } else if (scheduler->task.state == ZULE_TASK_RUNNING) {
            // Preserve ZULE_TASK_FAULT_NOT_DISPATCHED as reason in case task eventually starts
            scheduler->task.fault = (scheduler->task.fault == ZULE_TASK_FAULT_NONE) ? ZULE_TASK_FAULT_EXECUTION_OVERRUN : scheduler->task.fault;
            // TODO: Critical error handling
        } else {
            // Idle - do nothing
        }

        // Prevent window advance in case of active faults
        if(scheduler->task.fault == ZULE_TASK_FAULT_NONE) {
            scheduler->active_window_ind = (scheduler->active_window_ind + 1u) % scheduler->schedule->window_count;
            active_window = &scheduler->schedule->windows[scheduler->active_window_ind];
        }
    }

    // Halt the scheduler in case of active faults
    if(scheduler->task.fault == ZULE_TASK_FAULT_NONE) {

        // Check for start of the new active window
        if(scheduler->slot == active_window->start_slot) {
            if(scheduler->task.state == ZULE_TASK_IDLE) {
                scheduler->task.function = active_window->task_function;
                scheduler->task.state = ZULE_TASK_PENDING;
            } else {
                // Handle overrun
            }
        }

        // Advance the slot
        scheduler->slot = (scheduler->slot + 1u) % scheduler->schedule->cycle_slots;
    }
}

void zule_run(zule_scheduler_t * const scheduler){
    // Dispatch the active task if it is pending and there is no active fault
    if((scheduler->task.fault == ZULE_TASK_FAULT_NONE) && (scheduler->task.state == ZULE_TASK_PENDING)) {
        scheduler->task.state = ZULE_TASK_RUNNING;
        scheduler->task.function();
        if(scheduler->task.state == ZULE_TASK_RUNNING) {
            scheduler->task.state = ZULE_TASK_COMPLETED;
        }
    }
}
