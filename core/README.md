# Zule core

`api/zule.h` contains configuration and fault types, `zule_configure()`, and the
blocking `zule_run()` entry point. `src/zule.c` implements the public API,
including task-name lookup, startup, dispatch polling, error propagation, and
tick shutdown. The CMake target is `zule::core`.

The target exposes only `api/` to application compilation. Headers in `inc/`
are internal and used privately by the core and its tests. Generated schedule
compilation explicitly adds `inc/` alongside the public API include path;
ordinary application sources do not need it. The port is a private core
dependency, with its library linked transitively without exporting port include paths.

The internal `inc/zule_scheduler.h` interface declares `zule_scheduler_init()`,
`zule_scheduler_tick()`, and `zule_scheduler_run()`. State and lifecycle types
remain private to `src/zule_scheduler.c`. Tick advances the timeline and publishes
work; scheduler run attempts one dispatch and returns OK when idle. Both return
NOT_INITIALIZED before successful scheduler initialization.

Schedule types live in `inc/zule_schedule.h`; `inc/zule_schedule_definition.h`
declares the generated getter. The definition includes `slot_us`, used by the
runner to configure tick delivery, so the timer and schedule share one period.
An immutable task array holds IDs and function pointers. The generator assigns
IDs through the enum in `zule_task_ids.h`; handwritten schedules supply their own
unique IDs. Windows point
to entries in this array; repeated invocations share the same task definition.
The `zule_task_t` type includes a `name` member only when task names are enabled.
The scheduler's private `zule_task_execution_t` tracks mutable execution state.
`zule_task_name_get()` provides public lookup
by ID; see [build configuration](../doc/build_configuration.md).
`zule_schedule_validate()` in `src/zule_schedule.c` checks pointers, nonzero
slot periods, counts and budgets, task functions, unique task IDs, window task
references, window bounds, ordering, and overlap.
End-bound checks avoid unsigned addition overflow. Init returns
`ZULE_RESULT_INVALID_SCHEDULE` on failure and leaves tick/run disabled, including
after a failed reinitialization. Pointer storage validity and execution-time
budgets still require external evidence; the validator checks structural timing
consistency, not whether tasks actually finish within their budgets.

Public `zule_run()` initializes the scheduler, passes `zule_scheduler_tick` directly to
`zule_port_tick_init()`, then dispatches and calls `zule_port_idle()` between
attempts. A scheduler fault or port error ends the loop. Tick delivery is stopped
and quiesced before returning, permitting a later retry. Port errors return
PORT_ERROR; recursive runner calls return ALREADY_RUNNING. Application calls
must come from a single context. There is no public stop API yet.

Missed dispatches and running tasks at window end latch faults and stop timeline
progression and further dispatch. Boundary processing continues so a late return
can retire to idle. The first fault remains latched until reinitialization.
There is no clock source, preemption, or forced task termination in the core.
Scheduler run protects the task handoff and completion through the port
critical-section API. The port invokes the callback from a non-reentrant ISR or
an excluded native timer thread; it does not depend on scheduler internals.
Initialization precedes tick delivery. A task that never returns still prevents
the runner from returning; there is no forced task termination.

Integrators can use `add_subdirectory(path/to/zule)` and link `zule::core`.
The demo is disabled by default, and building only the core requires no Python.
Select a port through `ZULE_PORT` or provide a `zule::port` target before adding
Zule. Its usage requirements supply the selected `zule_port_types.h` and library.

Enable `ZULE_BUILD_TESTS` to build the standalone core API tests and run them
with CTest. They supply their own schedule and deterministic test port, with no
timer threads or Python tools. Runner lifecycle tests use that same port to
exercise startup failures, faults, shutdown, and retries. The POSIX mutex and
timer lifecycle are tested separately.

## Application fault handler

Optionally call `zule_configure()` with a `zule_config_t` before running. The
configuration is copied and persists across runs; the caller need not keep the
configuration struct alive. A null handler disables notification. Passing a
null configuration returns INVALID_CONFIG and preserves the previous settings.
Configuration changes while running return ALREADY_RUNNING. All configuration
and runner calls must come from the same application context.

The scheduler captures the configured handler at initialization. On the first
missed dispatch or execution overrun, tick latches the fault and calls the
handler once with its reason, task ID, window index, slot, and cycle count.
Subsequent faulted ticks
and eventual task retirement do not repeat notification or clear the fault.
Initialization and port errors are reported by return code without calling it.

The `uint32_t` cycle count is zero-based, resets on initialization, and wraps
modulo 2^32. Together, the snapshot's cycle count and slot identify when the
fault was detected. A fault detected at slot zero reports the new cycle even
when the faulting window started in the previous cycle.
The counter advances with the schedule timeline and stops progressing on a fault.

The handler executes synchronously in tick context: an ISR on embedded ports,
or the excluded POSIX timer thread. Keep it bounded and nonblocking. Do not call
Zule APIs, timer lifecycle functions, synchronous logging, or allocation from it.
Copy the snapshot if needed after the call and synchronize application data
accessed by other contexts. Notification does not abort an executing task or
establish a safe hardware state by itself; the application supplies that response.
