#include "zule_task_stats.h"
#include "zule_port.h"
#include "zule_task_stats_hooks.h"

#if ZULE_CONFIG_MODULE_TASK_STATS
#include "zule_modules_task_data.h"
#include "zule_schedule_definition.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Each task's statistics live in the per-task storage the generated schedule
 * defines; the totals and the run's state live here. Written by the hook
 * handlers, which never run concurrently with each other, and read or cleared by
 * the API inside a critical section, so access never overlaps. */
static zule_task_stats_summary_t summary;

/* The current run's measurement state, cleared when each run starts. */
typedef struct {
    /* Only one task is released or running at a time, so one release and one
     * start time serve every task. */
    uint32_t release_counts;
    uint32_t start_counts;
    /* Execution time so far in the current cycle. */
    uint32_t cycle_busy_counts;
    /* The current cycle, valid once the run's first tick has started it. */
    uint32_t cycle_count;
    bool cycle_started;
} zule_task_stats_run_t;

static zule_task_stats_run_t run;

/* Return a task's statistics. Hooks pass valid IDs, and the API checks them. */
static zule_task_stats_t *task_stats(uint32_t task_id) {
    return &zule_modules_task_data_get()[(size_t)task_id].task_stats;
}

void zule_task_stats_init(void) {
    run = (zule_task_stats_run_t){0};
}

void zule_task_stats_tick_start(uint32_t cycle_count) {
    /* A new cycle count means the previous cycle completed. After a fault the
     * count stops advancing, so that cycle never completes. The first tick of a
     * run starts its first cycle. */
    if (run.cycle_started && (cycle_count != run.cycle_count)) {
        ++summary.cycles;
        if (run.cycle_busy_counts > summary.max_cycle_busy_counts) {
            summary.max_cycle_busy_counts = run.cycle_busy_counts;
        }
        run.cycle_busy_counts = 0U;
    }
    run.cycle_count = cycle_count;
    run.cycle_started = true;
}

void zule_task_stats_tick_task_release(uint32_t time_counts) {
    run.release_counts = time_counts;
}

void zule_task_stats_task_start(uint32_t time_counts, uint32_t task_id) {
    run.start_counts = time_counts;
    zule_task_stats_t *const stats = task_stats(task_id);
    /* Unsigned subtraction measures any interval up to one timebase wrap. */
    const uint32_t latency_counts = time_counts - run.release_counts;
    if (latency_counts > stats->max_dispatch_latency_counts) {
        stats->max_dispatch_latency_counts = latency_counts;
    }
}

void zule_task_stats_task_end(uint32_t time_counts, uint32_t task_id) {
    zule_task_stats_t *const stats = task_stats(task_id);
    const uint32_t execution_counts = time_counts - run.start_counts;
    if ((stats->executions == 0U) || (execution_counts < stats->min_execution_counts)) {
        stats->min_execution_counts = execution_counts;
    }
    if (execution_counts > stats->max_execution_counts) {
        stats->max_execution_counts = execution_counts;
    }
    ++stats->executions;
    stats->total_execution_counts += execution_counts;
    summary.busy_counts += execution_counts;
    /* Executions in a cycle that completes do not overrun, so they fit in one
     * cycle, which is below 2^32 counts. */
    run.cycle_busy_counts += execution_counts;
}

bool zule_task_stats_get(uint32_t task_id, zule_task_stats_t *stats) {
    bool found = false;
    if ((size_t)task_id < zule_schedule_definition_get()->tasks.count) {
        const zule_port_critical_state_t state = zule_port_critical_enter();
        *stats = *task_stats(task_id);
        zule_port_critical_exit(state);
        found = true;
    }
    return found;
}

void zule_task_stats_summary_get(zule_task_stats_summary_t *snapshot) {
    const zule_port_critical_state_t state = zule_port_critical_enter();
    *snapshot = summary;
    zule_port_critical_exit(state);
}

void zule_task_stats_reset(void) {
    zule_modules_task_data_t *const data = zule_modules_task_data_get();
    const size_t task_count = zule_schedule_definition_get()->tasks.count;
    const zule_port_critical_state_t state = zule_port_critical_enter();
    summary = (zule_task_stats_summary_t){0};
    for (size_t index = 0U; index < task_count; ++index) {
        data[index].task_stats = (zule_task_stats_t){0};
    }
    zule_port_critical_exit(state);
}

#endif
