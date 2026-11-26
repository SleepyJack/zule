# POSIX development port

This port implements the [port contract](../../../doc/port.md) for native
development on POSIX hosts.

Select this port with `ZULE_PORT=posix`. It has no
[port settings](../../../doc/configuration.md#port-settings) yet and provides
the optional timebase.

This is a native development port: locking can block and has no bounded-latency
guarantee. It is not usable from a POSIX signal handler. Unlike embedded ports,
it does not mask interrupts.

`zule_port_tick_init()` starts a timer thread with the supplied period and
callback.
Deadlines stay on a fixed grid, so after a host delay the
overdue ticks arrive back to back. The port does not judge tick timing; enable
the core's [tick timing check](../../../doc/core.md#tick-timing-check) to detect
late ticks. Without it, compressed slots usually surface as missed-dispatch or
overrun faults.

When `ZULE_CONFIG_TIMEBASE` is `1`, the
[timebase](../../../doc/port.md#timebase) reads `CLOCK_MONOTONIC` in
microseconds, so `zule_port_time_hz()` returns 1 000 000 and the count wraps
about every 71 minutes. It is valid at any time, not only while ticks run,
and advances during sleeps and critical sections. Reads use the host clock and
have no bounded-latency guarantee.

This development port provides no hard real-time guarantees, and neither do
the examples built with it.

The tests in `tests/` cover critical-section nesting and exclusion, tick
delivery, restart and shutdown, idle waiting, and the timebase. They build their
own copy of the port with the timebase enabled. See [TESTING.md](../../../TESTING.md)
to run them.
