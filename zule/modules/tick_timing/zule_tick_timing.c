#include "zule_tick_timing.h"
#include "zule_port.h"
#include "zule_tick_timing_hooks.h"
#include "zule_tick_tracker.h"

#if ZULE_CONFIG_MODULE_TICK_TIMING

/* Written by the hook handlers in tick context, and read or cleared by the API
 * inside a critical section, so access never overlaps. */
static zule_tick_timing_t timing;
static zule_tick_tracker_t tracker;
static uint32_t tick_start_counts;

zule_result_t zule_tick_timing_init(const zule_schedule_t *schedule) {
    return zule_tick_tracker_init(&tracker, schedule->slot_us, zule_port_time_hz());
}

void zule_tick_timing_tick_start(uint32_t time_counts) {
    tick_start_counts = time_counts;
    zule_tick_deviation_t deviation;
    if (zule_tick_tracker_update(&tracker, time_counts, &deviation)) {
        ++timing.ticks;
        if (deviation.late) {
            if (deviation.magnitude_counts > timing.max_late_counts) {
                timing.max_late_counts = deviation.magnitude_counts;
            }
        } else {
            if (deviation.magnitude_counts > timing.max_early_counts) {
                timing.max_early_counts = deviation.magnitude_counts;
            }
        }
    }
}

void zule_tick_timing_tick_end(uint32_t time_counts) {
    const uint32_t tick_counts = time_counts - tick_start_counts;
    if (tick_counts > timing.max_tick_counts) {
        timing.max_tick_counts = tick_counts;
    }
}

void zule_tick_timing_get(zule_tick_timing_t *snapshot) {
    const zule_port_critical_state_t state = zule_port_critical_enter();
    *snapshot = timing;
    zule_port_critical_exit(state);
}

void zule_tick_timing_reset(void) {
    const zule_port_critical_state_t state = zule_port_critical_enter();
    timing = (zule_tick_timing_t){0};
    zule_port_critical_exit(state);
}

#endif
