#ifndef ZULE_SCHEDULER_H
#define ZULE_SCHEDULER_H

#include "zule.h"

/* Internal API. Init requires quiescent tick delivery and no executing task.
 * Validation failure leaves tick and run disabled. */
zule_result_t zule_scheduler_init(void);

/* Advance one logical slot from a non-reentrant ISR or an excluded native
 * callback. Faulted calls still retire completed work without advancing time. */
zule_result_t zule_scheduler_tick(void);

/* Dispatch at most one task from the main loop. Idle polling returns OK.
 * Both tick and run return NOT_INITIALIZED until successful initialization.
 * Task identity stays stable; callbacks execute outside the critical section. */
zule_result_t zule_scheduler_run(void);

#endif
