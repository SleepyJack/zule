#ifndef ZULE_SCHEDULE_DEFINITION_H
#define ZULE_SCHEDULE_DEFINITION_H

#include "zule_schedule.h"

/* Generated configuration must remain alive and immutable throughout use.
 * It must have nonzero cycle/window counts, non-null windows and task pointers,
 * and ordered, nonoverlapping windows contained within the cycle. */
const zule_schedule_t *zule_schedule_get(void);

#endif
