# mps2-an385 test target

Zule's support for running [target tests](../../../../TESTING.md#target-tests)
on QEMU's `mps2-an385` machine: Arm's MPS2 board with its AN385 FPGA image, a
Cortex-M3 at 25 MHz with 4 MiB of code memory at address 0 and 4 MiB of RAM at
`0x20000000`. The real board is a prototyping system for chip designers; Zule
uses it only in QEMU.

- [`startup.S`](startup.S): the vector table and reset entry. Every exception
  handler is weak, so a port or test defines the ones it uses, such as
  `SysTick_Handler`; the others fail the test.
- [`zule_target.h`](zule_target.h) and [`zule_target.c`](zule_target.c): output
  and exit through semihosting, startup initialisation, and the memory functions
  GCC may call. `main()` returns 0 for success.
- [`mps2-an385.ld`](mps2-an385.ld): the linker script.
- [`target.cmake`](target.cmake): `zule_target_mps2_an385_executable()` adds the
  above to a test program, and `zule_target_mps2_an385_add_test()` runs it in QEMU
  with `-icount`, so guest time follows executed instructions.

Tests can use the machine's peripherals directly, such as its CMSDK APB timers at
`0x40000000`, which count at 25 MHz, as an independent reference clock. SysTick's
reference clock is 1 MHz, and `SYST_CALIB` reads 9999, with no `NOREF`.

This is Zule's own test support. The examples have their own board code,
since they are user-domain code meant to be copied.
