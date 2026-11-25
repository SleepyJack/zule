/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>

#include "zule.h"
#include "zule_modules_schedule_data.h"
#include "zule_modules_task_data.h"
#include "zule_schedule_definition.h"
#include "zule_scheduler.h"
#include "zule_task_stats.h"
#include "zule_test_port.h"

/* Built twice: with the task statistics module alone, and with the tick timing
 * module as well, so the modules share hook entry points. Times are scripted:
 * each tick and each task start reads a set time, and a task sets the time its
 * end reads. */

static uint32_t fault_calls;
static zule_fault_info_t last_fault;

static void handle_fault(const zule_fault_info_t *fault) {
    ++fault_calls;
    last_fault = *fault;
}

/* The time the running task's end reads, and whether it overruns by delivering
 * ticks to the end of its window. */
static uint32_t task_end_counts;
static bool task_overruns;

static void task(void) {
    if (task_overruns) {
        zule_test_port_control.time_counts = task_end_counts - 1U;
        zule_test_port_fire_tick();
        zule_test_port_fire_tick();
    }
    zule_test_port_control.time_counts = task_end_counts;
}

static const zule_task_t tasks[] = {
    {.id = 0U, .function = task},
    {.id = 1U, .function = task},
};
static const zule_window_t windows[] = {{&tasks[0], 0U, 2U}, {&tasks[1], 2U, 1U}};
/* A second schedule, with only task 1 at slot 0, to switch to. */
static const zule_window_t other_windows[] = {{&tasks[1], 0U, 1U}};
static const zule_schedule_t schedules[] = {
    {4U, windows, sizeof(windows) / sizeof(windows[0])},
    {2U, other_windows, sizeof(other_windows) / sizeof(other_windows[0])},
};

static const zule_schedule_definition_t definition = {
    .slot_us = 100U,
    .tasks = {tasks, sizeof(tasks) / sizeof(tasks[0])},
    .schedules = {schedules, sizeof(schedules) / sizeof(schedules[0])},
};

const zule_schedule_definition_t *zule_schedule_definition_get(void) {
    return &definition;
}

/* What the generated schedule defines: one entry per task. */
zule_modules_task_data_t *zule_modules_task_data_get(void) {
    static zule_modules_task_data_t data[sizeof(tasks) / sizeof(tasks[0])];
    return data;
}

/* What the generated schedule defines: one entry per schedule. */
zule_modules_schedule_data_t *zule_modules_schedule_data_get(void) {
    static zule_modules_schedule_data_t data[sizeof(schedules) / sizeof(schedules[0])];
    return data;
}

static void start(void) {
    zule_test_port_reset();
    zule_test_port_control.time_step = 0U;
    fault_calls = 0U;
    task_overruns = false;
    assert(zule_scheduler_init(handle_fault, 0U) == ZULE_RESULT_OK);
    assert(zule_port_tick_init(definition.slot_us, zule_scheduler_tick) == ZULE_PORT_RESULT_OK);
}

static void stop(void) {
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
}

/* Deliver a tick whose time reads, including any release, return time_counts. */
static void tick_at(uint32_t time_counts) {
    zule_test_port_control.time_counts = time_counts;
    zule_test_port_fire_tick();
}

/* Dispatch the pending task: its start reads start_counts and its end
 * end_counts. */
static void run_task(uint32_t start_counts, uint32_t end_counts) {
    zule_test_port_control.time_counts = start_counts;
    task_end_counts = end_counts;
    (void)zule_scheduler_run();
}

static void check_task(uint32_t task_id, uint64_t executions, uint64_t total_counts,
                       uint32_t min_counts, uint32_t max_counts, uint32_t max_latency_counts) {
    zule_task_stats_t stats;
    assert(zule_task_stats_get(task_id, &stats));
    assert(!zule_test_port_is_critical());
    assert(stats.executions == executions);
    assert(stats.total_execution_counts == total_counts);
    assert(stats.min_execution_counts == min_counts);
    assert(stats.max_execution_counts == max_counts);
    assert(stats.max_dispatch_latency_counts == max_latency_counts);
}

static void check_schedule_summary(uint32_t schedule_id, uint64_t cycles, uint64_t busy_counts,
                                   uint32_t max_cycle_busy_counts) {
    zule_task_stats_summary_t summary;
    assert(zule_task_stats_summary_get(schedule_id, &summary));
    assert(!zule_test_port_is_critical());
    assert(summary.cycles == cycles);
    assert(summary.busy_counts == busy_counts);
    assert(summary.max_cycle_busy_counts == max_cycle_busy_counts);
}

/* The totals for schedule 0, which every run starts with. */
static void check_summary(uint64_t cycles, uint64_t busy_counts, uint32_t max_cycle_busy_counts) {
    check_schedule_summary(0U, cycles, busy_counts, max_cycle_busy_counts);
}

/* Run one cycle from slot 0, with each task starting at its release plus the
 * given latency and running for the given time. */
static void run_cycle(uint32_t cycle_start_counts, uint32_t latency_counts,
                      uint32_t execution_counts) {
    tick_at(cycle_start_counts);
    run_task(cycle_start_counts + latency_counts,
             cycle_start_counts + latency_counts + execution_counts);
    tick_at(cycle_start_counts + 100U);
    tick_at(cycle_start_counts + 200U);
    run_task(cycle_start_counts + 200U + latency_counts,
             cycle_start_counts + 200U + latency_counts + execution_counts);
    tick_at(cycle_start_counts + 300U);
}

