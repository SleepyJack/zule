# Modes example

Operating modes as [multiple schedules](../../doc/configuration.md#multiple-schedules),
a [switch](../../doc/application.md#switching-schedules) between them, and a
[stop](../../doc/application.md#stopping-the-scheduler) from a task:

- [`config/schedule.json`](config/schedule.json): three schedules of 10 ms slots
  sharing four tasks. `startup` is four slots: three idle, which absorb
  startup latency, then `mode_manager`. `operational` runs `sense` and
  `control`, with `mode_manager` in its last slot. `shutdown` is four slots
  beginning with `safe_state`, whose two slots are reserved for stopping as they
  would be for any other work.
- [`main.c`](main.c): the run starts with `startup`. The mode manager stands in
  for startup checks, then requests `operational`, which starts at slot 0 of the
  next cycle. The tasks print their names. After three operational cycles the
  mode manager requests `shutdown`, and `safe_state` puts the outputs in a safe
  state and calls `zule_terminate()`.

It uses no build settings. Unlike the [minimal](../minimal/README.md) and
[showcase](../showcase/README.md) examples, which run until a fault or port
error, it ends by itself: `zule_run()` returns `ZULE_RESULT_TERMINATED` and the
program exits 0 after about a third of a second.

## Targets

| Target | Build and run |
| --- | --- |
| `posix` | `cmake --preset posix`, `cmake --build --preset posix`, then `../../build/examples/modes/posix/zule_modes`; it stops itself. |
| `qemu-mps2-an385` | `cmake --preset qemu-mps2-an385`, `cmake --build --preset qemu-mps2-an385`, then `qemu-system-arm -machine mps2-an385 -nographic -semihosting-config enable=on,target=native -kernel ../../build/examples/modes/qemu-mps2-an385/zule_modes`; it stops itself, ending QEMU with its exit status. |

See [examples](../README.md#targets) for how targets work.
