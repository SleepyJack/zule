#ifndef ZULE_PORT_CONFIG_H
#define ZULE_PORT_CONFIG_H

/* Cortex-M port settings. Override them in zule_user_port_config.h; see
 * README.md. */
#ifndef ZULE_USE_USER_PORT_CONFIG
#define ZULE_USE_USER_PORT_CONFIG 0
#endif

#if ZULE_USE_USER_PORT_CONFIG
#include "zule_user_port_config.h"
#endif

/* Required: the frequency of SysTick's clock, in Hz. It depends on the board and
 * its clock configuration, so there is no default. */
#ifndef ZULE_PORT_CORTEX_M_SYSTICK_HZ
#error "ZULE_PORT_CORTEX_M_SYSTICK_HZ must be set to the SysTick clock frequency in Hz"
#endif

#if (ZULE_PORT_CORTEX_M_SYSTICK_HZ < 1) || (ZULE_PORT_CORTEX_M_SYSTICK_HZ > 4294967295)
#error "ZULE_PORT_CORTEX_M_SYSTICK_HZ must be from 1 to 4294967295"
#endif

/* SysTick's clock: 1 for the processor clock, or 0 for the implementation-defined
 * reference clock, such as the processor clock divided by 8 on many parts. */
#ifndef ZULE_PORT_CORTEX_M_SYSTICK_PROCESSOR_CLOCK
#define ZULE_PORT_CORTEX_M_SYSTICK_PROCESSOR_CLOCK 1
#endif

#if ZULE_PORT_CORTEX_M_SYSTICK_PROCESSOR_CLOCK != 0 &&                                             \
    ZULE_PORT_CORTEX_M_SYSTICK_PROCESSOR_CLOCK != 1
#error "ZULE_PORT_CORTEX_M_SYSTICK_PROCESSOR_CLOCK must be 0 or 1"
#endif

#if ZULE_CONFIG_TIMEBASE
#error "The Cortex-M port does not provide the timebase yet"
#endif

#endif
