#ifndef ZULE_PORT_CONFIG_H
#define ZULE_PORT_CONFIG_H

/* Cortex-M port settings. Override them in zule_user_port_config.h; see
 * README.md. */
#include <stdint.h>

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

/* C99 makes UINT32_MAX usable in #if. */
#if (ZULE_PORT_CORTEX_M_SYSTICK_HZ < 1) || (ZULE_PORT_CORTEX_M_SYSTICK_HZ > UINT32_MAX)
#error "ZULE_PORT_CORTEX_M_SYSTICK_HZ must be from 1 to UINT32_MAX"
#endif

/* Set to 1 to clock SysTick from the external reference clock, which the
 * implementation defines, such as the processor clock divided by 8 on many parts,
 * instead of the processor clock. */
#ifndef ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK
#define ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK 0
#endif

#if ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK != 0 &&                                         \
    ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK != 1
#error "ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK must be 0 or 1"
#endif

#if ZULE_CONFIG_TIMEBASE
#error "The Cortex-M port does not provide the timebase yet"
#endif

#endif
