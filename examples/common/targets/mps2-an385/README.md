# mps2-an385 board support

Code shared by the examples' `mps2-an385` targets: QEMU's emulation of Arm's
MPS2 board with its AN385 FPGA image, a Cortex-M3 at 25 MHz. The real board is a
prototyping system for chip designers, so these targets run only in QEMU.

It provides building blocks and knows nothing about the examples:

- [`startup.S`](startup.S): the vector table and reset entry, with weak exception
  handlers. Zule's Cortex-M port defines `SysTick_Handler`.
- [`board.c`](board.c): start-up, which runs `main()` and ends QEMU with its
  result through semihosting, and the memory functions GCC may call.
- [`mps2-an385.ld`](mps2-an385.ld): the linker script.
- [`board.cmake`](board.cmake): `example_mps2_an385_board()` adds all of the above
  to an example's executable.

Semihosting only works under a debugger or emulator, so on a real board the
program would stop at its first semihosting call. A real board's target would
replace this support with its vendor's start-up code.
