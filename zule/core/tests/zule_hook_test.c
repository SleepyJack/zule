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

typedef enum {
    EVENT_TICK_START,
    EVENT_TICK_END,
    EVENT_TICK_TASK_RELEASE,
    EVENT_TASK_START,
    EVENT_TASK_END,
} event_type_t;

typedef struct {
    event_type_t type;
    uint32_t time_counts;
    uint32_t value;
    size_t window_index;
    bool critical;
} event_t;

static event_t events[64];
static size_t event_count;
static bool overrun;

static void record(event_type_t type, uint32_t time_counts, uint32_t value, size_t window_index) {
    assert(event_count < sizeof(events) / sizeof(events[0]));
    events[event_count] =
        (event_t){type, time_counts, value, window_index, zule_test_port_is_critical()};
    ++event_count;
}

void zule_user_hook_tick_start(uint32_t time_counts, uint32_t slot, uint32_t cycle_count) {
    /* Pack slot and cycle into the record for compact expectations. */
    record(EVENT_TICK_START, time_counts, slot, (size_t)cycle_count);
}

void zule_user_hook_tick_end(uint32_t time_counts) {
    record(EVENT_TICK_END, time_counts, 0U, 0U);
}

void zule_user_hook_tick_task_release(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    record(EVENT_TICK_TASK_RELEASE, time_counts, task_id, window_index);
}

/* With a single schedule, no switch ever takes place. */
static uint32_t schedule_switch_calls;

void zule_user_hook_tick_schedule_switch(uint32_t time_counts, uint32_t schedule_id) {
    (void)time_counts;
    (void)schedule_id;
    ++schedule_switch_calls;
}

void zule_user_hook_task_start(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    record(EVENT_TASK_START, time_counts, task_id, window_index);
}

void zule_user_hook_task_end(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    record(EVENT_TASK_END, time_counts, task_id, window_index);
}

static void first_task(void) {
    assert(!zule_test_port_is_critical());
    if (overrun) {
        /* Ticks interrupt the task; the second ends its window while it runs. */
        zule_test_port_fire_tick();
        zule_test_port_fire_tick();
    }
}

static void second_task(void) {
    assert(!zule_test_port_is_critical());
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

static const zule_schedule_definition_t definition = {
    .slot_us = 100U,
    .tasks = {tasks, sizeof(tasks) / sizeof(tasks[0])},
    .schedules = {&schedule, 1U},
};

const zule_schedule_definition_t *zule_schedule_definition_get(void) {
    return &definition;
}

/* Check the recorded events against expectations. Every hook runs with the tick
 * excluded, and each reads the timebase exactly once, so with the scripted
 * timebase the n-th event carries time n. */
static void check(const event_t *expected, size_t count) {
    assert(event_count == count);
    for (size_t index = 0U; index < count; ++index) {
        assert(events[index].type == expected[index].type);
        assert(events[index].value == expected[index].value);
        assert(events[index].window_index == expected[index].window_index);
        assert(events[index].time_counts == (uint32_t)index);
        assert(events[index].critical);
    }
}

static void start(void) {
    zule_test_port_reset();
    event_count = 0U;
    assert(zule_scheduler_init(NULL, 0U) == ZULE_RESULT_OK);
    assert(zule_port_tick_init(definition.slot_us, zule_scheduler_tick) == ZULE_PORT_RESULT_OK);
}

int main(void) {
    zule_test_port_reset();
    assert(zule_timebase_hz() == 1000000U);

    /* A full cycle and the start of the next, dispatching each released task. */
    start();
    zule_test_port_fire_tick(); /* Slot 0: release task 0. */
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    zule_test_port_fire_tick(); /* Slot 1. */
    zule_test_port_fire_tick(); /* Slot 2: release task 1. */
    assert(zule_scheduler_run() == ZULE_RESULT_OK);
    assert(zule_scheduler_run() == ZULE_RESULT_OK); /* Nothing pending: no hooks. */
    zule_test_port_fire_tick();                     /* Slot 3. */
    zule_test_port_fire_tick();                     /* Slot 0 of cycle 1. */
    const event_t cycle[] = {
        {EVENT_TICK_START, 0U, 0U, 0U, true},        {EVENT_TICK_TASK_RELEASE, 0U, 0U, 0U, true},
        {EVENT_TICK_END, 0U, 0U, 0U, true},          {EVENT_TASK_START, 0U, 0U, 0U, true},
        {EVENT_TASK_END, 0U, 0U, 0U, true},          {EVENT_TICK_START, 0U, 1U, 0U, true},
        {EVENT_TICK_END, 0U, 0U, 0U, true},          {EVENT_TICK_START, 0U, 2U, 0U, true},
        {EVENT_TICK_TASK_RELEASE, 0U, 1U, 1U, true}, {EVENT_TICK_END, 0U, 0U, 0U, true},
        {EVENT_TASK_START, 0U, 1U, 1U, true},        {EVENT_TASK_END, 0U, 1U, 1U, true},
        {EVENT_TICK_START, 0U, 3U, 0U, true},        {EVENT_TICK_END, 0U, 0U, 0U, true},
        {EVENT_TICK_START, 0U, 0U, 1U, true},        {EVENT_TICK_TASK_RELEASE, 0U, 0U, 0U, true},
        {EVENT_TICK_END, 0U, 0U, 0U, true},
    };
    check(cycle, sizeof(cycle) / sizeof(cycle[0]));
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);

    /* Ticks interrupt an overrunning task; task end still follows, and ticks
     * after the fault still report start and end, without releases. */
    start();
    overrun = true;
    zule_test_port_fire_tick(); /* Slot 0: release task 0. */
    assert(zule_scheduler_run() == ZULE_RESULT_EXECUTION_OVERRUN);
    overrun = false;
    zule_test_port_fire_tick(); /* Faulted: the slot does not advance. */
    const event_t overrun_events[] = {
        {EVENT_TICK_START, 0U, 0U, 0U, true}, {EVENT_TICK_TASK_RELEASE, 0U, 0U, 0U, true},
        {EVENT_TICK_END, 0U, 0U, 0U, true},   {EVENT_TASK_START, 0U, 0U, 0U, true},
        {EVENT_TICK_START, 0U, 1U, 0U, true}, {EVENT_TICK_END, 0U, 0U, 0U, true},
        {EVENT_TICK_START, 0U, 2U, 0U, true}, {EVENT_TICK_END, 0U, 0U, 0U, true},
        {EVENT_TASK_END, 0U, 0U, 0U, true},   {EVENT_TICK_START, 0U, 2U, 0U, true},
        {EVENT_TICK_END, 0U, 0U, 0U, true},
    };
    check(overrun_events, sizeof(overrun_events) / sizeof(overrun_events[0]));
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
    assert(schedule_switch_calls == 0U);
    return 0;
}
