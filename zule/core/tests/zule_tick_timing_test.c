/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>

#include "zule.h"
#include "zule_schedule_definition.h"
#include "zule_scheduler.h"
#include "zule_test_port.h"
#include "zule_tick_timing.h"

/* Built twice: with the tick timing module alone, and with the core tick timing
 * check as well (10 us tolerance). At the default 1 MHz test timebase, a 100 us
 * slot is 100 counts. */

static uint32_t fault_calls;
static zule_fault_info_t last_fault;

static void handle_fault(const zule_fault_info_t *fault) {
    ++fault_calls;
    last_fault = *fault;
}

static void task(void) {
}

static const zule_task_t tasks[] = {
    {.id = 0U, .function = task},
    {.id = 1U, .function = task},
};
static const zule_window_t windows[] = {{&tasks[0], 0U, 2U}, {&tasks[1], 2U, 1U}};
static const zule_schedule_t schedule = {
    .slot_us = 100U,
    .cycle_slots = 4U,
    .windows = windows,
    .window_count = sizeof(windows) / sizeof(windows[0]),
    .tasks = tasks,
    .task_count = sizeof(tasks) / sizeof(tasks[0]),
};

const zule_schedule_t *zule_schedule_get(void) {
    return &schedule;
}

static void start(void) {
    zule_test_port_reset();
    fault_calls = 0U;
    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_OK);
    assert(zule_port_tick_init(schedule.slot_us, zule_scheduler_tick) == ZULE_PORT_RESULT_OK);
}

static void stop(void) {
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
}

/* Deliver a tick that starts at a scripted time and whose handler takes
 * tick_counts: the tick start read returns time_counts and the tick end read
 * returns time_counts + tick_counts. */
static void tick_at(uint32_t time_counts, uint32_t tick_counts) {
    zule_test_port_control.time_counts = time_counts;
    zule_test_port_control.time_step = tick_counts;
    const uint32_t reads_before = zule_test_port_control.time_reads;
    zule_test_port_fire_tick();
    /* One read at tick start, shared with the core check when enabled, and one
     * at tick end. */
    assert(zule_test_port_control.time_reads == reads_before + 2U);
    (void)zule_scheduler_run();
}

static void check_timing(uint32_t ticks, uint32_t max_late_counts, uint32_t max_early_counts,
                         uint32_t max_tick_counts) {
    zule_tick_timing_t timing;
    zule_tick_timing_get(&timing);
    assert(!zule_test_port_is_critical());
    assert(timing.ticks == ticks);
    assert(timing.max_late_counts == max_late_counts);
    assert(timing.max_early_counts == max_early_counts);
    assert(timing.max_tick_counts == max_tick_counts);
}

int main(void) {
    /* Worst late and early deviations, and the longest tick. The reference tick
     * is not measured for deviation, but its duration counts. */
    start();
    tick_at(1000U, 3U);
    check_timing(0U, 0U, 0U, 3U);
    tick_at(1105U, 2U);
    tick_at(1195U, 9U);
    tick_at(1290U, 1U);
    tick_at(1408U, 4U);
    check_timing(4U, 8U, 10U, 9U);
    assert(fault_calls == 0U);

    /* Reset clears results but keeps the run's reference. */
    zule_tick_timing_reset();
    check_timing(0U, 0U, 0U, 0U);
    tick_at(1500U, 2U);
    check_timing(1U, 0U, 0U, 2U);
    stop();

    /* A new run starts a new reference and keeps accumulated results. */
    start();
    tick_at(5000U, 1U);
    check_timing(1U, 0U, 0U, 2U);
    tick_at(5107U, 1U);
    check_timing(2U, 7U, 0U, 2U);
    stop();

    /* Expected times wrap with the timebase. */
    start();
    zule_tick_timing_reset();
    tick_at(4294967196U, 1U);
    tick_at(3U, 1U);
    tick_at(95U, 1U);
    check_timing(2U, 3U, 5U, 1U);
    stop();

    /* A stall of 2^31 counts past the expected time is the documented limit: it
     * reads as the most negative deviation, INT32_MIN, whose magnitude of 2^31
     * must be reported without overflow. */
    start();
    zule_tick_timing_reset();
    tick_at(0U, 1U);
    tick_at(100U + 2147483648U, 1U);
    check_timing(1U, 0U, 2147483648U, 1U);
    stop();

    /* The module requires a slot of whole timebase counts; zule_run() reports the
     * failure before starting ticks. */
    zule_test_port_reset();
    zule_test_port_control.time_hz = 1000001U;
    assert(zule_scheduler_init(NULL) == ZULE_RESULT_INVALID_CONFIG);
    assert(zule_run() == ZULE_RESULT_INVALID_CONFIG);
    assert(zule_test_port_control.init_calls == 0U);

#if ZULE_CONFIG_TICK_CHECK
    /* The module and the core check measure the same deviation from the same
     * time read, and the module records the tick that faults. */
    start();
    zule_tick_timing_reset();
    tick_at(1000U, 1U);
    tick_at(1111U, 1U);
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_TICK_TIMING);
    assert(last_fault.tick_deviation_counts == 11);
    check_timing(1U, 11U, 0U, 1U);
    stop();
#endif
    return 0;
}
