# Minimal example

The smallest complete Zule application, and the one in the README's
[Writing an application](../../README.md#writing-an-application):

- [`main.c`](main.c): two tasks, `read_sensors` and `update_control`, a fault
  handler that keeps the fault report where a debugger can read it, and
  `zule_run()`.
- [`config/schedule.json`](config/schedule.json): a 100 ms cycle of ten 10 ms
  slots.
- [`CMakeLists.txt`](CMakeLists.txt): the integration, with no build settings,
  so Zule uses its defaults.

It uses no optional features and produces no output, so it can run on every
target. It runs until a fault or port error, when `zule_run()` returns and the
program exits nonzero; while it keeps running, its tasks meet their windows. CI
runs it briefly as the smoke test, since it does not depend on host timing.

## Targets

| Target | Build and run |
| --- | --- |
| `posix` | `cmake --preset posix`, `cmake --build --preset posix`, then `../../build/examples/minimal/posix/zule_minimal`; stop it with Ctrl-C. |
| `mps2-an385` | `cmake --preset mps2-an385`, `cmake --build --preset mps2-an385`, then `qemu-system-arm -machine mps2-an385 -nographic -semihosting-config enable=on,target=native -kernel ../../build/examples/minimal/mps2-an385/zule_minimal`; stop it with Ctrl-A, then X. Its port settings give the board's 25 MHz clock. |

See [examples](../README.md#targets) for how targets work.
