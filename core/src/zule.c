#include "zule.h"
#include "zule_port.h"
#include "zule_schedule_definition.h"
#include "zule_scheduler.h"

#include <stdbool.h>

static bool running;

zule_result_t zule_run(void) {
    if (running) {
        return ZULE_RESULT_ALREADY_RUNNING;
    }
    running = true;
    zule_result_t result = zule_scheduler_init();
    if (result == ZULE_RESULT_OK) {
        const zule_schedule_t *schedule = zule_schedule_get();
        if (zule_port_tick_init(schedule->slot_us, zule_scheduler_tick) != ZULE_PORT_RESULT_OK) {
            result = ZULE_RESULT_PORT_ERROR;
        } else {
            while (result == ZULE_RESULT_OK) {
                result = zule_scheduler_run();
                if (result == ZULE_RESULT_OK && zule_port_idle() != ZULE_PORT_RESULT_OK) {
                    result = ZULE_RESULT_PORT_ERROR;
                }
            }
            if (zule_port_tick_stop() != ZULE_PORT_RESULT_OK) {
                result = ZULE_RESULT_PORT_ERROR;
            }
        }
    }
    running = false;
    return result;
}
