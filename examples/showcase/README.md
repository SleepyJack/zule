# Showcase example

A tour of Zule's features, and what [Getting started](../../README.md#getting-started)
runs:

- [`main.c`](main.c): the [application](../../doc/application.md). It provides
  three tasks that print their names, installs a fault handler, and calls
  `zule_run()`. Every ten cycles, which is every 5 seconds, it prints each
  task's mean and longest execution time and worst dispatch latency, and the
  busiest cycle, from the task statistics module, and the worst tick deviations
  and longest tick, from the tick timing module.
- [`config/`](config): the [configuration](../../doc/configuration.md). It
  contains the schedule `schedule.json` and the build settings
  `zule_user_config.h`.
- [`CMakeLists.txt`](CMakeLists.txt): a standalone project following
  [CMake integration](../../doc/application.md#cmake-integration).
- [`targets/`](targets): the targets it runs on; see
  [examples](../README.md#targets).

The showcase prints with the C library's `printf`, so each of its targets needs
a C library whose output goes somewhere visible: standard output on the host,
and a serial port on an embedded target.

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
to stop it.

| Target | Output |
| --- | --- |
| `posix` | Standard output. |
| `qemu-mps2-an385` | QEMU's standard output, through semihosting. Build with `cmake --preset qemu-mps2-an385` and `cmake --build --preset qemu-mps2-an385`, then run `qemu-system-arm -machine mps2-an385 -nographic -semihosting-config enable=on,target=native -kernel ../../build/examples/showcase/qemu-mps2-an385/zule_showcase`; stop it with Ctrl-A, then X. Its port settings give the board's 25 MHz clock. |
