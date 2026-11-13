# POSIX development port

This port implements the [port contract](../../../doc/port.md) for native
development on POSIX hosts.

Select this port with `ZULE_PORT=posix`. It uses a lazily initialized recursive
pthread mutex. Each enter locks once and each exit unlocks once, preserving
outer critical sections. The saved token records the owning thread; restore it
on that thread, in reverse entry order. Initialization or mutex errors abort,
since the port API has no error return and cannot safely proceed without exclusion.

This is a native development port: locking can block and has no bounded-latency
guarantee. It is not usable from a POSIX signal handler. Unlike embedded ports,
it does not mask interrupts. Every native tick thread must enter the same
critical section around the callback; the scheduler protects its own dispatch transitions.
The mutex provides inter-thread exclusion and memory synchronization, including
when the host schedules threads on different CPUs. Tick calls must still be
serialized, and initialization must finish before either context starts.

`zule_port_tick_init()` starts a timer thread with the supplied period and
callback. It uses absolute `CLOCK_MONOTONIC` deadlines and invokes the callback
under the critical mutex. The thread knows nothing about schedules, task states,
or demo duration. Callback results are discarded: scheduler faults are latched
and handled by the core runner, independently of timer failures.
Tick delivery at least one slot late is reported as a port
error rather than replaying missed ticks in a burst.

`zule_port_idle()` sleeps for up to approximately 1 ms plus host scheduling delay
and reports timer errors. It must be called outside the critical section.
`zule_port_tick_stop()` signals a monotonic-clock condition variable and joins
the thread, so stopping does not wait for the next timer deadline. Repeated
stops are safe, and a stopped timer can be restarted. Lifecycle calls have one
main-loop caller and cannot run from the callback. They require stopped delivery
before initialization; timer creation failures leave no live worker.

Timer errors return `ZULE_PORT_RESULT_ERROR`; synchronization failures or an
unrecoverable join failure abort rather than falsely report exclusion or
quiescence. Neither this development port nor the demo provides hard real-time
guarantees.

Core unit tests compile against a separate, single-threaded test port that
checks critical-section nesting. A POSIX port test separately exercises nested
locking and mutual exclusion across four threads, plus callback delivery,
restart, and shutdown of a timer with a long period.
