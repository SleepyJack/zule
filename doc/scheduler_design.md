# Scheduler design and intended behaviour

This document defines the scheduler's execution contract. See the
[architecture overview](architecture.md) for components and execution contexts.

## Timeline and windows

- Initialization validates the schedule and resets execution to idle, with slot
  and cycle count both zero. It does not release a task; the first tick processes
  slot zero. The port determines the delay before that first tick.
- Each fault-free tick processes one slot boundary, then advances the timeline.
  Slots wrap at the cycle length; the 32-bit cycle count wraps modulo 2^32.
- A window occupies `[start_slot, start_slot + budget_slots)`. Windows are
  ordered, nonoverlapping, and contained within the cycle; gaps are idle time.
- A window's task becomes pending at its start. Application context dispatches
  it when polling observes pending work. Dispatch delay consumes the window's
  budget; it does not move the deadline.
- Early completion does not advance later windows. At a shared boundary, the
  finishing window is checked before the next task is released. A deadline at
  the cycle end is checked at slot zero of the following cycle.

## Execution state

Only one task invocation is active at a time. Tasks execute sequentially and
return normally; the scheduler does not switch between executing tasks.

| Transition | Condition |
| --- | --- |
| Idle → Pending | The window starts and no fault is active. |
| Pending → Running | Application context claims the task and no fault is active. |
| Running → Completed | The task returns and application context publishes completion. |
| Completed → Idle | Tick processes the window's end boundary. |

Completion must be published before the deadline check. Returning from the
task and publishing completion are separate operations; critical-section
ordering determines which state the deadline check observes.

## Ownership and synchronisation

Tick context owns timeline progression, task release, deadline checks, and fault
notification. Application context owns dispatch and completion. It protects
shared execution-state transitions using port critical sections, while task
functions execute outside those sections. Task definitions remain immutable.

Tick callbacks are non-reentrant and excluded from protected application-context
transitions. Critical sections preserve prior exclusion state, support nesting,
and provide memory ordering. Initialization requires quiescent tick delivery
and no executing task. Failed initialization leaves tick and dispatch disabled.

## Faults and shutdown

At a window's end, pending work raises a missed-dispatch fault; running work
raises an execution-overrun fault. A non-idle state at a new window's release
also raises an overrun defensively; valid state transitions should prevent it.

The first fault is retained and its optional handler is called once,
synchronously in tick context. The snapshot identifies the task, window, and
detection cycle/slot. Handlers must be bounded and nonblocking, must not call
Zule APIs, and must copy the snapshot if retaining it.

A fault prevents further window advancement, task release, dispatch, and
slot/cycle advancement, including on the detecting tick. An executing task may
still return and publish completion. Subsequent ticks can retire it to idle;
this does not clear the fault or restart scheduling. Reinitialization resets it.

The core detects timing violations but does not forcibly terminate tasks or
establish an application safe state. A task that never returns prevents the
runner from returning, although tick-context fault notification can still occur
if ticks continue. The application owns the fault response.

The public runner stops and quiesces tick delivery before returning on failure.
Initialization and port errors are reported by return code, without invoking the
task-fault handler. Automatic recovery and a public termination API are not
currently provided.

## Timing assumptions

The port supplies periodic ticks using the schedule's slot period. The core
counts delivered ticks; it does not independently measure elapsed time or detect
lost ticks. A common policy for late or missed ticks remains to be specified;
each port must document its delivery latency and failure behaviour.

Window budgets must allow for dispatch, completion bookkeeping, scheduler work,
and interrupt interference. Structural validation establishes neither execution
time bounds nor correspondence between generated tables and source JSON.
