#include "example_console.h"

#include <stdio.h>

/* The console is standard output, unbuffered so output appears as it is written
 * and none is lost when the process is stopped. */
void example_console_init(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0);
}

void example_console_write(const char *text) {
    (void)fputs(text, stdout);
}
