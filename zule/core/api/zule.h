#ifndef ZULE_H
#define ZULE_H

#include "zule_config_apply.h"
#include "zule_hooks.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ZULE_RESULT_OK,
    ZULE_RESULT_NOT_INITIALIZED,
    ZULE_RESULT_INVALID_SCHEDULE,
    ZULE_RESULT_DISPATCH_MISSED,
    ZULE_RESULT_EXECUTION_OVERRUN,
    ZULE_RESULT_PORT_ERROR,
    ZULE_RESULT_ALREADY_RUNNING,
    ZULE_RESULT_INVALID_CONFIG,
    ZULE_RESULT_TICK_TIMING,
    ZULE_RESULT_TERMINATED,
} zule_result_t;

typedef struct {
    /* DISPATCH_MISSED, EXECUTION_OVERRUN, or TICK_TIMING. */
    zule_result_t reason;
    /* The active schedule's ID, as in zule_schedule_ids.h; window_index is an
     * index into its windows. */
    uint32_t schedule_id;
    size_t window_index;
    /* Slot at which the fault was detected. */
    uint32_t slot;
    /* Zero-based cycle count, reset at initialization and continuing across
     * schedule switches. Wraps modulo 2^32. A fault detected at slot zero
     * reports the new cycle, even if the faulting window started in the
     * previous cycle. */
    uint32_t cycle_count;
    /* ID of the active window's task: the faulting task, or for a tick timing
     * fault, the task whose window is in progress or next to start. */
    uint32_t task_id;
#if ZULE_CONFIG_TICK_CHECK
    /* Deviation of the tick that detected the fault from its expected time, in
     * timebase counts: positive when late, negative when early. */
    int32_t tick_deviation_counts;
#endif
} zule_fault_info_t;

/* Called once after the first missed-dispatch, overrun, or tick timing fault is
 * latched.
 * Runs in tick/ISR context (the excluded timer thread on native ports), possibly
 * interrupting a task. Must be bounded and nonblocking: do not allocate, log
 * synchronously, call Zule APIs or port lifecycle functions, or wait for main.
 * The snapshot pointer is valid only during the call; copy it if needed later.
 * Application-owned state shared with main needs appropriate synchronization.
 * Returning does not clear the fault or terminate an overrunning function.
 * Initialization and port errors do not invoke this handler. */
typedef void (*zule_fault_handler_t)(const zule_fault_info_t *fault);

typedef struct {
    zule_fault_handler_t on_fault;
    /* The schedule zule_run() starts, as a schedule ID from the generated
     * zule_schedule_ids.h, such as ZULE_SCHEDULE_ID_MAIN for a single schedule.
     * ID 0 is the first schedule by name. */
    uint32_t schedule_id;
} zule_config_t;

/* Copy configuration while stopped. While running, return ALREADY_RUNNING.
 * Otherwise NULL, or a schedule ID that is not one of the application's
 * schedules, returns INVALID_CONFIG without changing the previous config.
 * Configuration is optional: without it, zule_run() starts the schedule with ID
 * 0 and notifies no fault handler. A null handler disables notification while
 * faults still latch. The copied configuration persists across runs. Use the
 * same application context as zule_run(); concurrent configuration is
 * unsupported. */
zule_result_t zule_configure(const zule_config_t *config);

/* Return the immutable diagnostic name for a declared task, or NULL for an
 * unknown ID or when the schedule has no names. Task IDs are task indexes,
 * starting at zero, as defined by the generated zule_task_ids.h. The linked schedule and names must
 * remain valid and unchanged for the lifetime of the application. */
const char *zule_task_name_get(uint32_t task_id);

/* Initialize the scheduler, start periodic ticks, and dispatch tasks until a
 * scheduler or port failure, or a termination request. This blocking entry point
 * must be called from one application context; recursive calls return
 * ALREADY_RUNNING. Concurrent calls from separate application threads are
 * unsupported. Tick delivery is stopped and quiesced before returning. Tasks
 * must return for the runner to exit. */
zule_result_t zule_run(void);

/* Request that the scheduler stop, so that zule_run() returns TERMINATED. Call
 * from a task. The requesting task runs to its end as usual, and the run then
 * returns without dispatching another task: a task already released is not
 * dispatched, and tick delivery is stopped and quiesced before returning, as on
 * any other exit. A request cannot be withdrawn, and repeating it changes
 * nothing. Ticks are delivered until the run returns, so one may still retire a
 * window, release a task that is then never dispatched, or latch a fault and
 * notify the handler. A fault latched before the run returns takes precedence,
 * so zule_run() reports the fault rather than the request; one latched after
 * that does not change what zule_run() returns. Returns OK, or NOT_INITIALIZED
 * when the scheduler is not running. Each run starts with no request pending, so a terminated
 * application can call zule_run() again. */
zule_result_t zule_terminate(void);

/* Request a switch to another schedule, by schedule ID from the generated
 * zule_schedule_ids.h. Call from a task. The switch takes effect at the start of
 * the next cycle, after the current schedule's final window: its first window
 * starts at slot 0 of the new schedule, and the cycle count continues. A later
 * request before then replaces an earlier one, and requesting the active
 * schedule cancels a pending switch. No switch takes place while a fault is
 * latched. Returns OK; INVALID_CONFIG for an ID that is not one of the
 * application's schedules, keeping any earlier request; and NOT_INITIALIZED when
 * the scheduler is not running. Each run starts with the configured schedule,
 * with no request pending. */
zule_result_t zule_schedule_request(uint32_t schedule_id);

#if ZULE_CONFIG_TIMEBASE
/* Return the timebase frequency in counts per second, for converting hook
 * timestamps and other timebase counts. Nonzero, constant, and callable at any
 * time. Declared only when ZULE_CONFIG_TIMEBASE is enabled. */
uint32_t zule_timebase_hz(void);
#endif

#endif
