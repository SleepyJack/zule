/* Board support for QEMU's mps2-an385 machine: start-up, and ending the program
 * through semihosting, which QEMU performs on the host. A program's exit status
 * becomes QEMU's: 0 for success, nonzero otherwise. QEMU only: on a real board
 * without a debugger, semihosting stops the core. */
#include <stddef.h>
#include <stdint.h>

#define SEMIHOSTING_SYS_EXIT 0x18U
#define SEMIHOSTING_APPLICATION_EXIT 0x20026U
#define SEMIHOSTING_RUN_TIME_ERROR 0x20023U

/* From the linker script. */
extern uint32_t board_data_start[];
extern uint32_t board_data_end[];
extern uint32_t board_data_load[];
extern uint32_t board_bss_start[];
extern uint32_t board_bss_end[];

int main(void);
uint32_t board_semihosting(uint32_t operation, uintptr_t argument);
void board_start(void);
void board_unexpected_exception(void);

static void board_exit(int status) {
    (void)board_semihosting(SEMIHOSTING_SYS_EXIT, (status == 0) ? SEMIHOSTING_APPLICATION_EXIT
                                                                : SEMIHOSTING_RUN_TIME_ERROR);
    for (;;) {
    }
}

/* Called from Reset_Handler: initialise data and bss, run the application, and
 * end QEMU with its result. */
void board_start(void) {
    const uint32_t *source = board_data_load;
    for (uint32_t *word = board_data_start; word < board_data_end; ++word) {
        *word = *source;
        ++source;
    }
    for (uint32_t *word = board_bss_start; word < board_bss_end; ++word) {
        *word = 0U;
    }
    board_exit(main());
}

/* An exception nothing handles, such as a hard fault, ends QEMU with a failure. */
void board_unexpected_exception(void) {
    board_exit(1);
}

/* GCC may call these even without a C library. */
void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);
int memcmp(const void *first, const void *second, size_t size);

void *memcpy(void *destination, const void *source, size_t size) {
    return memmove(destination, source, size);
}

void *memmove(void *destination, const void *source, size_t size) {
    unsigned char *to = destination;
    const unsigned char *from = source;
    if (to < from) {
        for (size_t index = 0U; index < size; ++index) {
            to[index] = from[index];
        }
    } else {
        for (size_t index = size; index > 0U; --index) {
            to[index - 1U] = from[index - 1U];
        }
    }
    return destination;
}

void *memset(void *destination, int value, size_t size) {
    unsigned char *to = destination;
    for (size_t index = 0U; index < size; ++index) {
        to[index] = (unsigned char)value;
    }
    return destination;
}

int memcmp(const void *first, const void *second, size_t size) {
    const unsigned char *a = first;
    const unsigned char *b = second;
    int result = 0;
    for (size_t index = 0U; (index < size) && (result == 0); ++index) {
        result = (int)a[index] - (int)b[index];
    }
    return result;
}
