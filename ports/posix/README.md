# POSIX development port

Select this port with `-DZULE_PORT=posix`. It uses a lazily initialized recursive
pthread mutex. Each enter locks once and each exit unlocks once, preserving
outer critical sections. The saved token records the owning thread; restore it
on that thread, in reverse entry order. Initialization or mutex errors abort,
since the port API has no error return and cannot safely proceed without exclusion.

This is a native development port: locking can block and has no bounded-latency
guarantee. It is not usable from a POSIX signal handler. Unlike embedded ports,
it does not mask interrupts. Every native tick thread must enter the same
critical section around `zule_tick()`; `zule_run()` protects its own transitions.
The mutex provides inter-thread exclusion and memory synchronization, including
when the host schedules threads on different CPUs. Tick calls must still be
serialized, and initialization must finish before either context starts.

The demo owns its timer thread and uses absolute `CLOCK_MONOTONIC` deadlines.
It stops on a scheduler fault or when tick delivery is at least one slot late,
rather than replaying missed ticks in a burst. It joins the timer before exit.
Neither the port nor the demo provides hard real-time guarantees.

Core unit tests compile against a separate, single-threaded test port that
checks critical-section nesting. A POSIX port test separately exercises nested
locking and mutual exclusion across four threads without timing assertions.
