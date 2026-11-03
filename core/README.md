# Zule core

`inc/zule.h` defines the public API and schedule types; `src/zule.c` contains
the platform-independent implementation. The CMake target is `zule::core`.

The current skeleton initializes a scheduler and advances one logical slot per
`zule_tick()` call, invoking tasks at their configured start slots and wrapping
at the cycle boundary. It requires a validated schedule. Tasks run synchronously;
there is no clock, preemption, overrun handling, or execution-budget enforcement.
`slot_us` and budgets are metadata at this stage.

Integrators can use `add_subdirectory(path/to/zule)` and link `zule::core`.
The demo is disabled by default, and building only the core requires no Python.
Hardware-specific integration can be added separately when needed.
