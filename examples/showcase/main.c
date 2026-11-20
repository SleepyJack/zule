#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "example_console.h"
#include "zule.h"
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

/* Write a number in decimal. The console takes only text, so the example
 * formats numbers itself rather than needing printf from a C library. */
static void write_number(uint64_t value) {
    char digits[21];
    size_t index = sizeof(digits) - 1U;
    digits[index] = '\0';
    do {
        --index;
        digits[index] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    example_console_write(&digits[index]);
}

static void write_us(uint64_t counts) {
    write_number(counts * 1000000U / zule_timebase_hz());
    example_console_write(" us");
}

static void write_task_name(uint32_t task_id) {
    const char *name = zule_task_name_get(task_id);
    example_console_write(name != NULL ? name : "unnamed");
}

/* Report task timing, from the task statistics module, and tick timing, from the
 * tick timing module. Their APIs take consistent snapshots, so tasks can call
 * them while ticks run. */
static void write_timing(void) {
    zule_task_stats_t stats;
    /* Task IDs run from zero; the module rejects the first ID past the last. */
    for (uint32_t task_id = 0U; zule_task_stats_get(task_id, &stats); ++task_id) {
        const uint64_t mean_counts =
            stats.executions != 0U ? stats.total_execution_counts / stats.executions : 0U;
        example_console_write("Task ");
        write_task_name(task_id);
        example_console_write(": mean ");
        write_us(mean_counts);
        example_console_write(", longest ");
        write_us(stats.max_execution_counts);
        example_console_write(", worst dispatch latency ");
        write_us(stats.max_dispatch_latency_counts);
        example_console_write("\n");
    }
    zule_task_stats_summary_t summary;
    zule_task_stats_summary_get(&summary);
    example_console_write("Cycles: ");
    write_number(summary.cycles);
    example_console_write(", busiest cycle ");
    write_us(summary.max_cycle_busy_counts);
    example_console_write(" of task time\n");
    zule_tick_timing_t timing;
    zule_tick_timing_get(&timing);
    example_console_write("Tick timing: worst late ");
    write_us(timing.max_late_counts);
    example_console_write(", worst early ");
    write_us(timing.max_early_counts);
    example_console_write(", longest tick ");
    write_us(timing.max_tick_counts);
    example_console_write("\n");
}

void zule_task_sense(void) {
    example_console_write("sense\n");
}

void zule_task_control(void) {
    example_console_write("control\n");
}

void zule_task_output(void) {
    example_console_write("output\n");
    /* Output runs once per cycle; report every ten cycles. */
    ++completed_cycles;
    if (completed_cycles % 10U == 0U) {
        write_timing();
    }
}

int main(void) {
    example_console_init();
    const zule_config_t config = {.on_fault = handle_fault};
    if (zule_configure(&config) != ZULE_RESULT_OK) {
        example_console_write("Zule configuration failed\n");
        return 1;
    }
    example_console_write("Zule showcase: running until a fault\n");
    const zule_result_t result = zule_run();
    example_console_write("Zule stopped: result ");
    write_number((uint64_t)result);
    example_console_write("\n");
    if (fault_seen) {
        example_console_write("Fault ");
        write_number((uint64_t)last_fault.reason);
        example_console_write(" in task ");
        write_task_name(last_fault.task_id);
        example_console_write(", window ");
        write_number(last_fault.window_index);
        example_console_write(", slot ");
        write_number(last_fault.slot);
        example_console_write(", cycle ");
        write_number(last_fault.cycle_count);
        example_console_write("\n");
#if ZULE_CONFIG_TICK_CHECK
        example_console_write("Detecting tick deviation: ");
        if (last_fault.tick_deviation_counts < 0) {
            example_console_write("-");
        }
        write_number((uint64_t)(last_fault.tick_deviation_counts < 0
                                    ? -(int64_t)last_fault.tick_deviation_counts
                                    : (int64_t)last_fault.tick_deviation_counts));
        example_console_write(" counts\n");
#endif
    }
    write_timing();
    return result == ZULE_RESULT_OK ? 0 : 1;
}
