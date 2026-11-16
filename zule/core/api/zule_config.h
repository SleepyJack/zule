#ifndef ZULE_CONFIG_H
#define ZULE_CONFIG_H

/* Apply the same settings to the core, port, and generated schedule. */
#ifndef ZULE_USE_USER_CONFIG
#define ZULE_USE_USER_CONFIG 0
#endif

#if ZULE_USE_USER_CONFIG
#include "zule_user_config.h"
#endif

/* UINT32_MAX, usable in #if, for range checks below. */
#include <stdint.h>

#ifndef ZULE_CONFIG_TASK_NAMES
#define ZULE_CONFIG_TASK_NAMES 0
#endif

#if ZULE_CONFIG_TASK_NAMES != 0 && ZULE_CONFIG_TASK_NAMES != 1
#error "ZULE_CONFIG_TASK_NAMES must be 0 or 1"
#endif

/* Optional feature: the port timebase, for sub-slot timing. The selected port
 * must provide it when enabled. */
#ifndef ZULE_CONFIG_TIMEBASE
#define ZULE_CONFIG_TIMEBASE 0
#endif

#if ZULE_CONFIG_TIMEBASE != 0 && ZULE_CONFIG_TIMEBASE != 1
#error "ZULE_CONFIG_TIMEBASE must be 0 or 1"
#endif

/* User hooks: set to 1 to have the core call the matching zule_user_hook_...()
 * function, which the application defines. See zule_hooks.h. */
#ifndef ZULE_CONFIG_USER_HOOK_TICK_START
#define ZULE_CONFIG_USER_HOOK_TICK_START 0
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_START != 0 && ZULE_CONFIG_USER_HOOK_TICK_START != 1
#error "ZULE_CONFIG_USER_HOOK_TICK_START must be 0 or 1"
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TICK_END
#define ZULE_CONFIG_USER_HOOK_TICK_END 0
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_END != 0 && ZULE_CONFIG_USER_HOOK_TICK_END != 1
#error "ZULE_CONFIG_USER_HOOK_TICK_END must be 0 or 1"
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
#define ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE 0
#endif

#if ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE != 0 && ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE != 1
#error "ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE must be 0 or 1"
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TASK_START
#define ZULE_CONFIG_USER_HOOK_TASK_START 0
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_START != 0 && ZULE_CONFIG_USER_HOOK_TASK_START != 1
#error "ZULE_CONFIG_USER_HOOK_TASK_START must be 0 or 1"
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TASK_END
#define ZULE_CONFIG_USER_HOOK_TASK_END 0
#endif

#if ZULE_CONFIG_USER_HOOK_TASK_END != 0 && ZULE_CONFIG_USER_HOOK_TASK_END != 1
#error "ZULE_CONFIG_USER_HOOK_TASK_END must be 0 or 1"
#endif

/* Every hook passes a timestamp, so hooks require the timebase. */
#if (ZULE_CONFIG_USER_HOOK_TICK_START || ZULE_CONFIG_USER_HOOK_TICK_END ||                         \
     ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE || ZULE_CONFIG_USER_HOOK_TASK_START ||                \
     ZULE_CONFIG_USER_HOOK_TASK_END) &&                                                            \
    !ZULE_CONFIG_TIMEBASE
#error "User hooks require ZULE_CONFIG_TIMEBASE"
#endif

/* Optional feature: tick timing check. Faults when a tick deviates from its
 * expected time, early or late, by more than ZULE_CONFIG_TICK_TOLERANCE_US
 * microseconds. Requires the timebase. */
#ifndef ZULE_CONFIG_TICK_CHECK
#define ZULE_CONFIG_TICK_CHECK 0
#endif

#if ZULE_CONFIG_TICK_CHECK != 0 && ZULE_CONFIG_TICK_CHECK != 1
#error "ZULE_CONFIG_TICK_CHECK must be 0 or 1"
#endif

#if ZULE_CONFIG_TICK_CHECK && !ZULE_CONFIG_TIMEBASE
#error "ZULE_CONFIG_TICK_CHECK requires ZULE_CONFIG_TIMEBASE"
#endif

/* Required with the tick check; there is no default. Initialization also checks
 * it against the slot and the timebase resolution. */
#if ZULE_CONFIG_TICK_CHECK
#ifndef ZULE_CONFIG_TICK_TOLERANCE_US
#error "ZULE_CONFIG_TICK_CHECK requires ZULE_CONFIG_TICK_TOLERANCE_US"
#endif
#if ZULE_CONFIG_TICK_TOLERANCE_US < 1 || ZULE_CONFIG_TICK_TOLERANCE_US > UINT32_MAX
#error "ZULE_CONFIG_TICK_TOLERANCE_US must be from 1 to UINT32_MAX"
#endif
#endif

/* Module settings and the hook conditions derived from them. */
#include "zule_modules_config.h"

#endif
