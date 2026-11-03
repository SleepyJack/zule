#ifndef ZULE_H
#define ZULE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void (*task)(void);
    uint32_t start_slot;
    uint32_t budget_slots;
} zule_window_t;

typedef struct {
    uint32_t slot_us;
    uint32_t cycle_slots;
    const zule_window_t *windows;
    size_t window_count;
} zule_schedule_t;

typedef struct {
    const zule_schedule_t *schedule;
    uint32_t slot;
    size_t next_window;
} zule_t;

/* All pointers must be non-null. The schedule must be validated and remain
 * alive and unchanged while in use. Initialization does not execute tasks. */
void zule_init(zule_t *scheduler, const zule_schedule_t *schedule);

/* Execute the current slot's task, if any, then advance one logical slot.
 * The first call executes slot zero. Calls must be serialized; tasks must
 * return and must not call back into this scheduler. No timing or budget
 * enforcement is implemented yet. */
void zule_tick(zule_t *scheduler);

#endif
