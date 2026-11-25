#include "zule.h"
#include "zule_port.h"
#include "zule_schedule_definition.h"
#include "zule_scheduler.h"

#include <stdbool.h>

static bool running;
static zule_config_t configuration;

/* Whether a schedule ID names one of the application's schedules. Init checks
 * again, after validating the schedules. */
static bool schedule_id_valid(uint32_t schedule_id) {
    const zule_schedule_definition_t *definition = zule_schedule_definition_get();
    return (definition != NULL) && ((size_t)schedule_id < definition->schedules.count);
}

zule_result_t zule_configure(const zule_config_t *config) {
    zule_result_t result = ZULE_RESULT_OK;
    if (running) {
        result = ZULE_RESULT_ALREADY_RUNNING;
    } else if ((config == NULL) || !schedule_id_valid(config->schedule_id)) {
        result = ZULE_RESULT_INVALID_CONFIG;
    } else {
        configuration = *config;
    }
    return result;
}

const char *zule_task_name_get(uint32_t task_id) {
    const char *name = NULL;
#if ZULE_CONFIG_TASK_NAMES
    const zule_schedule_definition_t *definition = zule_schedule_definition_get();
    if (definition != NULL) {
        const zule_task_table_t *tasks = &definition->tasks;
        /* Task IDs are task table indexes; check the entry in case the definition
         * is invalid. */
        if ((tasks->entries != NULL) && ((size_t)task_id < tasks->count) &&
            (tasks->entries[task_id].id == task_id)) {
            name = tasks->entries[task_id].name;
        }
    }
#else
    (void)task_id;
#endif
    return name;
}

zule_result_t zule_schedule_request(uint32_t schedule_id) {
    zule_result_t result = ZULE_RESULT_NOT_INITIALIZED;
    if (running) {
        result = zule_scheduler_schedule_request(schedule_id);
    }
    return result;
}

#if ZULE_CONFIG_TIMEBASE
uint32_t zule_timebase_hz(void) {
    return zule_port_time_hz();
}
#endif

zule_result_t zule_run(void) {
    zule_result_t result = ZULE_RESULT_ALREADY_RUNNING;
    if (!running) {
        running = true;
        result = zule_scheduler_init(configuration.on_fault, configuration.schedule_id);
        if (result == ZULE_RESULT_OK) {
            const zule_schedule_definition_t *definition = zule_schedule_definition_get();
            if (zule_port_tick_init(definition->slot_us, zule_scheduler_tick) !=
                ZULE_PORT_RESULT_OK) {
                result = ZULE_RESULT_PORT_ERROR;
            } else {
                while (result == ZULE_RESULT_OK) {
                    result = zule_scheduler_run();
                    if (result == ZULE_RESULT_OK) {
                        result = zule_scheduler_idle();
                    }
                }
                if (zule_port_tick_stop() != ZULE_PORT_RESULT_OK) {
                    result = ZULE_RESULT_PORT_ERROR;
                }
            }
        }
        running = false;
    }
    return result;
}
