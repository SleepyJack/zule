#ifndef ZULE_SCHEDULE_DEFINITION_H
#define ZULE_SCHEDULE_DEFINITION_H

#include "zule_schedule.h"

/* Implemented by the generated schedule: return the application's schedules.
 * They must remain alive and immutable throughout use. The core checks them
 * with zule_schedules_validate() before running any. */
const zule_schedules_t *zule_schedules_get(void);

#endif
