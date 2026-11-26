# Miscellaneous details

Details taken out of the component documents during the documentation cleanup,
pending review: each is to be kept here, moved back, moved into code comments,
or dropped. Most are also stated in code comments.

## Core

- The scheduler entry point validates every schedule before starting the port
  timer, so tick context never validates.
- Only tick context makes a task pending, so the dispatch loop's check for a
  pending task and the port's idle wait share one critical section, and a tick
  that makes a task pending cannot be missed between them.
- At a window's end, tick context returns the execution state to idle before
  processing the next window.
- A switch is applied by the tick that processes slot zero of the next cycle,
  after the old schedule's final window is retired, so no task is pending or
  running then. The new schedule's first window is released in the same tick if
  it starts at slot zero. The cycle count continues across switches.
- No switch takes place while a fault is latched, so a task that overruns the
  end of the cycle is reported against the schedule it ran in.
- A request for the active schedule cancels a pending switch. Each run starts
  with no request pending.
- The tick timing check runs before the tick does any other processing, and its
  fault reports the deviation in timebase counts. Expected times wrap with the
  timebase, so the check is exact for the life of a run.
- Initialization checks that the timebase supports the tick timing check: each
  slot a whole number of counts and shorter than 2^31 counts, and the tolerance,
  rounded down to whole counts, at least one count and less than one slot. The
  tolerance resolution is therefore one count.
- With hooks enabled, initialization checks that every schedule's cycle is
  shorter than 2^32 timebase counts, so any interval up to a cycle can be
  measured by unsigned subtraction.
- After a fault, ticks continue, and so do tick hooks, until the scheduler stops
  tick delivery.

## Application

- `zule_configure()` copies the configuration, which persists across runs. A
  null configuration, or a schedule ID that names no schedule, returns
  `ZULE_RESULT_INVALID_CONFIG` and keeps the previous settings; while running,
  it returns `ZULE_RESULT_ALREADY_RUNNING`. Without any configuration,
  `zule_run()` runs the schedule with ID 0 and no fault handler.
- `zule_run()` and `zule_configure()` must be called from a single application
  context.
- The fault handler is not called for invalid schedules or configurations, or
  for port errors, which are reported only through the `zule_run()` result. The
  snapshot pointer is valid only during the call.
- `zule_schedule_request()` results: OK, `INVALID_CONFIG` for an ID that names
  no schedule (keeping any earlier request), and `NOT_INITIALIZED` when the
  scheduler is not running.
- Hook order: within each tick, tick start comes first and tick end last; a
  schedule switch comes before any release in its tick; a window's release comes
  before its task start, which comes before its task end; ticks can occur
  between a task's start and end. Modules receive each event before the
  application's hooks.
- Tick hooks run in tick context and the others inside the scheduler's critical
  section, so data that only application-context hooks write can be read by
  tasks without synchronization. Hook run time adds to the tick and to the
  critical section.
- The schedule switch hook is not called for the schedule a run starts with.
- `zule_task_name_get()` can be called at any time, including before
  `zule_run()`.
- The [static checks](../TESTING.md#static-checks) check that Zule needs only
  `memcpy`, `memmove`, `memset`, and `memcmp` from the target environment.
- `zule_add_schedule()` output defaults to `<target>_schedule/` in the calling
  binary directory; one schedule file per application is supported. The
  interpreter choice is cached in the build directory, and building Zule
  without `zule_add_schedule()` needs no Python.
- Applications see only the public API; the internal headers in
  `zule/core/inc/` and `zule/modules/inc/` are visible only to the Zule library,
  its tests, and the generated schedule.

## Configuration

- Generation requires every timing value to fit `uint32_t`.
- Each task ID is the task's index in the generated task table, which the core
  validates, so tools and modules can index per-task data by ID. IDs include
  tasks without windows.
- The `zule_task_` prefix lets task names such as `switch` avoid C keywords.
- `ZULE_CONFIG_TASK_NAMES` changes the layout of the schedule's task type, so
  changing it needs every component recompiled, but no schedule regeneration.
- Which optional features a port must provide, such as the timebase, is a core
  build setting, not a port setting; a port can require settings only when a
  feature needs them. The POSIX port has no settings.
- Only one of `ZULE_USER_CONFIG_DIR` and `ZULE_USER_PORT_CONFIG_DIR` may hold
  `zule_user_port_config.h`.

## Modules

- A module may check, when `zule_run()` starts, that it can work with the
  schedule and timebase; a failure stops `zule_run()` with
  `ZULE_RESULT_INVALID_CONFIG` before ticks start.
- Module results are copied or cleared inside a critical section, so reads are
  consistent while ticks run. Module APIs are declared only when the module is
  enabled, and cannot be called from hooks or the fault handler.
- Task statistics' reset clears every task and schedule inside one critical
  section, so its duration grows with their number.
- Task statistics totals and counts are 64-bit, lasting over 100 years even at
  the fastest timebase, so readers can measure any interval by subtracting two
  snapshots.
- Per-task and per-schedule module storage makes the generated schedule use RAM,
  not just constant tables.
- The core calls each hook point through an inline wrapper that reads the
  timebase once, then calls the module entry point and the user hook; a hook
  that neither consumes compiles to nothing, including the time read.
- A module may share a helper's code with the core, such as the tick tracker
  that the tick timing check also uses, but owns its own state.

## Port

- Contract summary, also in `zule_port.h`: `critical_enter` and
  `critical_exit` from application context; `tick_init` and `tick_stop` from
  application context; `idle` from application context in a critical section;
  `time_hz` from any context and `time_get` from application and tick context,
  when the timebase is enabled.
- The core only stores the critical-section state returned by enter and passes
  it to exit; for example, a port masking interrupts might store the previous
  mask, and the POSIX port stores the owning thread.
