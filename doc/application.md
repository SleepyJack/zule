# Application

The application is owned by the user (see [ownership](architecture.md#ownership)).
It implements the tasks, runs the scheduler, responds to faults, and builds Zule
into its executable. The public API is [`zule.h`](../zule/core/api/zule.h), whose
comments describe each function; the [examples](../examples/README.md) are
complete applications.

## Tasks

Provide one function for each task declared in the
[schedule](configuration.md#schedule):

```c
void zule_task_sense(void) {
    /* ... */
}
```

Tasks run one after another in [application context](architecture.md#application-context),
so data shared only between tasks needs no synchronization. Data shared with the
fault handler or tick hooks, which run in tick context, does. A task must return
within its window's budget; Zule detects an overrun but cannot stop the task.

## Running the scheduler

Configure the schedule to start with and, optionally, a fault handler, then run:

```c
#include "zule.h"
#include "zule_schedule_ids.h"

static void handle_fault(const zule_fault_info_t *fault) { /* ... */ }

int main(void) {
    const zule_config_t config = {
        .on_fault = handle_fault,
        .schedule_id = ZULE_SCHEDULE_ID_MAIN,
    };
    zule_configure(&config);
    const zule_result_t result = zule_run();
    /* zule_run() returns only on a fault or port error. */
    return (int)result;
}
```

Set the schedule ID explicitly: ID 0 is the first schedule by name, which may not
be the intended one.

`zule_run()` blocks until a fault or port error, stops tick delivery, and
returns the reason:

| Result | Cause |
| --- | --- |
| `ZULE_RESULT_INVALID_SCHEDULE` | The linked schedules failed validation. |
| `ZULE_RESULT_INVALID_CONFIG` | The configured schedule ID names no schedule, or the timebase cannot support what the enabled features measure. |
| `ZULE_RESULT_DISPATCH_MISSED` | A task was not dispatched within its window. |
| `ZULE_RESULT_EXECUTION_OVERRUN` | A task was still running at the end of its window. |
| `ZULE_RESULT_TICK_TIMING` | A tick deviated from its expected time by more than the tolerance. |
| `ZULE_RESULT_PORT_ERROR` | The port failed to start, deliver, or stop ticks. |
| `ZULE_RESULT_ALREADY_RUNNING` | `zule_run()` was called again while running, for example from a task. |

`zule_run()` can be called again after it returns, starting a fresh run. There
is no public stop API yet.

## Switching schedules

With [multiple schedules](configuration.md#multiple-schedules), a task can
request another:

```c
void zule_task_mode_manager(void) {
    if (startup_complete()) {
        (void)zule_schedule_request(ZULE_SCHEDULE_ID_OPERATIONAL);
    }
}
```

The switch takes effect at the start of the next cycle; see
[schedule switching](core.md#schedule-switching). The last request before then
wins.

## Fault handler

The handler is called once, on the first missed-dispatch, overrun, or tick
timing fault, with a snapshot of the fault (`zule_fault_info_t`). It runs in
[tick context](architecture.md#tick-context), possibly interrupting a task, so it
must be bounded and nonblocking and must not call Zule or port functions. Copy
what you need from the snapshot; returning does not clear the fault or stop a
running task, and the application supplies any safe-state response.

The showcase example copies the snapshot in the handler and prints it after
`zule_run()` returns.

## Hooks

Hooks report [scheduler events](core.md#hooks) to the application, for
measurement and diagnostics. Enable each with its
`ZULE_CONFIG_USER_HOOK_<HOOK>` [build setting](configuration.md#build-settings),
which requires the timebase, and define the function declared in
[`zule_hooks.h`](../zule/core/api/zule_hooks.h):

| Hook | Context | Use |
| --- | --- | --- |
| `zule_user_hook_tick_start` | Tick | Tick arrival time, slot, and cycle count. |
| `zule_user_hook_tick_end` | Tick | With tick start, the tick handler's duration. |
| `zule_user_hook_tick_task_release` | Tick | When a window starts. |
| `zule_user_hook_tick_schedule_switch` | Tick | When a requested schedule takes effect. |
| `zule_user_hook_task_start` | Application | Just before a task runs: with release, its dispatch latency. |
| `zule_user_hook_task_end` | Application | Just after a task returns: with start, its execution time. |

Each hook receives a timebase count; convert counts with `zule_timebase_hz()`,
and measure intervals by unsigned subtraction. Hooks never run concurrently with
each other or the tick. Like the fault handler, they must be bounded and
nonblocking and must not call Zule or port functions.

The [task statistics module](modules.md#task-statistics) makes these
measurements for every task without any hooks of the application's own.

## Target environment

Zule needs no C library and includes only the headers C99 guarantees in a
freestanding environment. Like any C code built with GCC or Clang, it may still
cause calls to `memcpy`, `memmove`, `memset`, and `memcmp`, so the target
environment must provide them. A target's C library normally does.

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
| `ZULE_PORT` | Required. A directory under `zule/ports/`, or `external` for a [custom port](port.md#linking-a-custom-port). |
| `ZULE_USER_CONFIG_DIR` | Optional absolute path of the directory holding [`zule_user_config.h`](configuration.md#build-settings), [`zule_user_port_config.h`](configuration.md#port-settings), or both. |
| `ZULE_USER_PORT_CONFIG_DIR` | Optional absolute path of a separate directory holding `zule_user_port_config.h`. |
| `ZULE_BUILD_TESTS` | Build Zule's own tests. Defaults to `ON` only when `zule/` is the top-level project. |

`zule::zule` is the Zule library: the core and the [modules](modules.md), with
the public include path and the build settings. `zule_add_schedule(<target>
SCHEDULE <json> [OUTPUT_DIR <dir>])` generates the schedule at build time and
links it into `<target>`, whose sources can then include `zule_task_ids.h` and
`zule_schedule_ids.h`. It needs Python 3.10 or later with
[`zule/tools/schedule/requirements.txt`](../zule/tools/schedule/requirements.txt)
installed; CMake uses the active virtual environment's interpreter, or pass
`-DPython3_EXECUTABLE=<python>`.

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
`zule_schedule_ids.h` to the output directory, which goes on the application's
include path.
