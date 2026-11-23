# qemu-mps2-an385 test target

Zule's support for running [target tests](../../../../TESTING.md#target-tests)
on QEMU's `mps2-an385` machine: Arm's MPS2 board with its AN385 FPGA image, a
Cortex-M3 at 25 MHz with 4 MiB of code memory at address 0 and 4 MiB of RAM at
`0x20000000`. The real board is a prototyping system for chip designers; Zule
uses it only in QEMU.

- [`startup.S`](startup.S): the vector table and reset entry. Every exception
  handler is weak, so a port or test defines the ones it uses, such as
  `SysTick_Handler`; the others fail the test.
- [`zule_target.c`](zule_target.c): start-up, which initialises memory and the
  standard streams, then exits with `main()`'s result. `main()` returns 0 for
  success, and a failed assertion, a call to `abort()`, or an unexpected
  exception fails the test.
- [`qemu-mps2-an385.ld`](qemu-mps2-an385.ld): the linker script.
- [`target.cmake`](target.cmake): `zule_target_qemu_mps2_an385_executable()`
  adds the above to a test program, linked with newlib-nano, the Arm GNU
  toolchain's small C library, and rdimon, its system calls over semihosting,
  and `zule_target_qemu_mps2_an385_add_test()`
  runs it in QEMU with `-icount shift=0,sleep=off`, so every run is the same;
  see [target tests](../../../../TESTING.md#target-tests).

Through rdimon, QEMU performs the C library's input and output on the host, so a
test's `printf()` and `fprintf(stderr, ...)` appear on QEMU's standard output and
error, and its exit status becomes QEMU's.

Tests can use the machine's peripherals directly, such as its CMSDK APB timers at
`0x40000000`, which count at 25 MHz, as an independent reference clock. SysTick's
reference clock is 1 MHz, and `SYST_CALIB` reads 9999, with no `NOREF`.

This is Zule's own test support. The examples have their own board code,
since they are user-domain code meant to be copied.
