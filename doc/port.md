# Port

A port supplies the target-specific services the core needs: a periodic tick,
exclusion between tick and application contexts, and idle behaviour in the
dispatch loop. Ports shipped in `zule/ports/` are owned by Zule; a port written
by an integrator is owned by the user (see [ownership](architecture.md#ownership)).

| Port | Target |
| --- | --- |
| [`posix`](../zule/ports/posix/README.md) | Native development on POSIX hosts, using a timer thread and a mutex. |

## Contract

The contract is declared in [`zule/ports/zule_port.h`](../zule/ports/zule_port.h).
Its comments are authoritative for each function; this section summarises the
model they share.

| Function | Called from | Purpose |
| --- | --- | --- |
| `zule_port_critical_enter()` | Application context | Exclude the tick and return the previous exclusion state. |
| `zule_port_critical_exit()` | Application context | Restore the state returned by the matching enter. |
| `zule_port_tick_init()` | Application context | Start delivering the tick callback every `slot_us` microseconds. |
| `zule_port_tick_stop()` | Application context | Stop delivery and wait for any in-flight callback. |
| `zule_port_idle()` | Application context, in a critical section | Wait for the next tick when no task is pending. |

### Execution contexts

The port defines [tick context](architecture.md#tick-context). On embedded
targets it is normally a timer ISR; the POSIX port uses a thread. Either way:

- the tick callback never runs concurrently with itself or with an application
  critical section;
- the callback may run before `zule_port_tick_init()` returns, because the
  scheduler is initialized first;
- `tick_init`, `tick_stop`, and `idle` have a single application-context
  caller and are never called from the callback. `tick_init` and `tick_stop`
  run outside critical sections; `idle` runs inside one (see [idle](#idle)).

### Critical sections

The core keeps critical sections short and never runs tasks inside them.

- Entry saves the current exclusion state; exit restores exactly that state.
  Never unconditionally re-enable interrupts on exit.
- Sections nest, and are exited in reverse entry order in the same context.
- Entry and exit provide the compiler and hardware memory ordering needed to
  keep protected accesses inside the section.
- Embedded ports enter and exit in bounded time, without waiting or spinning.
  Native development ports may block on host synchronization, and must document
  it.
- Interrupt masking provides exclusion only on a single core.

### Tick lifecycle

- `zule_port_tick_init()` is called with delivery stopped, a nonzero period,
  and a non-null callback. On failure, delivery remains stopped and quiescent.
- `zule_port_tick_stop()` returns only once no callback is running or can start.
  Repeated calls are safe, and even an error return must leave delivery
  quiescent. A stopped tick can be started again.
- The port may discard the callback's result. A scheduler fault is core policy
  and must not be reported as a port error.
- The port reports its own timer failures, such as late delivery, through
  `ZULE_PORT_RESULT_ERROR` from `zule_port_idle()`.

### Idle

The core decides when to wait. After each dispatch attempt it enters a critical
section and checks for a pending task or a latched fault. If there is neither,
it calls `zule_port_idle()` without leaving the critical section, so no tick can
run between the check and the wait.

The port supplies only the wait:

- Release exclusion while waiting and restore it before returning, atomically,
  so that a tick that becomes due after the core's check still ends the wait.
- The core calls idle at exactly one level of nesting.
- Returning early is allowed; the core checks again.
- Report timer failures through `ZULE_PORT_RESULT_ERROR`, including failures
  that occur while waiting, rather than waiting indefinitely.

On a Cortex-M core with PRIMASK-based critical sections, `WFI` meets this
directly: a pending interrupt wakes it even while masked, and the tick handler
runs when the core exits the critical section. Interrupts masked through BASEPRI
do not wake `WFI`, so a BASEPRI-based port must mask with PRIMASK and lift
BASEPRI around the `WFI`. A native port can use a condition wait on its
critical-section lock, signalled after each tick. A low-power mode must keep the
tick timer running and able to wake the core.

### Types

A port provides `zule_port_types.h`, which defines `zule_port_critical_state_t`
completely. Its size and contents are up to the port; the core only stores the
value returned by enter and passes it to exit. For example, a Cortex-M port
might store the previous `PRIMASK`, and the POSIX port stores the owning thread.

## Creating a port

A port consists of:

- `zule_port_types.h`;
- sources implementing every function in `zule_port.h`;
- a README describing target assumptions, timing limits, and any blocking or
  latency behaviour;
- tests, where the target allows.

Port sources include `zule_port.h`, which in turn includes `zule.h` and
`zule_port_types.h`.

### Linking a custom port

An application can supply a port without modifying Zule. Set `ZULE_PORT` to
`external` and define a `zule::port` alias, before or after adding Zule:

```cmake
set(ZULE_PORT external)
add_subdirectory(path/to/zule zule)

add_library(my_port STATIC my_port/my_port.c)
target_include_directories(my_port PUBLIC my_port)   # zule_port_types.h
target_link_libraries(my_port PUBLIC zule::port_api)
add_library(zule::port ALIAS my_port)
```

`zule::port_api` provides `zule_port.h`, the public API, and the shared
[build settings](configuration.md#build-settings). The port's public include
directories must supply `zule_port_types.h`. The core links the port privately,
so applications do not see the port's headers.

### Adding a port to Zule

A port shipped with Zule lives in `zule/ports/<name>/` and is selected with
`ZULE_PORT=<name>`. Its `CMakeLists.txt` defines the same target shape as a
custom port, plus tests in `tests/` built when `ZULE_BUILD_TESTS` is on. See
[`zule/ports/posix/`](../zule/ports/posix) for an example.

### Testing a port

Core tests use a deterministic test port and do not exercise a real port. Test
each port on its target for:

- nested critical sections, restoring pre-existing exclusion;
- mutual exclusion between the tick callback and critical sections;
- tick delivery at the configured period, including start, stop, and restart;
- idle waiting until a tick, including one that becomes due while the caller
  holds the critical section, and returning an error on timer failure;
- quiescence after `zule_port_tick_stop()`, including with a long period;
- error reporting for invalid arguments and late delivery;
- bounded entry, exit, and interrupt latency on embedded targets.
