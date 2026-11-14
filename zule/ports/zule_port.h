#ifndef ZULE_PORT_H
#define ZULE_PORT_H

/* The selected port supplies its settings and capabilities in this header,
 * including any application overrides it accepts. */
#include "zule_port_config.h"
/* The selected port supplies this header and a complete definition of
 * zule_port_critical_state_t. Its size and representation are port-defined;
 * callers only save the returned token and pass it back to exit. */
#include "zule_port_types.h"
#include "zule.h"

#include <stdint.h>

/* Capability: 1 if the port provides the timebase functions below, otherwise 0. */
#ifndef ZULE_PORT_CONFIG_TIME
#define ZULE_PORT_CONFIG_TIME 0
#endif

#if ZULE_PORT_CONFIG_TIME != 0 && ZULE_PORT_CONFIG_TIME != 1
#error "ZULE_PORT_CONFIG_TIME must be 0 or 1"
#endif

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
 * Tick init and stop have one main-loop caller and must run outside critical
 * sections; they must not be called by the tick callback. */
zule_port_result_t zule_port_tick_init(uint32_t slot_us, zule_port_tick_callback_t callback);

/* Stop delivery and wait for any in-flight callback before returning. Repeated
 * stop calls are safe. Even an error return must leave delivery quiescent. */
zule_port_result_t zule_port_tick_stop(void);

/* Wait for tick delivery. The core calls this only from the dispatch loop, inside
 * exactly one level of critical section, after finding no pending task. Release
 * exclusion while waiting and restore it before returning. The release and wait
 * must be atomic, so a tick that becomes due after the core's check still ends the
 * wait: for example, WFI with interrupts masked, or a condition wait signalled by
 * the tick. Returning early is allowed. Report asynchronous timer failures through
 * ERROR, including failures that occur while waiting. */
zule_port_result_t zule_port_idle(void);

#if ZULE_PORT_CONFIG_TIME
/* Return the timebase frequency in counts per second. It is nonzero, constant
 * for the life of the program, and callable at any time. */
uint32_t zule_port_time_hz(void);

/* Return a free-running timebase count that wraps modulo 2^32, so unsigned
 * subtraction measures intervals shorter than one wrap period. Valid from a
 * successful tick init until tick stop. Callable from application and tick
 * context, including inside critical sections, without changing the exclusion
 * state. Embedded ports read in bounded time. The count keeps advancing while
 * the tick is delayed or masked and during idle; a port documents any limit,
 * such as masking for longer than one tick period. */
uint32_t zule_port_time_get(void);
#endif

#endif
