# Zule core

`inc/zule.h` defines the public result enum and no-argument `zule_init()`,
`zule_tick()`, and `zule_run()` functions. Schedule types live in
`inc/zule_schedule.h`; `inc/zule_schedule_definition.h` declares the generated
getter. `src/zule_scheduler.c` contains the private
state and platform-independent implementation. The CMake target is `zule::core`.

`zule_init()` obtains the generated schedule through `zule_schedule_get()` in
`inc/zule_schedule_definition.h` and initializes the private singleton. `zule_tick()` advances
logical slots and publishes tasks; `zule_run()` dispatches pending tasks from
the main loop. Tick and run return `ZULE_RESULT_NOT_INITIALIZED` until init
succeeds. Idle polling returns `ZULE_RESULT_OK`; faults are returned until reset.
`zule_schedule_validate()` in `src/zule_schedule.c` checks pointers, nonzero
counts and budgets, task functions, window bounds, ordering, and overlap.
End-bound checks avoid unsigned addition overflow. Init returns
`ZULE_RESULT_INVALID_SCHEDULE` on failure and leaves tick/run disabled, including
after a failed reinitialization. Pointer storage validity and execution-time
budgets still require external evidence; the validator checks structural timing
consistency, not whether tasks actually finish within their budgets.

Missed dispatches and running tasks at window end latch faults and stop timeline
progression and further dispatch. Boundary processing continues so a late return
can retire to idle. The first fault remains latched until reinitialization.
There is no hardware clock, preemption, or forced task termination. ISR/main-loop
synchronization remains to be implemented. `slot_us` is separate timing metadata.

Integrators can use `add_subdirectory(path/to/zule)` and link `zule::core`.
The demo is disabled by default, and building only the core requires no Python.
Hardware-specific integration can be added separately when needed.

Enable `ZULE_BUILD_TESTS` to build the standalone core API tests and run them
with CTest. They supply their own schedule and require no Python tools.
