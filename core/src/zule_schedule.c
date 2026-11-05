#include "zule_schedule.h"

zule_result_t zule_schedule_validate(const zule_schedule_t *schedule) {
    if (schedule == NULL || schedule->slot_us == 0U || schedule->cycle_slots == 0U ||
        schedule->windows == NULL || schedule->window_count == 0U ||
        schedule->window_count > schedule->cycle_slots) {
        return ZULE_RESULT_INVALID_SCHEDULE;
    }

    uint32_t previous_end_slot = 0U;
    for (size_t index = 0U; index < schedule->window_count; ++index) {
        const zule_window_t *window = &schedule->windows[index];
        if (window->task_function == NULL || window->budget_slots == 0U ||
            window->start_slot >= schedule->cycle_slots) {
            return ZULE_RESULT_INVALID_SCHEDULE;
        }

        /* Subtract before adding so an overflowing end cannot pass validation. */
        if (window->budget_slots > schedule->cycle_slots - window->start_slot ||
            window->start_slot < previous_end_slot) {
            return ZULE_RESULT_INVALID_SCHEDULE;
        }
        previous_end_slot = window->start_slot + window->budget_slots;
    }
    return ZULE_RESULT_OK;
}
