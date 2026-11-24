/* Board support for QEMU's mps2-an385 machine: start-up. The C library, newlib,
 * provides everything else through rdimon, its semihosting system calls, which
 * QEMU performs on the host: standard output and error appear on QEMU's, and a
 * program's exit status becomes QEMU's: 0 for success, nonzero otherwise. QEMU
 * only: on a real board without a debugger, semihosting stops the core. */
#include <stdint.h>
#include <stdlib.h>

/* From the linker script. */
extern uint32_t board_data_start[];
extern uint32_t board_data_end[];
extern uint32_t board_data_load[];
extern uint32_t board_bss_start[];
extern uint32_t board_bss_end[];

/* From rdimon: opens the standard streams on the host. */
extern void initialise_monitor_handles(void);

int main(void);
void board_start(void);
void board_unexpected_exception(void);

/* Called from Reset_Handler: initialise data and bss and the standard streams,
 * run the application, and end QEMU with its result. */
void board_start(void) {
    const uint32_t *source = board_data_load;
    for (uint32_t *word = board_data_start; word < board_data_end; ++word) {
        *word = *source;
        ++source;
    }
    for (uint32_t *word = board_bss_start; word < board_bss_end; ++word) {
        *word = 0U;
    }
    initialise_monitor_handles();
    exit(main());
}

/* An exception nothing handles, such as a hard fault, ends QEMU with a failure. */
void board_unexpected_exception(void) {
    abort();
}
