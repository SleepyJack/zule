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
#include "zule_tick_tracker.h"

/* Built with ZULE_CONFIG_TICK_TOLERANCE_US=10 and the tick start hook. At the
 * default 1 MHz test timebase, a 100 us slot is 100 counts and the tolerance is
 * 10 counts. */

static uint32_t fault_calls;
static zule_fault_info_t last_fault;
static uint32_t hook_calls;
static uint32_t last_hook_counts;

static void handle_fault(const zule_fault_info_t *fault) {
    ++fault_calls;
    last_fault = *fault;
}

void zule_user_hook_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
    (void)slot;
    (void)cycle_count;
    ++hook_calls;
    last_hook_counts = time_counts;
}

static void task(void) {
}

static const zule_task_t tasks[] = {
    {.id = 0U, .function = task},
    {.id = 1U, .function = task},
};
static const zule_window_t windows[] = {{&tasks[0], 0U, 2U}, {&tasks[1], 2U, 1U}};
/* The first window starts at slot 2, so a timing fault can come before any task. */
static const zule_window_t late_windows[] = {{&tasks[0], 2U, 1U}};
static const zule_window_t single_window[] = {{&tasks[0], 0U, 1U}};

static zule_schedule_t schedule;
static zule_schedule_definition_t definition;

const zule_schedule_definition_t *zule_schedule_definition_get(void) {
    return &definition;
}

/* Use a single schedule. */
static void use_schedule(uint32_t slot_us, uint32_t cycle_slots, const zule_window_t *list,
                         size_t window_count) {
    schedule = (zule_schedule_t){
        .cycle_slots = cycle_slots,
        .windows = list,
        .window_count = window_count,
    };
    definition = (zule_schedule_definition_t){
        .slot_us = slot_us,
        .tasks = {tasks, sizeof(tasks) / sizeof(tasks[0])},
        .schedules = {&schedule, 1U},
    };
}

static void use_default_schedule(void) {
    use_schedule(100U, 4U, windows, sizeof(windows) / sizeof(windows[0]));
}

static void start(void) {
    zule_test_port_reset();
    zule_test_port_control.time_step = 0U;
    fault_calls = 0U;
    hook_calls = 0U;
    assert(zule_scheduler_init(handle_fault, 0U) == ZULE_RESULT_OK);
    assert(zule_port_tick_init(definition.slot_us, zule_scheduler_tick) == ZULE_PORT_RESULT_OK);
}

static void stop(void) {
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
}

/* Deliver a tick at a scripted time; optionally dispatch afterwards. */
static void tick_at(uint32_t time_counts, bool dispatch) {
    zule_test_port_control.time_counts = time_counts;
    const uint32_t reads_before = zule_test_port_control.time_reads;
    zule_test_port_fire_tick();
    /* The tick start hook and the tick check share a single read. */
    assert(zule_test_port_control.time_reads == reads_before + 1U);
    assert(last_hook_counts == time_counts);
    if (dispatch) {
        (void)zule_scheduler_run();
    }
}

static void check_fault(int32_t deviation_counts, uint32_t slot, uint32_t cycle_count) {
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_TICK_TIMING);
    assert(last_fault.tick_deviation_counts == deviation_counts);
    assert(last_fault.slot == slot);
    assert(last_fault.cycle_count == cycle_count);
    assert(zule_scheduler_run() == ZULE_RESULT_TICK_TIMING);
}

static void check_init(zule_result_t expected, uint32_t time_hz) {
    zule_test_port_reset();
    zule_test_port_control.time_hz = time_hz;
    assert(zule_scheduler_init(NULL, 0U) == expected);
}

