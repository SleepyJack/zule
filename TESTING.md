# Testing

Zule is checked in three ways, each with its own place in the repository:

| Kind | What it shows | Where |
| --- | --- | --- |
| [Host tests](#host-tests) | Behaviour of the core, modules, ports, and schedule tool, run on the build machine. | `zule/core/tests/`, `zule/ports/<port>/tests/`, `zule/tools/schedule/tests/`, and the [examples](examples/README.md) |
| [Target tests](#target-tests) | Behaviour on a target, for now in QEMU. | `zule/ports/<port>/tests/qemu/`, with board support in `zule/tests/targets/` |
| [Static checks](#static-checks) | That Zule compiles and links cleanly for target cores, and passes static analysis, including MISRA C:2012. Nothing runs. | `zule/tests/static/` |

The commands below assume the development environment from
[CONTRIBUTING.md](CONTRIBUTING.md#setup) is active, or are run in the
development image. Builds use
[CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html):
each project defines its own, in its `CMakePresets.json`, and builds to
`build/` in the repository root.

## Host tests

### Core and port tests

```sh
cd zule
cmake --preset tests
cmake --build --preset tests
ctest --preset tests
```

- **Core and module tests** in `zule/core/tests/` run the core against a
  deterministic test port, with a scripted timebase, so they never depend on
  host timing. Each feature builds in its own configuration of settings, and
  `zule_add_test()` adds a test in one call.
- **POSIX port tests** in `zule/ports/posix/tests/` run the real port, with
  threads and timers.
- **Cortex-M port host tests** in `zule/ports/cortex_m/tests/host/` compile the
  port's C against fake registers and instructions, so they check its logic, not
  the hardware.

The `sanitize` preset builds and runs the same tests under
UndefinedBehaviorSanitizer and AddressSanitizer, in `build/zule-sanitize/`. Any
undefined behaviour or invalid memory access they detect fails the test:

```sh
cd zule
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

### Schedule tool tests

```sh
cd zule/tools/schedule
python -m unittest discover -s tests -v
```

They cover loading, validation, and generation; the generation tests also
compile the generated C with the core and run it. See the
[schedule tool](zule/tools/schedule/README.md#development) for their layout.

### Examples

Each [example](examples/README.md) has a preset for each target it supports.
Building one also regenerates its schedule:

```sh
cd examples/showcase
cmake --preset posix
cmake --build --preset posix
../../build/examples/showcase/posix/zule_showcase
```

An example runs until stopped, since it returns only on a fault, so CI runs each
for two seconds and checks it is still running:

- **On POSIX,** the minimal example is the stable smoke test, since it does not
  depend on host timing. The showcase must also print every task's name, and its
  tick check can fault on a heavily loaded host.
- **On `qemu-mps2-an385`,** both examples run in QEMU with `-icount` and
  `sleep=off`, so their timing is deterministic, and a fault ends QEMU with a
  failure status. The minimal example prints nothing, so CI also counts SysTick
  exceptions in QEMU's exception log (`-d int`), to tell a running scheduler
  from a stuck program. The showcase prints through the board's UART: CI checks
  every task's name, and that its tick timing report shows no deviation, which
  only QEMU's exact timing can promise.

## Target tests

Target tests run on a target, for now
[`qemu-mps2-an385`](zule/tests/targets/qemu-mps2-an385/README.md): QEMU's
emulation of Arm's MPS2 reference board with a Cortex-M3. Each is a standalone
project, since it builds with the target's toolchain, with a preset per target
and `ctest` running QEMU:

```sh
cd zule/ports/cortex_m/tests/qemu
cmake --preset qemu-mps2-an385
cmake --build --preset qemu-mps2-an385
ctest --preset qemu-mps2-an385
```

The **core tests** also run on the target, from
[`zule/core/tests/qemu/`](zule/core/tests/qemu): the same tests, in the same
configurations, against the same deterministic test port as on the host, listed
once in [`zule/core/tests/tests.cmake`](zule/core/tests/tests.cmake). They show
that the core behaves the same with the target's compiler, type sizes, and
arithmetic. Each test is its own program, run in QEMU, and a failed `assert()`
prints its condition and fails the test.

The [Cortex-M port](zule/ports/cortex_m/README.md)'s target tests check what its
host tests cannot: that ticks arrive at the programmed rate against an
independent timer, that critical sections hold ticks off, including nested
ones, that idle wakes for each tick and loses none, and that stopping leaves no
tick pending. The last test runs the real scheduler on the port until a task
overruns on purpose. The `qemu-mps2-an385-all` preset also enables every
feature: it checks the timebase against the independent timer and across
millions of reads, and runs the scheduler with every hook, the tick timing
check, and both modules.

QEMU runs with `-icount shift=0,sleep=off`: guest time follows executed
instructions, and jumps to the next timer event while the guest waits in `WFI`,
so every run is the same, whatever the host's load. Without `sleep=off`, waits
follow the host's clock, and tick timing varies between runs. Tests report
through semihosting, which also sets QEMU's exit status, and link with newlib,
the Arm GNU toolchain's C library, so they can use `assert()` and `printf()`. QEMU shows the logic against its model of
the hardware, not real timing, interrupt latency, or behaviour a model may not
capture; those need hardware.

## Static checks

The static checks build Zule with a target compiler, currently
`arm-none-eabi-gcc`, in a standalone project in
[`zule/tests/static/`](zule/tests/static). Each links the Zule library, a port,
and a generated schedule into a small program that nothing runs: freestanding,
with no C library, and with warnings as errors, including `-Wconversion`,
`-Wshadow`, and `-Wstrict-prototypes`. Use of a C library function, or a warning
with the target's type sizes, fails the build. GCC requires `memcpy`,
`memmove`, `memset`, and `memcmp` even in a freestanding build, so the program
provides them, standing in for the target environment.

Its presets, named `<port>-<core>[-all]`, choose the port, the core, and the
features:

| Preset | Port | Core | Features |
| --- | --- | --- | --- |
| `stub-m3` | [Stub](zule/ports/stub/README.md) | Cortex-M3 (Armv7-M) | Defaults |
| `stub-m3-all` | Stub | Cortex-M3 (Armv7-M) | All enabled |
| `stub-m0plus` | Stub | Cortex-M0+ (Armv6-M) | Defaults |
| `stub-m0plus-all` | Stub | Cortex-M0+ (Armv6-M) | All enabled |
| `cortex-m-m3` | [Cortex-M](zule/ports/cortex_m/README.md) | Cortex-M3 (Armv7-M) | Defaults |
| `cortex-m-m3-all` | Cortex-M | Cortex-M3 (Armv7-M) | All enabled |
| `cortex-m-m0plus` | Cortex-M | Cortex-M0+ (Armv6-M) | Defaults |
| `cortex-m-m0plus-all` | Cortex-M | Cortex-M0+ (Armv6-M) | All enabled |

The stub port checks the Zule library alone; a real port's presets check it with
the library.

```sh
cd zule/tests/static
cmake --preset stub-m3-all
cmake --build --preset stub-m3-all
```

After compiling, each build runs [Cppcheck](https://cppcheck.sourceforge.io/)
and its MISRA C:2012 addon over the Zule library, the selected port, and the
generated schedule, analysed together as one program from the build's compile
database. It checks for errors, warnings, style, and portability problems,
assuming 32-bit types as on Cortex-M, for functions that are never used, and
for violations of the MISRA rules the addon covers; see
[MISRA C](doc/CODING_GUIDELINES.md#misra-c). A finding fails the build. Fix the
code rather than suppress the finding; see
[static analysis findings](doc/CODING_GUIDELINES.md#static-analysis-findings).

## Adding tests

- **Core or module behaviour:** a test in `zule/core/tests/`, added with
  `zule_add_test()` in `tests.cmake`, in a test configuration with the settings
  it needs. It then runs on the host and on the target.
- **A port:** host tests in the port's `tests/`, or `tests/host/` for a port that
  also has target tests, in `tests/qemu/`. [Testing a port](doc/port.md#testing-a-port)
  lists what to cover.
- **Support for another emulated board:** a directory in `zule/tests/targets/`,
  like `qemu-mps2-an385/`, with its startup code, linker script, and CMake helper.
- **The schedule tool:** a test module in `zule/tools/schedule/tests/`, with JSON
  cases in its `fixtures/`.
- **A static check for a new core, port, or feature set:** a preset in
  `zule/tests/static/CMakePresets.json`, added to the CI job's list.

## Continuous integration

GitHub Actions first builds the [development image](.devcontainer/README.md),
caching its layers between runs, and passes it to the other jobs, which run
every step inside it:

| Job | Runs |
| --- | --- |
| Tests and formatting | Handwritten and generated C formatting, the schedule tool tests, the core and port tests, and the examples on POSIX. |
| Tests under sanitizers | The core and port tests with the `sanitize` preset. |
| Static checks | Every static check preset. |
| Target tests | The Cortex-M port's target tests, the core tests, and both examples, in QEMU. |

CI runs for pushes and pull requests, and the
[CI workflow](.github/workflows/ci.yml) can also be run manually.
