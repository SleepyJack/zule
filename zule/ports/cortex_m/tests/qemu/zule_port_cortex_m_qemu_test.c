/* Target tests of the Cortex-M port, in QEMU's mps2-an385 machine. The port
 * drives the emulated SysTick, PRIMASK, and WFI, so these check what the host
 * tests cannot: that ticks arrive at the programmed rate, that critical sections
 * hold them off, that idle wakes for them, and that stopping leaves none behind.
 * The last test runs the real scheduler on the port. Each failure prints its
 * reason and fails the run. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zule.h"
#include "zule_port.h"
#include "zule_target.h"
#include "zule_task_ids.h"

/* A slot of 40 us is 1000 SysTick counts at the machine's 25 MHz. */
#define SLOT_US 40U
#define SLOT_COUNTS 1000U

/* The machine's first CMSDK APB timer, an independent reference clock at the same
 * 25 MHz. It counts down from its reload value. */
#define REFERENCE_TIMER_CTRL (*(volatile uint32_t *)0x40000000U)
#define REFERENCE_TIMER_VALUE (*(volatile uint32_t *)0x40000004U)
#define REFERENCE_TIMER_RELOAD (*(volatile uint32_t *)0x40000008U)

/* SysTick's control and status register, to check that ticks are stopped. */
#define SYST_CSR (*(volatile uint32_t *)0xE000E010U)

static bool failed;

static void check(bool condition, const char *message) {
    if (!condition) {
        zule_target_write("FAIL: ");
        zule_target_write(message);
        zule_target_write("\n");
        failed = true;
    }
}

static uint32_t reference_now(void) {
    return REFERENCE_TIMER_VALUE;
}

/* Busy-wait for a number of reference counts, without touching the port. */
static void wait_counts(uint32_t counts) {
    const uint32_t start = reference_now();
    while ((start - reference_now()) < counts) {
    }
}

/* Tick callback for the port tests: counts ticks, and records the reference time
 * of two chosen ticks to measure the period between them. */
static volatile uint32_t ticks;
static volatile uint32_t first_timed_tick_counts;
static volatile uint32_t last_timed_tick_counts;
#define FIRST_TIMED_TICK 1U
#define LAST_TIMED_TICK 101U

static zule_result_t on_tick(void) {
    const uint32_t now = reference_now();
    ++ticks;
    if (ticks == FIRST_TIMED_TICK) {
        first_timed_tick_counts = now;
    } else if (ticks == LAST_TIMED_TICK) {
        last_timed_tick_counts = now;
    } else {
        /* Untimed tick. */
    }
    return ZULE_RESULT_OK;
}

static void start_ticks(void) {
    ticks = 0U;
    check(zule_port_tick_init(SLOT_US, on_tick) == ZULE_PORT_RESULT_OK, "tick init");
}

static void stop_ticks(void) {
    check(zule_port_tick_stop() == ZULE_PORT_RESULT_OK, "tick stop");
}

static void wait_ticks(uint32_t count) {
    while (ticks < count) {
    }
}

/* Ticks arrive once per slot: 100 periods measure 100 slots on the reference
 * clock. An off-by-one reload value would be 100 counts out. */
static void test_tick_rate(void) {
    start_ticks();
    wait_ticks(LAST_TIMED_TICK);
    stop_ticks();
    const uint32_t measured = first_timed_tick_counts - last_timed_tick_counts;
    const uint32_t expected = (LAST_TIMED_TICK - FIRST_TIMED_TICK) * SLOT_COUNTS;
    zule_target_write("Tick rate: ");
    zule_target_write_number(measured);
    zule_target_write(" counts for 100 ticks, expected ");
    zule_target_write_number(expected);
    zule_target_write("\n");
    check((measured >= (expected - 10U)) && (measured <= (expected + 10U)), "tick rate");
}

/* A critical section holds ticks off. SysTick has one pending flag, so however
 * many periods pass, one tick runs when the section ends. Nested sections hold
 * ticks until the outermost ends. */
