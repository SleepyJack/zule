#include "zule_schedule.h"

#include <stdbool.h>

/* Return whether a window's task is an entry in the task table. */
static bool window_task_declared(const zule_task_table_t *tasks, const zule_window_t *window) {
    bool declared = false;
    for (size_t index = 0U; (index < tasks->count) && !declared; ++index) {
        declared = (window->task == &tasks->entries[index]);
    }
    return declared;
}

/* A nonempty table of callable tasks, each of whose ID is its index, which also
 * makes IDs unique. */
static zule_result_t validate_tasks(const zule_task_table_t *tasks) {
    zule_result_t result = ZULE_RESULT_OK;
    if ((tasks->entries == NULL) || (tasks->count == 0U)) {
        result = ZULE_RESULT_INVALID_SCHEDULE;
    }
    for (size_t index = 0U; (result == ZULE_RESULT_OK) && (index < tasks->count); ++index) {
        if ((tasks->entries[index].function == NULL) ||
            ((size_t)tasks->entries[index].id != index)) {
            result = ZULE_RESULT_INVALID_SCHEDULE;
        }
    }
    return result;
}

static zule_result_t validate_schedule(const zule_task_table_t *tasks,
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
        const bool declared = window_task_declared(tasks, window);
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

/* A nonempty table of schedules, each valid over the task table. */
static zule_result_t validate_schedules(const zule_task_table_t *tasks,
                                        const zule_schedule_table_t *schedules) {
    zule_result_t result = ZULE_RESULT_OK;
    if ((schedules->entries == NULL) || (schedules->count == 0U)) {
        result = ZULE_RESULT_INVALID_SCHEDULE;
    }
    for (size_t index = 0U; (result == ZULE_RESULT_OK) && (index < schedules->count); ++index) {
        result = validate_schedule(tasks, &schedules->entries[index]);
    }
    return result;
}

zule_result_t zule_schedule_definition_validate(const zule_schedule_definition_t *definition) {
    zule_result_t result = ZULE_RESULT_OK;
    if ((definition == NULL) || (definition->slot_us == 0U)) {
        result = ZULE_RESULT_INVALID_SCHEDULE;
    }
    if (result == ZULE_RESULT_OK) {
        result = validate_tasks(&definition->tasks);
    }
    if (result == ZULE_RESULT_OK) {
        result = validate_schedules(&definition->tasks, &definition->schedules);
    }
    return result;
}
