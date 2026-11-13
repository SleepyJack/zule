# Application

The application is owned by the user (see [ownership](architecture.md#ownership)).
It implements the tasks, runs the scheduler, responds to faults, and builds Zule
into its executable. The public API is [`zule.h`](../zule/core/api/zule.h);
[`demo/`](../demo) is a complete example.

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
dispatches tasks. It blocks until a fault or port error, stops and quiesces tick
delivery, and returns the reason:

| Result | Cause |
| --- | --- |
| `ZULE_RESULT_INVALID_SCHEDULE` | The linked schedule failed validation. |
| `ZULE_RESULT_DISPATCH_MISSED` | A task was not dispatched within its window. |
| `ZULE_RESULT_EXECUTION_OVERRUN` | A task was still running at the end of its window. |
| `ZULE_RESULT_PORT_ERROR` | The port failed to start, deliver, or stop ticks. |
| `ZULE_RESULT_ALREADY_RUNNING` | `zule_run()` was called recursively, for example from a task. |

Call `zule_run()` and `zule_configure()` from a single application context.
After `zule_run()` returns, it can be called again; each call reinitializes the
scheduler and clears the previous fault. A task that never returns prevents
`zule_run()` from returning. There is no public stop API yet.

## Fault handler

Optionally install a fault handler before running:

```c
static void handle_fault(const zule_fault_info_t *fault) { /* ... */ }

const zule_config_t config = {.on_fault = handle_fault};
zule_configure(&config);
```

`zule_configure()` copies the configuration, which persists across runs. A null
handler disables notification; faults still latch. A null configuration returns
`ZULE_RESULT_INVALID_CONFIG` and keeps the previous settings. Calling it while
running returns `ZULE_RESULT_ALREADY_RUNNING`.

The handler is called once, on the first missed-dispatch or overrun fault. It is
not called for invalid schedules or port errors, which are reported only through
the `zule_run()` result. It receives a snapshot:

| Member | Meaning |
| --- | --- |
| `reason` | `ZULE_RESULT_DISPATCH_MISSED` or `ZULE_RESULT_EXECUTION_OVERRUN`. |
| `task_id` | ID of the faulting task; see [task names and IDs](#task-names-and-ids). |
| `window_index` | Index of the faulting window in the schedule. |
| `slot` | Slot at which the fault was detected. |
| `cycle_count` | Zero-based cycle count, reset at initialization. Wraps modulo 2^32. |

Together, `cycle_count` and `slot` identify when the fault was detected. A fault
detected at slot zero reports the new cycle, even if the faulting window started
in the previous cycle.

The handler runs in [tick context](architecture.md#tick-context), possibly
interrupting a task: an ISR on embedded ports, or a timer thread on native ports.
It must be bounded and nonblocking. Do not call Zule or port functions, allocate,
or log synchronously from it. The snapshot pointer is valid only during the
call, so copy what you need. Returning does not clear the fault or stop a running
task; the application supplies any safe-state response.

The demo copies the snapshot in the handler and prints it after `zule_run()`
returns, when tick delivery is quiescent.

## Task names and IDs

The generated header `zule_task_ids.h` defines `zule_task_id_t` and constants
such as `ZULE_TASK_ID_SENSE`, for comparison with `zule_fault_info_t.task_id`.
IDs are specific to a build; see [task names and IDs](configuration.md#task-names-and-ids).

`zule_task_name_get(task_id)` returns a task's name, or `NULL` for an unknown ID
or when [task names](configuration.md#task-names) are disabled. It can be called
at any time, including before `zule_run()`.

## CMake integration

Add the `zule/` directory to the application's CMake project, select a port,
link `zule::core`, and generate the schedule.
[demo/CMakeLists.txt](../demo/CMakeLists.txt) is the reference integration:

```cmake
cmake_minimum_required(VERSION 3.31)
project(app LANGUAGES C)

set(ZULE_PORT posix CACHE STRING "Zule port")
set(ZULE_USER_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_subdirectory(path/to/zule zule)

add_executable(app main.c)
target_link_libraries(app PRIVATE zule::core)
zule_add_schedule(app SCHEDULE config/schedule.json)
```

Set these variables before `add_subdirectory()`:

| Variable | Meaning |
| --- | --- |
| `ZULE_PORT` | Required. A directory under `zule/ports/` (currently `posix`), or `external` for a [custom port](port.md#creating-a-port). |
| `ZULE_USER_CONFIG_DIR` | Optional absolute path of the directory holding [`zule_user_config.h`](configuration.md#build-settings). |
| `ZULE_BUILD_TESTS` | Build Zule's own tests. Defaults to `ON` only when `zule/` is the top-level project. |

Zule provides:

| Name | Use |
| --- | --- |
| `zule::core` | The library to link. It carries the public include path and the build settings, so the application, core, port, and generated schedule share them. |
| `zule_add_schedule(<target> SCHEDULE <json> [OUTPUT_DIR <dir>])` | Generates the schedule at build time, compiles it, and links it into `<target>`, whose sources can then include `zule_task_ids.h`. Output defaults to `<target>_schedule/` in the calling binary directory. Only one schedule per application is supported. |

`zule_add_schedule()` needs Python 3.10 or later with
[`zule/tools/schedule/requirements.txt`](../zule/tools/schedule/requirements.txt)
installed. CMake uses the active virtual environment's interpreter if there is
one; otherwise pass `-DPython3_EXECUTABLE=<python>`. The choice is cached in the
build directory. Building Zule without `zule_add_schedule()` needs no Python.

Applications see only the public API. The core's internal headers in
`zule/core/inc/` are visible only to the core, its tests, and the generated schedule.

## Other build systems

Without CMake, compile and link:

- the sources in `zule/core/src/`;
- the selected port's sources, for example `zule/ports/posix/*.c`;
- the generated `zule_schedule_definition.c`;
- the application.

Every component needs `zule/core/api/` on its include path, plus the
[build settings](configuration.md#build-settings). The core and generated
schedule also need `zule/core/inc/`. The core and port need `zule/ports/` and the
port's own directory. Apply any definitions the port's README requires. Generate
the schedule from `zule/tools/schedule/`:

```sh
python -m zule_schedule generate path/to/schedule.json path/to/output
```

This writes `zule_schedule_definition.c` and `zule_task_ids.h`; add the output
directory to the application's include path.
