#ifndef ZULE_PORT_TYPES_H
#define ZULE_PORT_TYPES_H

#include <stdint.h>

/* The interrupt mask, PRIMASK, as it was before the critical section. */
typedef struct {
    uint32_t primask;
} zule_port_critical_state_t;

#endif
