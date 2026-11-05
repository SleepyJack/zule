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
static bool overrun;
static bool recurse;

static void first_task(void) {
    assert(!zule_test_port_is_critical());
    ++first_calls;
    if (recurse) {
        assert(zule_run() == ZULE_RESULT_ALREADY_RUNNING);
    }
    if (overrun) {
        zule_test_port_fire_tick();
        zule_test_port_fire_tick();
    }
}

static void second_task(void) {
    assert(!zule_test_port_is_critical());
    ++second_calls;
}

static const zule_window_t windows[] = {{first_task, 0U, 2U}, {second_task, 2U, 1U}};
static const zule_schedule_t schedule = {250U, 4U, windows, 2U};
static const zule_schedule_t *definition = &schedule;

const zule_schedule_t *zule_schedule_get(void) {
    return definition;
}

static void check_stopped(void) {
    assert(zule_test_port_control.stop_calls == 1U);
    assert(!zule_test_port_control.active);
    assert(!zule_test_port_is_critical());
}

int main(void) {
    zule_test_port_reset();
    definition = NULL;
    assert(zule_run() == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_test_port_control.init_calls == 0U);
    assert(zule_test_port_control.stop_calls == 0U);
    definition = &schedule;

    zule_test_port_reset();
    zule_test_port_control.init_result = ZULE_PORT_RESULT_ERROR;
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    assert(zule_test_port_control.init_calls == 1U);
    assert(zule_test_port_control.stop_calls == 0U);
    assert(!zule_test_port_control.active);

    zule_test_port_reset();
    recurse = true;
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    recurse = false;
    assert(first_calls == 1U && second_calls == 1U);
    assert(zule_test_port_control.slot_us == schedule.slot_us);
    check_stopped();

    zule_test_port_reset();
    zule_test_port_control.ticks_per_idle = 3U;
    assert(zule_run() == ZULE_RESULT_DISPATCH_MISSED);
    assert(first_calls == 1U && second_calls == 1U);
    check_stopped();

    zule_test_port_reset();
    overrun = true;
    assert(zule_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    overrun = false;
    assert(first_calls == 2U && second_calls == 1U);
    check_stopped();

    zule_test_port_reset();
    zule_test_port_control.idle_result = ZULE_PORT_RESULT_ERROR;
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    check_stopped();

    zule_test_port_reset();
    zule_test_port_control.ticks_per_idle = 3U;
    zule_test_port_control.stop_result = ZULE_PORT_RESULT_ERROR;
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    check_stopped();
    return 0;
}
