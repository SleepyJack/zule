#ifndef ZULE_CONFIG_H
#define ZULE_CONFIG_H

/* Apply the same settings to the core, port, and generated schedule. */
#ifndef ZULE_USE_USER_CONFIG
#define ZULE_USE_USER_CONFIG 0
#endif

#if ZULE_USE_USER_CONFIG
#include "zule_user_config.h"
#endif

#ifndef ZULE_CONFIG_TASK_NAMES
#define ZULE_CONFIG_TASK_NAMES 0
#endif

#if ZULE_CONFIG_TASK_NAMES != 0 && ZULE_CONFIG_TASK_NAMES != 1
#error "ZULE_CONFIG_TASK_NAMES must be 0 or 1"
#endif

#endif
