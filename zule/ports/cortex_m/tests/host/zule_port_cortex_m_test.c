/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zule_port.h"
#include "zule_port_cortex_m.h"

/* Fake registers, in ordinary memory instead of at their hardware addresses. */
zule_port_cortex_m_systick_t zule_port_cortex_m_systick;
zule_port_cortex_m_scb_t zule_port_cortex_m_scb;

/* Fake instructions, modelling PRIMASK and counting waits. */
static uint32_t primask;
static uint32_t waits;

uint32_t zule_port_cortex_m_interrupts_disable(void) {
    const uint32_t previous = primask;
    primask = 1U;
    return previous;
}

void zule_port_cortex_m_interrupts_restore(uint32_t value) {
    primask = value;
}

void zule_port_cortex_m_wait_for_interrupt(void) {
    ++waits;
}

static uint32_t ticks;

static zule_result_t on_tick(void) {
    ++ticks;
    /* The port discards the callback's result. */
    return ZULE_RESULT_DISPATCH_MISSED;
}

#if ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK
#define EXPECTED_CSR 0x3U
#else
#define EXPECTED_CSR 0x7U
#endif

/* Set registers to values the port must overwrite, then start ticks and check
 * the programmed reload value. */
static void check_init(uint32_t slot_us, uint32_t expected_reload) {
    zule_port_cortex_m_systick.rvr = 0xDEADU;
    zule_port_cortex_m_systick.cvr = 0xBEEFU;
    zule_port_cortex_m_scb.icsr = 0U;
    assert(zule_port_tick_init(slot_us, on_tick) == ZULE_PORT_RESULT_OK);
    assert(zule_port_cortex_m_systick.rvr == expected_reload);
    assert(zule_port_cortex_m_systick.cvr == 0U);
    assert(zule_port_cortex_m_scb.icsr == ZULE_PORT_CORTEX_M_ICSR_PENDSTCLR);
    assert(zule_port_cortex_m_systick.csr == EXPECTED_CSR);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
}

/* Check that a slot is rejected without touching the registers. */
static void check_rejected(uint32_t slot_us) {
    zule_port_cortex_m_systick.csr = 0U;
    zule_port_cortex_m_systick.rvr = 0xDEADU;
    assert(zule_port_tick_init(slot_us, on_tick) == ZULE_PORT_RESULT_ERROR);
    assert(zule_port_cortex_m_systick.csr == 0U);
    assert(zule_port_cortex_m_systick.rvr == 0xDEADU);
}

static void test_critical_sections(void) {
    /* Nesting restores each level's previous mask. */
    primask = 0U;
    const zule_port_critical_state_t outer = zule_port_critical_enter();
    assert(primask == 1U);
    const zule_port_critical_state_t inner = zule_port_critical_enter();
    assert(primask == 1U);
    zule_port_critical_exit(inner);
    assert(primask == 1U);
    zule_port_critical_exit(outer);
    assert(primask == 0U);

    /* Exit preserves masking the caller already had. */
    primask = 1U;
    const zule_port_critical_state_t masked = zule_port_critical_enter();
    zule_port_critical_exit(masked);
    assert(primask == 1U);
}

static void test_tick_periods(void) {
#if ZULE_PORT_CORTEX_M_SYSTICK_HZ == 1000000U
    /* At 1 MHz, a slot of n microseconds is n counts and reloads n - 1. */
    check_init(1000U, 999U);
    check_init(2U, 1U);
    check_init(16777216U, 0x00FFFFFFU);
    /* One count gives no tick, and more than 2^24 counts do not fit. */
    check_rejected(1U);
    check_rejected(16777217U);
    check_rejected(UINT32_MAX);
#else
    /* At 1.5 MHz, only even slots are whole counts. */
    check_init(2U, 2U);
    check_init(1000U, 1499U);
    /* 11184810 us is 16777215 counts; 11184812 us is 16777218, too many. */
    check_init(11184810U, 16777214U);
    check_rejected(1U);
    check_rejected(1001U);
    check_rejected(11184812U);
#endif
    check_rejected(0U);
}

