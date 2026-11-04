/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>

#include "zule.h"
#include "zule_schedule_definition.h"
#include "zule_test_port.h"

static uint32_t first_calls;
static uint32_t second_calls;
static bool simulate_overrun;

static void first_task(void) {
    assert(!zule_test_port_is_critical());
    ++first_calls;
    if (simulate_overrun) {
        /* Simulate two timer interrupts while the task is executing. */
        assert(zule_tick() == ZULE_RESULT_OK);
        assert(zule_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    }
}

static void second_task(void) {
    assert(!zule_test_port_is_critical());
    ++second_calls;
}

static const zule_window_t windows[] = {
    {first_task, 0U, 2U},
    {second_task, 2U, 1U},
};

static const zule_schedule_t schedule = {
    .cycle_slots = 4U,
    .windows = windows,
    .window_count = sizeof(windows) / sizeof(windows[0]),
};

static const zule_schedule_t *schedule_definition = &schedule;

static const zule_schedule_t invalid_schedule = {
    .cycle_slots = 0U,
    .windows = windows,
    .window_count = sizeof(windows) / sizeof(windows[0]),
};

const zule_schedule_t *zule_schedule_get(void) {
    return schedule_definition;
}

int main(void) {
    assert(zule_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_run() == ZULE_RESULT_NOT_INITIALIZED);
    assert(first_calls == 0U && second_calls == 0U);

    schedule_definition = NULL;
    assert(zule_init() == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_run() == ZULE_RESULT_NOT_INITIALIZED);
    assert(first_calls == 0U && second_calls == 0U);
    schedule_definition = &invalid_schedule;
    assert(zule_init() == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_run() == ZULE_RESULT_NOT_INITIALIZED);
    schedule_definition = &schedule;

    assert(zule_init() == ZULE_RESULT_OK);
    assert(zule_run() == ZULE_RESULT_OK);
    assert(first_calls == 0U && second_calls == 0U);
    for (uint32_t cycle = 0U; cycle < 2U; ++cycle) {
        for (uint32_t slot = 0U; slot < schedule.cycle_slots; ++slot) {
            assert(zule_tick() == ZULE_RESULT_OK);
            assert(zule_run() == ZULE_RESULT_OK);
            assert(zule_run() == ZULE_RESULT_OK);
            assert(first_calls == cycle + 1U);
            assert(second_calls == cycle + (slot >= 2U ? 1U : 0U));
        }
    }

    assert(zule_init() == ZULE_RESULT_OK);
    assert(zule_tick() == ZULE_RESULT_OK);
    assert(zule_tick() == ZULE_RESULT_OK);
    assert(zule_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(zule_run() == ZULE_RESULT_DISPATCH_MISSED);
    assert(zule_tick() == ZULE_RESULT_DISPATCH_MISSED);
    assert(first_calls == 2U && second_calls == 2U);

    assert(zule_init() == ZULE_RESULT_OK);
    simulate_overrun = true;
    assert(zule_tick() == ZULE_RESULT_OK);
    assert(zule_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(zule_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(zule_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(zule_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(first_calls == 3U && second_calls == 2U);

    simulate_overrun = false;
    assert(zule_init() == ZULE_RESULT_OK);
    assert(zule_tick() == ZULE_RESULT_OK);
    assert(zule_run() == ZULE_RESULT_OK);
    assert(first_calls == 4U && second_calls == 2U);

    /* A failed reinitialization must disable an already initialized scheduler. */
    schedule_definition = NULL;
    assert(zule_init() == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_tick() == ZULE_RESULT_NOT_INITIALIZED);
    assert(zule_run() == ZULE_RESULT_NOT_INITIALIZED);
    assert(first_calls == 4U && second_calls == 2U);
    schedule_definition = &schedule;
    assert(zule_init() == ZULE_RESULT_OK);
    assert(zule_run() == ZULE_RESULT_OK);
    assert(!zule_test_port_is_critical());
    return 0;
}
