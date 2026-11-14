#include "zule_port.h"

#include <stdlib.h>
#include <time.h>

static const uint32_t microseconds_per_second = 1000000U;
static const uint32_t nanoseconds_per_microsecond = 1000U;

uint32_t zule_port_time_hz(void) {
    return microseconds_per_second;
}

uint32_t zule_port_time_get(void) {
    struct timespec now;
    /* The API has no error return, and a guessed time would corrupt measurements. */
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        abort();
    }
    /* CLOCK_MONOTONIC is nonnegative. Truncation to 32 bits is the intended
     * modulo-2^32 wrap. */
    const uint64_t microseconds = (uint64_t)now.tv_sec * microseconds_per_second +
                                  (uint64_t)now.tv_nsec / nanoseconds_per_microsecond;
    return (uint32_t)microseconds;
}
