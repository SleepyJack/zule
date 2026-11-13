# Planning

Open topics for future work. These are backlog items, not settled implementation
decisions or a committed order of delivery.

## Development environment

- Make CMake a persistent part of the documented development environment;
  decide between system installation and a pinned development dependency.
- Consider a Docker image or dev container for a reproducible compiler,
  CMake, Python, and formatting toolchain.
- Harmonise the existing requirements files, keeping schedule-tool runtime
  dependencies distinct from development-only dependencies where useful.
- Align local setup and CI so the same build and check commands work in both.

## Static analysis

- Evaluate the free, open-source edition of Cppcheck, previously considered
  for C defect analysis.
- Decide the initial checks, supported tool version, and how findings are
  reviewed and addressed.
- Cover handwritten core and port code as well as generated C, using the
  actual build configuration and include paths.
- Decide how to invoke analysis locally and in CI. Keep formatting as a
  separate responsibility of clang-format.

## Schedule definition tooling

- Consider a tool to help users define and edit schedules, including task
  budgets, cycle length, and window placement.
- Explore suitable interfaces, such as an editor extension, interactive CLI,
  or visual timeline, without committing to one yet.
- Reuse the existing schema and validator for feedback on bounds, overlaps,
  and task references, and save definitions in the existing JSON format.

## Timebase and tick timing checks

- Add a port API for a monotonic timebase with sub-tick resolution, usable from
  both application and tick contexts. Define units, resolution, wraparound,
  safe reads, and bounded read overhead. It must continue advancing while tick
  interrupts are delayed or masked.
- Use the timebase for core timing checks and task profiling. Keep timing policy
  and fault detection in the core, with clock and timer implementation in the port.
- Establish the first expected tick boundary and compare subsequent ticks with
  expected deadlines, accounting for accumulated drift as well as individual
  delays. Define acceptable lateness and the fault response for missed periods.
- Test delayed ticks, missed periods, timebase wraparound, and startup timing.
  Tick timing checks cannot detect a complete loss of execution on their own;
  retain independent watchdog monitoring where required.

## Watchdog integration

- Consider an application callback from tick context to support watchdog
  integration. Define when it runs relative to timing and deadline checks,
  what scheduler health information it exposes, and its behaviour after faults.
- Keep the callback bounded and nonblocking. Define how application task progress
  contributes to watchdog servicing so continued ticks alone do not conceal a
  stalled task.
- Document the application's responsibility for watchdog configuration and
  servicing policy, including detection of lost ticks or a stalled scheduler.

## Task profiling and benchmarking

- Add tooling to measure task execution times and compare them with configured
  budgets, reporting observed maxima, variation, and remaining margin.
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

## Startup delay

- Allow a configurable startup delay of `n` slots. Tick delivery runs during
  this delay, but tick processing must not inspect the window schedule or mark
  tasks pending.
- Begin the first schedule cycle at slot zero after the delay; a delay of zero
  retains immediate startup behavior.
- Use the delay to accommodate latency between enabling ticks and entering the
  task polling loop, making first-cycle behavior more similar to later cycles.
- Define where the setting belongs and test zero delay, delay countdown, and
  the transition into normal schedule processing.

## Core and port configuration

- Keep core defaults and validation in `zule/core/api/zule_config.h`, with optional
  application overrides in `zule_user_config.h`.
- Let each port define its own `zule_port_config.h` for defaults and validation,
  with optional application overrides in `zule_user_port_config.h`. Introduce
  port configuration headers when settings are needed; keep the core independent
  of port-specific settings.
- Use these Zule-owned headers to document supported settings, defaults,
  constraints, and required values in one place for each scope.
- Apply sensible defaults, but fail compilation for missing required hardware
  information, such as a timer clock frequency, rather than guessing a value.
- Consider opt-in diagnostics for applied defaults, keeping normal builds quiet.
  Check compiler support for `#warning` in the supported C99 toolchains and avoid
  excessive repeated diagnostics across compilation units.
- Document and verify build integration so the core, selected port, application,
  and generated code receive consistent settings. Keep `slot_us` in the schedule
  JSON; the port converts it to timer settings and checks representability.

## Additional ports

- Add a Cortex-M port, choosing an initial core and board/toolchain to target.
- Implement saved-state critical sections and periodic timer delivery against
  the generic port contract.
- Verify nesting, ordering barriers, timer start/stop behavior, and bounded
  execution and interrupt latency on the selected target.
- Provide an embedded example and document assumptions specific to the port.

## Documentation

- Improve the top-level README with a clear project introduction, current
  capabilities and limitations, architecture overview, and quick-start steps.
