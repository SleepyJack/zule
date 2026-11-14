#include "zule_port.h"
#include "zule_port_posix.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer_condition;
static pthread_once_t initialize_once = PTHREAD_ONCE_INIT;

static struct {
    pthread_t thread;
    uint32_t slot_us;
    zule_port_tick_callback_t callback;
    bool active;
    bool stop;
    bool failed;
} timer;

static void check_result(int result) {
    if (result != 0) {
        abort();
    }
}

static void initialize_condition(void) {
    pthread_condattr_t attributes;
    check_result(pthread_condattr_init(&attributes));
    check_result(pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC));
    check_result(pthread_cond_init(&timer_condition, &attributes));
    check_result(pthread_condattr_destroy(&attributes));
}

/* Called only from the timer thread, outside the critical section. */
static void record_failure(void) {
    check_result(pthread_mutex_lock(&timer_mutex));
    timer.failed = true;
    check_result(pthread_mutex_unlock(&timer_mutex));
    /* Wake an idle wait. Idle checks for failure inside the critical section, so this
     * notification cannot fall between that check and the wait. */
    const zule_port_critical_state_t state = zule_port_critical_enter();
    zule_port_posix_critical_notify();
    zule_port_critical_exit(state);
}

static bool timer_unavailable(void) {
    check_result(pthread_mutex_lock(&timer_mutex));
    const bool unavailable = !timer.active || timer.failed;
    check_result(pthread_mutex_unlock(&timer_mutex));
    return unavailable;
}

static void advance_deadline(struct timespec *deadline, uint32_t slot_us) {
    /* At most 4295 seconds are added; the native monotonic clock must have
     * that much time_t headroom. Nanosecond addition fits a POSIX long. */
    deadline->tv_sec += (time_t)(slot_us / 1000000U);
    deadline->tv_nsec += (long)(slot_us % 1000000U) * 1000L;
    if (deadline->tv_nsec >= 1000000000L) {
        ++deadline->tv_sec;
        deadline->tv_nsec -= 1000000000L;
    }
}

static void *timer_thread(void *argument) {
    (void)argument;
    check_result(pthread_mutex_lock(&timer_mutex));
    const uint32_t slot_us = timer.slot_us;
    const zule_port_tick_callback_t callback = timer.callback;
    check_result(pthread_mutex_unlock(&timer_mutex));

    struct timespec deadline;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        record_failure();
        return NULL;
    }
    while (true) {
        advance_deadline(&deadline, slot_us);
        check_result(pthread_mutex_lock(&timer_mutex));
        while (!timer.stop) {
            const int result = pthread_cond_timedwait(&timer_condition, &timer_mutex, &deadline);
            if (result == ETIMEDOUT) {
                break;
            }
            if (result != 0) {
                check_result(pthread_mutex_unlock(&timer_mutex));
                record_failure();
                return NULL;
            }
        }
        const bool stop = timer.stop;
        check_result(pthread_mutex_unlock(&timer_mutex));
        if (stop) {
            return NULL;
        }

        const zule_port_critical_state_t state = zule_port_critical_enter();
        struct timespec now;
        struct timespec following_deadline = deadline;
        advance_deadline(&following_deadline, slot_us);
        /* Report a full missed period instead of replaying stale ticks. */
        const bool failed =
            clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec > following_deadline.tv_sec ||
            (now.tv_sec == following_deadline.tv_sec && now.tv_nsec >= following_deadline.tv_nsec);
        if (!failed) {
            (void)callback();
            zule_port_posix_critical_notify();
        }
        zule_port_critical_exit(state);
        if (failed) {
            record_failure();
            return NULL;
        }
    }
}

zule_port_result_t zule_port_tick_init(uint32_t slot_us, zule_port_tick_callback_t callback) {
    if (slot_us == 0U || callback == NULL) {
        return ZULE_PORT_RESULT_ERROR;
    }
    check_result(pthread_once(&initialize_once, initialize_condition));
    check_result(pthread_mutex_lock(&timer_mutex));
    if (timer.active) {
        check_result(pthread_mutex_unlock(&timer_mutex));
        return ZULE_PORT_RESULT_ERROR;
    }
    timer.slot_us = slot_us;
    timer.callback = callback;
    timer.failed = false;
    timer.stop = false;
    timer.active = true;
    const int result = pthread_create(&timer.thread, NULL, timer_thread, NULL);
    if (result != 0) {
        timer.active = false;
        timer.callback = NULL;
    }
    check_result(pthread_mutex_unlock(&timer_mutex));
    return result == 0 ? ZULE_PORT_RESULT_OK : ZULE_PORT_RESULT_ERROR;
}

zule_port_result_t zule_port_tick_stop(void) {
    check_result(pthread_mutex_lock(&timer_mutex));
    if (!timer.active) {
        check_result(pthread_mutex_unlock(&timer_mutex));
        return ZULE_PORT_RESULT_OK;
    }
    timer.stop = true;
    check_result(pthread_cond_signal(&timer_condition));
    const pthread_t thread = timer.thread;
    check_result(pthread_mutex_unlock(&timer_mutex));
    /* A failed join cannot meet the quiescence contract; do not return. */
    check_result(pthread_join(thread, NULL));
    check_result(pthread_mutex_lock(&timer_mutex));
    const bool failed = timer.failed;
    timer.active = false;
    timer.callback = NULL;
    check_result(pthread_mutex_unlock(&timer_mutex));
    return failed ? ZULE_PORT_RESULT_ERROR : ZULE_PORT_RESULT_OK;
}

zule_port_result_t zule_port_idle(void) {
    /* Called inside one level of the critical section. The wait releases it, letting the
     * timer thread deliver a tick or record a failure, either of which notifies. */
    if (timer_unavailable()) {
        return ZULE_PORT_RESULT_ERROR;
    }
    zule_port_posix_critical_wait();
    return timer_unavailable() ? ZULE_PORT_RESULT_ERROR : ZULE_PORT_RESULT_OK;
}
