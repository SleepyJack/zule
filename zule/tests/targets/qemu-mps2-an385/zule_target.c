#include "zule_target.h"

#include <stddef.h>
#include <stdint.h>

/* Semihosting operations and the exit reasons QEMU maps to its exit status. */
#define SEMIHOSTING_SYS_WRITE0 0x04U
#define SEMIHOSTING_SYS_EXIT 0x18U
#define SEMIHOSTING_APPLICATION_EXIT 0x20026U
#define SEMIHOSTING_RUN_TIME_ERROR 0x20023U

/* From the linker script. */
extern uint32_t zule_target_data_start[];
extern uint32_t zule_target_data_end[];
extern uint32_t zule_target_data_load[];
extern uint32_t zule_target_bss_start[];
extern uint32_t zule_target_bss_end[];

int main(void);
uint32_t zule_target_semihosting(uint32_t operation, uintptr_t argument);
void zule_target_start(void);
void zule_target_unexpected_exception(void);

void zule_target_write(const char *text) {
    (void)zule_target_semihosting(SEMIHOSTING_SYS_WRITE0, (uintptr_t)text);
}

void zule_target_write_number(uint32_t value) {
    char digits[11];
    size_t index = sizeof(digits) - 1U;
    digits[index] = '\0';
    do {
        --index;
        digits[index] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    zule_target_write(&digits[index]);
}

void zule_target_exit(int status) {
    (void)zule_target_semihosting(SEMIHOSTING_SYS_EXIT, (status == 0) ? SEMIHOSTING_APPLICATION_EXIT
                                                                      : SEMIHOSTING_RUN_TIME_ERROR);
    for (;;) {
    }
}

/* Called from Reset_Handler: initialise data and bss, then run the test. */
void zule_target_start(void) {
    const uint32_t *source = zule_target_data_load;
    for (uint32_t *word = zule_target_data_start; word < zule_target_data_end; ++word) {
        *word = *source;
        ++source;
    }
    for (uint32_t *word = zule_target_bss_start; word < zule_target_bss_end; ++word) {
        *word = 0U;
    }
    zule_target_exit(main());
}

/* Called from every exception handler nothing else defines, such as a hard
 * fault, so a crash fails the test rather than hanging it. */
void zule_target_unexpected_exception(void) {
    zule_target_write("FAIL: unexpected exception\n");
    zule_target_exit(1);
}

/* GCC may call these even in a freestanding build; there is no C library. */
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
