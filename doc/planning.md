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

- Add a fault event, for modules only, with the [trace module](#trace-module),
  which freezes on it. The application already receives faults through its fault
  handler, so there is no user fault hook.
- Add idle start and end events only when a consumer needs them, since they
  fire on every dispatch loop iteration.
- Deferred: equal treatment for several separate user modules. That needs a
  registry, most cleanly a generated dispatcher built from a list of modules and
  the hooks each needs. Only the modules layer would change; the core's
  wrappers and the hooks stay the same.

## Trace module

A module that records hook events in a ring buffer, read back through an API,
for two uses: a fault trace, kept rolling and read once the run has stopped, and
a stream, drained periodically by a task. One ring serves both, and the
configuration says which.

- Events are fixed-size records pushed by the hook entry points. Hooks never run
  concurrently with each other or with the tick, so the ring has one logical
  producer and one consumer, and a push needs no critical section. A
  power-of-two capacity makes the wrap a mask, so no division runs on the tick
  path, and the storage is static, sized by the capacity setting. Initialization
  rejects a capacity that is not a power of two of at least two events, and the
  module needs the timebase.
- One setting per hook decides what enters the buffer, as the user hook settings
  do, so an event nothing records compiles the hook point out of the core
  instead of being tested on every tick. On by default: task release, task
  start, task end, schedule switch, and the [fault event](#hooks-and-modules).
  Off by default: tick start and tick end, which are two events per tick and the
  bulk of the volume.
- Keep the record narrow: a timestamp, the kind, a task ID, and one detail field
  carrying the window index, slot, or schedule ID. The slot and cycle of an
  event are then reconstructed on the host from timestamps, which needs an
  anchor at a known slot: task release fires at a window's start slot, which is
  why it is on by default and why a timeline does not need tick start.
- Count what was lost, and make the count readable. A trace with silent holes is
  worse than no trace.
- Overwriting the oldest event while a consumer reads is the one combination
  that cannot work: the producer would reclaim slots the consumer holds, and a
  record could be read half-overwritten. So the rolling buffer is read once the
  run has stopped, when ticks are quiescent, and the streaming mode drops new
  events when full instead, which never tears a record.
- Freeze the rolling buffer on the fault event, so that the ticks after a fault,
  which still run, do not overwrite the lead-up the trace was kept for.
- Read in batches, so a consumer serializes a block at a time, and report what a
  decoder cannot know: the events enabled, the capacity, the slot length, the
  timebase frequency, the dropped count, and whether the trace is frozen.
- Offload through a debugger or the application, and decode on the host, where
  the schedule JSON supplies task names, budgets, and window boundaries. Keep
  encoding on the target to packing records into bytes, since formatting there
  costs code space and tick time for no benefit. Timestamps wrap in 32 bits, so
  the host works in deltas.
- Write the decoded trace in a format an existing viewer opens, rather than
  building a viewer. Chrome's trace event JSON, which Perfetto's web UI reads,
  draws each task as a labelled span on its own track and is a few lines to
  emit. VCD, the waveform format that Verilog tools such as GTKWave read, suits
  comparing a trace against a logic analyser capture of the same run, which the
  GPIO toggles under
  [task profiling](#task-profiling-and-benchmarking) produce. Both are text, and
  neither asks anything of the target.
- Consider sharing a timeline renderer with the
  [schedule editor](../zule/tools/schedule/README.md#editing), showing planned
  and actual timing together.
- Start with the fault trace: it cannot be approximated another way, needs no
  fast offload path, and is read when ticks are quiescent, which removes the
  questions above about reading a live buffer. Streaming follows, adding the
  drop-counting mode and the batch read from a task. The showcase can then
  report its trace after zule_run() returns, beside the fault it already prints.

## Task profiling and benchmarking

- Add host tooling that reads the [task statistics](modules.md#task-statistics)
  with the schedule JSON and reports each task's measured maximum, budget, and
  margin, suggesting `budget_slots`.
- Simple modules, such as GPIO toggles per event, support measurement with a
  logic analyser.
- Benchmark scheduler overhead, dispatch latency, and tick jitter on supported
  targets, accounting for instrumentation overhead and interrupt interference.
- Define reproducible workloads and record target, clock, compiler, and build
  settings. Distinguish measured results from proven worst-case timing bounds.

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