int main(void) {
    /* Storage is zero before any run, and IDs outside the schedule are
     * rejected without writing. */
    check_task(0U, 0U, 0U, 0U, 0U, 0U);
    zule_task_stats_t untouched = {.executions = 7U};
    assert(!zule_task_stats_get(2U, &untouched));
    assert(!zule_task_stats_get(UINT32_MAX, &untouched));
    assert(untouched.executions == 7U);
    check_summary(0U, 0U, 0U);
    check_schedule_summary(1U, 0U, 0U, 0U);
    zule_task_stats_summary_t untouched_summary = {.cycles = 7U};
    assert(!zule_task_stats_summary_get(2U, &untouched_summary));
    assert(!zule_task_stats_summary_get(UINT32_MAX, &untouched_summary));
    assert(untouched_summary.cycles == 7U);

    /* Execution times, dispatch latency, and cycle totals. */
    start();
    tick_at(1000U);
    run_task(1004U, 1030U);
    tick_at(1100U);
    tick_at(1200U);
    run_task(1201U, 1251U);
    tick_at(1300U);
    check_task(0U, 1U, 26U, 26U, 26U, 4U);
    check_task(1U, 1U, 50U, 50U, 50U, 1U);
    /* The first cycle completes when the second starts. */
    check_summary(0U, 76U, 0U);
    tick_at(1400U);
    check_summary(1U, 76U, 76U);
    run_task(1410U, 1420U);
    tick_at(1500U);
    tick_at(1600U);
    run_task(1600U, 1700U);
    tick_at(1700U);
    tick_at(1800U);
    check_task(0U, 2U, 36U, 10U, 26U, 10U);
    check_task(1U, 2U, 150U, 50U, 100U, 1U);
    check_summary(2U, 186U, 110U);

    /* Reset clears every task and the totals, but the cycle in progress is still
     * measured in full: task 0 ran before the reset, in the same cycle. */
    run_task(1805U, 1815U);
    zule_task_stats_reset();
    assert(!zule_test_port_is_critical());
    check_task(0U, 0U, 0U, 0U, 0U, 0U);
    check_summary(0U, 0U, 0U);
    tick_at(1900U);
    tick_at(2000U);
    run_task(2000U, 2020U);
    tick_at(2100U);
    tick_at(2200U);
    check_task(0U, 0U, 0U, 0U, 0U, 0U);
    check_task(1U, 1U, 20U, 20U, 20U, 0U);
    check_summary(1U, 20U, 30U);

    /* A run stopped mid-cycle leaves that cycle incomplete; a new run keeps the
     * results and starts its own first cycle. */
    run_task(2203U, 2290U);
    stop();
    start();
    run_cycle(5000U, 2U, 5U);
    tick_at(5400U);
    check_summary(2U, 20U + 87U + 10U, 30U);
    check_task(0U, 2U, 92U, 5U, 87U, 3U);
    stop();

    /* Intervals wrap with the timebase. */
    zule_task_stats_reset();
    start();
    tick_at(4294967290U);
    run_task(4294967295U, 9U);
    check_task(0U, 1U, 10U, 10U, 10U, 5U);
    stop();

    /* An overrunning execution counts, but its cycle never completes: after the
     * fault, ticks continue while the cycle count stops. */
    zule_task_stats_reset();
    start();
    tick_at(0U);
    task_overruns = true;
    run_task(1U, 250U);
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_EXECUTION_OVERRUN);
    check_task(0U, 1U, 249U, 249U, 249U, 1U);
    for (uint32_t tick = 0U; tick < 8U; ++tick) {
        tick_at(300U + (tick * 100U));
    }
    check_summary(0U, 249U, 0U);
    stop();

    /* A missed dispatch records no execution and no cycle. */
    zule_task_stats_reset();
    start();
    tick_at(0U);
    tick_at(100U);
    tick_at(200U);
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_DISPATCH_MISSED);
    tick_at(300U);
    tick_at(400U);
    check_task(0U, 0U, 0U, 0U, 0U, 0U);
    check_summary(0U, 0U, 0U);
    stop();

    /* Each schedule keeps its own totals. The cycle before a switch counts toward
     * the old schedule, and executions toward the schedule active when they
     * end. Task statistics continue across the switch, and reset clears every
     * schedule's totals. */
    zule_task_stats_reset();
    start();
    run_cycle(10000U, 0U, 10U);
    assert(zule_scheduler_schedule_request(1U) == ZULE_RESULT_OK);
    tick_at(10400U);
    check_schedule_summary(0U, 1U, 20U, 20U);
    check_schedule_summary(1U, 0U, 0U, 0U);
    run_task(10400U, 10430U);
    tick_at(10500U);
    tick_at(10600U);
    check_schedule_summary(0U, 1U, 20U, 20U);
    check_schedule_summary(1U, 1U, 30U, 30U);
    check_task(1U, 2U, 40U, 10U, 30U, 0U);
    stop();
    zule_task_stats_reset();
    check_schedule_summary(1U, 0U, 0U, 0U);
    return 0;
}
