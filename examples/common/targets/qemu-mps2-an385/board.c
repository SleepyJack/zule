/* Board support for QEMU's mps2-an385 machine: start-up, and the C library's
 * system calls, with output and exit through semihosting, which QEMU performs on
 * the host. Output goes to QEMU's standard output, and a program's exit status
 * becomes QEMU's: 0 for success, nonzero otherwise. QEMU only: on a real board
 * without a debugger, semihosting stops the core. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#define SEMIHOSTING_SYS_WRITE0 0x04U
#define SEMIHOSTING_SYS_EXIT 0x18U
#define SEMIHOSTING_APPLICATION_EXIT 0x20026U
#define SEMIHOSTING_RUN_TIME_ERROR 0x20023U

/* From the linker script. */
extern uint32_t board_data_start[];
extern uint32_t board_data_end[];
extern uint32_t board_data_load[];
extern uint32_t board_bss_start[];
extern uint32_t board_bss_end[];
extern char end[];

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

/* System calls the C library, newlib, needs from the board. Output, standard and
 * error alike, goes to QEMU's standard output, and exit ends QEMU. The heap, for the C
 * library's own buffers, grows from the end of bss towards the stack. There are
 * no files, input, or processes, so the remaining calls fail. */
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

/* Semihosting writes null-terminated strings, so write in terminated chunks. */
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
        (void)board_semihosting(SEMIHOSTING_SYS_WRITE0, (uintptr_t)chunk);
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
    board_exit(status);
}
