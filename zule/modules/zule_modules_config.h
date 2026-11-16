#ifndef ZULE_MODULES_CONFIG_H
#define ZULE_MODULES_CONFIG_H

/* Module settings, included by zule_config.h after the core settings. Each
 * module is enabled by a ZULE_CONFIG_MODULE_... setting, defaulting to 0. No
 * modules exist yet.
 *
 * For each hook, ZULE_MODULES_HOOK_... is 1 when any enabled module consumes
 * it. The core reads only these derived conditions, to decide whether to call
 * the module entry points declared in the core's zule_module_hooks.h; it never names a module. A
 * module adds its setting to the condition of each hook it consumes. */
#define ZULE_MODULES_HOOK_TICK_START 0
#define ZULE_MODULES_HOOK_TICK_END 0
#define ZULE_MODULES_HOOK_TICK_TASK_RELEASE 0
#define ZULE_MODULES_HOOK_TASK_START 0
#define ZULE_MODULES_HOOK_TASK_END 0

/* Every hook passes a timestamp, so modules require the timebase. */
#if (ZULE_MODULES_HOOK_TICK_START || ZULE_MODULES_HOOK_TICK_END ||                                 \
     ZULE_MODULES_HOOK_TICK_TASK_RELEASE || ZULE_MODULES_HOOK_TASK_START ||                        \
     ZULE_MODULES_HOOK_TASK_END) &&                                                                \
    !ZULE_CONFIG_TIMEBASE
#error "Enabled modules require ZULE_CONFIG_TIMEBASE"
#endif

#endif
