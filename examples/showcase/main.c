#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "zule.h"
#include "zule_schedule_ids.h"
#include "zule_task_stats.h"
#include "zule_tick_timing.h"

/* Only the handler writes these while running. Main reads after zule_run()
 * returns, when tick delivery has been stopped and quiesced. */
static zule_fault_info_t last_fault;
static bool fault_seen;

static void handle_fault(const zule_fault_info_t *fault) {
    last_fault = *fault;
    fault_seen = true;
}

static uint32_t completed_cycles;

static unsigned long counts_to_us(uint64_t counts) {
    return (unsigned long)(counts * 1000000U / zule_timebase_hz());
}

/* Report task timing, from the task statistics module, and tick timing, from the
 * tick timing module. Their APIs take consistent snapshots, so tasks can call
 * them while ticks run. */
static void print_timing(FILE *stream) {
    zule_task_stats_t stats;
    /* Task IDs run from zero; the module rejects the first ID past the last. */
    for (uint32_t task_id = 0U; zule_task_stats_get(task_id, &stats); ++task_id) {
        const char *name = zule_task_name_get(task_id);
        const uint64_t mean_counts =
            stats.executions != 0U ? stats.total_execution_counts / stats.executions : 0U;
        (void)fprintf(stream,
                      "Task %s: mean %lu us, longest %lu us, worst dispatch latency %lu us\n",
                      name != NULL ? name : "unnamed", counts_to_us(mean_counts),
                      counts_to_us(stats.max_execution_counts),
                      counts_to_us(stats.max_dispatch_latency_counts));
    }
    zule_task_stats_summary_t summary;
    zule_task_stats_summary_get(&summary);
    (void)fprintf(stream, "Cycles: %lu, busiest cycle %lu us of task time\n",
                  (unsigned long)summary.cycles, counts_to_us(summary.max_cycle_busy_counts));
    zule_tick_timing_t timing;
    zule_tick_timing_get(&timing);
    (void)fprintf(stream,
                  "Tick timing: worst late %lu us, worst early %lu us, longest tick %lu us\n",
                  counts_to_us(timing.max_late_counts), counts_to_us(timing.max_early_counts),
                  counts_to_us(timing.max_tick_counts));
}

void zule_task_sense(void) {
    puts("sense");
}

void zule_task_control(void) {
    puts("control");
}

void zule_task_output(void) {
    puts("output");
    /* Output runs once per cycle; report every ten cycles. */
    ++completed_cycles;
    if (completed_cycles % 10U == 0U) {
        print_timing(stdout);
    }
}

int main(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    const zule_config_t config = {
        .on_fault = handle_fault,
        .schedule_id = ZULE_SCHEDULE_ID_MAIN,
    };
    if (zule_configure(&config) != ZULE_RESULT_OK) {
        (void)fputs("Zule configuration failed\n", stderr);
        return 1;
    }
    puts("Zule showcase: running until a fault");
    const zule_result_t result = zule_run();
    (void)fprintf(stderr, "Zule stopped: %u\n", (unsigned int)result);
    if (fault_seen) {
        const char *name = zule_task_name_get(last_fault.task_id);
        (void)fprintf(stderr, "Task %u (%s): fault %u in window %zu at slot %u (cycle %u)\n",
                      (unsigned int)last_fault.task_id, name != NULL ? name : "unnamed",
                      (unsigned int)last_fault.reason, last_fault.window_index,
                      (unsigned int)last_fault.slot, (unsigned int)last_fault.cycle_count);
#if ZULE_CONFIG_TICK_CHECK
        (void)fprintf(stderr, "Detecting tick deviation: %ld counts\n",
                      (long)last_fault.tick_deviation_counts);
#endif
    }
    print_timing(stderr);
    return result == ZULE_RESULT_OK ? 0 : 1;
}
