# mps2-an385 target: QEMU's emulation of Arm's MPS2 board with a Cortex-M3,
# through Zule's Cortex-M port. The port settings in this directory give the
# board's clock.
set(ZULE_PORT cortex_m CACHE STRING "Zule port")
set(ZULE_USER_PORT_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../../../common/targets/mps2-an385/board.cmake")

function(example_target_configure executable)
    example_mps2_an385_board(${executable})
endfunction()
