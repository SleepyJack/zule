#ifndef ZULE_H
#define ZULE_H

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
} zule_result_t;

typedef struct {
    zule_result_t reason;
    size_t window_index;
    uint32_t slot;
    uint32_t cycle_count;
} zule_fault_info_t;

/* Called once after the first missed-dispatch or overrun fault is latched.
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
} zule_config_t;

/* Copy configuration while stopped. While running, return ALREADY_RUNNING.
 * Otherwise NULL returns INVALID_CONFIG without changing the previous config.
 * Configuration is optional; a null handler disables notification while faults
 * still latch. The copied configuration persists across runs. Use the same
 * application context as zule_run(); concurrent configuration is unsupported. */
zule_result_t zule_configure(const zule_config_t *config);

/* Initialize the scheduler, start periodic ticks, and dispatch tasks until a
 * scheduler or port failure. This blocking entry point must be called from one
 * application context; recursive calls return ALREADY_RUNNING. Concurrent calls
 * from separate application threads are unsupported. Tick delivery is stopped
 * and quiesced before returning. Tasks must return for the runner to exit. */
zule_result_t zule_run(void);

#endif
