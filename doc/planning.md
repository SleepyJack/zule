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
- Add a porting guide covering port selection, required types and functions,
  critical-section semantics, tick callbacks, idle behavior, errors, and
  shutdown/quiescence requirements.
- Explain application integration, generated schedule compilation, and the
  distinction between the public API, internal interfaces, and port API.
