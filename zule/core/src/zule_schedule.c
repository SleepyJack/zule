#include "zule_schedule.h"

#include <stdbool.h>

/* Return whether a window's task is an entry in the schedule's task array. */
static bool window_task_declared(const zule_schedule_t *schedule, const zule_window_t *window) {
    bool declared = false;
    for (size_t index = 0U; (index < schedule->task_count) && !declared; ++index) {
        declared = (window->task == &schedule->tasks[index]);
    }
    return declared;
}

zule_result_t zule_schedule_validate(const zule_schedule_t *schedule) {
    zule_result_t result = ZULE_RESULT_OK;
    if ((schedule == NULL) || (schedule->slot_us == 0U) || (schedule->cycle_slots == 0U) ||
        (schedule->windows == NULL) || (schedule->window_count == 0U) ||
        (schedule->window_count > schedule->cycle_slots) || (schedule->tasks == NULL) ||
        (schedule->task_count == 0U)) {
        result = ZULE_RESULT_INVALID_SCHEDULE;
    }

    /* Each task's ID is its index in the task array, which also makes IDs unique. */
    for (size_t index = 0U; (result == ZULE_RESULT_OK) && (index < schedule->task_count); ++index) {
        if ((schedule->tasks[index].function == NULL) ||
            ((size_t)schedule->tasks[index].id != index)) {
            result = ZULE_RESULT_INVALID_SCHEDULE;
        }
    }

    uint32_t previous_end_slot = 0U;
    for (size_t index = 0U; (result == ZULE_RESULT_OK) && (index < schedule->window_count);
         ++index) {
        const zule_window_t *window = &schedule->windows[index];
        const bool declared = window_task_declared(schedule, window);
        /* The start is checked before the budget, so the subtraction cannot wrap,
         * and subtracting before adding means an overflowing end cannot pass. */
        if (!declared || (window->budget_slots == 0U) ||
            (window->start_slot >= schedule->cycle_slots) ||
            (window->budget_slots > (schedule->cycle_slots - window->start_slot)) ||
            (window->start_slot < previous_end_slot)) {
            result = ZULE_RESULT_INVALID_SCHEDULE;
        } else {
            previous_end_slot = window->start_slot + window->budget_slots;
        }
    }
    return result;
}
