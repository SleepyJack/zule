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

- The port timebase API is defined: an optional capability
  (`ZULE_PORT_CONFIG_TIME`) providing a wrapping 32-bit count and its frequency.
  See [timebase](port.md#timebase). The POSIX port implements it.
- Use the timebase for core timing checks and task profiling, as core settings
  that fail compilation with a port lacking the capability. At initialization,
  check that each slot is a whole number of counts and that one cycle is
  shorter than 2^31 counts. Keep timing policy
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
- Each port defines its own `zule_port_config.h` for capabilities, defaults,
  and validation, with optional application overrides in
  `zule_user_port_config.h`, found through `ZULE_USER_CONFIG_DIR`. Keep the core
  independent of port-specific settings; it reads only generic capabilities.
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
- Provide the timebase from SysTick, and require the timer clock frequency as a
  port setting in `zule_user_port_config.h`.
- Verify nesting, ordering barriers, timer start/stop behavior, and bounded
  execution and interrupt latency on the selected target.
- Provide an embedded example and document assumptions specific to the port.
