/* Memory layout of QEMU's mps2-an385 machine: code and read-only data in the
 * first 4 MiB of SRAM, at address 0 where the vector table must be, and data and
 * the stack in the second. There is no heap. */
MEMORY
{
    CODE (rx) : ORIGIN = 0x00000000, LENGTH = 4M
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        *(.ARM.exidx*)
    } > CODE

    .data :
    {
        board_data_start = .;
        *(.data*)
        board_data_end = .;
    } > RAM AT > CODE
    board_data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        board_bss_start = .;
        *(.bss*)
        *(COMMON)
        board_bss_end = .;
    } > RAM

    board_stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
