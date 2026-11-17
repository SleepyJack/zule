#include "zule_scheduler.h"
#include "zule_hook_points.h"
#include "zule_port.h"
#include "zule_schedule_definition.h"
#include "zule_tick_tracker.h"

#include <stdbool.h>
#include <stdint.h>

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
#if ZULE_CONFIG_TICK_CHECK
    zule_tick_tracker_t tick_tracker;
    uint32_t tolerance_counts;
    /* Deviation of the current tick, reported with any fault it detects. */
    zule_tick_deviation_t tick_deviation;
#endif
} zule_scheduler_t;

static zule_scheduler_t scheduler;

#if ZULE_HOOKS_USED || ZULE_CONFIG_TICK_CHECK
/* Check that the timebase can represent what the enabled features measure. The
 * frequency is constant and valid at any time, so this runs before ticks start. */
static zule_result_t check_timebase_limits(const zule_schedule_t *schedule) {
    zule_result_t result = ZULE_RESULT_OK;
    const uint64_t hz = zule_port_time_hz();
    /* Slot length in counts, scaled by one million; both factors are below 2^32,
     * so the product fits. */
    const uint64_t slot_scaled_counts = (uint64_t)schedule->slot_us * hz;
    if (hz == 0U) {
        result = ZULE_RESULT_INVALID_CONFIG;
    }
#if ZULE_HOOKS_USED
    /* Hooks promise that unsigned subtraction measures any interval up to one
     * cycle, which needs a cycle shorter than one timebase wrap of 2^32 counts.
     * A nonzero frequency and slot make the divisor nonzero. */
    const uint64_t wrap_scaled_counts = (uint64_t)ZULE_MICROSECONDS_PER_SECOND << 32U;
    if ((result == ZULE_RESULT_OK) &&
        ((slot_scaled_counts >= wrap_scaled_counts) ||
         ((uint64_t)schedule->cycle_slots > ((wrap_scaled_counts - 1U) / slot_scaled_counts)))) {
        result = ZULE_RESULT_INVALID_CONFIG;
    }
#endif
#if ZULE_CONFIG_TICK_CHECK
    if (result == ZULE_RESULT_OK) {
        /* Also clears the tracker's reference, so each run starts from its first tick. */
        result = zule_tick_tracker_init(&scheduler.tick_tracker, schedule->slot_us, (uint32_t)hz);
    }
    if (result == ZULE_RESULT_OK) {
        /* Round down, so the tolerance never exceeds what was configured. */
        const uint64_t tolerance_counts =
            ((uint64_t)ZULE_CONFIG_TICK_TOLERANCE_US * hz) / ZULE_MICROSECONDS_PER_SECOND;
        if ((tolerance_counts < 1U) ||
            (tolerance_counts >= scheduler.tick_tracker.counts_per_slot)) {
            result = ZULE_RESULT_INVALID_CONFIG;
        } else {
            scheduler.tolerance_counts = (uint32_t)tolerance_counts;
        }
    }
#endif
    return result;
}
#endif

zule_result_t zule_scheduler_init(zule_fault_handler_t on_fault) {
    scheduler.initialized = false;
    scheduler.schedule = zule_schedule_get();
    zule_result_t result = zule_schedule_validate(scheduler.schedule);
#if ZULE_HOOKS_USED || ZULE_CONFIG_TICK_CHECK
    if (result == ZULE_RESULT_OK) {
        result = check_timebase_limits(scheduler.schedule);
    }
#endif
#if ZULE_MODULES_ANY
    if (result == ZULE_RESULT_OK) {
        result = zule_modules_init(scheduler.schedule);
    }
#endif
    if (result == ZULE_RESULT_OK) {
#if ZULE_CONFIG_TICK_CHECK
        scheduler.tick_deviation = (zule_tick_deviation_t){0};
#endif
        scheduler.slot = 0U;
        scheduler.cycle_count = 0U;
        scheduler.active_window_ind = 0U;
        scheduler.exec.task = NULL;
        scheduler.exec.state = ZULE_TASK_IDLE;
        scheduler.exec.fault = ZULE_RESULT_OK;
        scheduler.on_fault = on_fault;
        scheduler.initialized = true;
    }
    return result;
}

