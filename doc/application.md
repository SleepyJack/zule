# Application

The application is owned by the user (see [ownership](architecture.md#ownership)).
It implements the tasks, runs the scheduler, responds to faults, and builds Zule
into its executable. The public API is [`zule.h`](../zule/core/api/zule.h);
the [examples](../examples/README.md) are complete applications.

## Tasks

Provide one function for each task declared in the
[schedule](configuration.md#schedule):

```c
void zule_task_sense(void) {
    /* ... */
}
```

All tasks run sequentially in [application context](architecture.md#application-context),
outside critical sections. There is no preemption between tasks, so data shared
only between tasks needs no synchronization. Data shared with the
[fault handler](#fault-handler), which runs in tick context, does.

A task must return within its window's budget. Zule detects an overrun but
cannot terminate the task; see [deadline monitoring](core.md#task-deadline-monitoring).

## Running the scheduler

`zule_run()` validates the schedule, starts periodic ticks through the port, and
dispatches tasks. It blocks until a fault, a port error, or a
[termination request](#stopping-the-scheduler), stops and quiesces tick
delivery, and returns the reason:

| Result | Cause |
| --- | --- |
| `ZULE_RESULT_INVALID_SCHEDULE` | The linked schedules failed validation. |
| `ZULE_RESULT_INVALID_CONFIG` | The configured schedule ID names no schedule, or the timebase cannot represent what enabled [hooks](#hooks) or the [tick timing check](core.md#tick-timing-check) measure for the schedules. |
| `ZULE_RESULT_DISPATCH_MISSED` | A task was not dispatched within its window. |
| `ZULE_RESULT_EXECUTION_OVERRUN` | A task was still running at the end of its window. |
| `ZULE_RESULT_TICK_TIMING` | A tick deviated from its expected time by more than the tolerance; see the [tick timing check](core.md#tick-timing-check). |
| `ZULE_RESULT_PORT_ERROR` | The port failed to start, deliver, or stop ticks. |
| `ZULE_RESULT_ALREADY_RUNNING` | `zule_run()` was called recursively, for example from a task. |
| `ZULE_RESULT_TERMINATED` | A task called `zule_terminate()`; see [stopping the scheduler](#stopping-the-scheduler). |

Call `zule_run()` and `zule_configure()` from a single application context.
After `zule_run()` returns, it can be called again; each call reinitializes the
scheduler and clears the previous fault and any termination request. A task that
never returns prevents `zule_run()` from returning.

## Switching schedules

With [multiple schedules](configuration.md#multiple-schedules), a task can
switch to another while the scheduler runs:

```c
#include "zule_schedule_ids.h"

void zule_task_mode_manager(void) {
    if (startup_complete()) {
        (void)zule_schedule_request(ZULE_SCHEDULE_ID_OPERATIONAL);
    }
}
```

The switch takes effect at the start of the next cycle: the current schedule
finishes its cycle, including its final window, and the new schedule starts at
its slot 0, so its first window can start in the same tick. A later request before then
replaces an earlier one, and a request for the active schedule cancels a
pending switch. No switch takes place while a fault is latched.

## Stopping the scheduler

A task can stop the run, so that the application decides when scheduling ends:

```c
void zule_task_safe_state(void) {
    outputs_safe();
    (void)zule_terminate();
}
```

The requesting task runs to its end as usual, and `zule_run()` then returns
`ZULE_RESULT_TERMINATED` without dispatching another task: a task already
released for a later window is not dispatched, and tick delivery is stopped and
quiesced as on any other exit. A request cannot be withdrawn, and repeating it
changes nothing.

A fault latched before the run returns takes precedence, so `zule_run()` reports
the fault rather than the request; a fault detected after the request is not
reported. `zule_terminate()` returns `ZULE_RESULT_NOT_INITIALIZED` when the
scheduler is not running.

With [multiple schedules](configuration.md#multiple-schedules), a shutdown
schedule whose task puts the outputs in a safe state and then terminates keeps
the stopping sequence in the schedule, where its timing is reserved like any
other work. The [modes example](../examples/modes/README.md) does this.

## Configuration

Configure the scheduler before running: the schedule to start with, and
optionally a fault handler:

```c
#include "zule_schedule_ids.h"

static void handle_fault(const zule_fault_info_t *fault) { /* ... */ }

const zule_config_t config = {
    .on_fault = handle_fault,
    .schedule_id = ZULE_SCHEDULE_ID_MAIN,
};
zule_configure(&config);
```

| Member | Meaning |
| --- | --- |
| `on_fault` | The [fault handler](#fault-handler). Null disables notification; faults still latch. |
| `schedule_id` | The schedule `zule_run()` starts, a `ZULE_SCHEDULE_ID_<NAME>` constant from the generated `zule_schedule_ids.h`; see [multiple schedules](configuration.md#multiple-schedules). |

Set the schedule ID explicitly: ID 0 is the first schedule by name, which may
not be the intended one. Without any configuration, `zule_run()` runs the
schedule with ID 0 and no fault handler.

## Fault handler

The fault handler is installed by the [configuration](#configuration).

The handler is called once, on the first missed-dispatch, overrun, or tick
timing fault.

The handler runs in [tick context](architecture.md#tick-context), possibly
interrupting a task: an ISR on embedded ports, or a timer thread on native ports.
It must be bounded and nonblocking. Do not call Zule or port functions, allocate,
or log synchronously from it. The snapshot pointer is valid only during the
call, so copy what you need. Returning does not clear the fault or stop a running
task; the application supplies any safe-state response.

The showcase example copies the snapshot in the handler and prints it after
`zule_run()` returns, when tick delivery is quiescent.

## Hooks

Hooks report scheduler events to the application, for measurement and
diagnostics. Each is enabled by its own
[build setting](configuration.md#build-settings), and requires the
[timebase](configuration.md#build-settings) (`ZULE_CONFIG_TIMEBASE`). Define a
function for each enabled hook; the declarations are in
[`zule_hooks.h`](../zule/core/api/zule_hooks.h), included by `zule.h`.

| Hook | Context | Called |
| --- | --- | --- |
| `zule_user_hook_tick_start(time_counts, slot, cycle_count)` | Tick | Before each tick processes its slot. |
| `zule_user_hook_tick_task_release(time_counts, task_id, window_index)` | Tick | When a window starts and its task becomes pending. |
| `zule_user_hook_tick_schedule_switch(time_counts, schedule_id)` | Tick | When a [requested schedule](#switching-schedules) takes effect, with the new schedule's ID. Not called for the schedule a run starts with. |
| `zule_user_hook_tick_end(time_counts)` | Tick | After each tick's processing. |
| `zule_user_hook_task_start(time_counts, task_id, window_index)` | Application | Immediately before a task function is called. |
| `zule_user_hook_task_end(time_counts, task_id, window_index)` | Application | Immediately after a task function returns, including after an overrun. |

`time_counts` is a timebase count, read once per event. Convert counts with
`zule_timebase_hz()`, and measure intervals by unsigned subtraction, which
handles the count wrapping. For example, `task_end` minus `task_start` is a
task's execution time, and `task_start` minus `tick_task_release` is its
dispatch latency.

Hooks never run concurrently with each other or with the tick: `tick_` hooks
run in tick context, and the others run in application context inside the
scheduler's critical section. Data that only application-context hooks write
can therefore be read by tasks without synchronization; data written by
`tick_` hooks needs the same care as fault handler data. Like the fault
handler, hooks must be bounded and nonblocking and must not call Zule or port
functions. Their run time adds to the tick and to the critical section.

The [task statistics module](modules.md#task-statistics) measures execution
time and dispatch latency for every task this way, without any hooks of the
application's own.

## Task names and IDs

`zule_task_name_get(task_id)` returns a task's name, or `NULL` for an unknown ID
or when [task names](configuration.md#task-names) are disabled. It can be called
at any time, including before `zule_run()`.

## Target environment

Zule needs no C library and includes only the headers C99 guarantees in a
freestanding environment. Like any C code built with GCC or Clang, it may still
cause calls to `memcpy`, `memmove`, `memset`, and `memcmp`, for example to copy
or initialise structures, so the target environment must provide them. A
target's C library normally does.

## CMake integration

Add the `zule/` directory to the application's CMake project, select a port,
link `zule::zule`, and generate the schedule.
[examples/minimal/CMakeLists.txt](../examples/minimal/CMakeLists.txt) is the
smallest integration, and the showcase example's adds build settings. In
outline:

```cmake
cmake_minimum_required(VERSION 3.31)
project(app LANGUAGES C)

set(ZULE_PORT posix CACHE STRING "Zule port")
set(ZULE_USER_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_subdirectory(path/to/zule zule)

add_executable(app main.c)
target_link_libraries(app PRIVATE zule::zule)
zule_add_schedule(app SCHEDULE config/schedule.json)
```

Set these variables before `add_subdirectory()`:

| Variable | Meaning |
| --- | --- |
| `ZULE_PORT` | Required. A directory under `zule/ports/`, or `external` for a [custom port](port.md#creating-a-port). |
| `ZULE_USER_CONFIG_DIR` | Optional absolute path of the directory holding [`zule_user_config.h`](configuration.md#build-settings), [`zule_user_port_config.h`](configuration.md#port-settings), or both. |
| `ZULE_USER_PORT_CONFIG_DIR` | Optional absolute path of a separate directory holding [`zule_user_port_config.h`](configuration.md#port-settings), when the port settings are kept apart from the build settings. Only one of the two directories may hold it. |
| `ZULE_BUILD_TESTS` | Build Zule's own tests. Defaults to `ON` only when `zule/` is the top-level project. |

Zule provides:

| Name | Use |
| --- | --- |
| `zule::zule` | The Zule library to link: the core and the [modules](modules.md). It carries the public include path and the build settings, so the application, Zule, the port, and the generated schedule share them. |
| `zule_add_schedule(<target> SCHEDULE <json> [OUTPUT_DIR <dir>])` | Generates the schedule at build time, compiles it, and links it into `<target>`, whose sources can then include `zule_task_ids.h` and `zule_schedule_ids.h`. Output defaults to `<target>_schedule/` in the calling binary directory. One schedule file per application is supported; it can define [multiple schedules](configuration.md#multiple-schedules). |

`zule_add_schedule()` needs Python 3.10 or later with
[`zule/tools/schedule/requirements.txt`](../zule/tools/schedule/requirements.txt)
installed. CMake uses the active virtual environment's interpreter if there is
one; otherwise pass `-DPython3_EXECUTABLE=<python>`. The choice is cached in the
build directory. Building Zule without `zule_add_schedule()` needs no Python.

## Other build systems

Without CMake, compile and link:

- the core and [module](modules.md) sources listed in
  [`zule/zule_sources.txt`](../zule/zule_sources.txt);
- the selected port's sources, for example `zule/ports/posix/*.c`;
- the generated `zule_schedule_definition.c`;
- the application.

The application's sources need `zule/core/api/` and `zule/modules/api/` on their
include path, and the same [build settings](configuration.md#build-settings) as
every other component. Compile Zule's own components with the include
directories and definitions of their targets in `zule/`'s CMake files, plus any
definitions the port's README requires. Generate the schedule from
`zule/tools/schedule/`:

```sh
python -m zule_schedule generate path/to/schedule.json path/to/output
```

This writes `zule_schedule_definition.c`, `zule_task_ids.h`, and
`zule_schedule_ids.h`; add the output
directory to the application's include path.
