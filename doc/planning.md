# Planning

Open topics for future work. These are backlog items, not settled implementation
decisions or a committed order of delivery.

## Development environment

- Consider a dev container for tools that `requirements-dev.txt` cannot pin,
  such as the host and cross compilers. It should install `requirements-dev.txt`
  rather than duplicate its versions.

## Licence

- The repository is public but deliberately unlicensed while pre-release. Add a
  licence before promoting the project, accepting outside contributions, or
  making a first release.
- MIT is the provisional choice. Decide the copyright holder named in the notice.
- Once added, the licence applies to everything published from then on, and
  published versions remain available under it.

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

- The port timebase API is defined: an optional core feature
  (`ZULE_CONFIG_TIMEBASE`) providing a wrapping 32-bit count and its frequency.
  See [timebase](port.md#timebase). The POSIX port implements it.
- Use the timebase for core tick timing checks, as a core setting that requires
  `ZULE_CONFIG_TIMEBASE`, and for [hook](#hooks-and-modules) timestamps used by
  profiling modules. At initialization, check that each slot is a whole number
  of counts and that one cycle is shorter than 2^31 counts. Keep timing policy
  and fault detection in the core, with clock and timer implementation in the
  port.
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
- Keep this callback separate from [hooks](#hooks-and-modules), so watchdog
  support does not require the timebase.
- Keep the callback bounded and nonblocking. Define how application task progress
  contributes to watchdog servicing so continued ticks alone do not conceal a
  stalled task.
- Document the application's responsibility for watchdog configuration and
  servicing policy, including detection of lost ticks or a stalled scheduler.

## Hooks and modules

- Split observation from enforcement. The core enforces the schedule: deadline
  checks, tick timing checks, and faults. Hooks report scheduler events, and
  their consumers observe without affecting scheduling. There are two kinds of
  consumer: user hooks, defined by the application, and modules, shipped with
  Zule.
- Hooks require `ZULE_CONFIG_TIMEBASE`; enabling a user hook or a module without
  it fails compilation. Every hook therefore has a fixed signature that includes
  a timestamp.
- **User hooks** are a core feature, like the fault handler. A core setting per
  hook, such as `ZULE_CONFIG_USER_HOOK_TASK_END`, makes the core call an
  application-defined function, such as `zule_user_hook_task_end()`. They do not
  depend on modules. An application with several consumers of its own combines
  them in its hook functions.
- **Modules** are enabled by one setting each, such as
  `ZULE_CONFIG_MODULE_STATS`, and are wired automatically: the application never
  calls module hook handlers. A module fails compilation if a feature it needs,
  such as the timebase, is disabled. Modules expose their own read APIs to the
  application.
- The modules layer provides, for each hook, a condition that is true when any
  enabled module needs it, and one entry point that calls each enabled module in
  a fixed order. The core knows modules only through these entry points and
  conditions, never individual modules, so adding a module does not change the
  core. The core includes the modules header as it includes `zule_port.h`.
- The core invokes each hook through a `static inline` wrapper in an internal
  core header, keeping call sites unconditional. The wrapper reads the timebase
  once, inside its enabled branch, then calls the modules entry point and the
  user hook, in that order. An unused hook compiles to nothing, including the
  time read; the time is read inside the wrapper rather than passed as an
  argument so that a disabled hook cannot still read the timer.
- Proposed layout, mirroring the ports: the public hook settings and user hook
  declarations in `zule/core/api/`, the inline wrappers in `zule/core/inc/`, and
  the modules header and entry points at the top of `zule/modules/`, with each
  module in its own subdirectory. A `zule_modules` library holds every module,
  each guarded by its setting, and the core links it privately.
- Guarantees: hooks never run concurrently with each other or with the tick.
  Tick-context hooks run in the tick, and application-context hooks run inside
  the core's critical section, so consumers need no locking of their own, and a
  read API only needs a critical section for a consistent snapshot. Hook run
  time therefore adds to critical sections and to measured tick duration.
- Hooks can run in tick context, so consumers must be bounded and nonblocking
  and must not call Zule APIs, like the fault handler. One slow consumer delays
  the tick for every other.
- Define the events, their arguments, contexts, and ordering. A starting point:
  run start (application context, before ticks), tick start and end, dispatch
  (just before the task call), task end (just after return), fault, and run
  stop (application context, after ticks are quiescent). Lifecycle events let
  modules reset or flush without separate init calls. Guarantee, for example,
  run start first, dispatch before the matching task end, fault at most once,
  and run stop last. Leave idle start and end out until a consumer needs them,
  since they fire on every loop.
- Give the core test port a deterministic fake timebase, so tests can check
  hook order, arguments, and timestamps exactly. Prove the design with a
  recording consumer in the core tests and a user hook in the demo; the
  statistics module then proves the module path.
- Deferred: equal treatment for several separate user modules. That needs a
  registry, most cleanly a generated dispatcher built from a list of modules and
  the hooks each needs. Only the dispatcher and the conditions would change;
  the core's wrappers and the module interface stay the same.
- Add the modules component to the architecture documentation and diagram,
  receiving events from the core and read by the application, with its own
  document. Show user hooks as an application extension point beside the fault
  handler.

## Task profiling and benchmarking

- Provide per-task statistics as the first module: execution time minimum,
  maximum, and count, and worst dispatch latency from window start to task
  start, which also consumes budget. Also record the maximum tick handler
  duration, since budgets must cover scheduler overhead. Size storage from a
  task count in the generated task-ID header.
- Keep monotonically increasing totals, such as busy time and cycle count, plus
  the busiest single cycle. Readers choose their own interval by subtracting two
  snapshots, so the module needs no measurement window. Idle time and slack
  within windows follow from the schedule; only running time is measured.
- Expose a snapshot copied under a critical section, and a reset. Keep the data
  in plain static structures that a debugger can also read.
- Open: statistics per task or per window, a mean (needing a 64-bit total), and
  whether statistics reset at each run or only on request.
- Add host tooling that reads the statistics with the schedule JSON and reports
  each task's measured maximum, budget, and margin, suggesting `budget_slots`.
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

- Keep core defaults and validation in `zule/core/api/zule_config.h`, with optional
  application overrides in `zule_user_config.h`.
- Each port defines its own `zule_port_config.h` for defaults and validation,
  with optional application overrides in `zule_user_port_config.h`, found
  through `ZULE_USER_CONFIG_DIR`. Keep the core independent of port-specific
  settings. Optional features are core settings that ports implement or reject.
  The Cortex-M port will add the first required setting, the timer clock
  frequency.
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

## Application critical sections

- Fault handlers run in tick context, so data shared between a fault handler and
  application context needs exclusion. Consider public wrappers for the port's
  critical section, with the same nesting and short-duration rules.
- Have the demo use them to protect the state its fault handler writes.

## Additional ports

- Add a Cortex-M port, choosing an initial core and board/toolchain to target.
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
