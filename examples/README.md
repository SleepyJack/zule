# Examples

Complete Zule applications, each a standalone CMake project in the user domain
(see [ownership](../doc/architecture.md#ownership)). Each has a different
purpose, and each lists the targets it runs on.

| Example | Purpose | Targets |
| --- | --- | --- |
| [`minimal`](minimal/README.md) | The smallest complete application: tasks, a fault handler, and `zule_run()`, with no optional features and no output. | `posix`, `qemu-mps2-an385` |
| [`showcase`](showcase/README.md) | A tour of Zule's features, with task names, the tick timing check, and the modules' timing reports printed with the C library's `printf`. What [Getting started](../README.md#getting-started) runs. | `posix`, `qemu-mps2-an385` |

## Targets

A target is a port and the hardware or host it runs on. An example supports a
target when it has a directory for it, `<example>/targets/<target>/`, holding only
what that example needs on that target:

- `target.cmake`, included before Zule is added, which selects the port and can
  point `ZULE_USER_PORT_CONFIG_DIR` at the target's port settings;
- when the target needs its own sources or options, such as startup code, an
  `example_target_configure(<executable>)` function, defined in `target.cmake`,
  which the example calls to add them to its executable.

Each example has a CMake preset for each of its targets, named after the target,
and builds to `build/examples/<example>/<target>/`:

```sh
cd examples/<example>
cmake --preset <target>
cmake --build --preset <target>
```

The example keeps its own build settings, `zule_user_config.h`, and the target
keeps the port settings, `zule_user_port_config.h`, as each belongs to a
different part of an application. When several examples run on the same
hardware, the code they share, such as startup code or a device driver, goes in
`common/targets/<target>/`, which provides building blocks and knows nothing
about the examples.

| Target | Port | Notes |
| --- | --- | --- |
| `posix` | [POSIX](../zule/ports/posix/README.md) | The host, with no real-time guarantees. Runs until stopped with Ctrl-C. |
| `qemu-mps2-an385` | [Cortex-M](../zule/ports/cortex_m/README.md) | QEMU's emulation of Arm's MPS2 board with a Cortex-M3, using the [board support](common/targets/qemu-mps2-an385/README.md) in `common/targets/`. Needs the Arm GNU toolchain and QEMU, which the development image has. A fault ends QEMU with a failure status; otherwise it runs until stopped with Ctrl-A, then X. |

On `qemu-mps2-an385`, output goes to QEMU's standard output through
semihosting, and with `-icount shift=0,sleep=off` added, every run's timing
is the same; see [target tests](../TESTING.md#target-tests). Without
`sleep=off`, QEMU keeps to real time.
