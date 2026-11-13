# Zule demo

A small user-domain application, and the reference for integrating Zule:

- [`main.c`](main.c): the [application](../doc/application.md). It provides three
  tasks that print their names, installs a fault handler, and calls `zule_run()`.
- [`config/`](config): the [configuration](../doc/configuration.md). It contains
  the schedule `control-loop.json` and the build settings `zule_user_config.h`,
  which enables task names.
- [`CMakeLists.txt`](CMakeLists.txt): a standalone project following
  [CMake integration](../doc/application.md#cmake-integration). It adds `../zule`,
  selects the POSIX port, and points Zule at `config/`.

The schedule is a 500 ms cycle of ten 50 ms slots: `sense`, `control`, and
`output`, followed by idle time. The slow slots suit ordinary host scheduling.

## Running

With the development environment from the [root README](../README.md) active:

```sh
cmake -S demo -B build/demo -G Ninja
cmake --build build/demo
./build/demo/zule_demo
```

The cycle repeats until a fault or port error, when the demo prints the fault,
including the task's name, and exits nonzero. Press Ctrl-C to stop it; there is
no public stop API yet. The fault handler only copies the snapshot; `main()`
prints it after `zule_run()` returns, rather than logging from tick context.

The [POSIX port](../zule/ports/posix/README.md) runs ticks on a timer thread and
gives no hard real-time guarantees. CI runs the demo briefly and checks that all
three tasks appear.
