#include "zule.h"

void zule_init(zule_t *scheduler, const zule_schedule_t *schedule)
{
    scheduler->schedule = schedule;
    scheduler->slot = 0U;
    scheduler->next_window = 0U;
}

void zule_tick(zule_t *scheduler)
{
    const zule_schedule_t *schedule = scheduler->schedule;

    if (scheduler->next_window < schedule->window_count) {
        const zule_window_t *window = &schedule->windows[scheduler->next_window];
        if (window->start_slot == scheduler->slot) {
            window->task();
            ++scheduler->next_window;
        }
    }

    ++scheduler->slot;
    if (scheduler->slot == schedule->cycle_slots) {
        scheduler->slot = 0U;
        scheduler->next_window = 0U;
    }
}
