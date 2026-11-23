# Zule's test support for QEMU's mps2-an385 machine, a Cortex-M3; see README.md.
# Include it from a test project built with the Arm GNU toolchain.
set(ZULE_TARGET_QEMU_MPS2_AN385_DIR "${CMAKE_CURRENT_LIST_DIR}")
find_program(ZULE_QEMU_SYSTEM_ARM qemu-system-arm REQUIRED)

# zule_target_qemu_mps2_an385_executable(<executable>)
#
# Add the startup code, semihosting support, and linker script to <executable>,
# which links with newlib-nano, the Arm GNU toolchain's small C library, and
# provides the system calls it needs.
function(zule_target_qemu_mps2_an385_executable executable)
    target_sources(${executable} PRIVATE
        "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/startup.S" "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/zule_target.c")
    target_include_directories(${executable} PRIVATE "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}")
    target_link_options(${executable} PRIVATE
        -nostartfiles --specs=nano.specs
        "-T${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
    set_property(TARGET ${executable} APPEND PROPERTY
        LINK_DEPENDS "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
endfunction()

# zule_target_qemu_mps2_an385_add_test(<name> <executable>)
#
# Run <executable> in QEMU as the test <name>. -icount makes guest time follow
# executed instructions, each taking 1 ns, and sleep=off makes it jump to the
# next timer event while the guest waits in WFI, rather than follow the host's
# clock. Together they make every run the same, whatever the host's load.
function(zule_target_qemu_mps2_an385_add_test name executable)
    add_test(NAME ${name} COMMAND "${ZULE_QEMU_SYSTEM_ARM}"
        -machine mps2-an385 -nographic -monitor none
        -semihosting-config enable=on,target=native
        -icount shift=0,sleep=off
        -kernel "$<TARGET_FILE:${executable}>")
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()
