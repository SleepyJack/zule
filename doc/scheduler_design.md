# Scheduler design and intended behaviour

This document defines the scheduler's execution contract. See the
[architecture overview](architecture.md) for components and execution contexts.

## Scheduling overview

Tasks are allocated fixed execution windows within a predefined cyclic schedule. The cycle duration is divided into slots. A periodic timer (implemented by the port) generates a tick which defines the slot timing. The tick runs in its own execution context - expected to be an ISR context for most ports. The tick handles slot boundary conditions; driving the schedule timing. When a task's window begins its execution is marked as pending from the tick context but the task execution is from the application context.

### Task execution

| State | Description |
|-------|-------------|
| Idle  | No task execution |
| Pending | Task pending execution |
| Running | Task execution in progress |
| Completed | Task execution complete |

| Transition | Context | Condition |
| --- | --- | --- |
| Idle → Pending | Tick | There is a task window beginning at the newly entered slot |
| Pending → Running | Application | There is a pending execution and the task has not yet been dispatched |
| Running → Completed | Application | The task execution completes (task function returns) |
| Completed → Idle | Tick | At the slot boundary following task completion, the task is returned to idle state |

// Drawing here: normal task timing flow

## Scheduler timing

### Scheduler initialisation

* Explain how the scheduler entry first enables the timer and then begins processing pending tasks
* Cover the implications for the first slot: could be extra overhead if the timer fires early
  * Concept of dead slots at the start of the schedule (not yet implemented)

### Task dispatch

* Maybe not needed if the 'normal' flow is covered aboive

### Task deadline monitoring

* Explain the definitions of the two faults: late dispatch and overrun (with diagrams?)

### Fault handling

* Explain that the scheduler halts (stops advancing the timeline) in case of a fault
  * Explain that the tick will still run if there is a task running
* Explain that the application can get immediate response via fault handlers **in tick context**
* Once the offending task completes (returns) the tick will be disabled and the scheduler exits (returns)
* Explain that the first encountered fault is latched: only the first fault reported

## Timing assumptions

* Explain that the port is responsible for timer accuracy, reliability etc.
* Should be checked by watchdog if necessary
  * We should maybe add a tick hook to support that
