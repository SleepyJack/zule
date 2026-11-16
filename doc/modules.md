# Modules

Modules are optional, on-target consumers of the core's [hooks](application.md#hooks),
shipped with Zule. They observe the schedule, for example to collect task
timing statistics, and cannot change scheduling. Modules are owned by Zule (see
[ownership](architecture.md#ownership)) and live in `zule/modules/`, outside the
core.

No modules exist yet. Per-task statistics are planned as the first.

## Using modules

Enable a module with its `ZULE_CONFIG_MODULE_<NAME>` [build setting](configuration.md#build-settings).
The module then receives the hook events it needs automatically; the
application does not call it from its own hooks. Like hooks, modules require
`ZULE_CONFIG_TIMEBASE`, and a module also fails compilation if another feature
it needs is disabled.

Each module documents its own API for reading its results. Because hooks never
run concurrently with each other or with the tick, a module's data only needs
a critical section to be read consistently.

Modules receive each event before the application's hooks, in a fixed order,
with the same contexts and guarantees as the [hooks](application.md#hooks).
User hooks and modules are independent: an application can use either, both, or
neither.

## Structure

Modules consume the core's hook interface the same way the application's hooks
do: the core defines the interface, and modules implement it.

- [`zule_hooks.h`](../zule/core/api/zule_hooks.h) declares, for each hook, a
  module entry point, `zule_modules_<hook>()`, alongside the user hook.
- [`zule_modules_config.h`](../zule/modules/zule_modules_config.h) holds the
  module settings and derives, for each hook, a condition,
  `ZULE_MODULES_HOOK_<HOOK>`, that is `1` when any enabled module consumes it.
  `zule_config.h` includes it, so the conditions are ordinary settings: the core
  reads them, but never names a module.
- `zule_modules.c` defines the entry points for consumed hooks, each calling the
  enabled modules in a fixed order.

The core calls each hook point through an inline wrapper that reads the
timebase once and then calls the module entry point and the user hook. A hook
that neither consumes compiles to nothing, including the time read. Adding a
module changes only the modules layer and its settings, never the core.

Module sources live in `zule/modules/` but are compiled into the core library,
so modules can call the core API, such as `zule_timebase_hz()`, without a
circular dependency between libraries.

## Adding a module

A module consists of:

- a subdirectory, `zule/modules/<name>/`, with its sources and a public header
  for its read API;
- a `ZULE_CONFIG_MODULE_<NAME>` setting in `zule_modules_config.h`, defaulting
  to `0`;
- its setting added to the `ZULE_MODULES_HOOK_<HOOK>` condition of each hook it
  consumes, and a call from each of those entry points in `zule_modules.c`;
- its sources added to the core library;
- compile-time checks for any further features it requires;
- tests, using the core test port's scripted timebase.

Module handlers follow the hook rules: bounded and nonblocking, no calls to
Zule or port functions, and safe in the context of each hook they consume.
Per-task data can be indexed by task ID, which equals the task's index in the
task table.
