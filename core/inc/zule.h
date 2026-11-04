#ifndef ZULE_H
#define ZULE_H

typedef enum {
    ZULE_RESULT_OK,
    ZULE_RESULT_NOT_INITIALIZED,
    ZULE_RESULT_INVALID_SCHEDULE,
    ZULE_RESULT_DISPATCH_MISSED,
    ZULE_RESULT_EXECUTION_OVERRUN,
} zule_result_t;

/* Initialize or reset the singleton. Tick delivery must be stopped and no task
 * may be executing. Returns INVALID_SCHEDULE if configuration validation fails,
 * leaving tick and run disabled until a successful initialization. */
zule_result_t zule_init(void);

/* Process one logical slot. Returns NOT_INITIALIZED before initialization;
 * otherwise returns OK or the latched fault. Faulted calls still process task
 * retirement but do not advance time or release tasks. ISR/main-loop
 * synchronization remains to be implemented. */
zule_result_t zule_tick(void);

/* Dispatch pending work synchronously from the main loop. Returns OK when
 * idle, NOT_INITIALIZED before initialization, or the latched fault. Tasks
 * must return and must not call zule_run() or zule_init() recursively. */
zule_result_t zule_run(void);

#endif
