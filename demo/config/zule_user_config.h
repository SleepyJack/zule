#ifndef ZULE_USER_CONFIG_H
#define ZULE_USER_CONFIG_H

/* Demo build settings. */
#define ZULE_CONFIG_TASK_NAMES 1

/* The timebase and two user hooks measure the longest task; see main.c. */
#define ZULE_CONFIG_TIMEBASE 1
#define ZULE_CONFIG_USER_HOOK_TASK_START 1
#define ZULE_CONFIG_USER_HOOK_TASK_END 1

/* Fault if a tick is more than half a 50 ms slot early or late. Generous,
 * because host thread scheduling is not real-time. */
#define ZULE_CONFIG_TICK_CHECK 1
#define ZULE_CONFIG_TICK_TOLERANCE_US 25000

#endif
