# Planning

Open topics for future work. These are backlog items, not settled implementation
decisions or a committed order of delivery.

## Development environment

- Consider a dev container for tools that `requirements-dev.txt` cannot pin,
  such as the host and cross compilers. It should install `requirements-dev.txt`
  rather than duplicate its versions.
- Keep the core, modules, and port source lists in one place. They are
  currently listed in the CMake targets, the core tests' CMake, and the
  schedule tool's generation tests, so a new source file must be added to
  each. Consider deriving the test lists from the CMake targets or a shared
  list.

## Licence

- The repository is public but deliberately unlicensed while pre-release. Add a
  licence before promoting the project, accepting outside contributions, or
  making a first release.
- MIT is the provisional choice. Decide the copyright holder named in the notice.
- Once added, the licence applies to everything published from then on, and
  published versions remain available under it.

## Demos

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

## Continuous integration

- Add CI jobs, run in parallel, that build and run the existing tests under:
  ThreadSanitizer, including a short demo run, since the POSIX port shares
  state between real threads; AddressSanitizer and UndefinedBehaviorSanitizer;
  Clang as a second compiler; and GCC with `-Wconversion -Wshadow
  -Wstrict-prototypes`. All have been clean in local runs, so no existing
  findings need fixing first.
- Treat warnings as errors in the existing CI build and in each new job.
- With the Cortex-M port, add a cross-compiled build of the core, port, and demo
  for the Armv6-M subset (`-mcpu=cortex-m0plus`), optionally running tests in
  QEMU.

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

- Provide per-task statistics as a module: execution time minimum,
  maximum, and count, and worst dispatch latency from window start to task
  start, which also consumes budget. The tick timing module already reports
  the longest tick handler. Index storage by task
  ID, which equals the task's table index; size it from a task count to be
  added to the generated task-ID header.
- Keep monotonically increasing totals, such as busy time and cycle count, plus
  the busiest single cycle. Readers choose their own interval by subtracting two
  snapshots, so the module needs no measurement window. Idle time and slack
  within windows follow from the schedule; only running time is measured.
- Expose a snapshot copied under a critical section, and a reset that the
  application calls when it wants, for example before `zule_run()`. Keep the
  data in plain static structures that a debugger can also read.
- Open: statistics per task or per window, and a mean (needing a 64-bit
  total).
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
