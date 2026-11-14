#include "zule_port.h"
#include "zule_port_posix.h"

#include <stdlib.h>

static pthread_mutex_t critical_mutex;
static pthread_cond_t critical_condition;
static pthread_once_t initialize_once = PTHREAD_ONCE_INIT;
/* Lock depth of the owning thread; accessed only while holding critical_mutex. */
static size_t critical_depth;

static void check_result(int result) {
    if (result != 0) {
        abort();
    }
}

static void initialize_mutex(void) {
    pthread_mutexattr_t attributes;
    check_result(pthread_mutexattr_init(&attributes));
    check_result(pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE));
    check_result(pthread_mutex_init(&critical_mutex, &attributes));
    check_result(pthread_mutexattr_destroy(&attributes));
    check_result(pthread_cond_init(&critical_condition, NULL));
}

zule_port_critical_state_t zule_port_critical_enter(void) {
    check_result(pthread_once(&initialize_once, initialize_mutex));
    check_result(pthread_mutex_lock(&critical_mutex));
    ++critical_depth;
    const zule_port_critical_state_t state = {pthread_self()};
    return state;
}

void zule_port_critical_exit(zule_port_critical_state_t previous_state) {
    if (!pthread_equal(previous_state.owner, pthread_self()) || critical_depth == 0U) {
        abort();
    }
    --critical_depth;
    check_result(pthread_mutex_unlock(&critical_mutex));
}

void zule_port_posix_critical_wait(void) {
    if (critical_depth != 1U) {
        abort();
    }
    critical_depth = 0U;
    check_result(pthread_cond_wait(&critical_condition, &critical_mutex));
    critical_depth = 1U;
}

void zule_port_posix_critical_notify(void) {
    if (critical_depth == 0U) {
        abort();
    }
    check_result(pthread_cond_broadcast(&critical_condition));
}
