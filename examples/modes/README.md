# Modes example

Operating modes as [multiple schedules](../../doc/configuration.md#multiple-schedules),
and a [switch](../../doc/application.md#switching-schedules) between them:

- [`config/schedule.json`](config/schedule.json): two schedules of 10 ms slots
  sharing three tasks. `startup` is four slots: three idle, which absorb
  startup latency, then `mode_manager`. `operational` runs `sense` and
  `control`, with `mode_manager` in its last slot.
- [`main.c`](main.c): the run starts with `startup`. The mode manager stands in
  for startup checks, then requests `operational`, which starts at slot 0 of the
  next cycle. The tasks print their names.

It uses no build settings, and runs until a fault or port error, like the
[minimal example](../minimal/README.md). CI runs it briefly on both targets and
checks that the switch is printed before `sense` first runs.

## Targets

| Target | Build and run |
| --- | --- |
| `posix` | `cmake --preset posix`, `cmake --build --preset posix`, then `../../build/examples/modes/posix/zule_modes`; stop it with Ctrl-C. |
| `qemu-mps2-an385` | `cmake --preset qemu-mps2-an385`, `cmake --build --preset qemu-mps2-an385`, then `qemu-system-arm -machine mps2-an385 -nographic -semihosting-config enable=on,target=native -kernel ../../build/examples/modes/qemu-mps2-an385/zule_modes`; stop it with Ctrl-A, then X. |

See [examples](../README.md#targets) for how targets work.
