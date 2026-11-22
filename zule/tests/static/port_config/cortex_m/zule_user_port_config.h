#ifndef ZULE_USER_PORT_CONFIG_H
#define ZULE_USER_PORT_CONFIG_H

/* Cortex-M port settings for the static checks. Nothing runs these
 * builds, so the clock only needs to be a valid setting; 25 MHz matches QEMU's
 * mps2-an385 machine, for consistency with the planned QEMU tests. */
#define ZULE_PORT_CORTEX_M_SYSTICK_HZ 25000000U

#endif
