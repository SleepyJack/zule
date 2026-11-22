#ifndef ZULE_PORT_CORTEX_M_H
#define ZULE_PORT_CORTEX_M_H

/* Internal to the Cortex-M port: the registers it uses, and the instructions C
 * cannot express. Names follow the Arm Architecture Reference Manuals for
 * Armv6-M, Armv7-M, and Armv8-M, which define these registers identically. */
#include <stdint.h>

/* SysTick: SYST_CSR, SYST_RVR, SYST_CVR, and SYST_CALIB. */
typedef struct {
    volatile uint32_t csr;
    volatile uint32_t rvr;
    volatile uint32_t cvr;
    volatile uint32_t calib;
} zule_port_cortex_m_systick_t;

#define ZULE_PORT_CORTEX_M_SYST_CSR_ENABLE 0x00000001U
#define ZULE_PORT_CORTEX_M_SYST_CSR_TICKINT 0x00000002U
#define ZULE_PORT_CORTEX_M_SYST_CSR_CLKSOURCE 0x00000004U
/* SYST_CALIB's NOREF bit is set when the part has no reference clock. */
#define ZULE_PORT_CORTEX_M_SYST_CALIB_NOREF 0x80000000U
/* SysTick counts down from the reload value, a 24-bit field. */
#define ZULE_PORT_CORTEX_M_SYST_RVR_MAX 0x00FFFFFFU

/* The start of the System Control Block: CPUID, then the Interrupt Control and
 * State Register, ICSR. */
typedef struct {
    volatile uint32_t cpuid;
    volatile uint32_t icsr;
} zule_port_cortex_m_scb_t;

/* ICSR's write-one-to-clear bit for a pending SysTick exception; writing zero to
 * its other bits has no effect. */
#define ZULE_PORT_CORTEX_M_ICSR_PENDSTCLR 0x02000000U

/* The registers. zule_port_cortex_m.S places them at their architecture-defined
 * addresses, so the C needs no conversion from an integer to a pointer, and host
 * tests define them in ordinary memory instead. */
extern zule_port_cortex_m_systick_t zule_port_cortex_m_systick;
extern zule_port_cortex_m_scb_t zule_port_cortex_m_scb;

/* Instructions, in zule_port_cortex_m.S. Each call is opaque to the compiler, so
 * it cannot move accesses to data the tick shares across one: that data is
 * reachable from the tick callback, which the compiler must assume any unknown
 * function can call. */

/* Mask interrupts by setting PRIMASK, and return its previous value. */
uint32_t zule_port_cortex_m_interrupts_disable(void);

/* Restore PRIMASK to a value from zule_port_cortex_m_interrupts_disable(). If
 * that unmasks interrupts, a pending one is taken before this returns. */
void zule_port_cortex_m_interrupts_restore(uint32_t primask);

/* Complete outstanding memory accesses, then wait for an interrupt. A pending
 * interrupt ends the wait even while PRIMASK masks it. */
void zule_port_cortex_m_wait_for_interrupt(void);

/* The SysTick exception handler, under the name Cortex-M startup code's vector
 * tables use. Defined by the port, which owns SysTick. */
void SysTick_Handler(void);

#endif
