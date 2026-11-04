#ifndef ZULE_PORT_H
#define ZULE_PORT_H

/* The selected port supplies this header and a complete definition of
 * zule_port_critical_state_t. Its size and representation are port-defined;
 * callers only save the returned token and pass it back to exit. */
#include "zule_port_types.h"

/* Save the current exclusion state and exclude the tick ISR before
 * returning. Provide compiler ordering barriers, and hardware barriers where
 * required, so protected memory accesses cannot move outside the section.
 * Embedded ports must provide bounded entry/exit without waiting or spinning.
 * Native development ports may document blocking host synchronization instead.
 * Embedded interrupt masking assumes a single core; native tick threads must
 * acquire the same exclusion before calling zule_tick().
 * Calls may nest; each saved state must be restored in reverse entry order.
 * The caller must keep protected work short and never execute tasks inside it. */
zule_port_critical_state_t zule_port_critical_enter(void);

/* Restore the state returned by the matching enter call in the same execution
 * context, preserving any pre-existing exclusion (including interrupt masking).
 * Do not unconditionally enable interrupts. Complete the required memory
 * ordering before restoring the previous exclusion state. */
void zule_port_critical_exit(zule_port_critical_state_t previous_state);

#endif
