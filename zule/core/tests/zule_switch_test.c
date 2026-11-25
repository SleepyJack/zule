/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>

#include "zule_schedule_definition.h"
#include "zule_scheduler.h"
#include "zule_test_port.h"

/* Switching between schedules. Built twice: with no hooks, and with the tick
 * start, task release, and schedule switch hooks, which record the order of
 * events. */

static uint32_t fault_calls;
static zule_fault_info_t last_fault;

static void handle_fault(const zule_fault_info_t *fault) {
    ++fault_calls;
    last_fault = *fault;
}

/* Task IDs in the order the tasks ran. */
static uint32_t ran[16];
static size_t ran_count;

/* A request each task makes when it runs, if set. */
static bool request_in_task;
static uint32_t task_request_id;

/* Whether task b overruns, by delivering ticks to the end of the cycle. */
static bool b_overruns;

static void record(uint32_t task_id) {
    assert(!zule_test_port_is_critical());
    assert(ran_count < (sizeof(ran) / sizeof(ran[0])));
    ran[ran_count] = task_id;
    ++ran_count;
    if (request_in_task) {
        request_in_task = false;
        assert(zule_scheduler_schedule_request(task_request_id) == ZULE_RESULT_OK);
    }
}

static void task_a(void) {
    record(0U);
}

static void task_b(void) {
    record(1U);
    if (b_overruns) {
        assert(zule_scheduler_tick() == ZULE_RESULT_OK);
        assert(zule_scheduler_tick() == ZULE_RESULT_EXECUTION_OVERRUN);
    }
}

static void task_c(void) {
    record(2U);
}

static const zule_task_t tasks[] = {
    {.id = 0U, .function = task_a},
    {.id = 1U, .function = task_b},
    {.id = 2U, .function = task_c},
};

/* Schedule 0: a then b, whose window ends with the cycle. */
static const zule_window_t windows_0[] = {{&tasks[0], 0U, 2U}, {&tasks[1], 2U, 2U}};
/* Schedule 1: c, starting after slot 0. */
static const zule_window_t windows_1[] = {{&tasks[2], 1U, 1U}};
static const zule_schedule_t schedules[] = {
    {4U, windows_0, sizeof(windows_0) / sizeof(windows_0[0])},
    {3U, windows_1, sizeof(windows_1) / sizeof(windows_1[0])},
};
static const zule_schedule_definition_t definition = {
    .slot_us = 100U,
    .tasks = {tasks, sizeof(tasks) / sizeof(tasks[0])},
    .schedules = {schedules, sizeof(schedules) / sizeof(schedules[0])},
};

const zule_schedule_definition_t *zule_schedule_definition_get(void) {
    return &definition;
}

#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
typedef enum {
    EVENT_TICK_START,
    EVENT_SWITCH,
    EVENT_RELEASE,
} event_kind_t;

typedef struct {
    event_kind_t kind;
    /* The slot and cycle for a tick start, the schedule for a switch, and the
     * task and window for a release. */
    uint32_t first;
    uint32_t second;
} event_t;

static event_t events[64];
static size_t event_count;
static uint32_t switch_calls;

static void add_event(event_kind_t kind, uint32_t first, uint32_t second) {
    assert(event_count < (sizeof(events) / sizeof(events[0])));
    events[event_count] = (event_t){kind, first, second};
    ++event_count;
}

void zule_user_hook_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
    (void)time_counts;
    add_event(EVENT_TICK_START, slot, cycle_count);
}

void zule_user_hook_tick_task_release(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)time_counts;
    add_event(EVENT_RELEASE, task_id, (uint32_t)window_index);
}

void zule_user_hook_tick_schedule_switch(uint32_t time_counts, uint32_t schedule_id) {
    (void)time_counts;
    ++switch_calls;
    add_event(EVENT_SWITCH, schedule_id, 0U);
}

/* Check that the last events were those given, in order. */
static void check_last_events(const event_t *expected, size_t count) {
    assert(event_count >= count);
    for (size_t index = 0U; index < count; ++index) {
        const event_t *actual = &events[event_count - count + index];
        assert(actual->kind == expected[index].kind);
        assert(actual->first == expected[index].first);
        assert(actual->second == expected[index].second);
    }
}
#endif

static void start(uint32_t schedule_id) {
    zule_test_port_reset();
    fault_calls = 0U;
    ran_count = 0U;
    assert(zule_scheduler_init(handle_fault, schedule_id) == ZULE_RESULT_OK);
}

