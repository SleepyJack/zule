# Planning

Open topics for future work. These are backlog items, not settled implementation
decisions or a committed order of delivery.

## Verification for embedded targets

Zule's main product is the Zule library cross-compiled for embedded ports, so
verification centres on the target compiler and on static analysis rather than
host-specific tools. The sections below depend on each other: the development
environment provides the tools, the [static checks](../TESTING.md#static-checks)
and tests on targets are the main checks, and CI runs everything with the same
tools and configurations.

### Development environment

- Host the development image on GitHub Container Registry. Add a workflow that
  builds and publishes it when the Dockerfile or `requirements-dev.txt`
  changes, tagged so that each commit's CI uses the matching image, and have CI
  jobs pull it instead of building it. Until then, CI builds the image on each
  run, with cached layers, and passes it to its jobs as an artifact.
- Consider pinning the base image by digest as well as tag.

### Static checks

- Keep the stub port alongside real ports. It checks the Zule library alone, so
  a finding cannot come from port code or vendor headers; it is the only way to
  check targets without a port; and it shows the smallest valid port. Real ports
  check their own code and, in QEMU, behaviour, which the stub cannot.
- Consider presets for combinations of features between the defaults and
  everything enabled, such as the tick check without hooks, if code depends on
  how settings combine. The host tests already build each feature in its own
  configuration.
- Later, consider a target where `int` is 16 bits, such as MSP430, built with
  the stub port, to catch assumptions about `int` size.

### Static analysis

- Supply the licensed MISRA rule texts to the addon, so its findings describe
  the rules.
- The free addon covers only part of MISRA C:2012. Consider a tool with full
  coverage if compliance needs to be claimed.
- Consider reporting findings as GitHub annotations, for example as SARIF.
- Generated code: the application is responsible for checking its generated
  schedule, and Zule makes that easy rather than validating the generator.
  - Provide a one-step check that runs Zule's static analysis, with the same
    options as the static checks, on an application's generated
    schedule and build settings.
  - Analyse a reference schedule that exercises every generator branch in the
    static checks: many tasks, a task in several windows, an unused
    task, the longest names, and values near their limits.
  - Document the generated code's structure, which parts vary with the input,
    and the application's responsibility for checking the result.

### Continuous integration

- Considered and not planned: ThreadSanitizer, which would only cover the POSIX
  port, a development and demonstration target; and Clang as a second host
  compiler, which adds little beyond the static checks.

### Examples

- Consider an example for measuring and checking timing on a target, such as
  setting budgets and the tick timing tolerance from the modules' measurements.
- Make the examples besides the showcase more embedded-focused: shorter slots,
  and consider dropping their POSIX targets, keeping the showcase runnable on
  the host for [Getting started](../README.md#getting-started).

## Schedule editor

The [editor](../zule/tools/schedule/README.md#editing) edits a file's tasks,
schedules and windows, with one cursor over the whole grid. Recurring placement
expands to ordinary windows rather than becoming new JSON syntax, so the format,
the validator, and the generator are unchanged and the committed schedule stays
explicit. A file that is not there yet is started by the editor rather than by
hand, and the slot length, the cycle lengths, the tasks and the windows are all
cells, so a schedule can be written without touching the JSON.

Constraints the editor follows:

- Nothing the editor needs is imported by the other commands, which every
  application build runs. It uses the standard library's `curses` alone.
- Each edit is a pure function from one schedule to another, and the frame the
  editor draws is a value, so both are tested without a terminal.
- Writing goes through the [formatter](../zule/tools/schedule/README.md#formatting),
  so an edit does not rewrite the parts it did not touch.

## Watchdog integration

- Consider an application callback from tick context to support watchdog
  integration. Define when it runs relative to timing and deadline checks,
  what scheduler health information it exposes, and its behaviour after faults.
- Keep this callback separate from [hooks](#hooks-and-modules), so watchdog
  support does not require the timebase.
- Keep the callback bounded and nonblocking. Define how application task progress
  contributes to watchdog servicing so continued ticks alone do not conceal a
  stalled task.
- Document the application's responsibility for watchdog configuration and
  servicing policy, including detection of lost ticks or a stalled scheduler.

## Hooks and modules

- Add a fault event, for modules only, with the trace module. The application
  already receives faults through its fault handler, so there is no user fault
  hook.
- Add idle start and end events only when a consumer needs them, since they
  fire on every dispatch loop iteration.
- Deferred: equal treatment for several separate user modules. That needs a
  registry, most cleanly a generated dispatcher built from a list of modules and
  the hooks each needs. Only the modules layer would change; the core's
  wrappers and the hooks stay the same.

## Task profiling and benchmarking

- Add host tooling that reads the [task statistics](modules.md#task-statistics)
  with the schedule JSON and reports each task's measured maximum, budget, and
  margin, suggesting `budget_slots`.
- Consider a trace module: a ring buffer of fixed-size timestamped events,
  frozen on a fault or on request to keep the lead-up. Offload it through a
  debugger or the application, and decode it on the host for timeline viewers,
  such as Perfetto or VCD. Consider sharing a timeline renderer with
  [the schedule editor](#schedule-editor), showing planned
  and actual timing together.
- Simple modules, such as GPIO toggles per event, support measurement with a
  logic analyser.
- Benchmark scheduler overhead, dispatch latency, and tick jitter on supported
  targets, accounting for instrumentation overhead and interrupt interference.
- Define reproducible workloads and record target, clock, compiler, and build
  settings. Distinguish measured results from proven worst-case timing bounds.

## Scheduler termination API

- Add an API to request that the scheduler halt, causing the run loop to exit
  and return `ZULE_RESULT_TERMINATED`.
- Stop and quiesce tick delivery before returning. Define allowed caller
  contexts, treatment of an executing task, and precedence between termination
  requests and faults.
- Show it in the [`modes` example](../examples/modes/README.md): a third,
  shutdown schedule whose task puts outputs in a safe state and then
  terminates, so the example exits cleanly, and CI checks its exit status
  rather than a timeout.

## Documentation

- Review [misc.md](misc.md): move each detail back into the document it came
  from, keep it there, or drop it, then remove the file.
- Slim code comments the same way, keeping what the code cannot state itself.

## Core and port configuration

- Consider opt-in diagnostics for applied defaults, keeping normal builds quiet.
  Check compiler support for `#warning` in the supported C99 toolchains and avoid
  excessive repeated diagnostics across compilation units.
- Consider detecting settings that differ between components, which a build
  outside CMake can cause by not giving every component the same definitions.

## Application critical sections

- Fault handlers run in tick context, so data shared between a fault handler and
  application context needs exclusion. Consider public wrappers for the port's
  critical section, with the same nesting and short-duration rules.
- Have the showcase example use them to protect the state its fault handler
  writes.

## Additional ports

The [Cortex-M port](../zule/ports/cortex_m/README.md) provides critical
sections, the tick, idle, and the timebase, with
[target tests](../TESTING.md#target-tests) in QEMU. Remaining steps:

- Verify on hardware what QEMU cannot show, on a NUCLEO-F411RE (Cortex-M4):
  tick timing and interrupt latency, `WFI` waking while masked, and the
  timebase, including a period starting between its counter and pending-flag
  reads, which QEMU does not reach. Cross-check against the DWT cycle counter
  and a logic analyser, and add the board as a target for the examples.
- Consider masking only the tick and lower priorities through BASEPRI on Armv7-M
  and later, so critical sections do not delay higher-priority interrupts.
  Interrupts masked through BASEPRI do not wake `WFI`, so idle would still mask
  with PRIMASK and lift BASEPRI around the `WFI`.
