/* Target tests of the Cortex-M port, in QEMU's mps2-an385 machine. The port
 * drives the emulated SysTick, PRIMASK, and WFI, so these check what the host
 * tests cannot: that ticks arrive at the programmed rate, that critical sections
 * hold them off, that idle wakes for them, and that stopping leaves none behind.
 * The last test runs the real scheduler on the port. Each failure prints its
 * reason and fails the run. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "zule.h"
#include "zule_port.h"
#include "zule_schedule_ids.h"
#include "zule_task_ids.h"

#if ZULE_CONFIG_MODULE_TASK_STATS
#include "zule_task_stats.h"
#endif
#if ZULE_CONFIG_MODULE_TICK_TIMING
#include "zule_tick_timing.h"
#endif

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
        (void)printf("FAIL: %s\n", message);
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
    (void)printf("Tick rate: %lu counts for 100 ticks, expected %lu\n", (unsigned long)measured,
                 (unsigned long)expected);
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

#if ZULE_CONFIG_TIMEBASE
/* Read the timebase and the reference clock together, with the tick held off,
 * so both describe the same moment. */
static void read_both(uint32_t *time_counts, uint32_t *reference_counts) {
    const zule_port_critical_state_t state = zule_port_critical_enter();
    *time_counts = zule_port_time_get();
    *reference_counts = reference_now();
    zule_port_critical_exit(state);
}

static void check_elapsed(uint32_t time_elapsed, uint32_t reference_elapsed, const char *message) {
    /* Both clocks run at 25 MHz; allow for the instructions between the reads. */
    const uint32_t difference = (time_elapsed > reference_elapsed)
                                    ? (time_elapsed - reference_elapsed)
                                    : (reference_elapsed - time_elapsed);
    check(difference <= 20U, message);
}

/* The timebase runs at its stated frequency: over 100 periods, it advances as
 * much as the independent reference clock. */
static void test_timebase_rate(void) {
    check(zule_port_time_hz() == 25000000U, "the timebase frequency is SysTick's clock");
    start_ticks();
    wait_ticks(1U);
    uint32_t time0;
    uint32_t reference0;
    read_both(&time0, &reference0);
    wait_ticks(101U);
    uint32_t time1;
    uint32_t reference1;
    read_both(&time1, &reference1);
    stop_ticks();
    check_elapsed(time1 - time0, reference0 - reference1, "the timebase rate");
}

/* Reads never go backwards and never jump, even when a reload happens in the
 * middle of a read: back-to-back reads over 2000 periods differ by far less than
 * the 1000-count period that a misread reload would add or lose. */
static void test_timebase_monotonic(void) {
    start_ticks();
    uint32_t previous = zule_port_time_get();
    uint32_t largest_step = 0U;
    uint32_t reads = 0U;
    while (ticks < 2000U) {
        const uint32_t now = zule_port_time_get();
        const uint32_t step = now - previous;
        if (step > largest_step) {
            largest_step = step;
        }
        previous = now;
        ++reads;
    }
    stop_ticks();
    (void)printf("Timebase: %lu reads over 2000 periods, largest step %lu counts\n",
                 (unsigned long)reads, (unsigned long)largest_step);
    check(largest_step < 500U, "timebase reads never go backwards or jump");
}

/* With the tick held off past a reload, the pending tick still counts the period
 * that ended, so the timebase keeps up with the reference clock. */
static void test_timebase_masked(void) {
    start_ticks();
    wait_ticks(1U);
    const zule_port_critical_state_t state = zule_port_critical_enter();
    const uint32_t time0 = zule_port_time_get();
    const uint32_t reference0 = reference_now();
    wait_counts((3U * SLOT_COUNTS) / 2U);
    const uint32_t time1 = zule_port_time_get();
    const uint32_t reference1 = reference_now();
    zule_port_critical_exit(state);
    stop_ticks();
    check_elapsed(time1 - time0, reference0 - reference1,
                  "the timebase counts a reload while the tick is held off");
}
#endif

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

/* User hooks count their calls; with every feature enabled, the core calls them
 * from the tick and around each task. */
#if ZULE_CONFIG_USER_HOOK_TICK_START
static uint32_t hook_tick_starts;
void zule_user_hook_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
    (void)time_counts;
    (void)slot;
    (void)cycle_count;
    ++hook_tick_starts;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_END
