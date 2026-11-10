# Build configuration

`core/api/zule_config.h` supplies compile-time defaults and validates
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

For example, a CMake integrator can apply settings in the parent directory
before adding Zule and the application targets:

```cmake
add_compile_definitions(ZULE_USE_USER_CONFIG=1)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/config")
add_subdirectory(path/to/zule)
```

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
