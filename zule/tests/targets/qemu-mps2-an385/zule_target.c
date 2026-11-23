/* Start-up for Zule's target tests on QEMU's mps2-an385 machine. The C library,
 * newlib, provides everything else through rdimon, its semihosting system calls,
 * which QEMU performs on the host: standard output and error appear on QEMU's,
 * and exit() ends QEMU with the given status. A test returns 0 from main() for
 * success, and a failed assertion or a call to abort() fails it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* From the linker script. */
extern uint32_t zule_target_data_start[];
extern uint32_t zule_target_data_end[];
extern uint32_t zule_target_data_load[];
extern uint32_t zule_target_bss_start[];
extern uint32_t zule_target_bss_end[];

/* From rdimon: opens the standard streams on the host. */
extern void initialise_monitor_handles(void);

int main(void);
void zule_target_start(void);
void zule_target_unexpected_exception(void);

/* Called from Reset_Handler: initialise data and bss and the standard streams,
 * then run the test and end QEMU with its result. */
void zule_target_start(void) {
    const uint32_t *source = zule_target_data_load;
    for (uint32_t *word = zule_target_data_start; word < zule_target_data_end; ++word) {
        *word = *source;
        ++source;
    }
    for (uint32_t *word = zule_target_bss_start; word < zule_target_bss_end; ++word) {
        *word = 0U;
    }
    initialise_monitor_handles();
    exit(main());
}

/* Called from every exception handler nothing else defines, such as a hard
 * fault, so a crash fails the test rather than hanging it. */
void zule_target_unexpected_exception(void) {
    (void)fputs("FAIL: unexpected exception\n", stderr);
    abort();
}
