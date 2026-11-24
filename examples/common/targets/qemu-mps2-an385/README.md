# qemu-mps2-an385 board support

Code shared by the examples' `qemu-mps2-an385` targets: QEMU's `mps2-an385`
machine, which emulates Arm's MPS2 board with its AN385 FPGA image, a Cortex-M3
at 25 MHz. The real board is a prototyping system for chip designers, so these
targets run only in QEMU, as their name says.

It provides building blocks and knows nothing about the examples:

- [`startup.S`](startup.S): the vector table and reset entry, with weak exception
  handlers. Zule's Cortex-M port defines `SysTick_Handler`.
- [`board.c`](board.c): start-up, which initialises memory and the standard
  streams, then exits with `main()`'s result.
- [`qemu-mps2-an385.ld`](qemu-mps2-an385.ld): the linker script.
- [`board.cmake`](board.cmake): `example_qemu_mps2_an385_board()` adds all of
  the above to an example's executable, which links with newlib-nano, the Arm
  GNU toolchain's small C library, and rdimon, its system calls over
  semihosting. QEMU then performs the program's input and output on the host:
  `printf()` appears on QEMU's standard output, and the program's exit status
  becomes QEMU's.

Semihosting only works under a debugger or emulator, so on a real board the
program would stop at its first semihosting call. A real board's target would
replace this support with its vendor's start-up code.
