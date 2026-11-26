# Miscellaneous details

Details removed from the documentation during its cleanup, collected by subject
for review. Each is to be moved back, kept here, or dropped. Most are also
stated in code comments.

## Scheduling and dispatch

- The scheduler records completion after the task function returns. At a
  window's end, tick context returns the execution state to idle before
  processing the next window.
- Only tick context makes a task pending. When no task is pending, the dispatch
  loop waits for the next tick through the port's idle, and the check and the
  wait share one critical section, so a tick that makes a task pending cannot be
  missed between them.
- Hook points sit beside the state transitions: release when a task becomes
  pending, task start just before running, task end as completion is recorded.

## Schedule switching

- A switch is applied by the tick that processes slot zero of the next cycle,
  after the current schedule's final window is retired. Every window lies within
  its cycle, so no task is pending or running then. The new schedule's first
  window is released in the same tick if it starts at slot zero.
- Ticks continue at the same rate across a switch, and the cycle count
  continues: all schedules share the slot length, so the port timer is never
  reprogrammed.
- All schedules are validated at initialization, so tick context never
  validates.
- No switch takes place while a fault is latched, so a task that overruns the
  end of the cycle is reported against the schedule it ran in.
- A request for the active schedule cancels a pending switch. Each run starts
  with the configured schedule and no request pending.
- `zule_schedule_request()` returns OK when the request is pending,
  `INVALID_CONFIG` for an ID that names no schedule (keeping any earlier
  request), and `NOT_INITIALIZED` when the scheduler is not running.
- The schedule switch hook reports each switch; it is not called for the
  schedule a run starts with.

## Faults

- The fault handler is not called for invalid schedules or configurations, or
  for port errors, which are reported only through the `zule_run()` result.
- A tick timing fault reports the deviation in timebase counts.
- Hooks continue for ticks after a fault.

## Timebase limits

Features using the timebase check at initialization that the schedule fits its
range, and `zule_run()` returns `ZULE_RESULT_INVALID_CONFIG` otherwise:

- **Hooks:** every schedule's cycle must be shorter than 2^32 counts, so any
  interval up to a cycle can be measured by unsigned subtraction.
- **Tick timing check:** each slot must be a whole number of counts, so expected
  tick times do not drift, and shorter than 2^31 counts, so deviations compare as
  signed differences. The tolerance, rounded down to whole counts, must be at
  least one count and less than one slot, so its resolution is one count.
- **Tick timing module:** the same slot rules as the check. A deviation of 2^31
  counts or more, such as a very long stall, is misread as early.

Expected tick times wrap with the timebase, so the check is exact for the life
of a run.

## Hooks

- Order: within each tick, tick start comes first and tick end last; a schedule
  switch comes before any release in its tick; a window's release comes before
  its task start, which comes before its task end; ticks can occur between a
  task's start and end.
- Modules receive each event before the application's hooks, in a fixed order.
- The core calls each hook point through an inline wrapper that reads the
  timebase once, then calls the module entry point and the user hook. A hook
  that neither consumes compiles to nothing, including the time read.

## Application API

- `zule_configure()` copies the configuration, which persists across runs. A
  null configuration, or a schedule ID that names no schedule, returns
  `ZULE_RESULT_INVALID_CONFIG` and keeps the previous settings; while running,
  it returns `ZULE_RESULT_ALREADY_RUNNING`.
- The generated `zule_task_ids.h` defines `zule_task_id_t` and constants such as
  `ZULE_TASK_ID_SENSE`, for comparison with `zule_fault_info_t.task_id`, and
  `zule_schedule_ids.h` likewise defines `zule_schedule_id_t` and the schedule ID
  constants.
- Applications see only the public API; the internal headers in
  `zule/core/inc/` and `zule/modules/inc/` are visible only to the Zule library,
  its tests, and the generated schedule.
- The static checks check that Zule needs only `memcpy`, `memmove`, `memset`,
  and `memcmp` from the target environment.

## Schedules and generation

- `ZULE_CONFIG_TASK_NAMES` changes the layout of the schedule's task type, so
  changing it needs every component recompiled, but no schedule regeneration:
  names are always generated, guarded by the setting.
- Generated tasks and names are in task-name order, and schedules in
  schedule-name order, whatever the JSON order; window order is preserved. The
  output is deterministic, with no timestamps or input paths.
- Each generated file is staged before replacing its destination, but replacing
  the set is not atomic: if writing fails, rerun generation before compiling.
- The schedule tool's modules: `schedule_io.py` loads JSON for all tools,
  `validation.py` checks schedules, `generation.py` emits C, and `cli.py`
  handles arguments and output. Each has its own test module.

## Modules