static void test_critical_sections(void) {
    start_ticks();
    wait_ticks(1U);

    zule_port_critical_state_t outer = zule_port_critical_enter();
    const uint32_t before = ticks;
    wait_counts(5U * SLOT_COUNTS);
    check(ticks == before, "a critical section holds ticks off");
    zule_port_critical_exit(outer);
    check(ticks == (before + 1U), "one held tick runs when the section ends");

    outer = zule_port_critical_enter();
    const zule_port_critical_state_t inner = zule_port_critical_enter();
    const uint32_t nested_before = ticks;
    wait_counts(2U * SLOT_COUNTS);
    zule_port_critical_exit(inner);
    check(ticks == nested_before, "an inner exit keeps ticks held");
    zule_port_critical_exit(outer);
    check(ticks == (nested_before + 1U), "the outer exit releases the held tick");
    stop_ticks();
}

/* Idle, called inside a critical section as the core calls it, waits until a
 * tick is due; the tick runs when the section ends. Waiting repeatedly loses no
 * tick. */
static void test_idle(void) {
    start_ticks();
    wait_ticks(1U);
    const uint32_t before = ticks;
    for (uint32_t wait = 0U; wait < 100U; ++wait) {
        const zule_port_critical_state_t state = zule_port_critical_enter();
        check(zule_port_idle() == ZULE_PORT_RESULT_OK, "idle while ticks run");
        zule_port_critical_exit(state);
    }
    check(ticks == (before + 100U), "each idle ends with one tick");
    stop_ticks();
    check(zule_port_idle() == ZULE_PORT_RESULT_ERROR, "idle without ticks fails");
}

/* Stop leaves no tick behind, even one already pending, and ticks restart. */
static void test_stop(void) {
    start_ticks();
    wait_ticks(1U);
    const zule_port_critical_state_t state = zule_port_critical_enter();
    wait_counts(2U * SLOT_COUNTS);
    stop_ticks();
    const uint32_t before = ticks;
    zule_port_critical_exit(state);
    check(ticks == before, "a tick pending at stop never runs");
    wait_counts(5U * SLOT_COUNTS);
    check(ticks == before, "no ticks after stop");
    check((SYST_CSR & 0x3U) == 0U, "SysTick and its interrupt are off after stop");

    start_ticks();
    wait_ticks(3U);
    stop_ticks();
}

/* The real scheduler on the port: tasks run each cycle until the first task
 * overruns its window on purpose, and zule_run() returns the fault with ticks
 * stopped. */
#define CYCLES_BEFORE_OVERRUN 20U
static uint32_t first_runs;
static uint32_t second_runs;
static zule_fault_info_t fault;

void zule_task_first(void) {
    ++first_runs;
    if (first_runs > CYCLES_BEFORE_OVERRUN) {
        /* Run past the one-slot window: three slots of 2500 counts. */
        wait_counts(3U * 2500U);
    }
}

void zule_task_second(void) {
    ++second_runs;
}

static void handle_fault(const zule_fault_info_t *info) {
    fault = *info;
}

static void test_scheduler(void) {
    const zule_config_t config = {.on_fault = handle_fault};
    check(zule_configure(&config) == ZULE_RESULT_OK, "configure");
    const zule_result_t result = zule_run();
    check(result == ZULE_RESULT_EXECUTION_OVERRUN, "zule_run() returns the overrun");
    check(fault.reason == ZULE_RESULT_EXECUTION_OVERRUN, "the fault handler sees the overrun");
    check(fault.task_id == (uint32_t)ZULE_TASK_ID_FIRST, "the fault names the overrunning task");
    check(first_runs == (CYCLES_BEFORE_OVERRUN + 1U), "the first task ran each cycle");
    check(second_runs == CYCLES_BEFORE_OVERRUN, "the second task ran each cycle");
    check((SYST_CSR & 0x3U) == 0U, "ticks are stopped when zule_run() returns");
}

int main(void) {
    REFERENCE_TIMER_RELOAD = UINT32_MAX;
    REFERENCE_TIMER_VALUE = UINT32_MAX;
    REFERENCE_TIMER_CTRL = 1U;

    test_tick_rate();
    test_critical_sections();
    test_idle();
    test_stop();
    test_scheduler();

    zule_target_write(failed ? "Cortex-M port target tests failed\n"
                             : "Cortex-M port target tests passed\n");
    return failed ? 1 : 0;
}