static void test_tick_lifecycle(void) {
    zule_port_cortex_m_systick.csr = 0U;
    assert(zule_port_tick_init(1000U, NULL) == ZULE_PORT_RESULT_ERROR);
    assert(zule_port_cortex_m_systick.csr == 0U);

    /* The handler calls the callback. */
    ticks = 0U;
    assert(zule_port_tick_init(1000U, on_tick) == ZULE_PORT_RESULT_OK);
    SysTick_Handler();
    SysTick_Handler();
    assert(ticks == 2U);

    /* Ticks must be stopped before another init. */
    assert(zule_port_tick_init(1000U, on_tick) == ZULE_PORT_RESULT_ERROR);
    assert(zule_port_cortex_m_systick.csr == EXPECTED_CSR);

    /* Idle waits for an interrupt while ticks run. */
    waits = 0U;
    assert(zule_port_idle() == ZULE_PORT_RESULT_OK);
    assert(waits == 1U);

    /* Stop disables the counter and its interrupt, and clears a pending tick. */
    zule_port_cortex_m_scb.icsr = 0U;
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
    assert(zule_port_cortex_m_systick.csr == 0U);
    assert(zule_port_cortex_m_scb.icsr == ZULE_PORT_CORTEX_M_ICSR_PENDSTCLR);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);

    /* Without ticks, nothing would end the wait, so idle fails without waiting. */
    assert(zule_port_idle() == ZULE_PORT_RESULT_ERROR);
    assert(waits == 1U);

    /* Ticks restart after a stop. */
    assert(zule_port_tick_init(500U, on_tick) == ZULE_PORT_RESULT_OK);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
}

/* With the reference clock selected, tick init fails on a part without one,
 * which SYST_CALIB's NOREF bit reports. With the processor clock, the bit does
 * not matter. */
static void test_clock_availability(void) {
    zule_port_cortex_m_systick.csr = 0U;
    zule_port_cortex_m_systick.calib = ZULE_PORT_CORTEX_M_SYST_CALIB_NOREF;
#if ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK
    assert(zule_port_tick_init(1000U, on_tick) == ZULE_PORT_RESULT_ERROR);
    assert(zule_port_cortex_m_systick.csr == 0U);
#else
    assert(zule_port_tick_init(1000U, on_tick) == ZULE_PORT_RESULT_OK);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
#endif
    zule_port_cortex_m_systick.calib = 0U;
    assert(zule_port_tick_init(1000U, on_tick) == ZULE_PORT_RESULT_OK);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
}

#if ZULE_CONFIG_TIMEBASE
/* The timebase counts the period start, which the handler advances by one period,
 * plus the counts elapsed in the current period, from SysTick's down counter. As
 * on hardware, the counter raises the tick on reaching 0, one count before it
 * reloads, so a period starts at 0. A pending tick means a period began before
 * the handler ran, so a read adds a period. Reads leave the interrupt mask as
 * they found it. */
static void test_timebase(void) {
    assert(zule_port_time_hz() == 1000000U);
    /* A 1000 us slot is 1000 counts: tick init clears the counter to 0, and it
     * then counts from 999 down to 0. */
    assert(zule_port_tick_init(1000U, on_tick) == ZULE_PORT_RESULT_OK);
    zule_port_cortex_m_scb.icsr = 0U;
    assert(zule_port_cortex_m_systick.cvr == 0U);
    const uint32_t start = zule_port_time_get();
    zule_port_cortex_m_systick.cvr = 999U;
    assert(zule_port_time_get() == start + 1U);
    zule_port_cortex_m_systick.cvr = 899U;
    assert(zule_port_time_get() == start + 101U);
    zule_port_cortex_m_systick.cvr = 1U;
    assert(zule_port_time_get() == start + 999U);

    /* Reaching 0 starts the next period and raises the tick, which is pending
     * until the handler runs; the count keeps advancing by one either way. */
    zule_port_cortex_m_systick.cvr = 0U;
    zule_port_cortex_m_scb.icsr = ZULE_PORT_CORTEX_M_ICSR_PENDSTSET;
    assert(zule_port_time_get() == start + 1000U);
    zule_port_cortex_m_systick.cvr = 999U;
    assert(zule_port_time_get() == start + 1001U);
    zule_port_cortex_m_scb.icsr = 0U;
    SysTick_Handler();
    assert(zule_port_time_get() == start + 1001U);
    zule_port_cortex_m_systick.cvr = 0U;
    SysTick_Handler();
    assert(zule_port_time_get() == start + 2000U);

    /* Reads leave the mask as they found it, inside a critical section or not. */
    primask = 0U;
    (void)zule_port_time_get();
    assert(primask == 0U);
    primask = 1U;
    (void)zule_port_time_get();
    assert(primask == 1U);
    primask = 0U;

    /* The count wraps modulo 2^32: it keeps advancing by whole periods. */
    for (uint32_t tick = 0U; tick < 5000000U; ++tick) {
        SysTick_Handler();
    }
    const uint32_t expected = start + 2000U + (uint32_t)(5000000000ULL % 4294967296ULL);
    assert(zule_port_time_get() == expected);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
}
#endif

int main(void) {
    test_critical_sections();
#if ZULE_CONFIG_TIMEBASE
    test_timebase();
#endif
    test_clock_availability();
    test_tick_periods();
    test_tick_lifecycle();
    return 0;
}
