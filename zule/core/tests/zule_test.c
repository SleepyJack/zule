/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "zule.h"
#include "zule_schedule_definition.h"
#include "zule_test_port.h"

static uint32_t first_calls;
static uint32_t second_calls;
static bool overrun;
static bool recurse;
static bool task_running;
static bool fault_during_task;
static uint32_t fault_calls;
static zule_fault_info_t last_fault;

static void handle_fault(const zule_fault_info_t *fault) {
    assert(zule_test_port_is_critical());
    ++fault_calls;
    last_fault = *fault;
    fault_during_task = task_running;
}

static void unexpected_fault(const zule_fault_info_t *fault) {
    (void)fault;
    assert(false);
}

static void first_task(void) {
    assert(!zule_test_port_is_critical());
    ++first_calls;
    if (recurse) {
        assert(zule_run() == ZULE_RESULT_ALREADY_RUNNING);
        const zule_config_t disabled = {.on_fault = NULL};
        assert(zule_configure(&disabled) == ZULE_RESULT_ALREADY_RUNNING);
    }
    if (overrun) {
        task_running = true;
        zule_test_port_fire_tick();
        zule_test_port_fire_tick();
        task_running = false;
    }
}

static void second_task(void) {
    assert(!zule_test_port_is_critical());
    ++second_calls;
}

static const zule_task_t tasks[] = {
    {.id = 0U, .function = first_task},
    {.id = 1U, .function = second_task},
};

static const zule_window_t windows[] = {{&tasks[0], 0U, 2U}, {&tasks[1], 2U, 1U}};
static const zule_schedule_t schedule = {
    .cycle_slots = 4U,
    .windows = windows,
    .window_count = sizeof(windows) / sizeof(windows[0]),
};
static const zule_schedules_t schedules = {
    .slot_us = 250U,
    .tasks = tasks,
    .task_count = sizeof(tasks) / sizeof(tasks[0]),
    .schedules = &schedule,
    .schedule_count = 1U,
};
static const zule_schedules_t *definition = &schedules;

const zule_schedules_t *zule_schedules_get(void) {
    return definition;
}

static void check_stopped(void) {
    assert(zule_test_port_control.stop_calls == 1U);
    assert(!zule_test_port_control.active);
    assert(!zule_test_port_is_critical());
}

int main(void) {
    assert(zule_task_name_get(10U) == NULL);
    definition = NULL;
    assert(zule_task_name_get(UINT32_MAX) == NULL);
#if ZULE_CONFIG_TASK_NAMES
    const zule_task_t named_tasks[] = {{0U, first_task, "sample"}};
    const zule_window_t named_window[] = {{&named_tasks[0], 0U, 1U}};
    const zule_schedule_t named_schedule = {1U, named_window, 1U};
    const zule_schedules_t named = {100U, named_tasks, 1U, &named_schedule, 1U};
    assert(zule_schedules_validate(&named) == ZULE_RESULT_OK);
    definition = &named;
    assert(strcmp(zule_task_name_get(0U), "sample") == 0);
    assert(zule_task_name_get(1U) == NULL);
    assert(zule_task_name_get(UINT32_MAX) == NULL);
    /* An entry whose ID does not match its index is never returned. */
    const zule_task_t mismatched_tasks[] = {{1U, first_task, "mismatched"}};
    const zule_schedules_t mismatched = {100U, mismatched_tasks, 1U, &named_schedule, 1U};
    definition = &mismatched;
    assert(zule_task_name_get(0U) == NULL);
#endif
    definition = &schedules;

    assert(zule_configure(NULL) == ZULE_RESULT_INVALID_CONFIG);
    zule_test_port_reset();
    definition = NULL;
    assert(zule_run() == ZULE_RESULT_INVALID_SCHEDULE);
    assert(zule_test_port_control.init_calls == 0U);
    assert(zule_test_port_control.stop_calls == 0U);
    definition = &schedules;

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
    assert(zule_test_port_control.slot_us == schedules.slot_us);
    check_stopped();

    zule_config_t config = {.on_fault = handle_fault};
    assert(zule_configure(&config) == ZULE_RESULT_OK);
    /* Neither caller mutation nor invalid configuration may replace the copy. */
    config.on_fault = unexpected_fault;
    assert(zule_configure(NULL) == ZULE_RESULT_INVALID_CONFIG);

    zule_test_port_reset();
    zule_test_port_control.ticks_per_idle = 6U;
    assert(zule_run() == ZULE_RESULT_DISPATCH_MISSED);
    assert(first_calls == 1U && second_calls == 1U);
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_DISPATCH_MISSED);
    assert(last_fault.window_index == 0U && last_fault.slot == 2U);
    assert(last_fault.task_id == 0U);
    assert(last_fault.cycle_count == 0U);
    assert(!fault_during_task);
    check_stopped();

    zule_test_port_reset();
    overrun = true;
    recurse = true;
    assert(zule_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    overrun = false;
    recurse = false;
    assert(first_calls == 2U && second_calls == 1U);
    assert(fault_calls == 2U);
    assert(last_fault.reason == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(last_fault.window_index == 0U && last_fault.slot == 2U);
    assert(last_fault.task_id == 0U);
    assert(last_fault.cycle_count == 0U);
    assert(fault_during_task);
    check_stopped();

    zule_test_port_reset();
    zule_test_port_control.idle_result = ZULE_PORT_RESULT_ERROR;
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    assert(fault_calls == 2U);
    check_stopped();

    zule_test_port_reset();
    zule_test_port_control.ticks_per_idle = 3U;
    zule_test_port_control.stop_result = ZULE_PORT_RESULT_ERROR;
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    assert(fault_calls == 3U);
    assert(last_fault.reason == ZULE_RESULT_DISPATCH_MISSED);
    check_stopped();

    const zule_config_t disabled = {.on_fault = NULL};
    assert(zule_configure(&disabled) == ZULE_RESULT_OK);
    zule_test_port_reset();
    zule_test_port_control.ticks_per_idle = 3U;
    assert(zule_run() == ZULE_RESULT_DISPATCH_MISSED);
    assert(fault_calls == 3U);
    check_stopped();

    /* With multiple schedules, the configured ID selects the one that runs. The
     * second schedule runs only the second task. */
    static const zule_window_t second_only[] = {{&tasks[1], 0U, 1U}};
    const zule_schedule_t pair[] = {schedule, {2U, second_only, 1U}};
    const zule_schedules_t two = {250U, tasks, sizeof(tasks) / sizeof(tasks[0]), pair, 2U};
    definition = &two;
    zule_config_t second_schedule = {.on_fault = NULL, .schedule_id = 1U};
    assert(zule_configure(&second_schedule) == ZULE_RESULT_OK);
    const uint32_t first_before = first_calls;
    const uint32_t second_before = second_calls;
    zule_test_port_reset();
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    assert(first_calls == first_before);
    assert(second_calls > second_before);
    check_stopped();

    /* An ID outside the set is rejected, keeping the previous configuration. */
    zule_config_t outside = {.on_fault = NULL, .schedule_id = 2U};
    assert(zule_configure(&outside) == ZULE_RESULT_INVALID_CONFIG);
    zule_test_port_reset();
    assert(zule_run() == ZULE_RESULT_PORT_ERROR);
    assert(first_calls == first_before);
    check_stopped();

    /* If the schedules no longer include the configured ID, the run fails before
     * ticks start. */
    definition = &schedules;
    zule_test_port_reset();
    assert(zule_run() == ZULE_RESULT_INVALID_CONFIG);
    assert(zule_test_port_control.init_calls == 0U);
    return 0;
}
