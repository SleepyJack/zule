# Planning

Open topics for future work. These are backlog items, not settled implementation
decisions or a committed order of delivery.

## Verification for embedded targets

Zule's main product is the Zule library cross-compiled for embedded ports, so
verification centres on the target compiler and on static analysis rather than
host-specific tools. The sections below depend on each other: the development
environment provides the tools, the cross build and static analysis are the
main checks, and CI runs everything with the same tools and configurations.

### Development environment

- Host the development image on GitHub Container Registry. Add a workflow that
  builds and publishes it when the Dockerfile or `requirements-dev.txt`
  changes, tagged so that each commit's CI uses the matching image, and have CI
  jobs pull it instead of building it. Until then, CI builds the image on each
  run, with cached layers, and passes it to its jobs as an artifact.
- Consider pinning the base image by digest as well as tag.

### Cross-compilation

- Keep the stub port alongside real ports. It checks the Zule library alone, so
  a finding cannot come from port code or vendor headers; it is the only way to
  check targets without a port; and it shows the smallest valid port. Real ports
  check their own code and, in QEMU, behaviour, which the stub cannot.
- The cross-compiled builds become combinations of port, core, and features:
  each real port joins as presets alongside the stub's. Compiler warnings and
  static analysis apply to whichever port is selected, so real ports are
  covered as soon as they are added.
- When the Cortex-M port arrives, move the Arm toolchain file into it. The stub
  port then becomes architecture-neutral, used with any toolchain.
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

### Continuous integration

- With the Cortex-M port, run the tests and the fully featured demo in QEMU;
  see [demos](#demos).
- Considered and not planned: ThreadSanitizer, which would only cover the POSIX
  port, a development and demonstration target; and Clang as a second host
  compiler, which adds little beyond the cross-compiled builds.

### Demos

- Split the demo by feature set as features grow. Keep a minimal POSIX demo
  (tasks and a fault handler only) for the CI smoke test, with no timing
  sensitivity to host load.
- Add a fully featured demo for the Cortex-M port, running in QEMU on the
  `mps2-an385` (Cortex-M3) machine: the timebase, hooks, the tick timing check,
  and modules as they arrive. Run it in CI with `-icount`, so guest time follows
  executed instructions and timing, including the tick check, is deterministic
  regardless of runner load. It also exercises SysTick and the idle wait, though
  QEMU's models do not prove behaviour on silicon.
- Until then, the existing demo keeps the tick check enabled in CI with a
  half-slot tolerance; an occasional failure on a heavily loaded runner is
  acceptable.

## Schedule definition tooling

- Consider a tool to help users define and edit schedules, including task
  budgets, cycle length, and window placement.
- Explore suitable interfaces, such as an editor extension, interactive CLI,
  or visual timeline, without committing to one yet.
- Reuse the existing schema and validator for feedback on bounds, overlaps,
  and task references, and save definitions in the existing JSON format.

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
  [schedule definition tooling](#schedule-definition-tooling), showing planned
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
- Termination followed by `zule_configure()` and `zule_run()` is the initial way
  to [switch schedules](#multiple-schedules).

## Multiple schedules

- Allow an application to define several schedules, such as operating modes,
  and select one per run. Keep the change in the JSON schema and generator; the
  core still runs one validated schedule at a time.
- Decide the JSON shape. A shared task set keeps task functions and IDs
  consistent across schedules, and a single `slot_us` applies to all of them
  (required for switching, below). Validate every schedule offline.
- Select the initial schedule through `zule_configure()`, using a generated
  schedule identifier, with a defined default.
- Switch initially by [termination](#scheduler-termination-api), then
  `zule_configure()` and `zule_run()`. This uses existing and planned APIs, but
  restarts tick delivery, so each switch has the same first-cycle latency as
  startup.
- Fault information should identify the active schedule, since window indexes
  are per schedule.
- Add a switching API that keeps tick delivery running. A task requests the
  next schedule from application context; tick context applies the request at
  the next cycle boundary, so a switch takes effect within one cycle. At that
  boundary, retire the old schedule's final window, then switch, then start the
  new schedule's first window. Apply a switch only while no fault is latched.
- Require a shared `slot_us`, so a switch never reprograms the port timer, and
  validate every schedule at init, so tick context never validates.

### Startup delay as a pattern

- Handle startup latency with a startup schedule rather than a dedicated core
  feature: idle slots followed by a window for an application-owned
  mode-manager task, which requests the operational schedule. The startup
  cycle length sets the delay, and the operational schedule starts at slot zero
  of the following cycle. The mode manager can also perform startup checks and
  choose the initial mode.
- Until switching exists, leave the first slots of the schedule idle, as
  described in [scheduler initialisation](core.md#scheduler-initialisation).
  This absorbs startup latency at the cost of those slots in every cycle.
- Cover the pattern with a demo example: a startup schedule, a mode-manager
  task, and a switch to the operational schedule.
- If switching is deferred indefinitely, reconsider a dedicated startup delay:
  a countdown of `n` slots in tick processing, without marking tasks pending,
  before the first cycle begins at slot zero.

## Core and port configuration

- The Cortex-M port will add the first required port setting, the timer clock
  frequency; see [additional ports](#additional-ports).
- Consider opt-in diagnostics for applied defaults, keeping normal builds quiet.
  Check compiler support for `#warning` in the supported C99 toolchains and avoid
  excessive repeated diagnostics across compilation units.
- Consider detecting settings that differ between components, which a build
  outside CMake can cause by not giving every component the same definitions.

## Application critical sections

- Fault handlers run in tick context, so data shared between a fault handler and
  application context needs exclusion. Consider public wrappers for the port's
  critical section, with the same nesting and short-duration rules.
- Have the demo use them to protect the state its fault handler writes.

## Additional ports

- Add a Cortex-M port, choosing an initial core and board/toolchain to target.
  It joins the cross-compiled builds as presets alongside the stub port, which
  also brings it under static analysis, takes over the Arm toolchain file (see
  [cross-compilation](#cross-compilation)), and enables tests and the fully
  featured demo in QEMU (`mps2-an385`).
- Implement saved-state critical sections, periodic timer delivery, and the
  atomic idle wait against the generic port contract.
- For idle, `WFI` inside a PRIMASK critical section meets the contract: a
  pending interrupt wakes it even while masked, and the tick handler runs when
  the core exits the critical section. Interrupts masked through BASEPRI do not
  wake `WFI`, so a BASEPRI-based port must mask with PRIMASK and lift BASEPRI
  around the `WFI`.
- Provide the timebase from SysTick, and require the timer clock frequency as a
  port setting in `zule_user_port_config.h` when the timebase is enabled. The
  count is the tick count times the reload period, plus the elapsed part of the
  current period from the SysTick counter. A read must account for a counter
  wrap whose interrupt is still pending. SysTick is 24 bits, so check that the
  slot period fits. The DWT cycle counter is optional in the architecture and
  often stops during `WFI`, so it is not suitable as the timebase.
- Verify nesting, ordering barriers, timer start/stop behavior, and bounded
  execution and interrupt latency on the selected target.
- Provide an embedded example and document assumptions specific to the port.
