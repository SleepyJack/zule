/* The Cortex-M port's register addresses, and the instructions C cannot
 * express; see zule_port_cortex_m.h. Written for Armv6-M, whose instructions
 * Armv7-M and Armv8-M also provide, and assembled for it whatever the build's
 * CPU flags. No floating-point registers are used, so the object links with
 * soft- and hard-float builds alike. */
    .syntax unified
    .arch armv6-m
    .thumb

/* Architecture-defined register addresses, the same on every Cortex-M. */
    .global zule_port_cortex_m_systick
    .set zule_port_cortex_m_systick, 0xE000E010
    .global zule_port_cortex_m_icsr
    .set zule_port_cortex_m_icsr, 0xE000ED04

    .text

/* uint32_t zule_port_cortex_m_interrupts_disable(void)
 * CPSID takes effect for the next instruction, so no barrier is needed. */
    .global zule_port_cortex_m_interrupts_disable
    .type zule_port_cortex_m_interrupts_disable, %function
    .thumb_func
zule_port_cortex_m_interrupts_disable:
    mrs r0, primask
    cpsid i
    bx lr
    .size zule_port_cortex_m_interrupts_disable, . - zule_port_cortex_m_interrupts_disable

/* void zule_port_cortex_m_interrupts_restore(uint32_t primask)
 * The ISB makes a newly unmasked pending interrupt be taken here. */
    .global zule_port_cortex_m_interrupts_restore
    .type zule_port_cortex_m_interrupts_restore, %function
    .thumb_func
zule_port_cortex_m_interrupts_restore:
    msr primask, r0
    isb
    bx lr
    .size zule_port_cortex_m_interrupts_restore, . - zule_port_cortex_m_interrupts_restore

/* void zule_port_cortex_m_wait_for_interrupt(void) */
    .global zule_port_cortex_m_wait_for_interrupt
    .type zule_port_cortex_m_wait_for_interrupt, %function
    .thumb_func
zule_port_cortex_m_wait_for_interrupt:
    dsb
    wfi
    bx lr
    .size zule_port_cortex_m_wait_for_interrupt, . - zule_port_cortex_m_wait_for_interrupt
