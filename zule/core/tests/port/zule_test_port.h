#ifndef ZULE_TEST_PORT_H
#define ZULE_TEST_PORT_H

#include <stdbool.h>
#include "zule_port.h"

typedef struct {
    zule_port_result_t init_result;
    zule_port_result_t stop_result;
    zule_port_result_t idle_result;
    uint32_t slot_us;
    uint32_t init_calls;
    uint32_t stop_calls;
    uint32_t idle_calls;
    uint32_t idle_limit;
    uint32_t ticks_per_idle;
    bool active;
} zule_test_port_control_t;

extern zule_test_port_control_t zule_test_port_control;

void zule_test_port_reset(void);
void zule_test_port_fire_tick(void);

bool zule_test_port_is_critical(void);

#endif
