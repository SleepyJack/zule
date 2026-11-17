#ifndef ZULE_TICK_TRACKER_H
#define ZULE_TICK_TRACKER_H

/* Internal helper, for the core and modules: compares tick times with their
 * expected times. The first tick after init is the reference, and each later
 * tick is expected one slot after the previous expected time. Each user owns its
 * own tracker; this header shares only the arithmetic. */
#include "zule.h"

#include <stdbool.h>
#include <stdint.h>

#define ZULE_MICROSECONDS_PER_SECOND 1000000U

typedef struct {
    uint32_t counts_per_slot;
    /* Expected count of the current tick, advanced by wrapping addition. */
    uint32_t expected_counts;
    bool reference_set;
} zule_tick_tracker_t;

/* Prepare for a run with slots of slot_us at a timebase of hz, clearing the
 * reference. Returns INVALID_CONFIG unless hz is nonzero and a slot is a whole
 * number of counts below 2^31, so expected times do not drift and deviations
 * compare as signed 32-bit differences. */
static inline zule_result_t zule_tick_tracker_init(zule_tick_tracker_t *tracker, uint32_t slot_us,
                                                   uint32_t hz) {
    /* Both factors are below 2^32, so the product fits. */
    const uint64_t slot_scaled_counts = (uint64_t)slot_us * hz;
    if (hz == 0U || slot_scaled_counts % ZULE_MICROSECONDS_PER_SECOND != 0U ||
        slot_scaled_counts / ZULE_MICROSECONDS_PER_SECOND >= ((uint64_t)1U << 31U)) {
        return ZULE_RESULT_INVALID_CONFIG;
    }
    tracker->counts_per_slot = (uint32_t)(slot_scaled_counts / ZULE_MICROSECONDS_PER_SECOND);
    tracker->expected_counts = 0U;
    tracker->reference_set = false;
    return ZULE_RESULT_OK;
}

/* Record a tick at now_counts. Returns false for the reference tick. Otherwise
 * returns true with the deviation from the expected time, positive when late,
 * and its magnitude. Wrapping arithmetic keeps this exact across timebase
 * rollover while the deviation is below 2^31 counts; a larger one, such as a
 * very long stall, reads as early. */
static inline bool zule_tick_tracker_update(zule_tick_tracker_t *tracker, uint32_t now_counts,
                                            int32_t *deviation_counts, uint32_t *magnitude_counts) {
    if (!tracker->reference_set) {
        tracker->reference_set = true;
        tracker->expected_counts = now_counts;
        return false;
    }
    tracker->expected_counts += tracker->counts_per_slot;
    const uint32_t late_counts = now_counts - tracker->expected_counts;
    /* Convert to signed explicitly; out-of-range conversions are implementation
     * defined. An early tick's magnitude is at most 2^31. */
    if (late_counts <= (uint32_t)INT32_MAX) {
        *magnitude_counts = late_counts;
        *deviation_counts = (int32_t)late_counts;
    } else {
        *magnitude_counts = tracker->expected_counts - now_counts;
        *deviation_counts = -(int32_t)(*magnitude_counts - 1U) - 1;
    }
    return true;
}

#endif
