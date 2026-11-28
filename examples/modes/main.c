#include <stdbool.h>
#include <stdio.h>

#include "zule.h"
#include "zule_schedule_ids.h"

/* Operating modes as schedules. The run starts in the startup schedule, whose
 * idle slots absorb startup latency, so no window of the operational schedule
 * is shortened by it. The startup cycle ends with the mode manager, which
 * checks the system and requests the operational schedule, which starts at slot
 * 0 of the next cycle. After a few operational cycles the mode manager requests
 * the shutdown schedule, whose task puts the outputs in a safe state and stops
 * the scheduler, so the example ends by itself rather than on a signal. */

/* Operational cycles to run before shutting down. The mode manager runs in the
 * last slot of each, so it counts them. */
#define OPERATIONAL_CYCLES 3U

/* How far through the modes the example is. Tasks run one after another, so this
 * needs no locking. */
static bool operational_requested;
static uint32_t operational_cycles;

void zule_task_sense(void) {
    puts("sense");
}

void zule_task_control(void) {
    puts("control");
}

void zule_task_mode_manager(void) {
    if (!operational_requested) {
        /* Stands in for startup checks: inputs valid, outputs safe. */
        if (zule_schedule_request(ZULE_SCHEDULE_ID_OPERATIONAL) == ZULE_RESULT_OK) {
            operational_requested = true;
            puts("mode_manager: startup complete, switching to operational");
        }
        return;
    }
    /* In the operational schedule, the mode manager watches for conditions that
     * need another mode. Here the condition is simply having run long enough. */
    ++operational_cycles;
    if (operational_cycles == OPERATIONAL_CYCLES) {
        if (zule_schedule_request(ZULE_SCHEDULE_ID_SHUTDOWN) == ZULE_RESULT_OK) {
            puts("mode_manager: work done, switching to shutdown");
        }
    }
}

void zule_task_safe_state(void) {
    /* Stands in for putting the outputs in a safe state before stopping. The
     * task returns as usual, and the run ends without dispatching another. */
    puts("safe_state: outputs safe, stopping the scheduler");
    if (zule_terminate() != ZULE_RESULT_OK) {
        (void)fputs("safe_state: the scheduler refused to stop\n", stderr);
    }
}

/* The fault handler runs in tick context, so it only keeps the report. main()
 * prints it after zule_run() returns. */
static zule_fault_info_t last_fault;
static bool fault_seen;

static void handle_fault(const zule_fault_info_t *fault) {
    last_fault = *fault;
    fault_seen = true;
}

int main(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    const zule_config_t config = {
        .on_fault = handle_fault,
        .schedule_id = ZULE_SCHEDULE_ID_STARTUP,
    };
    if (zule_configure(&config) != ZULE_RESULT_OK) {
        (void)fputs("Zule configuration failed\n", stderr);
        return 1;
    }
    puts("Zule modes: starting in the startup schedule");
    const zule_result_t result = zule_run();
    if (result == ZULE_RESULT_TERMINATED) {
        puts("Zule modes: the scheduler stopped as requested");
    } else {
        (void)fprintf(stderr, "Zule stopped: %u\n", (unsigned int)result);
    }
    if (fault_seen) {
        (void)fprintf(stderr, "Fault %u in schedule %u, window %zu, at slot %u (cycle %u)\n",
                      (unsigned int)last_fault.reason, (unsigned int)last_fault.schedule_id,
                      last_fault.window_index, (unsigned int)last_fault.slot,
                      (unsigned int)last_fault.cycle_count);
    }
    /* Termination is this example's successful end; anything else is a failure. */
    return result == ZULE_RESULT_TERMINATED ? 0 : 1;
}
