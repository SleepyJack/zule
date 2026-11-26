# Modules

Modules are optional, on-target consumers of the core's [hooks](application.md#hooks),
shipped with Zule. They observe the schedule, for example to measure tick
timing, and cannot change scheduling. Modules are owned by Zule (see
[ownership](architecture.md#ownership)) and live in `zule/modules/`, outside the
core.

| Module | Setting | Reports |
| --- | --- | --- |
| [Tick timing](#tick-timing) | `ZULE_CONFIG_MODULE_TICK_TIMING` | Worst tick deviations and the longest tick handler. |
| [Task statistics](#task-statistics) | `ZULE_CONFIG_MODULE_TASK_STATS` | Execution times and dispatch latency for each task, and totals for each schedule. |

## Using modules

Enable a module with its `ZULE_CONFIG_MODULE_<NAME>` [build setting](configuration.md#build-settings).
The module then receives the hook events it needs automatically; the
application does not call it from its own hooks. Like hooks, modules require
`ZULE_CONFIG_TIMEBASE`, and a module also fails compilation if another feature
it needs is disabled.

Each module has a public header in `zule/modules/api/`, which is on the
application's include path, with its API for reading results. The API is
declared only when the module is enabled. It can be called from application
context, including tasks, but not from hooks or the fault handler. Results are
copied or cleared inside a critical section, so reads are consistent while ticks
run.

Modules receive each event before the application's hooks, in a fixed order,
with the same contexts and guarantees as the [hooks](application.md#hooks).
User hooks and modules are independent: an application can use either, both, or
neither.

## Tick timing

Measures how far ticks deviate from their expected times, and how long the tick
handler takes, so the [tick timing tolerance](configuration.md#tick-timing-tolerance)
and budget margins can be chosen from measurements. It works with or without the
[tick timing check](core.md#tick-timing-check), and measures the same deviations
from the same time reads.

```c
#include "zule_tick_timing.h"

zule_tick_timing_t timing;
zule_tick_timing_get(&timing);
```

Values are timebase counts; convert them with `zule_timebase_hz()`.
`zule_tick_timing_reset()` clears the results.

The showcase example enables the module and prints its results every ten cycles.

## Task statistics

Measures how long each task runs and how long it waits to start, and totals
for each schedule, so [budgets](configuration.md#schedule) can be chosen and
checked from measurements.

```c
#include "zule_task_stats.h"

zule_task_stats_t stats;
if (zule_task_stats_get(ZULE_TASK_ID_CONTROL, &stats)) {
    /* stats holds the control task's statistics. */
}

zule_task_stats_summary_t summary;
if (zule_task_stats_summary_get(ZULE_SCHEDULE_ID_MAIN, &summary)) {
    /* summary holds the totals while the main schedule was active. */
}
```

`zule_task_stats_get()` copies one task's statistics, and returns `false`,
leaving its output unchanged, for an ID that is not one of the schedule's. Since
task IDs run from zero, an application can read every task by counting up until
it returns `false`.

`zule_task_stats_summary_get()` likewise copies one schedule's totals, over all
tasks while that schedule was active, and returns `false` for an ID that is not
one of the application's schedules. Each schedule has its own totals, since
schedules can differ widely in how busy they are. Task statistics cover every
schedule.

An execution runs from the `task_start` hook event to the `task_end` event: the
task function's elapsed time, including any interrupts and ticks that preempt
it. Values are timebase counts; convert them with `zule_timebase_hz()`.

`zule_task_stats_reset()` clears every task's statistics and every schedule's
totals together, so they always cover the same interval; the cycle in progress is still
measured in full. Results accumulate across runs, but a run that stops
mid-cycle does not complete that cycle.

Each task's statistics and each schedule's totals are kept in
[per-task and per-schedule storage](#per-task-and-per-schedule-storage) that the
generated schedule defines, so the module needs no setting for the number of
tasks or schedules.

The showcase example enables the module and prints each task's mean and longest execution
time and worst dispatch latency, and the busiest cycle, every ten cycles.

## Structure

Modules consume the core's hook interface the same way the application's hooks
do: the core defines the interface, and modules implement it.

Adding a module changes only the modules layer and its
settings, never the core.

Module sources live in `zule/modules/` but are compiled into the Zule library
with the core, so modules can call the core API, such as `zule_timebase_hz()`,
and the core's internal helpers without a circular dependency between libraries.

### Per-task and per-schedule storage

A module that keeps data for each task or each schedule, such as task
statistics, does not size its own storage. Instead, the generated schedule
defines it, with one entry per task or per schedule, so their numbers are set
only by the schedule JSON.

This is the same arrangement as the schedule itself: Zule declares the
interface, and the generated schedule implements it. The generated code only
reserves the storage; the modules own and update its contents.

## Adding a module

A module consists of:

- a subdirectory, `zule/modules/<name>/`, with its sources and an internal
  header declaring the handlers that `zule_modules.c` calls;
- a public header in `zule/modules/api/` for its read API, declared only when the
  module is enabled;
- a `ZULE_CONFIG_MODULE_<NAME>` setting, defaulting to `0` in
  `zule_modules_config_defaults.h`, checked in `zule_modules_config_apply.h`,
  and added there to `ZULE_MODULES_ANY` and to the `ZULE_MODULES_HOOK_<HOOK>`
  condition of each hook it consumes;
- calls from `zule_modules_init()` and from each consumed hook's entry point in
  `zule_modules.c`;
- its sources listed in [`zule/zule_sources.txt`](../zule/zule_sources.txt), the
  single list of Zule's sources;
- for data kept for each task or schedule, a member in
  `zule_modules_task_data_t` or `zule_modules_schedule_data_t`, and its setting
  added to `ZULE_MODULES_TASK_DATA` or `ZULE_MODULES_SCHEDULE_DATA`; see
  [per-task and per-schedule storage](#per-task-and-per-schedule-storage);
- compile-time checks for any further features it requires;
- tests, using the core test port's scripted timebase.

Module handlers follow the hook rules: bounded and nonblocking, no calls to
Zule or port functions, and safe in the context of each hook they consume. Its
init may validate and prepare, but keeps accumulated results unless the module
documents otherwise. Per-task data is indexed by task ID, which equals the
task's index in the task table, and per-schedule data by schedule ID.
