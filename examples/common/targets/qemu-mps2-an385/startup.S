/* Vector table and reset entry for QEMU's mps2-an385 machine, a Cortex-M3. Only
 * the system exceptions are listed. Every handler is weak, so the application
 * or a library defines the ones it uses; Zule's Cortex-M port defines
 * SysTick_Handler. */
    .syntax unified
    .arch armv7-m
    .thumb

    .section .vectors, "a"
    .word board_stack_top
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
    bl board_start
    b .
    .size Reset_Handler, . - Reset_Handler

    .type board_default_handler, %function
    .thumb_func
board_default_handler:
    bl board_unexpected_exception
    b .
    .size board_default_handler, . - board_default_handler

    .macro weak_handler name
    .weak \name
    .thumb_set \name, board_default_handler
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

/* uint32_t board_semihosting(uint32_t operation, uintptr_t argument) */
    .global board_semihosting
    .type board_semihosting, %function
    .thumb_func
board_semihosting:
    bkpt 0xab
    bx lr
    .size board_semihosting, . - board_semihosting
