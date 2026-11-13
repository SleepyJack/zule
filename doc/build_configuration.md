# Build configuration

`zule/core/api/zule_config.h` supplies compile-time defaults and validates
settings. All core, port, application, and generated schedule sources must use
the same settings. `ZULE_CONFIG_TASK_NAMES` changes the layout of `zule_task_t`,
so changing it requires recompiling every component that uses the task type.

| Setting | Default | Meaning |
| --- | --- | --- |
| `ZULE_USE_USER_CONFIG` | `0` | Set to `1` to include an integrator-supplied `zule_user_config.h`. |
| `ZULE_CONFIG_TASK_NAMES` | `0` | Set to `1` to compile diagnostic task names into the generated schedule. Must be `0` or `1`. |

The default build needs no user header. Individual settings can be supplied as
compiler definitions, for example `-DZULE_CONFIG_TASK_NAMES=1`.

For a group of overrides, create `zule_user_config.h` in an application-owned
directory:

```c
#ifndef ZULE_USER_CONFIG_H
#define ZULE_USER_CONFIG_H

#define ZULE_CONFIG_TASK_NAMES 1

#endif
```

Add that directory to the include path and define `ZULE_USE_USER_CONFIG=1` for
every component. Zule includes the user header first and supplies defaults for
settings left unspecified. Do not edit or replace Zule's own settings header.
CMake integrators set `ZULE_USER_CONFIG_DIR` instead; see below.

Changing the names setting requires recompilation, but no schedule regeneration.
Names are the JSON task keys, stored in each immutable `zule_task_t` object's
`name` member. With names disabled, both the member and its strings are excluded.
`zule_task_name_get(task_id)` returns a name or `NULL` if the ID is unknown or
the schedule has no names. It can be called before starting the scheduler;
the linked schedule must remain valid and immutable throughout the application.

Task names, budgets, and window timing belong in the schedule JSON. Task IDs
are generated in sorted task-name order and exposed in `zule_task_ids.h`.
Their numeric values apply to a particular build and can change when tasks are
added, removed, or renamed. Fault-handler selection remains runtime
configuration through `zule_configure()`.

## CMake integration

The repository mirrors the [ownership domains](architecture.md#ownership).
`zule/` is the Zule domain: core, ports, schedule tools, and the CMake entry
point. An application owns its sources and configuration, and adds `zule/` as
a subdirectory. [demo/CMakeLists.txt](../demo/CMakeLists.txt) is the reference
integration:

```cmake
set(ZULE_PORT posix CACHE STRING "Zule port")
set(ZULE_USER_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_subdirectory(path/to/zule zule)

add_executable(app main.c)
target_link_libraries(app PRIVATE zule::core)
zule_add_schedule(app SCHEDULE config/schedule.json)
```

Inputs, set before `add_subdirectory()`:

| Variable | Meaning |
| --- | --- |
| `ZULE_PORT` | Required. A directory under `zule/ports/` (currently `posix`), or `external` when the application defines `zule::port`. |
| `ZULE_USER_CONFIG_DIR` | Optional absolute path of the directory holding `zule_user_config.h`. Enables `ZULE_USE_USER_CONFIG=1`. |
| `ZULE_BUILD_TESTS` | Build the core and port tests. Defaults to `ON` only when `zule/` is the top-level project. |

Targets and functions:

| Name | Use |
| --- | --- |
| `zule::core` | Link from the application. Carries the public API include path and the user configuration, so the application, core, port, and generated schedule share the same settings. |
| `zule_add_schedule(<target> SCHEDULE <json> [OUTPUT_DIR <dir>])` | Generates the schedule at build time and links it into `<target>`, exposing `zule_task_ids.h`. Requires Python 3.10 or later with `zule/tools/schedule/requirements.txt`. An active virtual environment is found automatically; otherwise select the interpreter with `Python3_EXECUTABLE`. Output defaults to `<target>_schedule/` in the calling binary directory. |
| `zule::port_api` | Link from an external port: provides `zule_port.h` and the public API. |
| `zule::port` | The selected port. An external port defines it as an alias, before or after adding Zule. The port's public include directories must supply `zule_port_types.h`. |

An external port, for example:

```cmake
set(ZULE_PORT external)
add_subdirectory(path/to/zule zule)
add_library(my_port STATIC my_port.c)
target_include_directories(my_port PUBLIC my_port)  # zule_port_types.h
target_link_libraries(my_port PUBLIC zule::port_api)
add_library(zule::port ALIAS my_port)
```

The core's `inc/` headers are internal. Only the core, its tests, and
generated schedule code compiled by `zule_add_schedule()` see them. A core-only
build without `zule_add_schedule()` needs no Python.

To build Zule and its tests on their own:

```sh
cmake -S zule -B build/zule -G Ninja -DZULE_PORT=posix
cmake --build build/zule
ctest --test-dir build/zule --output-on-failure
```
