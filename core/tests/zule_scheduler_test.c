/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>

#include "zule_scheduler.h"
#include "zule_schedule_definition.h"
#include "zule_test_port.h"

static uint32_t first_calls;
static uint32_t second_calls;
static bool simulate_overrun;
static bool simulate_boundary_overrun;
static uint32_t fault_calls;
static zule_fault_info_t last_fault;

static void handle_fault(const zule_fault_info_t *fault) {
    ++fault_calls;
    last_fault = *fault;
}

static void first_task(void) {
    assert(!zule_test_port_is_critical());
    ++first_calls;
    if (simulate_overrun) {
        /* Simulate two timer interrupts while the task is executing. */
        assert(zule_scheduler_tick() == ZULE_RESULT_OK);
        assert(zule_scheduler_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    }
}

static void second_task(void) {
    assert(!zule_test_port_is_critical());
    ++second_calls;
    if (simulate_boundary_overrun) {
        assert(zule_scheduler_tick() == ZULE_RESULT_OK);
        assert(zule_scheduler_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    }
}

static const zule_window_t windows[] = {
    {first_task, 0U, 2U},
    {second_task, 2U, 1U},
};

static const zule_schedule_t schedule = {
    .slot_us = 100U,
    .cycle_slots = 4U,
    .windows = windows,
    .window_count = sizeof(windows) / sizeof(windows[0]),
};

static const zule_window_t boundary_windows[] = {
    {first_task, 0U, 2U},
    {second_task, 2U, 2U},
};

static const zule_schedule_t boundary_schedule = {
    .slot_us = 100U,
    .cycle_slots = 4U,
    .windows = boundary_windows,
    .window_count = sizeof(boundary_windows) / sizeof(boundary_windows[0]),
};

static const zule_schedule_t *schedule_definition = &schedule;

static const zule_schedule_t invalid_schedule = {
    .slot_us = 100U,
    .cycle_slots = 0U,
    .windows = windows,
    .window_count = sizeof(windows) / sizeof(windows[0]),
};

const zule_schedule_t *zule_schedule_get(void) {
    return schedule_definition;
}

int main(void) {
    assert(zule_scheduler_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_scheduler_run() == ZULE_RESULT_NOT_INITIALIZED);
    assert(first_calls == 0U && second_calls == 0U);

    schedule_definition = NULL;
    assert(zule_scheduler_init(NULL) == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_scheduler_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_scheduler_run() == ZULE_RESULT_NOT_INITIALIZED);
    assert(first_calls == 0U && second_calls == 0U);
    schedule_definition = &invalid_schedule;
    assert(zule_scheduler_init(NULL) == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_scheduler_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_scheduler_run() == ZULE_RESULT_NOT_INITIALIZED);
    schedule_definition = &schedule;

    assert(zule_scheduler_init(NULL) == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    assert(first_calls == 0U && second_calls == 0U);
    for (uint32_t cycle = 0U; cycle < 2U; ++cycle) {
        for (uint32_t slot = 0U; slot < schedule.cycle_slots; ++slot) {
            assert(zule_scheduler_tick() == ZULE_RESULT_OK);
            assert(zule_scheduler_run() == ZULE_RESULT_OK);
            assert(zule_scheduler_run() == ZULE_RESULT_OK);
            assert(first_calls == cycle + 1U);
            assert(second_calls == cycle + (slot >= 2U ? 1U : 0U));
        }
    }

    assert(zule_scheduler_init(NULL) == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(zule_scheduler_run() == ZULE_RESULT_DISPATCH_MISSED);
    assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(first_calls == 2U && second_calls == 2U);

    assert(zule_scheduler_init(NULL) == ZULE_RESULT_OK);
    simulate_overrun = true;
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(zule_scheduler_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(zule_scheduler_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(zule_scheduler_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(first_calls == 3U && second_calls == 2U);

    simulate_overrun = false;
    assert(zule_scheduler_init(NULL) == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    assert(first_calls == 4U && second_calls == 2U);

    /* A failed reinitialization must disable an already initialized scheduler. */
    schedule_definition = NULL;
    assert(zule_scheduler_init(NULL) == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_scheduler_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_scheduler_run() == ZULE_RESULT_NOT_INITIALIZED);
    assert(first_calls == 4U && second_calls == 2U);
    schedule_definition = &schedule;
    assert(zule_scheduler_init(NULL) == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    assert(!zule_test_port_is_critical());

    /* The tick that latches the fault must notify before main-loop polling. */
    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_DISPATCH_MISSED);
    assert(last_fault.window_index == 0U && last_fault.slot == 2U);
    assert(last_fault.cycle_count == 0U);
    assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(fault_calls == 1U);

    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_OK);
    simulate_overrun = true;
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    simulate_overrun = false;
    assert(fault_calls == 2U);
    assert(last_fault.reason == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(last_fault.cycle_count == 0U);
    assert(zule_scheduler_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(zule_scheduler_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(fault_calls == 2U);

    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(fault_calls == 3U);
    assert(last_fault.window_index == 1U && last_fault.slot == 3U);
    assert(last_fault.cycle_count == 0U);

    schedule_definition = NULL;
    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_scheduler_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(fault_calls == 3U);

    /* Idle slots still advance the cycle count; faulting ticks do not. */
    schedule_definition = &schedule;
    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_OK);
    for (uint32_t cycle = 0U; cycle < 3U; ++cycle) {
        for (uint32_t slot = 0U; slot < schedule.cycle_slots; ++slot) {
            assert(zule_scheduler_tick() == ZULE_RESULT_OK);
            assert(zule_scheduler_run() == ZULE_RESULT_OK);
        }
    }
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(last_fault.cycle_count == 3U);
    assert(last_fault.window_index == 0U && last_fault.slot == 2U);
    for (uint32_t tick = 0U; tick < 8U; ++tick) {
        assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    }
    assert(fault_calls == 4U && last_fault.cycle_count == 3U);

    /* A fault detected at slot zero reports the new cycle. */
    schedule_definition = &boundary_schedule;
    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_OK);
    for (uint32_t cycle = 0U; cycle < 2U; ++cycle) {
        for (uint32_t slot = 0U; slot < boundary_schedule.cycle_slots; ++slot) {
            assert(zule_scheduler_tick() == ZULE_RESULT_OK);
            assert(zule_scheduler_run() == ZULE_RESULT_OK);
        }
    }
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(fault_calls == 5U);
    assert(last_fault.cycle_count == 3U);
    assert(last_fault.window_index == 1U && last_fault.slot == 0U);

    /* Reinitialization resets the count, including for a boundary overrun. */
    assert(zule_scheduler_init(handle_fault) == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    assert(zule_scheduler_tick() == ZULE_RESULT_OK);
    simulate_boundary_overrun = true;
    assert(zule_scheduler_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    simulate_boundary_overrun = false;
    assert(fault_calls == 6U);
    assert(last_fault.reason == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(last_fault.cycle_count == 1U);
    assert(last_fault.window_index == 1U && last_fault.slot == 0U);
    return 0;
}