- A module may check, when `zule_run()` starts, that it can work with the
  schedule and timebase; a failure stops `zule_run()` with
  `ZULE_RESULT_INVALID_CONFIG` before ticks start. The core passes module init
  the ID of the schedule the run starts with.
- Results accumulate across runs until the module's reset function is called.
- Task statistics: single measurements are exact up to one timebase wrap of
  2^32 counts. Totals and counts are 64-bit, lasting over 100 years even at the
  fastest timebase, so readers can measure any interval by subtracting two
  snapshots. Reset clears every task and schedule inside one critical section,
  so its duration grows with their number.
- A module may share a helper's code with the core, such as the tick tracker
  that the tick timing check also uses, but owns its own state.
- Module file structure:
  - `zule_modules.h`, an internal core header, declares the module entry points,
    `zule_modules_init()` and one per hook; applications see only the user hooks
    in `zule_hooks.h`.
  - `zule_modules_config_defaults.h` holds the module settings, and
    `zule_modules_config_apply.h` checks them and derives the conditions the core
    reads: `ZULE_MODULES_ANY`, and for each hook, `ZULE_MODULES_HOOK_<HOOK>`.
  - `zule_modules.c` defines the entry points, each calling the enabled modules
    in a fixed order.
  - `zule_modules_task_data.h` and `zule_modules_schedule_data.h` define the
    per-task and per-schedule storage: types combining what every enabled module
    keeps for one task or schedule, the `ZULE_MODULES_TASK_DATA` and
    `ZULE_MODULES_SCHEDULE_DATA` conditions, and the `..._get()` functions the
    generated schedule defines, returning static, zero-initialized arrays.
- With per-task and per-schedule storage, the generated schedule uses RAM, not
  just constant tables.

## Ports

- Which optional features a port must provide, such as the timebase, is a core
  build setting, not a port setting; a port can require settings only when a
  feature needs them, for example a clock frequency only with the timebase.
- Each port's README lists its settings. The POSIX port has none yet.

### Cortex-M port

- SysTick raises the tick when its counter reaches 0, one count before it
  reloads, so a period starts there. A timebase read masks interrupts briefly, so
  the handler cannot run in the middle of it, and restores the previous mask.
  While the tick is held off, the pending tick shows that a period has started
  that the handler has not yet counted, and the read counts it.
- Stopping disables SysTick and clears any pending SysTick exception. On a
  single core, stop runs in application context, which a tick in progress would
  have preempted until it finished, so no callback is in flight when it returns.
- Files: `zule_port.c` implements the contract; `zule_port_cortex_m.h` holds
  register definitions, named after the Arm Architecture Reference Manuals, and
  the assembly functions; `zule_port_cortex_m.S` holds the instructions C cannot
  express (`mrs`, `cpsid`, `msr`, `wfi`, and barriers) and the registers'
  addresses, so the C has no casts from integers to pointers. Each call to the
  assembly is opaque to the compiler, so it cannot move accesses to shared data
  across a critical section's entry or exit.

### POSIX port

- Critical sections use a lazily initialized recursive pthread mutex: each enter
  locks once and each exit unlocks once, preserving outer critical sections. The
  saved state records the owning thread, so it is restored on that thread, in
  reverse entry order. The mutex provides inter-thread exclusion and memory
  synchronization across CPUs.
- The timer thread uses absolute `CLOCK_MONOTONIC` deadlines and calls the tick
  callback under the critical mutex, discarding its result. It knows nothing
  about schedules or tasks.
- Idle waits on a condition variable bound to the critical mutex, which the timer
  thread signals after each tick and on failure. A condition wait releases only
  one level of the recursive mutex, so idle aborts if called nested. Timer errors
  are reported before and after the wait.
- Stop signals a condition variable and joins the thread, so it does not wait
  for the next deadline. Repeated stops are safe, and a stopped timer can be
  restarted; timer creation failures leave no live worker.
- Timer errors return `ZULE_PORT_RESULT_ERROR`. Mutex, initialization, clock,
  and unrecoverable join failures abort, rather than falsely report exclusion or
  quiescence, since the port API has no error return for them.

## Build, examples, and test targets

- The formatting script formats handwritten C under `zule/` and `examples/` by
  default, excluding generated files. `--check` reports differences without
  editing and fails if formatting is needed. Paths are resolved from the script,
  so it runs from any directory; explicit file arguments are relative to the
  caller's directory.
- An example supports a target when it has a directory for it,
  `<example>/targets/<target>/`, holding only what that example needs there: a
  `target.cmake`, included before Zule is added, which selects the port and can
  point `ZULE_USER_PORT_CONFIG_DIR` at the target's port settings, and, when the
  target needs its own sources or options, an `example_target_configure()`
  function, which the example calls on its executable.
- The `qemu-mps2-an385` test target has 4 MiB of code memory at address 0 and
  4 MiB of RAM at `0x20000000`.
