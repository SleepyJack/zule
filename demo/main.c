#include <stdio.h>

#include "zule.h"

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
    puts("Zule demo: running until a fault; press Ctrl-C to exit");
    const zule_result_t result = zule_run();
    (void)fprintf(stderr, "Zule stopped: %u\n", (unsigned int)result);
    return result == ZULE_RESULT_OK ? 0 : 1;
}
