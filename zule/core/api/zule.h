#ifndef ZULE_H
#define ZULE_H

#include "zule_config.h"
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
} zule_result_t;

typedef struct {
    zule_result_t reason;
    size_t window_index;
    uint32_t slot;
    uint32_t cycle_count;
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
     * Left at 0, it selects the schedule with ID 0, the first by name. */
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
 * scheduler or port failure. This blocking entry point must be called from one
 * application context; recursive calls return ALREADY_RUNNING. Concurrent calls
 * from separate application threads are unsupported. Tick delivery is stopped
 * and quiesced before returning. Tasks must return for the runner to exit. */
zule_result_t zule_run(void);

#if ZULE_CONFIG_TIMEBASE
/* Return the timebase frequency in counts per second, for converting hook
 * timestamps and other timebase counts. Nonzero, constant, and callable at any
 * time. Declared only when ZULE_CONFIG_TIMEBASE is enabled. */
uint32_t zule_timebase_hz(void);
#endif

#endif
