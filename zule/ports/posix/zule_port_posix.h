#ifndef ZULE_PORT_POSIX_H
#define ZULE_PORT_POSIX_H

/* Internal to the POSIX port: a condition wait on the critical-section mutex. */

/* Release the critical section, wait for a notification, and reacquire it. The caller
 * must hold exactly one level of the critical section; a condition wait releases only
 * one level of the recursive mutex, so deeper nesting would keep ticks excluded and
 * aborts instead. Spurious wakeups are possible. */
void zule_port_posix_critical_wait(void);

/* Wake every critical-section waiter. The caller must hold the critical section, so a
 * notification cannot fall between a waiter's condition check and its wait. */
void zule_port_posix_critical_notify(void);

#endif
