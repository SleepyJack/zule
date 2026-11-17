#ifndef ZULE_TICK_TIMING_HOOKS_H
#define ZULE_TICK_TIMING_HOOKS_H

/* Internal: the tick timing module's handlers, called by the module entry
 * points in zule_modules.c with the guarantees of the hooks they serve. */
#include "zule_schedule.h"

#include <stdint.h>

#if ZULE_CONFIG_MODULE_TICK_TIMING
/* Start a run: clear the reference. Fails with INVALID_CONFIG unless a slot is a
 * whole number of timebase counts below 2^31. Keeps accumulated results. */
zule_result_t zule_tick_timing_init(const zule_schedule_t *schedule);
void zule_tick_timing_tick_start(uint32_t time_counts);
void zule_tick_timing_tick_end(uint32_t time_counts);
#endif

#endif
