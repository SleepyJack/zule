/* Memory layout of QEMU's mps2-an385 machine for Zule's target tests: code and
 * read-only data in the first 4 MiB of SRAM, at address 0 where the vector table
 * must be, and data and the stack in the second. There is no heap. */
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
        zule_target_data_start = .;
        *(.data*)
        zule_target_data_end = .;
    } > RAM AT > CODE
    zule_target_data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        zule_target_bss_start = .;
        *(.bss*)
        *(COMMON)
        zule_target_bss_end = .;
    } > RAM

    zule_target_stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