static uint32_t hook_tick_ends;
void zule_user_hook_tick_end(uint32_t time_counts) {
    (void)time_counts;
    ++hook_tick_ends;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
static uint32_t hook_releases;
void zule_user_hook_tick_task_release(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)time_counts;
    (void)task_id;
    (void)window_index;
    ++hook_releases;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
static uint32_t hook_schedule_switches;
void zule_user_hook_tick_schedule_switch(uint32_t time_counts, uint32_t schedule_id) {
    (void)time_counts;
    (void)schedule_id;
    ++hook_schedule_switches;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_START
static uint32_t hook_task_starts;
void zule_user_hook_task_start(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)time_counts;
    (void)task_id;
    (void)window_index;
    ++hook_task_starts;
}
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_END
static uint32_t hook_task_ends;
void zule_user_hook_task_end(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)time_counts;
    (void)task_id;
    (void)window_index;
    ++hook_task_ends;
}
#endif

static void test_scheduler(void) {
    const zule_config_t config = {
        .on_fault = handle_fault,
        .schedule_id = ZULE_SCHEDULE_ID_MAIN,
    };
    check(zule_configure(&config) == ZULE_RESULT_OK, "configure");
    const zule_result_t result = zule_run();
    check(result == ZULE_RESULT_EXECUTION_OVERRUN, "zule_run() returns the overrun");
    check(fault.reason == ZULE_RESULT_EXECUTION_OVERRUN, "the fault handler sees the overrun");
    check(fault.task_id == (uint32_t)ZULE_TASK_ID_FIRST, "the fault names the overrunning task");
    check(fault.schedule_id == (uint32_t)ZULE_SCHEDULE_ID_MAIN, "the fault names the schedule");
    check(first_runs == (CYCLES_BEFORE_OVERRUN + 1U), "the first task ran each cycle");
    check(second_runs == CYCLES_BEFORE_OVERRUN, "the second task ran each cycle");
    check((SYST_CSR & 0x3U) == 0U, "ticks are stopped when zule_run() returns");

#if ZULE_CONFIG_TIMEBASE
    /* With the timebase, every feature runs: the hooks, the tick timing check,
     * which must not fault, and both modules. */
    const uint32_t task_runs = first_runs + second_runs;
#if ZULE_CONFIG_USER_HOOK_TASK_START && ZULE_CONFIG_USER_HOOK_TASK_END
    check(hook_task_starts == task_runs, "the task start hook runs for each task");
    check(hook_task_ends == task_runs, "the task end hook runs for each task");
#endif
#if ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
    check(hook_releases >= task_runs, "the release hook runs for each window");
#endif
#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
    check(hook_schedule_switches == 0U, "a single schedule never switches");
#endif
#if ZULE_CONFIG_USER_HOOK_TICK_START && ZULE_CONFIG_USER_HOOK_TICK_END
    check((hook_tick_starts > 0U) && (hook_tick_starts == hook_tick_ends),
          "the tick hooks run in pairs");
#endif
#if ZULE_CONFIG_MODULE_TASK_STATS
    zule_task_stats_t stats;
    check(zule_task_stats_get((uint32_t)ZULE_TASK_ID_FIRST, &stats), "first task statistics");
    check(stats.executions == (CYCLES_BEFORE_OVERRUN + 1U), "first task executions");
    check(stats.max_execution_counts >= (3U * 2500U), "the overrun is the longest execution");
    check(zule_task_stats_get((uint32_t)ZULE_TASK_ID_SECOND, &stats), "second task statistics");
    check(stats.executions == CYCLES_BEFORE_OVERRUN, "second task executions");
    zule_task_stats_summary_t summary;
    check(zule_task_stats_summary_get((uint32_t)ZULE_SCHEDULE_ID_MAIN, &summary),
          "schedule totals");
    check(summary.cycles == CYCLES_BEFORE_OVERRUN, "completed cycles");
#endif
#if ZULE_CONFIG_MODULE_TICK_TIMING
    zule_tick_timing_t timing;
    zule_tick_timing_get(&timing);
    (void)printf("Tick timing: worst late %lu, worst early %lu, longest tick %lu counts\n",
                 (unsigned long)timing.max_late_counts, (unsigned long)timing.max_early_counts,
                 (unsigned long)timing.max_tick_counts);
    check(timing.ticks > 0U, "the tick timing module measures ticks");
    check((timing.max_late_counts < 250U) && (timing.max_early_counts < 250U),
          "ticks are within the tick check's tolerance");
#endif
    (void)task_runs;
#endif
}

int main(void) {
    REFERENCE_TIMER_RELOAD = UINT32_MAX;
    REFERENCE_TIMER_VALUE = UINT32_MAX;
    REFERENCE_TIMER_CTRL = 1U;

    test_tick_rate();
    test_critical_sections();
    test_idle();
    test_stop();
#if ZULE_CONFIG_TIMEBASE
    test_timebase_rate();
    test_timebase_monotonic();
    test_timebase_masked();
#endif
    test_scheduler();

    (void)fputs(failed ? "Cortex-M port target tests failed\n"
                       : "Cortex-M port target tests passed\n",
                stdout);
    return failed ? 1 : 0;
}
