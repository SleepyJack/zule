#include <stdio.h>

#include "zule.h"

void zule_task_sense(void)
{
    puts("sense");
}

void zule_task_control(void)
{
    puts("control");
}

void zule_task_output(void)
{
    puts("output");
}

int main(void)
{
    zule_scheduler_t *scheduler = zule_init();

    puts("Zule demo: one cycle of logical slots");
    for (uint32_t slot = 0U; slot < scheduler->schedule->cycle_slots; ++slot) {
        printf("slot %u\n", (unsigned int)slot);
        zule_tick(scheduler);
        zule_run(scheduler);
        if (scheduler->task.fault != ZULE_TASK_FAULT_NONE) {
            fputs("Scheduler fault\n", stderr);
            return 1;
        }
    }
    return 0;
}
