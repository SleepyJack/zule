#ifndef EXAMPLE_CONSOLE_H
#define EXAMPLE_CONSOLE_H

/* The showcase's text output. Each target the showcase supports implements it,
 * in targets/<target>/, so the example itself needs no C library. */

/* Prepare the console; called once, before any output. */
void example_console_init(void);

/* Write text, a null-terminated string, without adding a newline. */
void example_console_write(const char *text);

#endif