/* Latch the first fault and notify the handler; later faults are ignored. */
static void raise_fault(zule_result_t reason) {
    if (scheduler.exec.fault == ZULE_RESULT_OK) {
        scheduler.exec.fault = reason;
        if (scheduler.on_fault != NULL) {
            /* Every fault is detected in a tick, while the active window identifies
             * the faulting task. A tick timing fault can occur before any task
             * runs, so the task comes from the window rather than the execution
             * state. */
            const zule_fault_info_t fault = {
                .reason = reason,
                .window_index = scheduler.active_window_ind,
                .slot = scheduler.slot,
                .cycle_count = scheduler.cycle_count,
                .task_id = scheduler.schedule->windows[scheduler.active_window_ind].task->id,
#if ZULE_CONFIG_TICK_CHECK
                .tick_deviation_counts = zule_tick_deviation_signed(scheduler.tick_deviation),
#endif
            };
            scheduler.on_fault(&fault);
        }
    }
}

#if ZULE_CONFIG_TICK_CHECK
/* Compare the tick with its expected time. The tolerance is below one slot, so a
 * fault comes before any deviation reaches 2^31 counts. */
static void check_tick_timing(uint32_t now_counts) {
    if (zule_tick_tracker_update(&scheduler.tick_tracker, now_counts, &scheduler.tick_deviation) &&
        (scheduler.tick_deviation.magnitude_counts > scheduler.tolerance_counts)) {
        raise_fault(ZULE_RESULT_TICK_TIMING);
    }
}
#endif

/* Read the tick's time once, when the tick start hook or the tick check needs it,
 * and pass it to both. The hook runs first, so it precedes any fault this tick. */
static void start_tick(void) {
#if ZULE_HOOK_TICK_START_USED || ZULE_CONFIG_TICK_CHECK
    const uint32_t now_counts = zule_port_time_get();
#if ZULE_HOOK_TICK_START_USED
    zule_hook_tick_start(now_counts, scheduler.slot, scheduler.cycle_count);
#endif
#if ZULE_CONFIG_TICK_CHECK
    if (scheduler.exec.fault == ZULE_RESULT_OK) {
        check_tick_timing(now_counts);
    }
#endif
#endif
}

/* Process one tick of an initialized scheduler. */
static void process_tick(void) {
    start_tick();
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
                (scheduler.active_window_ind + 1U) % scheduler.schedule->window_count;
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
                zule_hook_tick_task_release(scheduler.exec.task->id, scheduler.active_window_ind);
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
        scheduler.slot = (scheduler.slot + 1U) % scheduler.schedule->cycle_slots;
        if (scheduler.slot == 0U) {
            ++scheduler.cycle_count;
        }
    }
    zule_hook_tick_end();
}

zule_result_t zule_scheduler_tick(void) {
    zule_result_t result = ZULE_RESULT_NOT_INITIALIZED;
    if (scheduler.initialized) {
        process_tick();
        result = scheduler.exec.fault;
    }
    return result;
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
            const zule_task_t *task = scheduler.exec.task;
            /* The window cannot advance while its task runs, but capture it so start
             * and end report the same window. */
            const size_t window_index = scheduler.active_window_ind;
            zule_hook_task_start(task->id, window_index);
            zule_port_critical_exit(critical_state);
            task->function();
            critical_state = zule_port_critical_enter();
            zule_hook_task_end(task->id, window_index);
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
    zule_result_t result = ZULE_RESULT_NOT_INITIALIZED;
    if (scheduler.initialized) {
        result = ZULE_RESULT_OK;
        const zule_port_critical_state_t critical_state = zule_port_critical_enter();
        /* Only tick context makes a task pending or latches a fault, and it cannot
         * run between this check and the port's atomic wait. */
        if (scheduler.exec.fault != ZULE_RESULT_OK) {
            result = scheduler.exec.fault;
        } else if (scheduler.exec.state != ZULE_TASK_PENDING) {
            if (zule_port_idle() != ZULE_PORT_RESULT_OK) {
                result = ZULE_RESULT_PORT_ERROR;
            }
        } else {
            /* A task is pending: return without waiting, so it is dispatched. */
        }
        zule_port_critical_exit(critical_state);
    }
    return result;
}
