#include <stdio.h>

#include "zule.h"
#include "zule_schedule_definition.h"

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
    zule_result_t result = zule_init();
    if (result != ZULE_RESULT_OK) {
        (void)fputs("Scheduler initialization failed\n", stderr);
        return 1;
    }
    const zule_schedule_t *schedule = zule_schedule_get();

    puts("Zule demo: one cycle of logical slots");
    for (uint32_t slot = 0U; slot < schedule->cycle_slots; ++slot) {
        printf("slot %u\n", (unsigned int)slot);
        result = zule_tick();
        if (result == ZULE_RESULT_OK) {
            result = zule_run();
        }
        if (result != ZULE_RESULT_OK) {
            (void)fprintf(stderr, "Scheduler fault: %u\n", (unsigned int)result);
            return 1;
        }
    }
    return 0;
}
