# Modules

Modules are optional, on-target consumers of the core's [hook events](core.md#hooks),
shipped with Zule. They observe the schedule, for example to measure tick
timing, and cannot change scheduling. Modules are owned by Zule (see
[ownership](architecture.md#ownership)) and live in `zule/modules/`, outside the
core.

| Module | Setting | Reports |
| --- | --- | --- |
| [Tick timing](#tick-timing) | `ZULE_CONFIG_MODULE_TICK_TIMING` | Worst tick deviations and the longest tick handler. |
| [Task statistics](#task-statistics) | `ZULE_CONFIG_MODULE_TASK_STATS` | Execution times and dispatch latency for each task, and totals for each schedule. |

## Using modules

Enable a module with its [build setting](configuration.md#build-settings); it
then receives the events it needs, independently of the application's hooks.
Each module has a header in `zule/modules/api/` with its API for reading
results, which tasks can call while the scheduler runs. Results accumulate
across runs until reset, and values are timebase counts; convert them with
`zule_timebase_hz()`.

## Tick timing

Measures how far ticks deviate from their expected times, and how long the tick
handler takes, so the [tick timing tolerance](configuration.md#tick-timing-tolerance)
and budget margins can be chosen from measurements. It works with or without the
[tick timing check](core.md#tick-timing-check), and uses the same expected
times. See [`zule_tick_timing.h`](../zule/modules/api/zule_tick_timing.h).

```c
#include "zule_tick_timing.h"

zule_tick_timing_t timing;
zule_tick_timing_get(&timing);
```

## Task statistics

Measures how long each task runs and how long it waits to start, and totals for
each schedule, so [budgets](configuration.md#schedule) can be chosen and checked
from measurements. See [`zule_task_stats.h`](../zule/modules/api/zule_task_stats.h).

```c
#include "zule_task_stats.h"

zule_task_stats_t stats;
if (zule_task_stats_get(ZULE_TASK_ID_CONTROL, &stats)) {
    /* The control task's executions, execution times, and dispatch latency. */
}

zule_task_stats_summary_t summary;
if (zule_task_stats_summary_get(ZULE_SCHEDULE_ID_MAIN, &summary)) {
    /* Cycles, busy time, and the busiest cycle while main was active. */
}
```

An execution is the task function's elapsed time, including any interrupts that
preempt it. Each schedule keeps its own totals, since schedules can differ
widely in how busy they are.

## Design

Modules consume the same hook events as the application, through entry points
the core declares and the modules layer implements. The core reads only derived
conditions, such as whether any enabled module consumes a hook, and never names
a module, so adding a module changes only the modules layer and its settings.
Modules are compiled into the Zule library with the core, so they can use the
core's internal helpers.

A module that keeps data for each task or schedule does not size its own
storage: the generated schedule defines it, with one entry per task or schedule,
so their numbers are set only by the schedule JSON. The modules own its
contents.

## Adding a module

A module consists of:

- a subdirectory, `zule/modules/<name>/`, with its sources and the handlers
  that `zule_modules.c` calls;
- a public header in `zule/modules/api/` for its read API, declared only when the
  module is enabled;
- a `ZULE_CONFIG_MODULE_<NAME>` setting in `zule_modules_config_defaults.h`,
  checked and added to the derived conditions in `zule_modules_config_apply.h`;
- its sources listed in [`zule/zule_sources.txt`](../zule/zule_sources.txt);
- for data kept for each task or schedule, a member in the storage types in
  `zule/modules/inc/`;
- tests, using the core test port's scripted timebase.

Module handlers follow the hook rules: bounded and nonblocking, no calls to Zule
or port functions, and safe in the context of each hook they consume.
