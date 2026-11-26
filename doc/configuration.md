# Configuration

Configuration is owned by the user (see [ownership](architecture.md#ownership)).
It consists of:

- the [schedule](#schedule): tasks, budgets, and window timing, in JSON;
- [build settings](#build-settings): compile-time options for the core in
  `zule_user_config.h`, and for the selected port in `zule_user_port_config.h`,
  or as compiler definitions.

Keep both in an application-owned directory, such as the showcase example's
[`examples/showcase/config/`](../examples/showcase/config). Runtime settings, such
as the fault handler, are part of the [application](application.md#running-the-scheduler).

## Schedule

A schedule describes one repeating cycle of slots. Each task has a budget in
slots, and each window places one invocation of a task at a start slot. For
example, the showcase example's schedule:

```json
{
  "$schema": "../../../zule/tools/schedule/zule_schedule/schedule.schema.json",
  "slot_us": 50000,
  "tasks": {
    "sense": {
      "description": "Sample inputs into a stable snapshot.",
      "budget_slots": 2
    },
    "control": {
      "description": "Compute control outputs from the input snapshot.",
      "budget_slots": 3
    },
    "output": {
      "description": "Publish the computed outputs.",
      "budget_slots": 2
    }
  },
  "cycle": {
    "slots": 10,
    "windows": [
      { "task": "sense", "start_slot": 0 },
      { "task": "control", "start_slot": 2 },
      { "task": "output", "start_slot": 5 }
    ]
  }
}
```

| Field | Meaning |
| --- | --- |
| `slot_us` | Slot duration in microseconds. The port uses it as the tick period. |
| `tasks.<name>` | A task, keyed by name. |
| `tasks.<name>.budget_slots` | Reserved duration of each invocation, in slots. |
| `tasks.<name>.description` | Optional free text. |
| `cycle.slots` | Cycle length in slots, including idle time. |
| `cycle.windows[]` | Windows in increasing `start_slot` order, each naming a declared `task`. |
| `schedules.<name>` | Instead of `cycle`, a cycle per named schedule; see [multiple schedules](#multiple-schedules). |
| `$schema` | Optional path to the schema, for editor support. |

Each window occupies `[start_slot, start_slot + budget_slots)`. Windows must lie
within the cycle and must not overlap, but may touch. A task may have several
windows or none; gaps between windows are idle.

Budgets are wall-clock reservations. They must cover the task's execution time
plus scheduler overhead and interrupt interference, and for the first window,
[startup](core.md#startup).

The [schedule tool](../zule/tools/schedule/README.md) validates the schedule
against its [schema](../zule/tools/schedule/zule_schedule/schedule.schema.json)
and checks its timing, then generates C. It checks internal consistency only,
not execution times, timer limits, or task dependencies.

### Task names and IDs

Each task name must be a C identifier, and determines:

- the task function the application provides, `void zule_task_<name>(void)`;
- a task ID constant, `ZULE_TASK_ID_<NAME>`, in the generated `zule_task_ids.h`;
- the diagnostic name returned by `zule_task_name_get()`, when
  `ZULE_CONFIG_TASK_NAMES` is enabled.

Names must have distinct uppercase forms, so that the constants cannot collide.
IDs start at zero in sorted task-name order. They are specific to a build:
adding, removing, or renaming a task may change other IDs, so interpret saved
fault logs against the matching schedule.

### Multiple schedules

An application with several modes, such as start-up and normal operation, can
define a schedule for each. Instead of `cycle`, a `schedules` object holds one
cycle per schedule, keyed by name. All schedules share the tasks and `slot_us`:

```json
"schedules": {
  "startup": {
    "slots": 5,
    "windows": [{ "task": "mode_manager", "start_slot": 4 }]
  },
  "operational": {
    "slots": 10,
    "windows": [
      { "task": "sense", "start_slot": 0 },
      { "task": "mode_manager", "start_slot": 9 }
    ]
  }
}
```

Schedule names follow the rules for task names, and each determines a constant,
`ZULE_SCHEDULE_ID_<NAME>`, in the generated `zule_schedule_ids.h`, numbered from
zero in name order. A file with `cycle` has one schedule, `ZULE_SCHEDULE_ID_MAIN`.
The application chooses the schedule to [start with](application.md#running-the-scheduler),
and tasks can [switch](application.md#switching-schedules) while it runs.

## Build settings

[`zule_config_defaults.h`](../zule/core/api/zule_config_defaults.h) lists the core
settings and their defaults. Every setting is `0` or `1` unless stated, and the
core, port, application, and generated schedule must all use the same values.

| Setting | Default | Meaning |
| --- | --- | --- |
| `ZULE_CONFIG_TASK_NAMES` | `0` | Compile task names into the schedule, for `zule_task_name_get()`. |
| `ZULE_CONFIG_TIMEBASE` | `0` | Enable the port's [timebase](port.md#timebase), for sub-slot timing. The port must provide it. |
| `ZULE_CONFIG_USER_HOOK_<HOOK>` | `0` | Call the application's [hook](application.md#hooks) for `<HOOK>`. Requires the timebase. |
| `ZULE_CONFIG_TICK_CHECK` | `0` | Enable the [tick timing check](core.md#tick-timing-check). Requires the timebase and a tolerance. |
| `ZULE_CONFIG_TICK_TOLERANCE_US` | None | The tick timing check's [tolerance](#tick-timing-tolerance), in microseconds. |
| `ZULE_CONFIG_MODULE_<NAME>` | `0` | Enable a [module](modules.md). Requires the timebase. |

Settings can be compiler definitions, for example `-DZULE_CONFIG_TASK_NAMES=1`,
or, for a group, a `zule_user_config.h` in an application-owned directory:

```c
#ifndef ZULE_USER_CONFIG_H
#define ZULE_USER_CONFIG_H

#define ZULE_CONFIG_TASK_NAMES 1

#endif
```

With CMake, set `ZULE_USER_CONFIG_DIR` to the header's directory; see
[CMake integration](application.md#cmake-integration). Other build systems add
the directory to the include path and define `ZULE_USE_USER_CONFIG=1` for every
component. Zule supplies defaults for anything the header leaves unset.

### Tick timing tolerance

`ZULE_CONFIG_TICK_TOLERANCE_US` sets how far a tick may deviate from its expected
time, early or late, before the [tick timing check](core.md#tick-timing-check)
faults. A late release tick shortens the released task's window, and an early
end tick cuts the running task's window short, so budgets must allow for the
tolerance. It must also exceed what the target really does: interrupt latency,
the longest interrupt-masked section, higher-priority interrupts, and hook run
time. The [tick timing module](modules.md#tick-timing) measures the deviations
the target really shows.

### Port settings

Each port's `zule_port_config.h` documents its settings, applies defaults, and
fails compilation if a required setting is missing, such as a board's clock
frequency. Override them in a `zule_user_port_config.h`, which can share a
directory with `zule_user_config.h` or have its own, set with
`ZULE_USER_PORT_CONFIG_DIR`, for example when the port settings belong to the
board. Other build systems define `ZULE_USE_USER_PORT_CONFIG=1` for every
component when the header is present. The port's README lists its settings.
