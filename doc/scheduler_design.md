# Scheduler design and intended behaviour

This document describes the scheduler's behaviour and timing model. See the
[architecture overview](architecture.md) for components and execution contexts.

## Scheduling overview

Tasks are allocated fixed execution windows within a predefined cyclic
schedule. The cycle is divided into slots, with slot timing defined by a
periodic timer implemented by the [port](architecture.md#port).

The tick handles window boundaries in [tick context](architecture.md#tick-context),
normally a timer ISR. At a window's start, it marks the task as pending;
[application context](architecture.md#application-context) then
dispatches and executes the task. Early completion does not advance the
schedule. Unallocated slots are idle.

### Task execution states

The scheduler tracks task execution using the following states.

| State | Description |
|-------|-------------|
| Idle | No active task execution. |
| Pending | Task is awaiting dispatch. |
| Running | Task has been dispatched; completion is not yet recorded. |
| Completed | Task function has returned and completion is recorded. |

The normal state transitions are below.

| Transition | Context | Condition |
| --- | --- | --- |
| Idle → Pending | Tick | The task's window begins. |
| Pending → Running | Application | The application loop dispatches the pending task. |
| Running → Completed | Application | The task returns and the scheduler records completion. |
| Completed → Idle | Tick | The task's window ends. |

The example below shows two tasks in a ten-slot cycle. Each remains in the
completed state until its window ends. Pending intervals are omitted at this
resolution; see [task dispatch](#task-dispatch) for the detailed timing.

![Example schedule](images/schedule.svg)

## Scheduler timing details

### Scheduler initialisation

The scheduler entry point validates the schedule, starts the port timer, and
enters the application dispatch loop. The first tick processes slot zero.

If the first tick occurs before timer initialization returns, the first task
can become pending before the application loop is ready to dispatch it.
Startup overhead then consumes part of the first window's budget. Leaving
initial slots idle allows time for initialization.

![Startup timing when the first tick arrives before timer initialization returns.](images/initialisation.svg)

### Task dispatch

At a window's start, tick context marks the task as pending. The application
dispatch loop sets the state to running before calling the task function.
Critical sections protect these [state transitions](#task-execution-states).
Task functions execute
outside the critical sections, allowing ticks to run during task execution.

![Expanded dispatch timing, from the release tick through pending to running.](images/dispatch.svg)

The scheduler records completion after the task function returns. At the
window's end, tick context returns the execution state to idle before processing
the next window.

The detailed diagrams illustrate an ISR interrupting application execution and
are not to scale. The POSIX port instead uses a timer thread.

### Task deadline monitoring

At a window's end, tick context checks the task's execution state. A task still
pending raises a **missed-dispatch** fault: it was not dispatched within its
window and will not be dispatched after the fault.

![A task remains pending until the deadline tick raises a missed-dispatch fault.](images/dispatch_missed.svg)

A task still running raises an **execution-overrun** fault. Completion must be
recorded before the deadline check, so the window must include the overhead of
returning from the task and updating its state. [Fault detection](#fault-handling) does not
terminate the task function.

![A task is running at its deadline; the tick raises an overrun fault while the task remains running.](images/execution_overrun.svg)

## Fault handling

A fault halts schedule progression and prevents further task dispatch. The
first fault remains latched until [reinitialization](#scheduler-initialisation);
subsequent checks do not replace it or repeat notification.

The application can supply a fault handler, invoked immediately in tick context.
This allows the application to respond independently of a running task. The
handler must be bounded and nonblocking, and must not call Zule APIs.

Tick processing continues while a running task completes. Once application
context observes the fault, the scheduler stops tick delivery and waits for any
active tick handler before returning. An executing task must return before the
scheduler entry point can return.

## Timing assumptions

The port is responsible for timer accuracy, reliable tick delivery, and
synchronization between tick and application contexts. Scheduler timing depends
on delivered ticks, so detecting timer failure may require independent
monitoring, such as a watchdog.

Task budgets must account for scheduler overhead and interrupt interference as
well as task execution time.
