/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "zule_schedule.h"

static void task(void) {
}

static const zule_task_t tasks[] = {{.id = 0U, .function = task}};
static const zule_task_t duplicate_tasks[] = {{.id = 0U, .function = task},
                                              {.id = 0U, .function = task}};
static const zule_task_t swapped_tasks[] = {{.id = 1U, .function = task},
                                            {.id = 0U, .function = task}};
static const zule_task_t offset_task[] = {{.id = 1U, .function = task}};
static const zule_task_t null_function[] = {{.id = 0U, .function = NULL}};
static const zule_task_t foreign_task = {.id = 0U, .function = task};
static const zule_window_t adjacent[] = {
    {&tasks[0], 0U, 2U},
    {&tasks[0], 2U, 1U},
};
static const zule_window_t gaps[] = {
    {&tasks[0], 1U, 1U},
    {&tasks[0], 4U, 2U},
};
static const zule_window_t exact_end[] = {{&tasks[0], UINT32_MAX - 1U, 1U}};
static const zule_window_t full_cycle[] = {{&tasks[0], 0U, UINT32_MAX}};
static const zule_window_t null_task[] = {{NULL, 0U, 1U}};
static const zule_window_t zero_budget[] = {{&tasks[0], 0U, 0U}};
static const zule_window_t start_outside[] = {{&tasks[0], 4U, 1U}};
static const zule_window_t end_outside[] = {{&tasks[0], 3U, 2U}};
static const zule_window_t overflowing_end[] = {{&tasks[0], UINT32_MAX - 1U, 2U}};
static const zule_window_t overlap[] = {
    {&tasks[0], 0U, 3U},
    {&tasks[0], 2U, 1U},
};
static const zule_window_t unordered[] = {
    {&tasks[0], 2U, 1U},
    {&tasks[0], 0U, 1U},
};
static const zule_window_t equal_starts[] = {
    {&tasks[0], 0U, 1U},
    {&tasks[0], 0U, 1U},
};

static const zule_window_t foreign_window[] = {{&foreign_task, 0U, 1U}};
static const zule_window_t duplicate_window[] = {{&duplicate_tasks[0], 0U, 1U}};
static const zule_window_t swapped_window[] = {{&swapped_tasks[0], 0U, 1U}};
static const zule_window_t offset_window[] = {{&offset_task[0], 0U, 1U}};

int main(void) {
    const zule_schedule_t valid[] = {
        {100U, 3U, adjacent, 2U, tasks, 1U},
        {100U, 8U, gaps, 2U, tasks, 1U},
        {100U, UINT32_MAX, exact_end, 1U, tasks, 1U},
        {100U, UINT32_MAX, full_cycle, 1U, tasks, 1U},
    };
    const zule_schedule_t invalid[] = {
        {0U, 4U, adjacent, 2U, tasks, 1U},
        {100U, 0U, adjacent, 2U, tasks, 1U},
        {100U, 4U, NULL, 1U, tasks, 1U},
        {100U, 4U, adjacent, 0U, tasks, 1U},
        {100U, 1U, adjacent, 2U, tasks, 1U},
        {100U, 4U, null_task, 1U, tasks, 1U},
        {100U, 4U, zero_budget, 1U, tasks, 1U},
        {100U, 4U, start_outside, 1U, tasks, 1U},
        {100U, 4U, end_outside, 1U, tasks, 1U},
        {100U, 4U, full_cycle, 1U, tasks, 1U},
        {100U, UINT32_MAX, overflowing_end, 1U, tasks, 1U},
        {100U, 4U, overlap, 2U, tasks, 1U},
        {100U, 4U, unordered, 2U, tasks, 1U},
        {100U, 4U, equal_starts, 2U, tasks, 1U},
    };

    zule_schedule_t missing_tasks = valid[0];
    missing_tasks.tasks = NULL;
    assert(zule_schedule_validate(&missing_tasks) == ZULE_RESULT_INVALID_SCHEDULE);
    missing_tasks = valid[0];
    missing_tasks.task_count = 0U;
    assert(zule_schedule_validate(&missing_tasks) == ZULE_RESULT_INVALID_SCHEDULE);
    missing_tasks = valid[0];
    missing_tasks.tasks = null_function;
    assert(zule_schedule_validate(&missing_tasks) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t duplicates = {100U, 4U, duplicate_window, 1U, duplicate_tasks, 2U};
    assert(zule_schedule_validate(&duplicates) == ZULE_RESULT_INVALID_SCHEDULE);
    /* Unique IDs are not enough: each ID must equal its task index. */
    const zule_schedule_t swapped = {100U, 4U, swapped_window, 1U, swapped_tasks, 2U};
    assert(zule_schedule_validate(&swapped) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t offset = {100U, 4U, offset_window, 1U, offset_task, 1U};
    assert(zule_schedule_validate(&offset) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t foreign = {100U, 4U, foreign_window, 1U, tasks, 1U};
    assert(zule_schedule_validate(&foreign) == ZULE_RESULT_INVALID_SCHEDULE);

    assert(zule_schedule_validate(NULL) == ZULE_RESULT_INVALID_SCHEDULE);
    for (size_t index = 0U; index < sizeof(valid) / sizeof(valid[0]); ++index) {
        assert(zule_schedule_validate(&valid[index]) == ZULE_RESULT_OK);
    }
    for (size_t index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        assert(zule_schedule_validate(&invalid[index]) == ZULE_RESULT_INVALID_SCHEDULE);
    }

    /* A set is valid when every schedule is, and all share the slot length and
     * the task table, so task IDs and the tick mean the same in each. */
    const zule_schedule_set_t one = {valid, 1U};
    assert(zule_schedule_set_validate(&one) == ZULE_RESULT_OK);
    const zule_schedule_set_t all_valid = {valid, sizeof(valid) / sizeof(valid[0])};
    assert(zule_schedule_set_validate(&all_valid) == ZULE_RESULT_OK);
    assert(zule_schedule_set_validate(NULL) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_set_t no_schedules = {NULL, 1U};
    assert(zule_schedule_set_validate(&no_schedules) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_set_t empty = {valid, 0U};
    assert(zule_schedule_set_validate(&empty) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t with_invalid[] = {valid[0], invalid[0]};
    const zule_schedule_set_t one_invalid = {with_invalid, 2U};
    assert(zule_schedule_set_validate(&one_invalid) == ZULE_RESULT_INVALID_SCHEDULE);
    zule_schedule_t other_slot = valid[1];
    other_slot.slot_us = 200U;
    assert(zule_schedule_validate(&other_slot) == ZULE_RESULT_OK);
    const zule_schedule_t slots_differ[] = {valid[0], other_slot};
    const zule_schedule_set_t different_slots = {slots_differ, 2U};
    assert(zule_schedule_set_validate(&different_slots) == ZULE_RESULT_INVALID_SCHEDULE);
    /* An identical but separate task table is not shared. */
    static const zule_task_t copied_tasks[] = {{.id = 0U, .function = task}};
    static const zule_window_t copied_window[] = {{&copied_tasks[0], 0U, 1U}};
    const zule_schedule_t other_tasks = {100U, 3U, copied_window, 1U, copied_tasks, 1U};
    assert(zule_schedule_validate(&other_tasks) == ZULE_RESULT_OK);
    const zule_schedule_t tasks_differ[] = {valid[0], other_tasks};
    const zule_schedule_set_t different_tasks = {tasks_differ, 2U};
    assert(zule_schedule_set_validate(&different_tasks) == ZULE_RESULT_INVALID_SCHEDULE);
    return 0;
}
