#ifndef ZULE_USER_CONFIG_H
#define ZULE_USER_CONFIG_H

/* Explicit demo settings; compiler definitions may override these values. */
#ifndef ZULE_CONFIG_TASK_NAMES
#define ZULE_CONFIG_TASK_NAMES 1
#endif

/* The timebase and two user hooks measure the longest task; see main.c. */
#ifndef ZULE_CONFIG_TIMEBASE
#define ZULE_CONFIG_TIMEBASE 1
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TASK_START
#define ZULE_CONFIG_USER_HOOK_TASK_START 1
#endif

#ifndef ZULE_CONFIG_USER_HOOK_TASK_END
#define ZULE_CONFIG_USER_HOOK_TASK_END 1
#endif

#endif
