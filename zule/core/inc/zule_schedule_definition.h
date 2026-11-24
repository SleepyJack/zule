#ifndef ZULE_SCHEDULE_DEFINITION_H
#define ZULE_SCHEDULE_DEFINITION_H

#include "zule_schedule.h"

/* Implemented by the generated schedule: return the application's schedules,
 * indexed by schedule ID. They must remain alive and immutable throughout use.
 * The core checks them with zule_schedule_set_validate() before running any. */
const zule_schedule_set_t *zule_schedule_set_get(void);

#endif
