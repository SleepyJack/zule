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

The core knows modules only through
[`zule_modules.h`](../zule/modules/zule_modules.h), much as it knows the port
through `zule_port.h`. For each hook, the header provides:

- a condition, `ZULE_MODULES_HOOK_<HOOK>`, that is `1` when any enabled module
  consumes the hook;
- an entry point, `zule_modules_<hook>()`, declared under that condition, which
  calls each enabled module in a fixed order.

The core calls each hook point through an inline wrapper that reads the
timebase once and then calls the modules entry point and the user hook. A hook
that neither consumes compiles to nothing, including the time read. Adding a
module therefore changes only the modules layer, never the core.

`zule_modules.c` defines the entry points. It is built into the `zule_modules`
library, which the core links privately.

## Adding a module

A module consists of:

- a subdirectory, `zule/modules/<name>/`, with its sources and a public header
  for its read API;
- a `ZULE_CONFIG_MODULE_<NAME>` setting in `zule_config.h`, defaulting to `0`;
- its setting added to the `ZULE_MODULES_HOOK_<HOOK>` condition of each hook it
  consumes, and a call from each of those entry points in `zule_modules.c`;
- compile-time checks for any further features it requires;
- tests, using the core test port's scripted timebase.

Module handlers follow the hook rules: bounded and nonblocking, no calls to
Zule or port functions, and safe in the context of each hook they consume.
Per-task data can be indexed by task ID, which equals the task's index in the
task table.
