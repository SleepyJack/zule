#include <stdio.h>
#include <stdbool.h>

#include "zule.h"

/* Only the handler writes these while running. Main reads after zule_run()
 * returns, when tick delivery has been stopped and quiesced. */
static zule_fault_info_t last_fault;
static bool fault_seen;

static void handle_fault(const zule_fault_info_t *fault) {
    last_fault = *fault;
    fault_seen = true;
}

void zule_task_sense(void) {
    puts("sense");
}

void zule_task_control(void) {
    puts("control");
}

void zule_task_output(void) {
    puts("output");
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
        (void)fprintf(stderr, "Fault %u in window %zu at slot %u\n",
                      (unsigned int)last_fault.reason, last_fault.window_index,
                      (unsigned int)last_fault.slot);
    }
    return result == ZULE_RESULT_OK ? 0 : 1;
}
