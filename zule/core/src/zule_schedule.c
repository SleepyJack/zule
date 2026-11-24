#include "zule_schedule.h"

#include <stdbool.h>

/* Return whether a window's task is an entry in the shared task array. */
static bool window_task_declared(const zule_schedules_t *schedules, const zule_window_t *window) {
    bool declared = false;
    for (size_t index = 0U; (index < schedules->task_count) && !declared; ++index) {
        declared = (window->task == &schedules->tasks[index]);
    }
    return declared;
}

/* Each task's ID is its index in the task array, which also makes IDs unique. */
static zule_result_t validate_tasks(const zule_schedules_t *schedules) {
    zule_result_t result = ZULE_RESULT_OK;
    for (size_t index = 0U; (result == ZULE_RESULT_OK) && (index < schedules->task_count);
         ++index) {
        if ((schedules->tasks[index].function == NULL) ||
            ((size_t)schedules->tasks[index].id != index)) {
            result = ZULE_RESULT_INVALID_SCHEDULE;
        }
    }
    return result;
}

static zule_result_t validate_schedule(const zule_schedules_t *schedules,
                                       const zule_schedule_t *schedule) {
    zule_result_t result = ZULE_RESULT_OK;
    if ((schedule->cycle_slots == 0U) || (schedule->windows == NULL) ||
        (schedule->window_count == 0U) || (schedule->window_count > schedule->cycle_slots)) {
        result = ZULE_RESULT_INVALID_SCHEDULE;
    }

    uint32_t previous_end_slot = 0U;
    for (size_t index = 0U; (result == ZULE_RESULT_OK) && (index < schedule->window_count);
         ++index) {
        const zule_window_t *window = &schedule->windows[index];
        const bool declared = window_task_declared(schedules, window);
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

zule_result_t zule_schedules_validate(const zule_schedules_t *schedules) {
    zule_result_t result = ZULE_RESULT_OK;
    if ((schedules == NULL) || (schedules->slot_us == 0U) || (schedules->tasks == NULL) ||
        (schedules->task_count == 0U) || (schedules->schedules == NULL) ||
        (schedules->schedule_count == 0U)) {
        result = ZULE_RESULT_INVALID_SCHEDULE;
    }
    if (result == ZULE_RESULT_OK) {
        result = validate_tasks(schedules);
    }
    for (size_t index = 0U; (result == ZULE_RESULT_OK) && (index < schedules->schedule_count);
         ++index) {
        result = validate_schedule(schedules, &schedules->schedules[index]);
    }
    return result;
}
