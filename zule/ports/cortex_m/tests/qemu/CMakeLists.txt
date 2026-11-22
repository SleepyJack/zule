cmake_minimum_required(VERSION 3.31)
project(zule_cortex_m_qemu_tests LANGUAGES C ASM)

# Target tests of the Cortex-M port, run in QEMU; see TESTING.md. A standalone
# project, since it builds with the Arm GNU toolchain. The test program drives
# the port directly, then runs the real scheduler on it.
set(ZULE_PORT cortex_m CACHE STRING "Zule port")
set(ZULE_USER_PORT_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port_config")
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
string(APPEND CMAKE_C_FLAGS " -ffreestanding")
add_subdirectory(../../../.. zule)
include(../../../../tests/targets/qemu-mps2-an385/target.cmake)

enable_testing()
add_executable(zule_cortex_m_qemu_tests zule_port_cortex_m_qemu_test.c)
zule_add_schedule(zule_cortex_m_qemu_tests SCHEDULE schedule.json)
target_link_libraries(zule_cortex_m_qemu_tests PRIVATE zule::zule zule::port zule_warnings)
zule_target_qemu_mps2_an385_executable(zule_cortex_m_qemu_tests)
zule_target_qemu_mps2_an385_add_test(zule_cortex_m_qemu zule_cortex_m_qemu_tests)
