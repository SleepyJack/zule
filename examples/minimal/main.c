#include <stdint.h>

#include "zule.h"

/* The smallest complete Zule application: two tasks, a fault handler, and
 * zule_run(). It uses no optional features and no output, so it runs on every
 * target. */

/* Data the tasks share. Tasks run one after another, never at the same time, so
 * they need no locking. */
static uint32_t sensor_reading;
static uint32_t control_output;

void zule_task_read_sensors(void) {
    /* Stands in for sampling inputs. */
    ++sensor_reading;
}

void zule_task_update_control(void) {
    /* Stands in for computing and applying outputs. */
    control_output = sensor_reading / 2U;
}

/* The fault handler runs in tick context, so it only keeps the report, where a
 * debugger can read it. zule_run() then returns the fault. */
static zule_fault_info_t last_fault;

static void handle_fault(const zule_fault_info_t *fault) {
    last_fault = *fault;
}

int main(void) {
    const zule_config_t config = {.on_fault = handle_fault};
    zule_result_t result = zule_configure(&config);
    if (result == ZULE_RESULT_OK) {
        /* Returns only on a fault or port error. */
        result = zule_run();
    }
    return (result == ZULE_RESULT_OK) ? 0 : 1;
}
