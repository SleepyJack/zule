#ifndef ZULE_PORT_TYPES_H
#define ZULE_PORT_TYPES_H

#include <pthread.h>

typedef struct {
    pthread_t owner;
} zule_port_critical_state_t;

#endif
