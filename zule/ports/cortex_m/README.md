# Cortex-M port

A generic port for Arm Cortex-M microcontrollers. It uses only what the Arm
architecture defines, with no vendor or CMSIS code:

- **Critical sections** mask interrupts with PRIMASK, saving and restoring its
  previous value, so they nest and preserve masking the caller already had.
- **The tick** is SysTick, programmed to interrupt once per slot. The port
  defines `SysTick_Handler`, the name Cortex-M startup code's vector tables use.
- **Idle** executes `WFI` inside the core's critical section. A pending tick ends
  the wait even while masked, and its handler runs once the core leaves the
  critical section, so no tick is missed.

It is written for Armv6-M, whose instructions Armv7-M and Armv8-M also provide,
so it serves Cortex-M0, M0+, M3, M4, M7, M23, M33, and later cores. It assumes a
single core, where masking interrupts excludes the tick, and a SysTick timer,
which is optional on Armv6-M and Armv8-M Baseline, though almost every part has
one. It does not provide the
[timebase](../../../doc/port.md#timebase) yet, so the timebase, hooks, the tick
timing check, and modules cannot be enabled with it.

Select it with `ZULE_PORT=cortex_m`. Build with the Arm GNU toolchain, for
example through [`arm-none-eabi.cmake`](arm-none-eabi.cmake), with flags for the
core, such as `-mcpu=cortex-m4 -mthumb`. The port includes a small assembly
file, for which its `CMakeLists.txt` enables CMake's ASM language; other build
systems assemble it with the same compiler.

## Settings

Set these in `zule_user_port_config.h`; see
[port settings](../../../doc/configuration.md#port-settings).

| Setting | Default | Meaning |
| --- | --- | --- |
| `ZULE_PORT_CORTEX_M_SYSTICK_HZ` | None, required | The frequency of SysTick's clock, in Hz, from 1 to 2^32 - 1. |
| `ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK` | `0` | `1` to clock SysTick from the external reference clock instead of the processor clock. The implementation defines the reference clock, such as the processor clock divided by 8 on many parts. Set `ZULE_PORT_CORTEX_M_SYSTICK_HZ` to match. |

Tick init fails, and `zule_run()` returns `ZULE_RESULT_PORT_ERROR`, unless a
slot is a whole number of SysTick clock counts, so ticks do not drift, and from
2 to 2^24 counts, SysTick's 24-bit range. With the reference clock selected, it
also fails on a part that has none, which SysTick's calibration register
reports, rather than leaving ticks never arriving. For example, at 170 MHz the longest
slot is about 98 ms; from the reference clock at 21.25 MHz, it is about 789 ms.

## Application responsibilities

- **SysTick and its interrupt belong to Zule.** Do not use SysTick for anything
  else. Vendor libraries that use it, such as STM32 HAL's `HAL_Delay()`, must be
  moved to another timer, as is usual with an RTOS. If an application's startup
  code needs a different handler name, it can copy the port and use it as an
  [custom port](../../../doc/port.md#linking-a-custom-port).
- **Interrupt priorities are the application's.** The port leaves SysTick's
  priority unchanged. A higher priority shortens tick latency. Critical sections
  mask every interrupt, not only the tick, for their short duration.
- **The clock must be running** at the configured frequency before `zule_run()`.

## Implementation

- [`zule_port.c`](zule_port.c): the port contract.
- [`zule_port_cortex_m.h`](zule_port_cortex_m.h): internal register definitions,
  named after the Arm Architecture Reference Manuals, and the assembly
  functions.
- [`zule_port_cortex_m.S`](zule_port_cortex_m.S): the instructions C cannot
  express (`mrs`, `cpsid`, `msr`, `wfi`, and barriers), and the registers'
  addresses, so the C has no casts from integers to pointers. Each call to it is
  opaque to the compiler, so it cannot move accesses to data the tick shares
  across a critical section's entry or exit.

Ticks stop with SysTick disabled and any pending SysTick exception cleared. On a
single core, stop runs in application context, which a tick in progress would
have preempted until it finished, so no callback is in flight when it returns.

## Tests

- [`tests/host/`](tests/host) checks the port's C on the host, against fake
  registers and instructions: critical-section nesting, reload values and slot
  limits at two clock frequencies and both clock sources, tick start and stop,
  and idle.
- [`tests/qemu/`](tests/qemu) runs the port on a Cortex-M3 in QEMU: the tick
  rate against an independent timer, critical sections holding ticks off, idle,
  and stop, then the real scheduler.
- The [static checks](../../../TESTING.md#static-checks) build it for Cortex-M0+
  and Cortex-M3, with static analysis.

See [TESTING.md](../../../TESTING.md) for how to run them. QEMU does not show
real timing or interrupt latency, which need hardware.
