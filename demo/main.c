#include <stdio.h>

#include "zule.h"

extern const zule_schedule_t zule_schedule;

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
    zule_t scheduler;
    zule_init(&scheduler, &zule_schedule);

    puts("Zule demo: one cycle of logical slots");
    for (uint32_t slot = 0U; slot < zule_schedule.cycle_slots; ++slot) {
        printf("slot %u\n", (unsigned int)slot);
        zule_tick(&scheduler);
    }
    return 0;
}
