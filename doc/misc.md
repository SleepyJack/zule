# Miscellaneous details

Text removed from the component documents during the documentation cleanup,
kept word for word for review, under the document and section it came from:
each passage is to be kept here, moved back, or dropped.

## core.md

### Task dispatch

The scheduler records completion after the task function returns. At the
window's end, tick context returns the execution state to idle before processing
the next window.

Only tick context makes a task pending, so when no task is pending the dispatch
loop waits for the next tick through the port's [idle](port.md#idle). The check
and the wait share one critical section, so a tick that makes a task pending
cannot be missed between them.

### Schedule switching

Every window lies within its cycle,
so at that point no task is pending or running. The tick then continues with
the new schedule's first window, which is released in the same tick if it
starts at slot zero. All schedules share the slot length, so the port timer is
never reprogrammed, and all were validated at initialization, so tick context
never validates. A latched fault prevents the switch: a task that overruns the
end of the cycle is reported against the schedule it ran in.

### Tick timing check

The fault reports the deviation in timebase counts.
Expected times wrap with the timebase, so the check is exact for the life of a
run.

Initialization checks that the timebase can support the check, returning
`ZULE_RESULT_INVALID_CONFIG` otherwise: each slot must be a whole number of
timebase counts and shorter than 2^31 counts, and the tolerance, rounded down to
whole counts, must be at least one count and less than one slot. The tolerance
resolution is therefore one timebase count.

### Hooks

Hook points sit beside
the state transitions above: release when a task becomes pending, start just
before running, end as completion is recorded.

## application.md

### Switching schedules

Ticks continue at
the same rate, and the cycle count continues.

Each run starts with the [configured](application.md#configuration) schedule, with no request
pending. The [`tick_schedule_switch` hook](application.md#hooks) reports each switch.

### Configuration

`zule_configure()` copies the configuration, which persists across runs. A null
configuration, or a schedule ID that names no schedule, returns
`ZULE_RESULT_INVALID_CONFIG` and keeps the previous settings. Calling it while
running returns `ZULE_RESULT_ALREADY_RUNNING`.

### Fault handler

It is not called for invalid schedules or configurations, or port
errors, which are reported only through the `zule_run()` result.

### Hooks

Any interval up to one cycle can be measured this
way: with hooks enabled, initialization checks that every schedule's cycle is
shorter than 2^32 counts, and `zule_run()` returns `ZULE_RESULT_INVALID_CONFIG` otherwise.

Within each tick, `tick_start` comes first and `tick_end` last. A
`tick_schedule_switch` comes before any `tick_task_release` in its tick. A window's
`tick_task_release` comes before its `task_start`, which comes before its
`task_end`; ticks can occur between a task's start and end. Hooks continue for
ticks after a fault. Enabled [modules](modules.md) receive the same events
before the application's hooks.

### Task names and IDs

The generated header `zule_task_ids.h` defines `zule_task_id_t` and constants
such as `ZULE_TASK_ID_SENSE`, for comparison with `zule_fault_info_t.task_id`.
IDs are specific to a build; see [task names and IDs](configuration.md#task-names-and-ids).
Likewise, `zule_schedule_ids.h` defines `zule_schedule_id_t` and constants such
as `ZULE_SCHEDULE_ID_MAIN`; see [multiple schedules](configuration.md#multiple-schedules).

### Target environment

The [static checks](../TESTING.md#static-checks)
check this requirement.

### CMake integration

Applications see only the public API. The internal headers in `zule/core/inc/`
and `zule/modules/inc/` are visible only to the Zule library, its tests, and the
generated schedule.

## configuration.md

### Task names

The setting
changes the layout of the schedule's task type, so changing it requires
recompiling every component, but no schedule regeneration.

### Port settings

The port's README lists its settings. Which optional features the port must
provide, such as the timebase, is a core build setting, not a port setting. A
port can require settings only when a feature needs them, for example a clock
frequency only with the timebase enabled. The POSIX port has no settings yet.

## modules.md

### Using modules

A module may also check, when `zule_run()` starts, that it can work with the
schedule and timebase. A failure stops `zule_run()` with
`ZULE_RESULT_INVALID_CONFIG` before ticks start. Results accumulate across runs
until the module's reset function is called.

### Tick timing

Expected times follow the same
rules as the tick timing check, so the module requires each slot to be a whole
number of timebase counts below 2^31. A deviation of 2^31 counts or more, such
as a very long stall, is misread as early.

### Task statistics

Single
measurements are exact up to one timebase wrap of 2^32 counts. Totals and counts
are 64-bit, so they do not wrap in practice: even at the fastest timebase,
2^32 - 1 Hz, they last over 100 years. Readers can measure over any interval by
subtracting two snapshots.

Its reset clears every task and schedule inside one
critical section, so its duration grows with their number.

### Structure

- [`zule_modules.h`](../zule/core/inc/zule_modules.h), an internal
  core header, declares the module entry points: `zule_modules_init()`, and for
  each hook, `zule_modules_<hook>()`. Only the Zule library, which also builds
  the modules, can include it; applications see only the user hooks in
  `zule_hooks.h`.
- [`zule_modules_config_defaults.h`](../zule/modules/api/zule_modules_config_defaults.h)
  holds the module settings, and
  [`zule_modules_config_apply.h`](../zule/modules/api/zule_modules_config_apply.h)
  checks them and derives the conditions the core reads:
  `ZULE_MODULES_ANY`, which is `1` when any module is enabled, and for each hook,
  `ZULE_MODULES_HOOK_<HOOK>`, which is `1` when any enabled module consumes it.
  `zule_config_apply.h` includes it, so the conditions are ordinary settings: the core
  reads them, but never names a module.
- `zule_modules.c` defines the entry points, each calling the enabled modules in
  a fixed order.
- [`zule_modules_task_data.h`](../zule/modules/inc/zule_modules_task_data.h) and
  [`zule_modules_schedule_data.h`](../zule/modules/inc/zule_modules_schedule_data.h),
  internal modules headers, define the
  [per-task and per-schedule storage](modules.md#per-task-and-per-schedule-storage).

The core calls `zule_modules_init()` on each run, after its own checks, with the
ID of the schedule the run starts with, and the first module failure stops the
run. The core calls each hook point through an
inline wrapper that reads the timebase once and then calls the module entry
point and the user hook. A hook that neither consumes compiles to nothing,
including the time read.

### Using modules

A module may
share a helper's code with the core, such as the tick tracker the tick timing
check also uses, but owns its own state.

### Per-task and per-schedule storage

- `zule_modules_task_data_t` combines what every enabled module keeps for one
  task, with a member for each, and `zule_modules_schedule_data_t` likewise for
  one schedule.
- `ZULE_MODULES_TASK_DATA` is `1` when any enabled module keeps per-task data,
  and `ZULE_MODULES_SCHEDULE_DATA` when any keeps per-schedule data.
- When they are, the generated schedule defines `zule_modules_task_data_get()`
  and `zule_modules_schedule_data_get()`, which return static, zero-initialized
  arrays of entries, indexed by task ID and schedule ID.

With
this storage, the generated schedule also uses RAM, not just constant tables.

## port.md

### Timebase

Core features using the timebase limit what a schedule may need, and check it
at initialization:

- [Hooks](application.md#hooks): one cycle must be shorter than 2^32 counts, so
  any interval up to a cycle can be measured.
- The [tick timing check](core.md#tick-timing-check): each slot must be a whole
  number of counts, so expected tick times do not drift, and shorter than 2^31
  counts, so deviations compare as signed differences.
