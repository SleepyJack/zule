/* The four functions GCC and Clang require even in a freestanding environment:
 * they may call them for struct copies and initialisation. The target's
 * environment normally provides them; this stands in for it, so the link check
 * still fails on any other C library function. Nothing runs these, and volatile
 * access stops the compiler turning the loops back into calls to themselves. */
#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t count);
void *memmove(void *destination, const void *source, size_t count);
void *memset(void *destination, int value, size_t count);
int memcmp(const void *left, const void *right, size_t count);

void *memcpy(void *destination, const void *source, size_t count) {
    volatile unsigned char *to = destination;
    const volatile unsigned char *from = source;
    for (size_t index = 0U; index < count; ++index) {
        to[index] = from[index];
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t count) {
    volatile unsigned char *to = destination;
    const volatile unsigned char *from = source;
    if (to < from) {
        for (size_t index = 0U; index < count; ++index) {
            to[index] = from[index];
        }
    } else {
        for (size_t index = count; index > 0U; --index) {
            to[index - 1U] = from[index - 1U];
        }
    }
    return destination;
}

void *memset(void *destination, int value, size_t count) {
    volatile unsigned char *to = destination;
    for (size_t index = 0U; index < count; ++index) {
        to[index] = (unsigned char)value;
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t count) {
    const volatile unsigned char *a = left;
    const volatile unsigned char *b = right;
    for (size_t index = 0U; index < count; ++index) {
        if (a[index] != b[index]) {
            return (a[index] < b[index]) ? -1 : 1;
        }
    }
    return 0;
}