static void test_init_limits(void) {
    use_default_schedule();
    check_init(ZULE_RESULT_OK, 1000000U);
    check_init(ZULE_RESULT_INVALID_CONFIG, 0U);

    /* A slot must be a whole number of counts: 100 us at 1.000001 MHz is not. */
    check_init(ZULE_RESULT_INVALID_CONFIG, 1000001U);
    check_init(ZULE_RESULT_OK, 1010000U);

    /* A slot must be shorter than 2^31 counts: 1 s at 2^31 Hz is exactly 2^31. */
    use_schedule(1000000U, 1U, single_window, 1U);
    check_init(ZULE_RESULT_INVALID_CONFIG, 2147483648U);
    check_init(ZULE_RESULT_OK, 2147483647U);

    /* The tolerance must be at least one count: 10 us at 50 kHz is half a count. */
    use_default_schedule();
    check_init(ZULE_RESULT_INVALID_CONFIG, 50000U);
    check_init(ZULE_RESULT_OK, 100000U);

    /* The tolerance must be less than one slot: a 10 us slot equals it. */
    use_schedule(10U, 4U, windows, sizeof(windows) / sizeof(windows[0]));
    check_init(ZULE_RESULT_INVALID_CONFIG, 1000000U);
    use_schedule(11U, 4U, windows, sizeof(windows) / sizeof(windows[0]));
    check_init(ZULE_RESULT_OK, 1000000U);

    /* With hooks enabled, a cycle must be shorter than 2^32 counts. */
    use_schedule(1073741824U, 4U, windows, sizeof(windows) / sizeof(windows[0]));
    check_init(ZULE_RESULT_INVALID_CONFIG, 1000000U);
    use_schedule(1073741823U, 4U, windows, sizeof(windows) / sizeof(windows[0]));
    check_init(ZULE_RESULT_OK, 1000000U);

    /* The limit applies to every schedule, not only the one that starts, since a
     * run may switch to any. At 1 MHz, 42949673 slots of 100 us reach 2^32 counts. */
    use_default_schedule();
    zule_schedule_t long_cycles[] = {schedule, schedule};
    long_cycles[1].cycle_slots = 42949673U;
    definition.schedules = (zule_schedule_table_t){long_cycles, 2U};
    check_init(ZULE_RESULT_INVALID_CONFIG, 1000000U);
    long_cycles[1].cycle_slots = 42949672U;
    check_init(ZULE_RESULT_OK, 1000000U);

    /* zule_run() reports the failure before starting ticks. */
    use_default_schedule();
    zule_test_port_reset();
    zule_test_port_control.time_hz = 1000001U;
    assert(zule_run() == ZULE_RESULT_INVALID_CONFIG);
    assert(zule_test_port_control.init_calls == 0U);
}

/* The signed form of a deviation, including the edges of the int32_t range. */
static void test_deviation_signed(void) {
    assert(zule_tick_deviation_signed((zule_tick_deviation_t){0U, false}) == 0);
    assert(zule_tick_deviation_signed((zule_tick_deviation_t){5U, false}) == -5);
    assert(zule_tick_deviation_signed((zule_tick_deviation_t){7U, true}) == 7);
    assert(zule_tick_deviation_signed((zule_tick_deviation_t){0x7FFFFFFFU, true}) == INT32_MAX);
    assert(zule_tick_deviation_signed((zule_tick_deviation_t){0x7FFFFFFFU, false}) == -INT32_MAX);
    assert(zule_tick_deviation_signed((zule_tick_deviation_t){0x80000000U, false}) == INT32_MIN);
}

int main(void) {
    test_deviation_signed();
    test_init_limits();

    /* Deviations up to the tolerance, early or late, are accepted. The first tick
     * is the reference. */
    use_default_schedule();
    start();
    tick_at(1000U, true);
    tick_at(1110U, true);
    tick_at(1190U, true);
    tick_at(1300U, true);
    tick_at(1400U, true);
    assert(fault_calls == 0U);
    assert(hook_calls == 5U);
    stop();

    /* A late tick. */
    start();
    tick_at(1000U, true);
    tick_at(1111U, true);
    check_fault(11, 1U, 0U);
    assert(last_fault.window_index == 0U && last_fault.task_id == 0U);
    tick_at(1500U, true); /* Faults latch: no second notification. */
    assert(fault_calls == 1U);
    stop();

    /* An early tick. */
    start();
    tick_at(1000U, true);
    tick_at(1089U, true);
    check_fault(-11, 1U, 0U);
    stop();

    /* Slow drift: each interval is close to a slot, but the absolute error grows. */
    start();
    tick_at(1000U, true);
    tick_at(1103U, true);
    tick_at(1206U, true);
    tick_at(1309U, true);
    assert(fault_calls == 0U);
    tick_at(1412U, true);
    check_fault(12, 0U, 1U);
    stop();

    /* A lost tick shows as a deviation of a whole slot. */
    start();
    tick_at(1000U, true);
    tick_at(1100U, true);
    tick_at(1300U, true);
    check_fault(100, 2U, 0U);
    stop();

    /* Expected times wrap with the timebase: late and early across rollover. */
    start();
    tick_at(4294967196U, true);
    tick_at(5U, true);
    assert(fault_calls == 0U);
    tick_at(120U, true);
    check_fault(20, 2U, 0U);
    stop();
    start();
    tick_at(4294967245U, true);
    tick_at(4294967280U, true);
    check_fault(-65, 1U, 0U);
    stop();

    /* Other faults report the deviation of the tick that detected them. */
    start();
    tick_at(1000U, false);
    tick_at(1105U, false);
    tick_at(1205U, false);
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_DISPATCH_MISSED);
    assert(last_fault.tick_deviation_counts == 5);
    stop();

    /* A timing fault before any task has been released names the active window. */
    use_schedule(100U, 4U, late_windows, 1U);
    start();
    tick_at(1000U, true);
    tick_at(1150U, true);
    check_fault(50, 1U, 0U);
    assert(last_fault.window_index == 0U && last_fault.task_id == 0U);
    stop();
    return 0;
}
