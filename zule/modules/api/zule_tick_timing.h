#ifndef ZULE_TICK_TIMING_H
#define ZULE_TICK_TIMING_H

/* Tick timing module: how far ticks deviate from their expected times, and how
 * long the tick handler takes, so the tick timing check's tolerance and budget
 * margins can be chosen from measurements. Enabled by
 * ZULE_CONFIG_MODULE_TICK_TIMING; declared only when enabled. */
#include "zule.h"

#include <stdint.h>

#if ZULE_CONFIG_MODULE_TICK_TIMING

/* All values are timebase counts; convert with zule_timebase_hz(). */
typedef struct {
    /* Ticks measured since the last reset, wrapping modulo 2^32. The first tick
     * of each run is its reference and is not measured. */
    uint32_t ticks;
    /* Worst deviation of a measured tick from its expected time, late and
     * early, as magnitudes. Deviations of 2^31 counts or more, such as a very
     * long stall, are misread as early. */
    uint32_t max_late_counts;
    uint32_t max_early_counts;
    /* Longest tick handler, from the tick start timestamp to the tick end
     * timestamp, including hook and fault handler time but not the port's
     * interrupt entry and exit. */
    uint32_t max_tick_counts;
} zule_tick_timing_t;

/* Copy a consistent snapshot of the results to *snapshot, which must point to
 * writable storage. Results accumulate across runs until reset. Call from
 * application context, including tasks; not from hooks or the fault handler. */
void zule_tick_timing_get(zule_tick_timing_t *snapshot);

/* Clear the results. The current run's reference is kept. Same calling rules as
 * zule_tick_timing_get(). */
void zule_tick_timing_reset(void);

#endif

#endif
