# Compile the core against a deterministic test port, independently of the
# selected runtime port's types and threading implementation.
set(ZULE_CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_library(zule_test_port INTERFACE)
target_sources(zule_test_port INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/port/zule_port.c")
target_include_directories(zule_test_port INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/port")
target_link_libraries(zule_test_port INTERFACE zule_port_api)

# Compile the modules layer with each test's own settings.
set(ZULE_MODULES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../modules")
add_library(zule_test_modules INTERFACE)
target_sources(zule_test_modules INTERFACE "${ZULE_MODULES_SOURCE_DIR}/zule_modules.c"
    "${ZULE_MODULES_SOURCE_DIR}/tick_timing/zule_tick_timing.c")

add_executable(zule_core_tests zule_scheduler_test.c
    "${ZULE_CORE_SOURCE_DIR}/zule_scheduler.c" "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")
target_link_libraries(zule_core_tests PRIVATE zule_test_port)

add_executable(zule_schedule_tests zule_schedule_test.c "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")

add_executable(zule_runner_tests zule_test.c "${ZULE_CORE_SOURCE_DIR}/zule.c"
    "${ZULE_CORE_SOURCE_DIR}/zule_scheduler.c" "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")
target_link_libraries(zule_runner_tests PRIVATE zule_test_port)

# Hooks are compiled away in the targets above. This target enables the timebase
# and every user hook, independently of the shared configuration.
add_executable(zule_hook_tests zule_hook_test.c "${ZULE_CORE_SOURCE_DIR}/zule.c"
    "${ZULE_CORE_SOURCE_DIR}/zule_scheduler.c" "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")
target_link_libraries(zule_hook_tests PRIVATE zule_test_port)
target_compile_definitions(zule_hook_tests PRIVATE
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_USER_HOOK_TICK_START=1
    ZULE_CONFIG_USER_HOOK_TICK_END=1
    ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE=1
    ZULE_CONFIG_USER_HOOK_TASK_START=1
    ZULE_CONFIG_USER_HOOK_TASK_END=1)

# The tick timing check, with a 10 us tolerance and the tick start hook, which
# shares the check's time read.
add_executable(zule_tick_check_tests zule_tick_check_test.c "${ZULE_CORE_SOURCE_DIR}/zule.c"
    "${ZULE_CORE_SOURCE_DIR}/zule_scheduler.c" "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")
target_link_libraries(zule_tick_check_tests PRIVATE zule_test_port)
target_compile_definitions(zule_tick_check_tests PRIVATE
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_TICK_CHECK=1
    ZULE_CONFIG_TICK_TOLERANCE_US=10
    ZULE_CONFIG_USER_HOOK_TICK_START=1)

# The tick timing module, alone and with the core tick timing check.
foreach(ZULE_TEST_TARGET IN ITEMS zule_tick_timing_tests zule_tick_timing_check_tests)
    add_executable(${ZULE_TEST_TARGET} zule_tick_timing_test.c "${ZULE_CORE_SOURCE_DIR}/zule.c"
        "${ZULE_CORE_SOURCE_DIR}/zule_scheduler.c" "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")
    target_link_libraries(${ZULE_TEST_TARGET} PRIVATE zule_test_port)
    target_compile_definitions(${ZULE_TEST_TARGET} PRIVATE
        ZULE_CONFIG_TIMEBASE=1 ZULE_CONFIG_MODULE_TICK_TIMING=1)
endforeach()
target_compile_definitions(zule_tick_timing_check_tests PRIVATE
    ZULE_CONFIG_TICK_CHECK=1 ZULE_CONFIG_TICK_TOLERANCE_US=10)

foreach(ZULE_TEST_TARGET IN ITEMS zule_core_tests zule_runner_tests zule_hook_tests
        zule_tick_check_tests zule_tick_timing_tests zule_tick_timing_check_tests)
    target_link_libraries(${ZULE_TEST_TARGET} PRIVATE zule_test_modules)
endforeach()
foreach(ZULE_TEST_TARGET IN ITEMS zule_core_tests zule_schedule_tests zule_runner_tests
        zule_hook_tests zule_tick_check_tests zule_tick_timing_tests zule_tick_timing_check_tests)
    target_link_libraries(${ZULE_TEST_TARGET} PRIVATE zule_schedule_internal zule_warnings)
endforeach()

add_test(NAME zule_core COMMAND zule_core_tests)
add_test(NAME zule_schedule COMMAND zule_schedule_tests)
add_test(NAME zule_runner COMMAND zule_runner_tests)
add_test(NAME zule_hooks COMMAND zule_hook_tests)
add_test(NAME zule_tick_check COMMAND zule_tick_check_tests)
add_test(NAME zule_tick_timing COMMAND zule_tick_timing_tests)
add_test(NAME zule_tick_timing_check COMMAND zule_tick_timing_check_tests)
set_tests_properties(zule_core zule_schedule zule_runner zule_hooks zule_tick_check
    zule_tick_timing zule_tick_timing_check PROPERTIES TIMEOUT 10)
