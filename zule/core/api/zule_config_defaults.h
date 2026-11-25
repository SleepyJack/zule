#ifndef ZULE_CONFIG_DEFAULTS_H
#define ZULE_CONFIG_DEFAULTS_H

/* The core settings and their defaults. Included by zule_config_apply.h, after
 * any zule_user_config.h, so each default applies only to a setting the
 * integrator left unset; not included on its own. The checks are in
 * zule_config_apply.h. */

/* Compile each task's diagnostic name into the schedule. */
#ifndef ZULE_CONFIG_TASK_NAMES
#define ZULE_CONFIG_TASK_NAMES 0
#endif

/* Optional feature: the port timebase, for sub-slot timing. The selected port
 * must provide it when enabled. */
#ifndef ZULE_CONFIG_TIMEBASE
#define ZULE_CONFIG_TIMEBASE 0
#endif

/* User hooks: set to 1 to have the core call the matching zule_user_hook_...()
 * function, which the application defines. See zule_hooks.h. */
#ifndef ZULE_CONFIG_USER_HOOK_TICK_START
#define ZULE_CONFIG_USER_HOOK_TICK_START 0
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TICK_END
#define ZULE_CONFIG_USER_HOOK_TICK_END 0
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE
#define ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE 0
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH
#define ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH 0
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TASK_START
#define ZULE_CONFIG_USER_HOOK_TASK_START 0
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TASK_END
#define ZULE_CONFIG_USER_HOOK_TASK_END 0
#endif

/* Optional feature: tick timing check. Faults when a tick deviates from its
 * expected time, early or late, by more than ZULE_CONFIG_TICK_TOLERANCE_US
 * microseconds, which has no default and is required with the check. */
#ifndef ZULE_CONFIG_TICK_CHECK
#define ZULE_CONFIG_TICK_CHECK 0
#endif

#endif
