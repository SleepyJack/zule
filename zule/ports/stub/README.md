# Stub port

A compile-check port: it implements every function in the
[port contract](../../../doc/port.md) without doing any work, so the Zule
library can be compiled and linked for a target before a real port exists. It
delivers no ticks, so a scheduler using it never dispatches a task. It is not
for applications.

Select it with `ZULE_PORT=stub`. It has no
[port settings](../../../doc/configuration.md#port-settings) and provides the
optional timebase, at a fixed 1 MHz with a constant count, so builds with the
timebase enabled also link.

The [cross-compiled builds](../../../CONTRIBUTING.md#cross-compiled-builds) use
it to check the Zule library with the target compiler: freestanding, with no C
library, and with strict warnings as errors.
