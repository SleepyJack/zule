#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "zule.h"

/* Only the handler writes these while running. Main reads after zule_run()
 * returns, when tick delivery has been stopped and quiesced. */
static zule_fault_info_t last_fault;
static bool fault_seen;

static void handle_fault(const zule_fault_info_t *fault) {
    last_fault = *fault;
    fault_seen = true;
}

/* User hooks measuring the longest task execution. Task start and end hooks run
 * in application context, like the tasks themselves, so tasks can read these
 * values without synchronization. */
static uint32_t task_start_counts;
static uint32_t longest_counts;
static uint32_t longest_task_id;
static uint32_t completed_cycles;

void zule_user_hook_task_start(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)task_id;
    (void)window_index;
    task_start_counts = time_counts;
}

void zule_user_hook_task_end(uint32_t time_counts, uint32_t task_id, size_t window_index) {
    (void)window_index;
    /* Unsigned subtraction handles a timebase wrap. */
    const uint32_t duration_counts = time_counts - task_start_counts;
    if (duration_counts > longest_counts) {
        longest_counts = duration_counts;
        longest_task_id = task_id;
    }
}

static void print_longest_task(FILE *stream) {
    const char *name = zule_task_name_get(longest_task_id);
    const uint64_t duration_us = (uint64_t)longest_counts * 1000000U / zule_timebase_hz();
    (void)fprintf(stream, "Longest task so far: %s, %lu us\n", name != NULL ? name : "unnamed",
                  (unsigned long)duration_us);
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
        print_longest_task(stdout);
    }
}

int main(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    const zule_config_t config = {.on_fault = handle_fault};
    if (zule_configure(&config) != ZULE_RESULT_OK) {
        (void)fputs("Zule configuration failed\n", stderr);
        return 1;
    }
    puts("Zule demo: running until a fault; press Ctrl-C to exit");
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
    print_longest_task(stderr);
    return result == ZULE_RESULT_OK ? 0 : 1;
}
