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
