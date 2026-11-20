# Showcase example

A tour of Zule's features, and what [Getting started](../../README.md#getting-started)
runs:

- [`main.c`](main.c): the [application](../../doc/application.md). It provides
  three tasks that print their names, installs a fault handler, and calls
  `zule_run()`. Every ten cycles, which is every 5 seconds, it prints each
  task's mean and longest execution time and worst dispatch latency, and the
  busiest cycle, from the task statistics module, and the worst tick deviations
  and longest tick, from the tick timing module.
- [`example_console.h`](example_console.h): the console the showcase prints to.
  Each target implements it, so the showcase itself uses no C library and formats
  numbers itself.
- [`config/`](config): the [configuration](../../doc/configuration.md). It
  contains the schedule `schedule.json` and the build settings
  `zule_user_config.h`, which enables task names, the timebase, the
  [tick timing check](../../doc/core.md#tick-timing-check) with a tolerance of
  half a slot, and the [tick timing](../../doc/modules.md#tick-timing) and
  [task statistics](../../doc/modules.md#task-statistics) modules.
- [`CMakeLists.txt`](CMakeLists.txt): a standalone project following
  [CMake integration](../../doc/application.md#cmake-integration). It adds
  `../../zule`, includes the selected target, and points Zule at `config/`.
- [`targets/`](targets): the targets it runs on; see
  [examples](../README.md#targets).

The schedule is a 500 ms cycle of ten 50 ms slots: `sense`, `control`, and
`output`, followed by idle time. The slow slots suit ordinary host scheduling.

## Running

With the development environment from [Getting started](../../README.md#getting-started)
active, on the `posix` target:

```sh
cd examples/showcase
cmake --preset posix
cmake --build --preset posix
../../build/examples/showcase/posix/zule_showcase
```

The cycle repeats until a fault or port error, when the showcase prints the
fault, including the task's name, and exits nonzero. On the host, press Ctrl-C
to stop it; there is no public stop API yet. The fault handler only copies the
snapshot; `main()` prints it after `zule_run()` returns, rather than logging
from tick context.

| Target | Console |
| --- | --- |
| `posix` | Standard output. The [POSIX port](../../zule/ports/posix/README.md) runs ticks on a timer thread and gives no hard real-time guarantees. |

CI runs the showcase briefly and checks that all three tasks appear.
