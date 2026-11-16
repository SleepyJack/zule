#include "zule_scheduler.h"
#include "zule_hook_points.h"
#include "zule_port.h"
#include "zule_schedule_definition.h"

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
    uint32_t counts_per_slot;
    uint32_t tolerance_counts;
    /* Expected timebase count of the current tick, advanced by wrapping addition. */
    uint32_t expected_tick_counts;
    bool tick_reference_set;
    int32_t tick_deviation_counts;
#endif
} zule_scheduler_t;

static zule_scheduler_t scheduler;

#if ZULE_HOOKS_USED || ZULE_CONFIG_TICK_CHECK
#define ZULE_MICROSECONDS_PER_SECOND 1000000U

/* Check that the timebase can represent what the enabled features measure. The
 * frequency is constant and valid at any time, so this runs before ticks start. */
static zule_result_t check_timebase_limits(const zule_schedule_t *schedule) {
    const uint64_t hz = zule_port_time_hz();
    if (hz == 0U) {
        return ZULE_RESULT_INVALID_CONFIG;
    }
    /* Slot length in counts, scaled by one million; both factors are below 2^32,
     * so the product fits. */
    const uint64_t slot_scaled_counts = (uint64_t)schedule->slot_us * hz;
#if ZULE_HOOKS_USED
    /* Hooks promise that unsigned subtraction measures any interval up to one
     * cycle, which needs a cycle shorter than one timebase wrap of 2^32 counts. */
    const uint64_t wrap_scaled_counts = (uint64_t)ZULE_MICROSECONDS_PER_SECOND << 32U;
    if (slot_scaled_counts >= wrap_scaled_counts ||
        (uint64_t)schedule->cycle_slots > (wrap_scaled_counts - 1U) / slot_scaled_counts) {
        return ZULE_RESULT_INVALID_CONFIG;
    }
#endif
#if ZULE_CONFIG_TICK_CHECK
    /* Expected tick times must not drift, and deviations must compare as signed
     * 32-bit differences. */
    if (slot_scaled_counts % ZULE_MICROSECONDS_PER_SECOND != 0U ||
        slot_scaled_counts / ZULE_MICROSECONDS_PER_SECOND >= ((uint64_t)1U << 31U)) {
        return ZULE_RESULT_INVALID_CONFIG;
    }
    const uint32_t counts_per_slot = (uint32_t)(slot_scaled_counts / ZULE_MICROSECONDS_PER_SECOND);
    /* Round down, so the tolerance never exceeds what was configured. */
    const uint64_t tolerance_counts =
        (uint64_t)ZULE_CONFIG_TICK_TOLERANCE_US * hz / ZULE_MICROSECONDS_PER_SECOND;
    if (tolerance_counts < 1U || tolerance_counts >= counts_per_slot) {
        return ZULE_RESULT_INVALID_CONFIG;
    }
    scheduler.counts_per_slot = counts_per_slot;
    scheduler.tolerance_counts = (uint32_t)tolerance_counts;
#endif
    return ZULE_RESULT_OK;
}
#endif

zule_result_t zule_scheduler_init(zule_fault_handler_t on_fault) {
    scheduler.initialized = false;
    scheduler.schedule = zule_schedule_get();
    zule_result_t result = zule_schedule_validate(scheduler.schedule);
    if (result != ZULE_RESULT_OK) {
        return result;
    }
#if ZULE_HOOKS_USED || ZULE_CONFIG_TICK_CHECK
    result = check_timebase_limits(scheduler.schedule);
    if (result != ZULE_RESULT_OK) {
        return result;
    }
#endif
#if ZULE_CONFIG_TICK_CHECK
    scheduler.tick_reference_set = false;
    scheduler.tick_deviation_counts = 0;
#endif
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
        /* Every fault is detected in a tick, while the active window identifies the
         * faulting task. A tick timing fault can occur before any task runs, so
         * the task comes from the window rather than the execution state. */
        const zule_fault_info_t fault = {
            .reason = reason,
            .window_index = scheduler.active_window_ind,
            .slot = scheduler.slot,
            .cycle_count = scheduler.cycle_count,
            .task_id = scheduler.schedule->windows[scheduler.active_window_ind].task->id,
#if ZULE_CONFIG_TICK_CHECK
            .tick_deviation_counts = scheduler.tick_deviation_counts,
#endif
        };
        scheduler.on_fault(&fault);
    }
}

#if ZULE_CONFIG_TICK_CHECK
/* Compare the tick with its expected time, using the first tick of the run as the
 * reference. Wrapping arithmetic keeps this exact across timebase rollover while
 * deviations stay below 2^31 counts, which the slot limit and tolerance ensure. */
static void check_tick_timing(uint32_t now_counts) {
    if (!scheduler.tick_reference_set) {
        scheduler.tick_reference_set = true;
        scheduler.expected_tick_counts = now_counts;
        scheduler.tick_deviation_counts = 0;
        return;
    }
    scheduler.expected_tick_counts += scheduler.counts_per_slot;
    const uint32_t late_counts = now_counts - scheduler.expected_tick_counts;
    uint32_t magnitude_counts;
    /* Convert to signed explicitly; out-of-range conversions are implementation
     * defined. An early tick's magnitude is at most 2^31. */
    if (late_counts <= (uint32_t)INT32_MAX) {
        magnitude_counts = late_counts;
        scheduler.tick_deviation_counts = (int32_t)late_counts;
    } else {
        magnitude_counts = scheduler.expected_tick_counts - now_counts;
        scheduler.tick_deviation_counts = -(int32_t)(magnitude_counts - 1U) - 1;
    }
    if (magnitude_counts > scheduler.tolerance_counts) {
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

zule_result_t zule_scheduler_tick(void) {
    if (!scheduler.initialized) {
        return ZULE_RESULT_NOT_INITIALIZED;
    }
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
        scheduler.slot = (scheduler.slot + 1u) % scheduler.schedule->cycle_slots;
        if (scheduler.slot == 0U) {
            ++scheduler.cycle_count;
        }
    }
    zule_hook_tick_end();
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
