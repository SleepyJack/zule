#ifndef ZULE_PORT_H
#define ZULE_PORT_H

/* The selected port supplies this header and a complete definition of
 * zule_port_critical_state_t. Its size and representation are port-defined;
 * callers only save the returned token and pass it back to exit. */
#include "zule_port_types.h"
#include "zule.h"

#include <stdint.h>

typedef enum {
    ZULE_PORT_RESULT_OK,
    ZULE_PORT_RESULT_ERROR,
} zule_port_result_t;

/* Scheduler results remain core policy. Ports may discard the callback result;
 * a scheduler fault must not be reclassified as a port failure. */
typedef zule_result_t (*zule_port_tick_callback_t)(void);

/* Save the current exclusion state and exclude the tick ISR before
 * returning. Provide compiler ordering barriers, and hardware barriers where
 * required, so protected memory accesses cannot move outside the section.
 * Embedded ports must provide bounded entry/exit without waiting or spinning.
 * Native development ports may document blocking host synchronization instead.
 * Embedded interrupt masking assumes a single core; native tick threads must
 * acquire the same exclusion before invoking the tick callback.
 * Calls may nest; each saved state must be restored in reverse entry order.
 * The caller must keep protected work short and never execute tasks inside it. */
zule_port_critical_state_t zule_port_critical_enter(void);

/* Restore the state returned by the matching enter call in the same execution
 * context, preserving any pre-existing exclusion (including interrupt masking).
 * Do not unconditionally enable interrupts. Complete the required memory
 * ordering before restoring the previous exclusion state. */
void zule_port_critical_exit(zule_port_critical_state_t previous_state);

/* Configure and start periodic delivery, which must already be stopped.
 * The period must be nonzero and the
 * callback non-null. The scheduler is initialized before this call. A callback
 * may run before init returns, but never concurrently with itself or another
 * scheduler-state writer. Failure leaves delivery stopped and quiescent.
 * Lifecycle functions have one main-loop caller and must run outside critical
 * sections; they must not be called by the tick callback. */
zule_port_result_t zule_port_tick_init(uint32_t slot_us, zule_port_tick_callback_t callback);

/* Stop delivery and wait for any in-flight callback before returning. Repeated
 * stop calls are safe. Even an error return must leave delivery quiescent. */
zule_port_result_t zule_port_tick_stop(void);

/* Yield or sleep in the dispatch loop. Must not lose pending work or hold the
 * scheduler critical section while waiting. Report asynchronous timer failures
 * through ERROR. The port determines the idle strategy and wake-up mechanism. */
zule_port_result_t zule_port_idle(void);

#endif
