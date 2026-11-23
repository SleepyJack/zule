#include "zule_target.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

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

/* System calls the C library, newlib, needs from the target. Output goes to
 * QEMU's standard output and exit ends QEMU, both through semihosting, so
 * printf(), assert(), and abort() work in tests. The heap, for the C library's
 * own buffers, grows from the end of bss towards the stack. There are no files,
 * input, or processes, so the remaining calls fail. */
int _write(int file, const char *data, int length);
int _read(int file, char *data, int length);
int _close(int file);
int _lseek(int file, int offset, int whence);
int _fstat(int file, struct stat *status);
int _isatty(int file);
int _getpid(void);
int _kill(int process, int signal);
void *_sbrk(ptrdiff_t increment);
void _exit(int status);

int _write(int file, const char *data, int length) {
    (void)file;
    char chunk[65];
    int written = 0;
    while (written < length) {
        size_t size = 0U;
        while ((size < (sizeof(chunk) - 1U)) && (written < length)) {
            chunk[size] = data[written];
            ++size;
            ++written;
        }
        chunk[size] = '\0';
        zule_target_write(chunk);
    }
    return length;
}

int _read(int file, char *data, int length) {
    (void)file;
    (void)data;
    (void)length;
    return 0;
}

int _close(int file) {
    (void)file;
    return -1;
}

int _lseek(int file, int offset, int whence) {
    (void)file;
    (void)offset;
    (void)whence;
    return 0;
}

int _fstat(int file, struct stat *status) {
    (void)file;
    status->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    (void)file;
    return 1;
}

int _getpid(void) {
    return 1;
}

int _kill(int process, int signal) {
    (void)process;
    (void)signal;
    errno = EINVAL;
    return -1;
}

/* From the linker script: the end of bss, where the heap starts. */
extern char end[];

void *_sbrk(ptrdiff_t increment) {
    static uintptr_t heap_end;
    if (heap_end == 0U) {
        heap_end = (uintptr_t)end;
    }
    const uintptr_t previous = heap_end;
    /* The stack grows down from the top of RAM; keep the heap below it. */
    char stack_marker;
    void *result = (void *)-1;
    if ((previous + (uintptr_t)increment) < (uintptr_t)&stack_marker) {
        heap_end = previous + (uintptr_t)increment;
        result = (void *)previous;
    } else {
        errno = ENOMEM;
    }
    return result;
}

void _exit(int status) {
    zule_target_exit(status);
    for (;;) {
    }
}
