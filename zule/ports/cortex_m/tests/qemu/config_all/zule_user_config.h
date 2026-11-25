#ifndef ZULE_USER_CONFIG_H
#define ZULE_USER_CONFIG_H

/* Every optional feature, so the target tests run all of Zule on the port: the
 * timebase, every user hook, the tick timing check, and both modules. Under
 * QEMU's -icount with sleep=off, ticks are on time to within a few counts, so a
 * 10 us tolerance, 250 counts at 25 MHz, is ample. */
#define ZULE_CONFIG_TASK_NAMES 1
#define ZULE_CONFIG_TIMEBASE 1
#define ZULE_CONFIG_USER_HOOK_TICK_START 1
#define ZULE_CONFIG_USER_HOOK_TICK_END 1
#define ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE 1
#define ZULE_CONFIG_USER_HOOK_TICK_SCHEDULE_SWITCH 1
#define ZULE_CONFIG_USER_HOOK_TASK_START 1
#define ZULE_CONFIG_USER_HOOK_TASK_END 1
#define ZULE_CONFIG_TICK_CHECK 1
#define ZULE_CONFIG_TICK_TOLERANCE_US 10
#define ZULE_CONFIG_MODULE_TICK_TIMING 1
#define ZULE_CONFIG_MODULE_TASK_STATS 1

#endif
