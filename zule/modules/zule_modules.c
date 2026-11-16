#include "zule_module_hooks.h"

/* Module entry points, declared in the core's internal zule_module_hooks.h. For each hook a module
 * consumes, define zule_modules_<hook>() here under its ZULE_MODULES_HOOK_...
 * condition, calling each enabled module in a fixed order. No modules exist
 * yet. Compiled into the core library, so modules can use the core API without
 * a link cycle. */
