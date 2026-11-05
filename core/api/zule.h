#ifndef ZULE_H
#define ZULE_H

typedef enum {
    ZULE_RESULT_OK,
    ZULE_RESULT_NOT_INITIALIZED,
    ZULE_RESULT_INVALID_SCHEDULE,
    ZULE_RESULT_DISPATCH_MISSED,
    ZULE_RESULT_EXECUTION_OVERRUN,
    ZULE_RESULT_PORT_ERROR,
    ZULE_RESULT_ALREADY_RUNNING,
} zule_result_t;

/* Initialize the scheduler, start periodic ticks, and dispatch tasks until a
 * scheduler or port failure. This blocking entry point must be called from one
 * application context; recursive calls return ALREADY_RUNNING. Concurrent calls
 * from separate application threads are unsupported. Tick delivery is stopped
 * and quiesced before returning. Tasks must return for the runner to exit. */
zule_result_t zule_run(void);

#endif
