# Board support for QEMU's mps2-an385 machine, shared by the examples' qemu-mps2-an385
# targets; see README.md. Include it from a target's target.cmake.
enable_language(ASM)
set(EXAMPLE_QEMU_MPS2_AN385_DIR "${CMAKE_CURRENT_LIST_DIR}")

# example_qemu_mps2_an385_board(<executable>)
#
# Add the startup code, board support, and linker script to <executable>, which
# links with no C library.
function(example_qemu_mps2_an385_board executable)
    target_sources(${executable} PRIVATE
        "${EXAMPLE_QEMU_MPS2_AN385_DIR}/startup.S" "${EXAMPLE_QEMU_MPS2_AN385_DIR}/board.c")
    target_link_options(${executable} PRIVATE
        -nostdlib "-T${EXAMPLE_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
    set_property(TARGET ${executable} APPEND PROPERTY
        LINK_DEPENDS "${EXAMPLE_QEMU_MPS2_AN385_DIR}/qemu-mps2-an385.ld")
    # board.c implements memset() and its relatives, so GCC must not turn their
    # loops, or start-up's, into calls to those same functions.
    set_property(SOURCE "${EXAMPLE_QEMU_MPS2_AN385_DIR}/board.c" TARGET_DIRECTORY ${executable}
        APPEND PROPERTY COMPILE_OPTIONS -fno-tree-loop-distribute-patterns)
    # libgcc supplies compiler support routines, such as 64-bit division, so it
    # comes after the libraries that need them, including the port.
    target_link_libraries(${executable} PRIVATE zule::port gcc)
endfunction()
