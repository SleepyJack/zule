#ifndef ZULE_SCHEDULE_DEFINITION_H
#define ZULE_SCHEDULE_DEFINITION_H

#include "zule_schedule.h"

/* Implemented by the generated schedule: return the application's schedule
 * definition. It must remain alive and immutable throughout use. The core checks
 * it with zule_schedule_definition_validate() before running any schedule. */
const zule_schedule_definition_t *zule_schedule_definition_get(void);

#endif
