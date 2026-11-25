#ifndef ZULE_CONFIG_APPLY_H
#define ZULE_CONFIG_APPLY_H

/* Apply the build settings, so the core, modules, port, application, and
 * generated schedule all compile with the same values. Included by zule.h;
 * applications do not include it directly. It includes the integrator's
 * zule_user_config.h when ZULE_USE_USER_CONFIG is 1, then the defaults for
 * settings left unset, from zule_config_defaults.h, then checks the values and
 * the requirements between them. */
#ifndef ZULE_USE_USER_CONFIG
#define ZULE_USE_USER_CONFIG 0
#endif

#if ZULE_USE_USER_CONFIG
#include "zule_user_config.h"
#endif

#include "zule_config_defaults.h"

/* UINT32_MAX, usable in #if, for range checks below. */
#include <stdint.h>

#if ZULE_CONFIG_TASK_NAMES != 0 && ZULE_CONFIG_TASK_NAMES != 1
#error "ZULE_CONFIG_TASK_NAMES must be 0 or 1"
#endif

#if ZULE_CONFIG_TIMEBASE != 0 && ZULE_CONFIG_TIMEBASE != 1
#error "ZULE_CONFIG_TIMEBASE must be 0 or 1"
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_START != 0 && ZULE_CONFIG_USER_HOOK_TICK_START != 1
#error "ZULE_CONFIG_USER_HOOK_TICK_START must be 0 or 1"
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_END != 0 && ZULE_CONFIG_USER_HOOK_TICK_END != 1
#error "ZULE_CONFIG_USER_HOOK_TICK_END must be 0 or 1"
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE != 0 && ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE != 1
#error "ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE must be 0 or 1"
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH != 0 &&                                             \
    ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH != 1
#error "ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH must be 0 or 1"
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_START != 0 && ZULE_CONFIG_USER_HOOK_TASK_START != 1
#error "ZULE_CONFIG_USER_HOOK_TASK_START must be 0 or 1"
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_END != 0 && ZULE_CONFIG_USER_HOOK_TASK_END != 1
#error "ZULE_CONFIG_USER_HOOK_TASK_END must be 0 or 1"
#endif

/* Every hook passes a timestamp, so hooks require the timebase. */
#if (ZULE_CONFIG_USER_HOOK_TICK_START || ZULE_CONFIG_USER_HOOK_TICK_END ||                         \
     ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE || ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH ||      \
     ZULE_CONFIG_USER_HOOK_TASK_START || ZULE_CONFIG_USER_HOOK_TASK_END) &&                        \
    !ZULE_CONFIG_TIMEBASE
#error "User hooks require ZULE_CONFIG_TIMEBASE"
#endif

#if ZULE_CONFIG_TICK_CHECK != 0 && ZULE_CONFIG_TICK_CHECK != 1
#error "ZULE_CONFIG_TICK_CHECK must be 0 or 1"
#endif

#if ZULE_CONFIG_TICK_CHECK && !ZULE_CONFIG_TIMEBASE
#error "ZULE_CONFIG_TICK_CHECK requires ZULE_CONFIG_TIMEBASE"
#endif

/* Initialization also checks the tolerance against the slot and the timebase
 * resolution. */
#if ZULE_CONFIG_TICK_CHECK
#ifndef ZULE_CONFIG_TICK_TOLERANCE_US
#error "ZULE_CONFIG_TICK_CHECK requires ZULE_CONFIG_TICK_TOLERANCE_US"
#endif
#if ZULE_CONFIG_TICK_TOLERANCE_US < 1 || ZULE_CONFIG_TICK_TOLERANCE_US > UINT32_MAX
#error "ZULE_CONFIG_TICK_TOLERANCE_US must be from 1 to UINT32_MAX"
#endif
#endif

/* Module settings and the conditions derived from them. */
#include "zule_modules_config_apply.h"

#endif
