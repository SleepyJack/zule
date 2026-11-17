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

/* A tick's deviation from its expected time, as a magnitude and a direction, so
 * consumers compare unsigned magnitudes and never need abs(). */
typedef struct {
    /* From 0 to 2^31 counts. */
    uint32_t magnitude_counts;
    /* True when late; false when early or exactly on time. */
    bool late;
} zule_tick_deviation_t;

/* Prepare for a run with slots of slot_us at a timebase of hz, clearing the
 * reference. Returns INVALID_CONFIG unless hz is nonzero and a slot is a whole
 * number of counts below 2^31, so expected times do not drift and deviations
 * compare as signed 32-bit differences. */
static inline zule_result_t zule_tick_tracker_init(zule_tick_tracker_t *tracker, uint32_t slot_us,
                                                   uint32_t hz) {
    zule_result_t result = ZULE_RESULT_OK;
    /* Both factors are below 2^32, so the product fits. */
    const uint64_t slot_scaled_counts = (uint64_t)slot_us * hz;
    if ((hz == 0U) || ((slot_scaled_counts % ZULE_MICROSECONDS_PER_SECOND) != 0U) ||
        ((slot_scaled_counts / ZULE_MICROSECONDS_PER_SECOND) >= ((uint64_t)1U << 31U))) {
        result = ZULE_RESULT_INVALID_CONFIG;
    } else {
        tracker->counts_per_slot = (uint32_t)(slot_scaled_counts / ZULE_MICROSECONDS_PER_SECOND);
        tracker->expected_counts = 0U;
        tracker->reference_set = false;
    }
    return result;
}

/* Record a tick at now_counts. Returns false for the reference tick. Otherwise
 * returns true with the deviation from the expected time. Wrapping arithmetic
 * keeps this exact across timebase rollover while the deviation is below 2^31
 * counts; a larger one, such as a very long stall, reads as early. */
static inline bool zule_tick_tracker_update(zule_tick_tracker_t *tracker, uint32_t now_counts,
                                            zule_tick_deviation_t *deviation) {
    const bool measured = tracker->reference_set;
    if (!measured) {
        tracker->reference_set = true;
        tracker->expected_counts = now_counts;
    } else {
        tracker->expected_counts += tracker->counts_per_slot;
        const uint32_t late_counts = now_counts - tracker->expected_counts;
        if (late_counts <= (uint32_t)INT32_MAX) {
            deviation->magnitude_counts = late_counts;
            deviation->late = (late_counts != 0U);
        } else {
            /* From 1 to 2^31 counts early. */
            deviation->magnitude_counts = tracker->expected_counts - now_counts;
            deviation->late = false;
        }
    }
    return measured;
}

/* Return a deviation as a signed count, positive when late, for reporting. Late
 * magnitudes are below 2^31, and early ones are at most 2^31. */
static inline int32_t zule_tick_deviation_signed(zule_tick_deviation_t deviation) {
    int32_t deviation_counts;
    if (deviation.late) {
        deviation_counts = (int32_t)deviation.magnitude_counts;
    } else if (deviation.magnitude_counts == 0x80000000U) {
        /* An early magnitude of exactly 2^31 is the one value whose positive form
         * does not fit an int32_t: converting it is implementation defined. Its
         * negation is INT32_MIN, which does fit, so map it directly. */
        deviation_counts = (int32_t)INT32_MIN;
    } else {
        deviation_counts = -(int32_t)deviation.magnitude_counts;
    }
    return deviation_counts;
}

#endif
