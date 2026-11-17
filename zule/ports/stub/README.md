# Stub port

An Arm Cortex-M compile-check port. It implements every function in the
[port contract](../../../doc/port.md) without doing any work, so the Zule
library can be compiled and linked with the target compiler before a real
Cortex-M port exists. It delivers no ticks, so a scheduler using it never
dispatches a task. It is not for applications.

Select it with `ZULE_PORT=stub`. It has no
[port settings](../../../doc/configuration.md#port-settings) and provides the
optional timebase, at a fixed 1 MHz with a constant count, so builds with the
timebase enabled also link.

[`arm-none-eabi.cmake`](arm-none-eabi.cmake) is the toolchain file for its
target, using the Arm GNU toolchain. The
[cross-compiled builds](../../../CONTRIBUTING.md#cross-compiled-builds) use the
port and toolchain file to check the Zule library: freestanding, with no C
library, with strict warnings as errors, and with static analysis, including
MISRA C:2012 checks. The stub
stays alongside real ports: it checks the Zule library alone, and targets that
have no port.
