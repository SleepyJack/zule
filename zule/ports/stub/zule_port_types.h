#ifndef ZULE_PORT_TYPES_H
#define ZULE_PORT_TYPES_H

#include <stdint.h>

/* The stub port saves no state; the token only satisfies the contract. */
typedef struct {
    uint32_t unused;
} zule_port_critical_state_t;

#endif
