# Board support for QEMU's mps2-an385 machine, shared by the examples' qemu-mps2-an385
# targets; see README.md. Include it from a target's target.cmake.
enable_language(ASM)
set(EXAMPLE_QEMU_MPS2_AN385_DIR "${CMAKE_CURRENT_LIST_DIR}")

# example_qemu_mps2_an385_board(<executable>)
#
# Add the startup code, board support, and linker script to <executable>, which
# links with newlib-nano, the Arm GNU toolchain's small C library.
function(example_qemu_mps2_an385_board executable)
    target_sources(${executable} PRIVATE
        "${EXAMPLE_QEMU_MPS2_AN385_DIR}/startup.S" "${EXAMPLE_QEMU_MPS2_AN385_DIR}/board.c")
    target_link_options(${executable} PRIVATE
        -nostartfiles --specs=nano.specs "-T${EXAMPLE_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
    set_property(TARGET ${executable} APPEND PROPERTY
        LINK_DEPENDS "${EXAMPLE_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
endfunction()
