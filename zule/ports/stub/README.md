# Stub port

A compile-check port for any target. It implements every function in the
[port contract](../../../doc/port.md) without doing any work, so the Zule
library can be compiled and linked with a target's compiler, even for a target
that has no port. It delivers no ticks, so a scheduler using it never dispatches
a task. It is not for applications.

Select it with `ZULE_PORT=stub`. It has no
[port settings](../../../doc/configuration.md#port-settings) and provides the
optional timebase, at a fixed 1 MHz with a constant count, so builds with the
timebase enabled also link.

The [cross-compiled builds](../../../CONTRIBUTING.md#cross-compiled-builds) use
it to check the Zule library alone, so a finding cannot come from port code:
freestanding, with no C library, with strict warnings as errors, and with static
analysis, including MISRA C:2012 checks. They build it for Cortex-M cores with
the [Cortex-M port's toolchain file](../cortex_m/arm-none-eabi.cmake), but it
works with any toolchain. It stays alongside real ports, which check their own
code.
