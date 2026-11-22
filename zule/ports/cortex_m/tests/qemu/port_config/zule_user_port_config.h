#ifndef ZULE_USER_PORT_CONFIG_H
#define ZULE_USER_PORT_CONFIG_H

/* QEMU's mps2-an385 machine clocks its Cortex-M3, and so SysTick, at 25 MHz. */
#define ZULE_PORT_CORTEX_M_SYSTICK_HZ 25000000U

#endif
