#include "zule_port.h"

/* Compile-check port: every function satisfies the port contract's signatures so
 * the Zule library can be compiled and linked for a target, but none does real
 * work. Ticks are never delivered, so zule_run() would never dispatch a task. */

zule_port_critical_state_t zule_port_critical_enter(void) {
    const zule_port_critical_state_t state = {0U};
    return state;
}

void zule_port_critical_exit(zule_port_critical_state_t previous_state) {
    (void)previous_state;
}

zule_port_result_t zule_port_tick_init(uint32_t slot_us, zule_port_tick_callback_t callback) {
    (void)slot_us;
    (void)callback;
    return ZULE_PORT_RESULT_OK;
}

zule_port_result_t zule_port_tick_stop(void) {
    return ZULE_PORT_RESULT_OK;
}

zule_port_result_t zule_port_idle(void) {
    return ZULE_PORT_RESULT_OK;
}

#if ZULE_CONFIG_TIMEBASE
uint32_t zule_port_time_hz(void) {
    return 1000000U;
}

uint32_t zule_port_time_get(void) {
    return 0U;
}
#endif
