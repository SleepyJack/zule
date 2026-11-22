#ifndef ZULE_TARGET_H
#define ZULE_TARGET_H

/* Zule's test support on QEMU's mps2-an385 machine: output and exit through
 * semihosting, which QEMU performs on the host. The startup code runs main() and
 * exits with its result, so a test returns 0 on success. */
#include <stdint.h>

/* Write text, a null-terminated string, to QEMU's standard output. */
void zule_target_write(const char *text);

/* Write a number in decimal. */
void zule_target_write_number(uint32_t value);

/* End the run: QEMU exits with status 0 when status is 0, and nonzero otherwise. */
void zule_target_exit(int status);

#endif
