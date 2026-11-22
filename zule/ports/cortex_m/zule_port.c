#include "zule_port.h"
#include "zule_port_cortex_m.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Generic Cortex-M port: PRIMASK for critical sections, SysTick for the tick, and
 * WFI for idle; see README.md. Single core: masking interrupts excludes the tick. */

#define ZULE_PORT_CORTEX_M_MICROSECONDS_PER_SECOND 1000000U

/* Apply the application's SysTick clock selection: SYST_CSR's CLKSOURCE bit
 * selects the processor clock when set, and the reference clock when clear. */
#if ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK
#define ZULE_PORT_CORTEX_M_CLOCK_SOURCE 0U
#else
#define ZULE_PORT_CORTEX_M_CLOCK_SOURCE ZULE_PORT_CORTEX_M_SYST_CSR_CLKSOURCE
#endif

/* Written in application context while ticks are stopped, and read by the
 * SysTick handler. Volatile, so the write is not moved after the register write
 * that enables the tick. */
static zule_port_tick_callback_t volatile tick_callback;
/* Whether SysTick is delivering ticks. Only application context changes it. */
static bool tick_running;

zule_port_critical_state_t zule_port_critical_enter(void) {
    const zule_port_critical_state_t state = {.primask = zule_port_cortex_m_interrupts_disable()};
    return state;
}

void zule_port_critical_exit(zule_port_critical_state_t previous_state) {
    zule_port_cortex_m_interrupts_restore(previous_state.primask);
}

/* Convert a slot of slot_us to SysTick's reload value, or 0 when the slot is not a
 * whole number of clock counts, which would make ticks drift, or does not fit
 * SysTick. SysTick counts from the reload value down to zero, so a period of n
 * counts reloads n - 1, from 1 to the 24-bit maximum. */
static uint32_t slot_us_to_reload(uint32_t slot_us) {
    uint32_t reload = 0U;
    /* Both factors are below 2^32, so the product fits. */
    const uint64_t scaled_counts = (uint64_t)slot_us * (uint64_t)ZULE_PORT_CORTEX_M_SYSTICK_HZ;
    if ((scaled_counts % ZULE_PORT_CORTEX_M_MICROSECONDS_PER_SECOND) == 0U) {
        const uint64_t counts = scaled_counts / ZULE_PORT_CORTEX_M_MICROSECONDS_PER_SECOND;
        if ((counts >= 2U) && (counts <= ((uint64_t)ZULE_PORT_CORTEX_M_SYST_RVR_MAX + 1U))) {
            reload = (uint32_t)(counts - 1U);
        }
    }
    return reload;
}

zule_port_result_t zule_port_tick_init(uint32_t slot_us, zule_port_tick_callback_t callback) {
    zule_port_result_t result = ZULE_PORT_RESULT_ERROR;
    const uint32_t reload = slot_us_to_reload(slot_us);
    bool valid = (callback != NULL) && (reload != 0U) && !tick_running;
#if ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK
    /* On a part with no reference clock, SysTick would never count, so ticks
     * would never arrive. */
    const uint32_t calib = zule_port_cortex_m_systick.calib;
    valid = valid && ((calib & ZULE_PORT_CORTEX_M_SYST_CALIB_NOREF) == 0U);
#endif
    if (valid) {
        tick_callback = callback;
        zule_port_cortex_m_systick.rvr = reload;
        /* Any write clears the counter, so the first tick comes one full period
         * after the counter starts. */
        zule_port_cortex_m_systick.cvr = 0U;
        zule_port_cortex_m_scb.icsr = ZULE_PORT_CORTEX_M_ICSR_PENDSTCLR;
        zule_port_cortex_m_systick.csr = ZULE_PORT_CORTEX_M_CLOCK_SOURCE |
                                         ZULE_PORT_CORTEX_M_SYST_CSR_TICKINT |
                                         ZULE_PORT_CORTEX_M_SYST_CSR_ENABLE;
        tick_running = true;
        result = ZULE_PORT_RESULT_OK;
    }
    return result;
}

/* Stop the counter and its interrupt, and clear a tick that became pending
 * before it stopped. On a single core, this runs in application context, which a
 * tick in progress would have preempted until it finished, so no callback is in
 * flight when this returns. */
zule_port_result_t zule_port_tick_stop(void) {
    zule_port_cortex_m_systick.csr = 0U;
    zule_port_cortex_m_scb.icsr = ZULE_PORT_CORTEX_M_ICSR_PENDSTCLR;
    tick_running = false;
    return ZULE_PORT_RESULT_OK;
}

/* Called inside one critical section, so interrupts are masked. A tick that
 * becomes due still ends the wait, and its handler runs once the core leaves the
 * critical section, so no tick is missed between the core's check and the wait.
 * Without ticks running, nothing would end the wait, so that is an error. */
zule_port_result_t zule_port_idle(void) {
    zule_port_result_t result = ZULE_PORT_RESULT_ERROR;
    if (tick_running) {
        zule_port_cortex_m_wait_for_interrupt();
        result = ZULE_PORT_RESULT_OK;
    }
    return result;
}

/* Reason: the processor calls the handler through the vector table, which the
 * analysis cannot see. */
/* cppcheck-suppress unusedFunction */
void SysTick_Handler(void) {
    /* The callback is set whenever the interrupt is enabled: tick init checks it
     * and stores it before enabling SysTick, and only init enables it. Scheduler
     * results remain core policy, so the port discards them. */
    (void)tick_callback();
}
