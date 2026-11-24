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

/* One schedule over the given tasks, at 100 us slots. */
static zule_result_t validate_one(const zule_schedule_t *schedule, const zule_task_t *task_table,
                                  size_t task_count) {
    const zule_schedules_t schedules = {100U, task_table, task_count, schedule, 1U};
    return zule_schedules_validate(&schedules);
}

int main(void) {
    const zule_schedule_t valid[] = {
        {3U, adjacent, 2U},
        {8U, gaps, 2U},
        {UINT32_MAX, exact_end, 1U},
        {UINT32_MAX, full_cycle, 1U},
    };
    const zule_schedule_t invalid[] = {
        {0U, adjacent, 2U},      {4U, NULL, 1U},
        {4U, adjacent, 0U},      {1U, adjacent, 2U},
        {4U, null_task, 1U},     {4U, zero_budget, 1U},
        {4U, start_outside, 1U}, {4U, end_outside, 1U},
        {4U, full_cycle, 1U},    {UINT32_MAX, overflowing_end, 1U},
        {4U, overlap, 2U},       {4U, unordered, 2U},
        {4U, equal_starts, 2U},
    };

    for (size_t index = 0U; index < sizeof(valid) / sizeof(valid[0]); ++index) {
        assert(validate_one(&valid[index], tasks, 1U) == ZULE_RESULT_OK);
    }
    for (size_t index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        assert(validate_one(&invalid[index], tasks, 1U) == ZULE_RESULT_INVALID_SCHEDULE);
    }

    /* The slot length and task table, shared by every schedule. */
    assert(zule_schedules_validate(NULL) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedules_t no_slot = {0U, tasks, 1U, valid, 1U};
    assert(zule_schedules_validate(&no_slot) == ZULE_RESULT_INVALID_SCHEDULE);
    assert(validate_one(&valid[0], NULL, 1U) == ZULE_RESULT_INVALID_SCHEDULE);
    assert(validate_one(&valid[0], tasks, 0U) == ZULE_RESULT_INVALID_SCHEDULE);
    assert(validate_one(&valid[0], null_function, 1U) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t duplicates = {4U, duplicate_window, 1U};
    assert(validate_one(&duplicates, duplicate_tasks, 2U) == ZULE_RESULT_INVALID_SCHEDULE);
    /* Unique IDs are not enough: each ID must equal its task index. */
    const zule_schedule_t swapped = {4U, swapped_window, 1U};
    assert(validate_one(&swapped, swapped_tasks, 2U) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t offset = {4U, offset_window, 1U};
    assert(validate_one(&offset, offset_task, 1U) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t foreign = {4U, foreign_window, 1U};
    assert(validate_one(&foreign, tasks, 1U) == ZULE_RESULT_INVALID_SCHEDULE);

    /* Multiple schedules are valid when every one is. */
    const zule_schedules_t all_valid = {100U, tasks, 1U, valid, sizeof(valid) / sizeof(valid[0])};
    assert(zule_schedules_validate(&all_valid) == ZULE_RESULT_OK);
    const zule_schedules_t no_schedules = {100U, tasks, 1U, NULL, 1U};
    assert(zule_schedules_validate(&no_schedules) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedules_t empty = {100U, tasks, 1U, valid, 0U};
    assert(zule_schedules_validate(&empty) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t with_invalid[] = {valid[0], invalid[0]};
    const zule_schedules_t one_invalid = {100U, tasks, 1U, with_invalid, 2U};
    assert(zule_schedules_validate(&one_invalid) == ZULE_RESULT_INVALID_SCHEDULE);
    const zule_schedule_t with_foreign[] = {valid[0], foreign};
    const zule_schedules_t one_foreign = {100U, tasks, 1U, with_foreign, 2U};
    assert(zule_schedules_validate(&one_foreign) == ZULE_RESULT_INVALID_SCHEDULE);
    return 0;
}
