# Zule's test support for QEMU's mps2-an385 machine, a Cortex-M3; see README.md.
# Include it from a test project built with the Arm GNU toolchain.
set(ZULE_TARGET_QEMU_MPS2_AN385_DIR "${CMAKE_CURRENT_LIST_DIR}")
find_program(ZULE_QEMU_SYSTEM_ARM qemu-system-arm REQUIRED)

# zule_target_qemu_mps2_an385_executable(<executable>)
#
# Add the startup code, semihosting support, and linker script to <executable>,
# which links with no C library.
function(zule_target_qemu_mps2_an385_executable executable)
    target_sources(${executable} PRIVATE
        "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/startup.S" "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/zule_target.c")
    target_include_directories(${executable} PRIVATE "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}")
    target_link_options(${executable} PRIVATE
        -nostdlib "-T${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
    set_property(TARGET ${executable} APPEND PROPERTY
        LINK_DEPENDS "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
    # zule_target.c implements memset() and its relatives, so GCC must not turn
    # their loops, or start-up's, into calls to those same functions.
    set_property(SOURCE "${ZULE_TARGET_QEMU_MPS2_AN385_DIR}/zule_target.c"
        TARGET_DIRECTORY ${executable}
        APPEND PROPERTY COMPILE_OPTIONS -fno-tree-loop-distribute-patterns)
    # libgcc supplies compiler support routines, such as 64-bit division, and
    # comes after the libraries that need them.
    target_link_libraries(${executable} PRIVATE gcc)
endfunction()

# zule_target_qemu_mps2_an385_add_test(<name> <executable>)
#
# Run <executable> in QEMU as the test <name>. -icount makes guest time follow
# executed instructions, so timing in the guest is deterministic whatever the
# host's load; each instruction takes 1 ns of guest time.
function(zule_target_qemu_mps2_an385_add_test name executable)
    add_test(NAME ${name} COMMAND "${ZULE_QEMU_SYSTEM_ARM}"
        -machine mps2-an385 -nographic -monitor none
        -semihosting-config enable=on,target=native
        -icount shift=0
        -kernel "$<TARGET_FILE:${executable}>")
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()
