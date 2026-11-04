# Zule core

`inc/zule.h` defines the public API and schedule types; `src/zule.c` contains
the platform-independent implementation. The CMake target is `zule::core`.

`zule_init()` obtains the generated schedule through `zule_schedule_get()` in
`inc/schedule.h` and returns the singleton scheduler. `zule_tick()` advances
logical slots and publishes tasks; `zule_run()` dispatches pending tasks from
the main loop. The schedule must be validated before use.

Missed dispatches and running tasks at window end latch faults and stop timeline
progression and further dispatch. Boundary processing continues so a late return
can retire to idle. The first fault remains latched until reinitialization.
There is no hardware clock, preemption, or forced task termination. ISR/main-loop
synchronization remains to be implemented. `slot_us` is separate timing metadata.

Integrators can use `add_subdirectory(path/to/zule)` and link `zule::core`.
The demo is disabled by default, and building only the core requires no Python.
Hardware-specific integration can be added separately when needed.
