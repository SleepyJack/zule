/* Vector table and reset entry for QEMU's mps2-an385 machine, a Cortex-M3. Only
 * the system exceptions are listed, since the tests use no peripheral
 * interrupts. Every handler is weak, so a port or test defines the ones it uses,
 * such as SysTick_Handler; the rest report an unexpected exception and fail. */
    .syntax unified
    .arch armv7-m
    .thumb

    .section .vectors, "a"
    .word zule_target_stack_top
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

    .text

    .global Reset_Handler
    .type Reset_Handler, %function
    .thumb_func
Reset_Handler:
    bl zule_target_start
    b .
    .size Reset_Handler, . - Reset_Handler

    .type zule_target_default_handler, %function
    .thumb_func
zule_target_default_handler:
    bl zule_target_unexpected_exception
    b .
    .size zule_target_default_handler, . - zule_target_default_handler

    .macro weak_handler name
    .weak \name
    .thumb_set \name, zule_target_default_handler
    .endm

    weak_handler NMI_Handler
    weak_handler HardFault_Handler
    weak_handler MemManage_Handler
    weak_handler BusFault_Handler
    weak_handler UsageFault_Handler
    weak_handler SVC_Handler
    weak_handler DebugMon_Handler
    weak_handler PendSV_Handler
    weak_handler SysTick_Handler
