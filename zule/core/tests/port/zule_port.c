#include "zule_port.h"
#include "zule_test_port.h"

#include <stdlib.h>

static size_t depth;
static zule_port_tick_callback_t tick_callback;
zule_test_port_control_t zule_test_port_control;

zule_port_critical_state_t zule_port_critical_enter(void) {
    const zule_port_critical_state_t state = {depth};
    ++depth;
    return state;
}

void zule_port_critical_exit(zule_port_critical_state_t previous_state) {
    if (depth == 0U || previous_state.previous_depth != depth - 1U) {
        abort();
    }
    depth = previous_state.previous_depth;
}

bool zule_test_port_is_critical(void) {
    return depth != 0U;
}

void zule_test_port_reset(void) {
    if (depth != 0U || zule_test_port_control.active) {
        abort();
    }
    zule_test_port_control = (zule_test_port_control_t){0};
    zule_test_port_control.ticks_per_idle = 1U;
    zule_test_port_control.idle_limit = 4U;
    tick_callback = NULL;
}

void zule_test_port_fire_tick(void) {
    if (!zule_test_port_control.active || tick_callback == NULL) {
        abort();
    }
    const zule_port_critical_state_t state = zule_port_critical_enter();
    (void)tick_callback();
    zule_port_critical_exit(state);
}

zule_port_result_t zule_port_tick_init(uint32_t slot_us, zule_port_tick_callback_t callback) {
    ++zule_test_port_control.init_calls;
    zule_test_port_control.slot_us = slot_us;
    if (depth != 0U || slot_us == 0U || callback == NULL || zule_test_port_control.active) {
        abort();
    }
    if (zule_test_port_control.init_result == ZULE_PORT_RESULT_OK) {
        tick_callback = callback;
        zule_test_port_control.active = true;
    }
    return zule_test_port_control.init_result;
}

zule_port_result_t zule_port_tick_stop(void) {
    if (depth != 0U) {
        abort();
    }
    ++zule_test_port_control.stop_calls;
    zule_test_port_control.active = false;
    tick_callback = NULL;
    return zule_test_port_control.stop_result;
}

zule_port_result_t zule_port_idle(void) {
    if (depth != 0U || !zule_test_port_control.active) {
        abort();
    }
    ++zule_test_port_control.idle_calls;
    if (zule_test_port_control.idle_result != ZULE_PORT_RESULT_OK ||
        zule_test_port_control.idle_calls >= zule_test_port_control.idle_limit) {
        return ZULE_PORT_RESULT_ERROR;
    }
    for (uint32_t tick = 0U; tick < zule_test_port_control.ticks_per_idle; ++tick) {
        zule_test_port_fire_tick();
    }
    return ZULE_PORT_RESULT_OK;
}
