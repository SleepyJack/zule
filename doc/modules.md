# Modules

Modules are optional, on-target consumers of the core's [hooks](application.md#hooks),
shipped with Zule. They observe the schedule, for example to measure tick
timing, and cannot change scheduling. Modules are owned by Zule (see
[ownership](architecture.md#ownership)) and live in `zule/modules/`, outside the
core.

| Module | Setting | Reports |
| --- | --- | --- |
| [Tick timing](#tick-timing) | `ZULE_CONFIG_MODULE_TICK_TIMING` | Worst tick deviations and the longest tick handler. |

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

A module may also check, when `zule_run()` starts, that it can work with the
schedule and timebase. A failure stops `zule_run()` with
`ZULE_RESULT_INVALID_CONFIG` before ticks start. Results accumulate across runs
until the module's reset function is called.

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

| Member | Meaning |
| --- | --- |
| `ticks` | Ticks measured since the last reset, wrapping modulo 2^32. The first tick of each run is its reference and is not measured. |
| `max_late_counts` | Worst late deviation of a measured tick from its expected time. |
| `max_early_counts` | Worst early deviation, as a magnitude. |
| `max_tick_counts` | Longest tick handler, from the tick start timestamp to the tick end timestamp. It includes hook and fault handler time, but not the port's interrupt entry and exit. |

Values are timebase counts; convert them with `zule_timebase_hz()`.
`zule_tick_timing_reset()` clears the results. Expected times follow the same
rules as the tick timing check, so the module requires each slot to be a whole
number of timebase counts below 2^31. A deviation of 2^31 counts or more, such
as a very long stall, is misread as early.

The demo enables the module and prints its results every ten cycles.

## Structure

Modules consume the core's hook interface the same way the application's hooks
do: the core defines the interface, and modules implement it.

- [`zule_modules.h`](../zule/core/inc/zule_modules.h), an internal
  core header, declares the module entry points: `zule_modules_init()`, and for
  each hook, `zule_modules_<hook>()`. Only the core library, which also builds
  the modules, can include it; applications see only the user hooks in
  `zule_hooks.h`.
- [`zule_modules_config.h`](../zule/modules/api/zule_modules_config.h) holds the
  module settings and derives the conditions the core reads:
  `ZULE_MODULES_ANY`, which is `1` when any module is enabled, and for each hook,
  `ZULE_MODULES_HOOK_<HOOK>`, which is `1` when any enabled module consumes it.
  `zule_config.h` includes it, so the conditions are ordinary settings: the core
  reads them, but never names a module.
- `zule_modules.c` defines the entry points, each calling the enabled modules in
  a fixed order.

The core calls `zule_modules_init()` on each run, after its own checks, and the
first module failure stops the run. The core calls each hook point through an
inline wrapper that reads the timebase once and then calls the module entry
point and the user hook. A hook that neither consumes compiles to nothing,
including the time read. Adding a module changes only the modules layer and its
settings, never the core.

Module sources live in `zule/modules/` but are compiled into the core library,
so modules can call the core API, such as `zule_timebase_hz()`, and the core's
internal helpers without a circular dependency between libraries. A module may
share a helper's code with the core, such as the tick tracker the tick timing
check also uses, but owns its own state.

## Adding a module

A module consists of:

- a subdirectory, `zule/modules/<name>/`, with its sources and an internal
  header declaring the handlers that `zule_modules.c` calls;
- a public header in `zule/modules/api/` for its read API, declared only when the
  module is enabled;
- a `ZULE_CONFIG_MODULE_<NAME>` setting in `zule_modules_config.h`, defaulting
  to `0`, added to `ZULE_MODULES_ANY` and to the `ZULE_MODULES_HOOK_<HOOK>`
  condition of each hook it consumes;
- calls from `zule_modules_init()` and from each consumed hook's entry point in
  `zule_modules.c`;
- its sources listed in [`zule/core_sources.txt`](../zule/core_sources.txt), the
  single list of the core library's sources;
- compile-time checks for any further features it requires;
- tests, using the core test port's scripted timebase.

Module handlers follow the hook rules: bounded and nonblocking, no calls to
Zule or port functions, and safe in the context of each hook they consume. Its
init may validate and prepare, but keeps accumulated results unless the module
documents otherwise. Per-task data can be indexed by task ID, which equals the
task's index in the task table.
