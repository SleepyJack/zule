#ifndef ZULE_SCHEDULER_H
#define ZULE_SCHEDULER_H

#include "zule.h"

/* Internal API. Init requires quiescent tick delivery and no executing task.
 * Validation failure leaves tick and run disabled. Capture the supplied handler
 * for this run; NULL disables notification. */
zule_result_t zule_scheduler_init(zule_fault_handler_t on_fault);

/* Advance one logical slot from a non-reentrant ISR or an excluded native
 * callback. Faulted calls still retire completed work without advancing time. */
zule_result_t zule_scheduler_tick(void);

/* Dispatch at most one task from the main loop. Idle polling returns OK.
 * Both tick and run return NOT_INITIALIZED until successful initialization.
 * Task identity stays stable; callbacks execute outside the critical section. */
zule_result_t zule_scheduler_run(void);

/* Wait for the next tick from the main loop when no task is pending. The check and the port's
 * idle wait share one critical section, so a tick that marks a task pending cannot be lost
 * between them. Returns a latched fault without waiting, PORT_ERROR if the port wait fails,
 * NOT_INITIALIZED before successful initialization, and otherwise OK. */
zule_result_t zule_scheduler_idle(void);

#endif
