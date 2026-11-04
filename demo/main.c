#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "zule.h"
#include "zule_port.h"
#include "zule_schedule_definition.h"

extern const uint32_t zule_slot_us;

typedef struct {
    uint32_t slot_us;
    uint32_t cycle_slots;
    bool stop;
    bool done;
    int error;
    zule_result_t result;
} zule_demo_timer_t;

static void advance_deadline(struct timespec *deadline, uint32_t slot_us) {
    deadline->tv_sec += slot_us / 1000000U;
    deadline->tv_nsec += (long)(slot_us % 1000000U) * 1000L;
    if (deadline->tv_nsec >= 1000000000L) {
        ++deadline->tv_sec;
        deadline->tv_nsec -= 1000000000L;
    }
}

static void *timer_thread(void *argument) {
    zule_demo_timer_t *timer = argument;
    struct timespec deadline;
    int error = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        error = errno;
    }

    for (uint32_t slot = 0U; slot < timer->cycle_slots && error == 0; ++slot) {
        advance_deadline(&deadline, timer->slot_us);
        do {
            error = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
        } while (error == EINTR);
        if (error != 0) {
            break;
        }

        const zule_port_critical_state_t state = zule_port_critical_enter();
        if (timer->stop) {
            zule_port_critical_exit(state);
            break;
        }

        /* Do not replay a burst of stale ticks if the host falls behind. */
        struct timespec following_deadline = deadline;
        struct timespec now;
        advance_deadline(&following_deadline, timer->slot_us);
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            error = errno;
        } else if (now.tv_sec > following_deadline.tv_sec ||
                   (now.tv_sec == following_deadline.tv_sec &&
                    now.tv_nsec >= following_deadline.tv_nsec)) {
            error = ETIMEDOUT;
        }
        if (error == 0) {
            timer->result = zule_tick();
        }
        const bool failed = timer->result != ZULE_RESULT_OK;
        zule_port_critical_exit(state);
        if (failed) {
            break;
        }
    }

    const zule_port_critical_state_t state = zule_port_critical_enter();
    timer->error = error;
    timer->done = true;
    zule_port_critical_exit(state);
    return NULL;
}

void zule_task_sense(void) {
    puts("sense");
}

void zule_task_control(void) {
    puts("control");
}

void zule_task_output(void) {
    puts("output");
}

int main(void) {
    zule_result_t result = zule_init();
    if (result != ZULE_RESULT_OK) {
        (void)fputs("Scheduler initialization failed\n", stderr);
        return 1;
    }
    const zule_schedule_t *schedule = zule_schedule_get();
    zule_demo_timer_t timer = {
        .slot_us = zule_slot_us,
        .cycle_slots = schedule->cycle_slots,
        .stop = false,
        .done = false,
        .error = 0,
        .result = ZULE_RESULT_OK,
    };
    printf("Zule POSIX demo: one cycle, %u us slots\n", (unsigned int)timer.slot_us);
    pthread_t thread;
    int error = pthread_create(&thread, NULL, timer_thread, &timer);
    if (error != 0) {
        (void)fprintf(stderr, "Cannot start timer: %s\n", strerror(error));
        return 1;
    }

    while (true) {
        const zule_port_critical_state_t state = zule_port_critical_enter();
        const bool done = timer.done;
        error = timer.error;
        result = timer.result;
        zule_port_critical_exit(state);
        if (error == 0 && result == ZULE_RESULT_OK) {
            /* Observe done before run so the final published task is drained. */
            result = zule_run();
        }
        if (done || error != 0 || result != ZULE_RESULT_OK) {
            break;
        }
        struct timespec pause = {0, 1000000L};
        while (nanosleep(&pause, &pause) != 0) {
            if (errno != EINTR) {
                error = errno;
                break;
            }
        }
        if (error != 0) {
            break;
        }
    }

    const zule_port_critical_state_t state = zule_port_critical_enter();
    timer.stop = true;
    zule_port_critical_exit(state);
    const int join_error = pthread_join(thread, NULL);
    if (join_error != 0) {
        (void)fprintf(stderr, "Cannot join timer: %s\n", strerror(join_error));
        return 1;
    }
    /* The timer may have reported a failure after our last polling snapshot. */
    if (error == 0) {
        error = timer.error;
    }
    if (result == ZULE_RESULT_OK) {
        result = timer.result;
    }
    if (error != 0) {
        (void)fprintf(stderr, "Timer error: %s\n", strerror(error));
    }
    if (result != ZULE_RESULT_OK) {
        (void)fprintf(stderr, "Scheduler fault: %u\n", (unsigned int)result);
    }
    return error == 0 && result == ZULE_RESULT_OK ? 0 : 1;
}
