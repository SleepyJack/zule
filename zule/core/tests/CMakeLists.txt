# Compile the core against a deterministic test port, independently of the
# selected runtime port's types and threading implementation.
set(ZULE_CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_library(zule_test_port INTERFACE)
target_sources(zule_test_port INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/port/zule_port.c")
target_include_directories(zule_test_port INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/port")
target_link_libraries(zule_test_port INTERFACE zule_port_api)

add_executable(zule_core_tests zule_scheduler_test.c
    "${ZULE_CORE_SOURCE_DIR}/zule_scheduler.c" "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")
target_link_libraries(zule_core_tests PRIVATE zule_test_port)

add_executable(zule_schedule_tests zule_schedule_test.c "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")

add_executable(zule_runner_tests zule_test.c "${ZULE_CORE_SOURCE_DIR}/zule.c"
    "${ZULE_CORE_SOURCE_DIR}/zule_scheduler.c" "${ZULE_CORE_SOURCE_DIR}/zule_schedule.c")
target_link_libraries(zule_runner_tests PRIVATE zule_test_port)

foreach(ZULE_TEST_TARGET IN ITEMS zule_core_tests zule_schedule_tests zule_runner_tests)
    target_link_libraries(${ZULE_TEST_TARGET} PRIVATE zule_schedule_internal zule_warnings)
endforeach()

add_test(NAME zule_core COMMAND zule_core_tests)
add_test(NAME zule_schedule COMMAND zule_schedule_tests)
add_test(NAME zule_runner COMMAND zule_runner_tests)
set_tests_properties(zule_core zule_schedule zule_runner PROPERTIES TIMEOUT 10)
