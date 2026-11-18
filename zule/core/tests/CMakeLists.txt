# Compile the core against a deterministic test port, independently of the
# selected runtime port's types and threading implementation.
set(ZULE_TEST_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port")

# zule_add_test_config(<config> [<definition>...])
#
# Build Zule's sources from zule/zule_sources.txt, with the test port, into a
# static library for one test configuration's settings. Tests link only the
# objects they use, so every test can link a complete library. The definitions
# are public, so a test compiles with the same settings as its configuration.
function(zule_add_test_config name)
    add_library(${name} STATIC ${ZULE_SOURCES} "${ZULE_TEST_PORT_DIR}/zule_port.c")
    target_include_directories(${name} PUBLIC "${ZULE_TEST_PORT_DIR}")
    target_compile_definitions(${name} PUBLIC ${ARGN})
    target_link_libraries(${name} PUBLIC zule_port_api zule_schedule_internal PRIVATE zule_warnings)
endfunction()

# zule_add_test(<name> <source> <config>)
#
# Add the test <name>, run by the executable <name>_tests built from <source> and
# linked to a test configuration from zule_add_test_config().
function(zule_add_test name source config)
    add_executable(${name}_tests ${source})
    target_link_libraries(${name}_tests PRIVATE ${config} zule_warnings)
    add_test(NAME ${name} COMMAND ${name}_tests)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

# Default settings: hooks, the timebase, and checks are compiled away.
zule_add_test_config(zule_test_config_default)
zule_add_test(zule_core zule_scheduler_test.c zule_test_config_default)
zule_add_test(zule_schedule zule_schedule_test.c zule_test_config_default)
zule_add_test(zule_runner zule_test.c zule_test_config_default)

# The timebase and every user hook.
zule_add_test_config(zule_test_config_hooks
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_USER_HOOK_TICK_START=1
    ZULE_CONFIG_USER_HOOK_TICK_END=1
    ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE=1
    ZULE_CONFIG_USER_HOOK_TASK_START=1
    ZULE_CONFIG_USER_HOOK_TASK_END=1)
zule_add_test(zule_hooks zule_hook_test.c zule_test_config_hooks)

# The tick timing check, with a 10 us tolerance and the tick start hook, which
# shares the check's time read.
zule_add_test_config(zule_test_config_tick_check
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_TICK_CHECK=1
    ZULE_CONFIG_TICK_TOLERANCE_US=10
    ZULE_CONFIG_USER_HOOK_TICK_START=1)
zule_add_test(zule_tick_check zule_tick_check_test.c zule_test_config_tick_check)

# The tick timing module, alone and with the core tick timing check.
zule_add_test_config(zule_test_config_tick_timing
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TICK_TIMING=1)
zule_add_test(zule_tick_timing zule_tick_timing_test.c zule_test_config_tick_timing)
zule_add_test_config(zule_test_config_tick_timing_check
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TICK_TIMING=1
    ZULE_CONFIG_TICK_CHECK=1
    ZULE_CONFIG_TICK_TOLERANCE_US=10)
zule_add_test(zule_tick_timing_check zule_tick_timing_test.c
    zule_test_config_tick_timing_check)
