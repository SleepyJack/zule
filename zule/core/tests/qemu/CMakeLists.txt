cmake_minimum_required(VERSION 3.31)
project(zule_core_qemu_tests LANGUAGES C ASM)

# The core tests on a target, in QEMU; see TESTING.md. A standalone project, since
# it builds with the target's toolchain. It runs the same tests in the same
# configurations as the host build, listed once in ../tests.cmake, against the
# same deterministic test port, so it shows the core behaves the same with the
# target's compiler, type sizes, and arithmetic.

# Zule's targets and settings. The core tests use their own test port, so the
# selected port only satisfies Zule's build; it is never linked into a test.
set(ZULE_PORT stub CACHE STRING "Zule port")
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
add_subdirectory(../../.. zule)
include(../../../tests/targets/qemu-mps2-an385/target.cmake)
get_target_property(zule_core_sources zule SOURCES)
set(ZULE_TEST_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../port")

enable_testing()

# zule_add_test_config(<config> [<definition>...])
#
# As in the host build: Zule's sources and the test port, built with one test
# configuration's settings.
function(zule_add_test_config name)
    add_library(${name} STATIC ${zule_core_sources} "${ZULE_TEST_PORT_DIR}/zule_port.c")
    target_include_directories(${name} PUBLIC "${ZULE_TEST_PORT_DIR}")
    target_compile_definitions(${name} PUBLIC ${ARGN})
    target_link_libraries(${name} PUBLIC zule_port_api zule_schedule_internal PRIVATE zule_warnings)
endfunction()

# zule_add_test(<name> <source> <config>)
#
# As in the host build, but each test is a program for the target, run in QEMU.
function(zule_add_test name source config)
    add_executable(${name}_tests "${CMAKE_CURRENT_SOURCE_DIR}/../${source}")
    target_link_libraries(${name}_tests PRIVATE ${config} zule_warnings)
    zule_target_qemu_mps2_an385_executable(${name}_tests)
    zule_target_qemu_mps2_an385_add_test(${name} ${name}_tests)
endfunction()

include(../tests.cmake)
