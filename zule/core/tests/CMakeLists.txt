# Compile the core against a deterministic test port, independently of the
# selected runtime port's types and threading implementation.
set(ZULE_TEST_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port")

# Build the core sources from zule/core_sources.txt, with the test port, into a
# static library for one test configuration. Tests link only the objects they
# use, so every test can link a complete library. The definitions are public, so
# a test compiles with the same settings as its library.
function(zule_add_test_core name)
    add_library(${name} STATIC ${ZULE_CORE_SOURCES} "${ZULE_TEST_PORT_DIR}/zule_port.c")
    target_include_directories(${name} PUBLIC "${ZULE_TEST_PORT_DIR}")
    target_compile_definitions(${name} PUBLIC ${ARGN})
    target_link_libraries(${name} PUBLIC zule_port_api zule_schedule_internal PRIVATE zule_warnings)
endfunction()

# Add a test executable linked to a test configuration's core library.
function(zule_add_core_test name source core)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${core} zule_warnings)
endfunction()

# Default settings: hooks, the timebase, and checks are compiled away.
zule_add_test_core(zule_test_core_default)
zule_add_core_test(zule_core_tests zule_scheduler_test.c zule_test_core_default)
zule_add_core_test(zule_schedule_tests zule_schedule_test.c zule_test_core_default)
zule_add_core_test(zule_runner_tests zule_test.c zule_test_core_default)

# The timebase and every user hook.
zule_add_test_core(zule_test_core_hooks
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_USER_HOOK_TICK_START=1
    ZULE_CONFIG_USER_HOOK_TICK_END=1
    ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE=1
    ZULE_CONFIG_USER_HOOK_TASK_START=1
    ZULE_CONFIG_USER_HOOK_TASK_END=1)
zule_add_core_test(zule_hook_tests zule_hook_test.c zule_test_core_hooks)

# The tick timing check, with a 10 us tolerance and the tick start hook, which
# shares the check's time read.
zule_add_test_core(zule_test_core_tick_check
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_TICK_CHECK=1
    ZULE_CONFIG_TICK_TOLERANCE_US=10
    ZULE_CONFIG_USER_HOOK_TICK_START=1)
zule_add_core_test(zule_tick_check_tests zule_tick_check_test.c zule_test_core_tick_check)

# The tick timing module, alone and with the core tick timing check.
zule_add_test_core(zule_test_core_tick_timing
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TICK_TIMING=1)
zule_add_core_test(zule_tick_timing_tests zule_tick_timing_test.c zule_test_core_tick_timing)
zule_add_test_core(zule_test_core_tick_timing_check
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TICK_TIMING=1
    ZULE_CONFIG_TICK_CHECK=1
    ZULE_CONFIG_TICK_TOLERANCE_US=10)
zule_add_core_test(zule_tick_timing_check_tests zule_tick_timing_test.c
    zule_test_core_tick_timing_check)

add_test(NAME zule_core COMMAND zule_core_tests)
add_test(NAME zule_schedule COMMAND zule_schedule_tests)
add_test(NAME zule_runner COMMAND zule_runner_tests)
add_test(NAME zule_hooks COMMAND zule_hook_tests)
add_test(NAME zule_tick_check COMMAND zule_tick_check_tests)
add_test(NAME zule_tick_timing COMMAND zule_tick_timing_tests)
add_test(NAME zule_tick_timing_check COMMAND zule_tick_timing_check_tests)
set_tests_properties(zule_core zule_schedule zule_runner zule_hooks zule_tick_check
    zule_tick_timing zule_tick_timing_check PROPERTIES TIMEOUT 10)
