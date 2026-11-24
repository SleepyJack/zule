/* Memory layout of QEMU's mps2-an385 machine: code and read-only data in the
 * first 4 MiB of SRAM, at address 0 where the vector table must be, and data, the
 * C library's heap, and the stack in the second. The heap starts at `end`, where
 * the C library's sbrk() expects it, and grows up towards the stack. */
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
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        __preinit_array_start = .;
        KEEP(*(.preinit_array))
        __preinit_array_end = .;
        __init_array_start = .;
        KEEP(*(.init_array*))
        __init_array_end = .;
        __fini_array_start = .;
        KEEP(*(.fini_array*))
        __fini_array_end = .;
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

    . = ALIGN(8);
    end = .;

    board_stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
