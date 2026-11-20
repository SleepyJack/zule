#ifndef ZULE_TASK_STATS_H
#define ZULE_TASK_STATS_H

/* Task statistics module: how long each task runs and how long it waits to
 * start, and totals over all tasks, so budgets can be chosen and checked from
 * measurements. Enabled by ZULE_CONFIG_MODULE_TASK_STATS; declared only when
 * enabled.
 *
 * Times are timebase counts; convert with zule_timebase_hz(). Single
 * measurements, such as one execution, are exact up to one timebase wrap of 2^32
 * counts. Totals and counts are 64-bit, so they do not wrap in practice: even at
 * the fastest timebase, 2^32 - 1 Hz, totals last over 100 years. */
#include "zule.h"

#include <stdbool.h>
#include <stdint.h>

#if ZULE_CONFIG_MODULE_TASK_STATS

/* Statistics for one task. */
typedef struct {
    /* Executions measured since the last reset. An execution runs from the task
     * start hook to the task end hook: the task function's elapsed time,
     * including interrupts and ticks that preempt it. Executions that overrun
     * are included. */
    uint64_t executions;
    /* Total execution time; divided by executions, the mean. */
    uint64_t total_execution_counts;
    /* Shortest and longest execution; 0 while executions is 0. */
    uint32_t min_execution_counts;
    uint32_t max_execution_counts;
    /* Longest dispatch latency: from the task's release, when its window
     * starts, to its start. Dispatch latency uses up the window's budget. */
    uint32_t max_dispatch_latency_counts;
} zule_task_stats_t;

/* Totals over all tasks. */
typedef struct {
    /* Cycles completed since the last reset. A cycle completes when the next
     * one starts; after a fault, the cycle never completes. */
    uint64_t cycles;
    /* Total execution time of all tasks. */
    uint64_t busy_counts;
    /* Most execution time in any completed cycle. An execution counts toward
     * the cycle in which it ends. */
    uint32_t max_cycle_busy_counts;
} zule_task_stats_summary_t;

/* Copy a consistent snapshot of one task's statistics to *stats, which must
 * point to writable storage, and return true. Return false, leaving *stats
 * unchanged, for an ID that is not one of the schedule's task IDs. Statistics
 * accumulate across runs until reset. Call from application context, including
 * tasks; not from hooks or the fault handler. */
bool zule_task_stats_get(uint32_t task_id, zule_task_stats_t *stats);

/* Copy a consistent snapshot of the totals to *snapshot, which must point to
 * writable storage. Same calling rules as zule_task_stats_get(). */
void zule_task_stats_summary_get(zule_task_stats_summary_t *snapshot);

/* Clear every task's statistics and the totals together, so they always cover
 * the same interval. The cycle in progress is still measured in full. Same
 * calling rules as zule_task_stats_get(). */
void zule_task_stats_reset(void);

#endif

#endif
