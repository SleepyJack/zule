#ifndef ZULE_USER_CONFIG_H
#define ZULE_USER_CONFIG_H

/* Showcase build settings. */
#define ZULE_CONFIG_TASK_NAMES 1

/* The timebase, which the tick timing check and the modules need. */
#define ZULE_CONFIG_TIMEBASE 1

/* Fault if a tick is more than half a 50 ms slot early or late. Generous,
 * because host thread scheduling is not real-time. */
#define ZULE_CONFIG_TICK_CHECK 1
#define ZULE_CONFIG_TICK_TOLERANCE_US 25000

/* Measure tick deviations, to see what tolerance the host needs. */
#define ZULE_CONFIG_MODULE_TICK_TIMING 1

/* Measure task execution times and dispatch latency, to compare with budgets. */
#define ZULE_CONFIG_MODULE_TASK_STATS 1

#endif