/* Deliver a tick, then dispatch any pending task. */
static void step(void) {
    (void)zule_scheduler_tick();
    (void)zule_scheduler_run();
}

static void steps(uint32_t count) {
    for (uint32_t index = 0U; index < count; ++index) {
        step();
    }
}

static void check_ran(const uint32_t *expected, size_t count) {
    assert(ran_count == count);
    for (size_t index = 0U; index < count; ++index) {
        assert(ran[index] == expected[index]);
    }
}

int main(void) {
    assert(zule_scheduler_schedule_request(1U) == ZULE_RESULT_NOT_INITIALIZED);

    /* A request from a task takes effect at the next cycle start, after the
     * current schedule's final window: a and b finish schedule 0's cycle, then
     * schedule 1's first window starts at its slot 1. */
    start(0U);
    request_in_task = true;
    task_request_id = 1U;
    steps(4U);
    const uint32_t first_cycle[] = {0U, 1U};
    check_ran(first_cycle, 2U);
#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
    assert(switch_calls == 0U);
#endif
    step();
#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
    /* The switch follows the tick start and precedes any release. */
    const event_t switch_events[] = {{EVENT_TICK_START, 0U, 1U}, {EVENT_SWITCH, 1U, 0U}};
    check_last_events(switch_events, 2U);
    assert(switch_calls == 1U);
#endif
    check_ran(first_cycle, 2U);
    steps(2U);
    const uint32_t after_switch[] = {0U, 1U, 2U};
    check_ran(after_switch, 3U);

    /* The last request wins: a request for the active schedule cancels a
     * pending switch. */
    assert(zule_scheduler_schedule_request(0U) == ZULE_RESULT_OK);
    assert(zule_scheduler_schedule_request(1U) == ZULE_RESULT_OK);
    steps(3U);
    const uint32_t stayed[] = {0U, 1U, 2U, 2U};
    check_ran(stayed, 4U);

    /* An unknown ID is rejected, keeping the earlier request. The new schedule's
     * first window starts at slot 0, so it is released in the switching tick. */
    assert(zule_scheduler_schedule_request(0U) == ZULE_RESULT_OK);
    assert(zule_scheduler_schedule_request(2U) == ZULE_RESULT_INVALID_CONFIG);
    assert(zule_scheduler_schedule_request(UINT32_MAX) == ZULE_RESULT_INVALID_CONFIG);
    step();
#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
    const event_t back_events[] = {
        {EVENT_TICK_START, 0U, 3U}, {EVENT_SWITCH, 0U, 0U}, {EVENT_RELEASE, 0U, 0U}};
    check_last_events(back_events, 3U);
    assert(switch_calls == 2U);
#endif
    const uint32_t switched_back[] = {0U, 1U, 2U, 2U, 0U};
    check_ran(switched_back, 5U);

    /* A fault reports the active schedule, its window, and the cycle count,
     * which continues across switches. */
    steps(3U);
    request_in_task = false;
    assert(zule_scheduler_schedule_request(1U) == ZULE_RESULT_OK);
    steps(1U);
    (void)zule_scheduler_tick();
    (void)zule_scheduler_tick();
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_DISPATCH_MISSED);
    assert(last_fault.schedule_id == 1U);
    assert(last_fault.window_index == 0U);
    assert(last_fault.slot == 2U);
    assert(last_fault.cycle_count == 4U);

    /* A run starts with its configured schedule and no request pending. */
    start(1U);
    assert(zule_scheduler_schedule_request(0U) == ZULE_RESULT_OK);
    start(1U);
    steps(6U);
    const uint32_t configured[] = {2U, 2U};
    check_ran(configured, 2U);
    assert(fault_calls == 0U);

    /* No switch takes place once a fault is latched: b overruns the end of its
     * window, which is the end of the cycle, so the fault is detected at slot 0
     * of the next cycle, before the requested switch. */
    start(0U);
    step();
    assert(zule_scheduler_schedule_request(1U) == ZULE_RESULT_OK);
#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
    const uint32_t switches_before = switch_calls;
#endif
    b_overruns = true;
    steps(2U);
    b_overruns = false;
    assert(fault_calls == 1U);
    assert(last_fault.reason == ZULE_RESULT_EXECUTION_OVERRUN);
    assert(last_fault.schedule_id == 0U);
    assert(last_fault.slot == 0U);
    steps(4U);
#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
    assert(switch_calls == switches_before);
#endif
    return 0;
}
