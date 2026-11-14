# Configuration

Configuration is owned by the user (see [ownership](architecture.md#ownership)).
It consists of:

- the [schedule](#schedule): tasks, budgets, and window timing, in JSON;
- [build settings](#build-settings): compile-time options for the core in
  `zule_user_config.h`, and for the selected port in `zule_user_port_config.h`,
  or as compiler definitions.

Keep both in an application-owned directory, such as the demo's
[`demo/config/`](../demo/config). Runtime settings, such as the fault handler,
are part of the [application](application.md#fault-handler).

## Schedule

A schedule describes one repeating cycle of slots. Each task has a budget in
slots, and each window places one invocation of a task at a start slot. For
example, the demo schedule:

```json
{
  "$schema": "../../zule/tools/schedule/zule_schedule/schedule.schema.json",
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
| `tasks.<name>` | A task, keyed by name. The name must be a C identifier. |
| `tasks.<name>.budget_slots` | Reserved duration of each invocation, in slots. |
| `tasks.<name>.description` | Optional free text. |
| `cycle.slots` | Cycle length in slots, including idle time. |
| `cycle.windows[]` | Windows in increasing `start_slot` order, each naming a declared `task`. |
| `$schema` | Optional path to the schema, for editor support. Tools ignore it. |

Each window occupies `[start_slot, start_slot + budget_slots)` of its task.
Windows must lie within the cycle and must not overlap, but may touch. A task
may have several windows or none; gaps between windows are idle. Early
completion does not advance the schedule.

Budgets are wall-clock reservations. They must cover the task's execution time
plus scheduler overhead and interrupt interference; see
[deadline monitoring](core.md#task-deadline-monitoring). Leaving the first slots
idle allows time for [startup](core.md#scheduler-initialisation).

Generation additionally requires every timing value to fit `uint32_t`.

The [schema](../zule/tools/schedule/zule_schedule/schedule.schema.json) defines
the structure. The [schedule tool](../zule/tools/schedule/README.md) validates
structure and timing, and generates C. Validation checks internal consistency
only. It does not verify execution times, timer limits, or task dependencies.

### Task names and IDs

Each task name determines:

- the task function the application provides, `void zule_task_<name>(void)`;
- a task ID constant, `ZULE_TASK_ID_<NAME>`, in the generated `zule_task_ids.h`;
- the diagnostic name, when [task names](#build-settings) are enabled.

Names must have distinct uppercase forms, so that the constants cannot collide.
The `zule_task_` prefix lets names such as `switch` avoid C keywords.

IDs start at zero in sorted task-name order, including tasks without windows.
They are specific to a build: adding, removing, or renaming a task may change
other IDs. Interpret saved fault logs against the matching build or schedule.

## Build settings

`zule/core/api/zule_config.h` supplies compile-time defaults and validates
settings. The core, port, application, and generated schedule must all use the
same settings.

| Setting | Default | Meaning |
| --- | --- | --- |
| `ZULE_USE_USER_CONFIG` | `0` | Set to `1` to include an integrator-supplied `zule_user_config.h`. |
| `ZULE_CONFIG_TASK_NAMES` | `0` | Set to `1` to compile diagnostic task names into the schedule. Must be `0` or `1`. |

The default build needs no user header. Individual settings can be supplied as
compiler definitions, for example `-DZULE_CONFIG_TASK_NAMES=1`. For a group of
overrides, create `zule_user_config.h` in an application-owned directory:

```c
#ifndef ZULE_USER_CONFIG_H
#define ZULE_USER_CONFIG_H

#define ZULE_CONFIG_TASK_NAMES 1

#endif
```

Zule includes the user header first and supplies defaults for settings left
unspecified. Do not edit or replace Zule's own settings header. With CMake, set
`ZULE_USER_CONFIG_DIR` to the header's directory; see
[CMake integration](application.md#cmake-integration). Other build systems add
the directory to the include path and define `ZULE_USE_USER_CONFIG=1` for every
component.

### Task names

With `ZULE_CONFIG_TASK_NAMES=1`, each task's name is stored in its schedule
entry and returned by [`zule_task_name_get()`](application.md#task-names-and-ids).
With names disabled, both the member and its strings are excluded. The setting
changes the layout of the schedule's task type, so changing it requires
recompiling every component, but no schedule regeneration.

### Port settings

Each port supplies its own `zule_port_config.h`, which documents the port's
settings, applies defaults, validates them, and fails compilation if a required
setting is missing rather than guessing it, for example a timer clock frequency
that depends on the board. For overrides, create `zule_user_port_config.h` next
to `zule_user_config.h`:

| Setting | Default | Meaning |
| --- | --- | --- |
| `ZULE_USE_USER_PORT_CONFIG` | `0` | Set to `1` to include an integrator-supplied `zule_user_port_config.h`. |

With CMake, `ZULE_USER_CONFIG_DIR` detects either header, or both, and defines
the matching `ZULE_USE_USER_...` setting. Other build systems define
`ZULE_USE_USER_PORT_CONFIG=1` for every component when the header is present.
Like core settings, port settings must be the same for every component.

The port's README lists its settings. A port also declares its
[capabilities](port.md#capabilities) in `zule_port_config.h`; these describe what
the port provides and cannot be overridden. The POSIX port has no settings yet.
