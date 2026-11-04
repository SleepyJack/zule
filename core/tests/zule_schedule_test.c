/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "zule_schedule.h"

static void task(void) {
}

static const zule_window_t adjacent[] = {
    {task, 0U, 2U},
    {task, 2U, 1U},
};
static const zule_window_t gaps[] = {
    {task, 1U, 1U},
    {task, 4U, 2U},
};
static const zule_window_t exact_end[] = {{task, UINT32_MAX - 1U, 1U}};
static const zule_window_t full_cycle[] = {{task, 0U, UINT32_MAX}};
static const zule_window_t null_task[] = {{NULL, 0U, 1U}};
static const zule_window_t zero_budget[] = {{task, 0U, 0U}};
static const zule_window_t start_outside[] = {{task, 4U, 1U}};
static const zule_window_t end_outside[] = {{task, 3U, 2U}};
static const zule_window_t overflowing_end[] = {{task, UINT32_MAX - 1U, 2U}};
static const zule_window_t overlap[] = {
    {task, 0U, 3U},
    {task, 2U, 1U},
};
static const zule_window_t unordered[] = {
    {task, 2U, 1U},
    {task, 0U, 1U},
};
static const zule_window_t equal_starts[] = {
    {task, 0U, 1U},
    {task, 0U, 1U},
};

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

    assert(zule_schedule_validate(NULL) == ZULE_RESULT_INVALID_SCHEDULE);
    for (size_t index = 0U; index < sizeof(valid) / sizeof(valid[0]); ++index) {
        assert(zule_schedule_validate(&valid[index]) == ZULE_RESULT_OK);
    }
    for (size_t index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        assert(zule_schedule_validate(&invalid[index]) == ZULE_RESULT_INVALID_SCHEDULE);
    }
    return 0;
}
