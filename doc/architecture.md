# Zule architecture

Zule is a static, time-triggered scheduler. Tasks execute within predefined bounded time windows in a repeating schedule cycle.

An application supplies a schedule and task implementations. The core manages the scheduling of tasks. A port supplies target-specific implementations for core functions (e.g. timing).

Each component below has its own document, linked from its section.

## System overview

![Zule architecture: configuration and application in the user domain, connected to the core and port in the Zule domain.](images/architecture.svg)

## Ownership

There are two ownership domains: the user domain and the Zule domain. The
**[application](#application)** and application-specific
**[configuration](#configuration)** are owned by the user. The Zule
**[core](#core)**, **[modules](#modules)**, and **[port](#port)** reside in the Zule ownership domain.

## Components

### Configuration

The configuration defines the set of tasks and the schedule, and the build settings for the core and the selected port. Core settings include which optional features are enabled, such as the sub-slot timebase, hooks, and modules. Port settings supply target information that only the application knows, such as a timer clock frequency, so configuration feeds both the core and the port.

See [configuration](configuration.md).

### Application

The application defines the implementation of the tasks and the high-level interactions with the scheduler (starting, stopping, [fault handling](core.md#fault-handling)). It can also define [hooks](application.md#hooks), which the core calls on scheduler events, and read results from enabled modules.

See [application](application.md).

### Core

The scheduler core dispatches tasks to be run according to the schedule and enforces schedule-defined timing constraints on task execution.

The core reports scheduler events through hooks, to enabled modules and then to the application's hooks. It never names individual modules.

See [core](core.md) for task execution and timing details.

### Modules

Optional consumers of the core's hook events, shipped with Zule and each enabled by a core setting. Modules observe the schedule, for example to collect task timing statistics, and cannot change scheduling. The application reads their results through each module's API.

See [modules](modules.md).

### Port

Target-specific implementations of services to support the scheduler: the periodic tick, an optional sub-slot timebase, and data synchronisation. The port reads its settings from the configuration and implements the optional features the configuration enables, or fails compilation if it cannot.

See [port](port.md).

## Execution Contexts

There are two execution contexts that span the various components:
**[application context](#application-context)** and
**[tick context](#tick-context)**.

### Application Context

All application tasks run in the application context. Application tasks run sequentially according to the defined schedule. There is no concurrency, preemption, or context switching between Zule tasks, so shared inter-task data requires no synchronisation. (The mechanisms used for inter-task communication are outside the scope of the Zule project, but can be public function calls or shared data with no need for concurrency considerations.) The invocation of the Zule scheduler entry point is from the application context: all tasks will run from this same context.

### Tick Context

The periodic timing tick used for task scheduling executes from the tick context. The implementation of the periodic tick is port-defined. For most targets the expectation is that the tick will execute from a timer-driven ISR (although this is not necessarily the case; for example, the POSIX port uses a thread for the tick timer). Synchronisation is required for data shared between tick and application contexts (synchronisation implementation is provided by the port).
