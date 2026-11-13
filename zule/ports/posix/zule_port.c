#include "zule_port.h"

#include <stdlib.h>

static pthread_mutex_t critical_mutex;
static pthread_once_t initialize_once = PTHREAD_ONCE_INIT;

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
}

zule_port_critical_state_t zule_port_critical_enter(void) {
    check_result(pthread_once(&initialize_once, initialize_mutex));
    check_result(pthread_mutex_lock(&critical_mutex));
    const zule_port_critical_state_t state = {pthread_self()};
    return state;
}

void zule_port_critical_exit(zule_port_critical_state_t previous_state) {
    if (!pthread_equal(previous_state.owner, pthread_self())) {
        abort();
    }
    check_result(pthread_mutex_unlock(&critical_mutex));
}
