set(ZULE_CORE_SOURCES src/zule.c src/zule_scheduler.c src/zule_schedule.c)
add_library(zule_core STATIC ${ZULE_CORE_SOURCES})
add_library(zule::core ALIAS zule_core)

target_include_directories(zule_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/api")
target_include_directories(zule_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/inc" "${CMAKE_CURRENT_SOURCE_DIR}/../ports")
target_link_libraries(zule_core PRIVATE zule::port)
target_compile_features(zule_core PUBLIC c_std_99)
set_target_properties(zule_core PROPERTIES C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zule_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ZULE_BUILD_TESTS)
    # Compile the core against a deterministic test port, independently of the
    # selected runtime port's types and threading implementation.
    add_executable(zule_core_tests tests/zule_scheduler_test.c src/zule_scheduler.c src/zule_schedule.c
        tests/port/zule_port.c)
    target_include_directories(zule_core_tests PRIVATE tests/port "${CMAKE_CURRENT_SOURCE_DIR}/../ports")
    add_executable(zule_schedule_tests tests/zule_schedule_test.c src/zule_schedule.c)
    add_executable(zule_runner_tests tests/zule_test.c ${ZULE_CORE_SOURCES} tests/port/zule_port.c)
    target_include_directories(zule_runner_tests PRIVATE tests/port "${CMAKE_CURRENT_SOURCE_DIR}/../ports")
    foreach(ZULE_TEST_TARGET IN ITEMS zule_core_tests zule_schedule_tests zule_runner_tests)
        target_include_directories(${ZULE_TEST_TARGET} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/api" "${CMAKE_CURRENT_SOURCE_DIR}/inc")
        target_compile_features(${ZULE_TEST_TARGET} PRIVATE c_std_99)
        set_target_properties(${ZULE_TEST_TARGET} PROPERTIES C_EXTENSIONS OFF)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${ZULE_TEST_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
    add_test(NAME zule_core COMMAND zule_core_tests)
    add_test(NAME zule_schedule COMMAND zule_schedule_tests)
    add_test(NAME zule_runner COMMAND zule_runner_tests)
    set_tests_properties(zule_core zule_schedule zule_runner PROPERTIES TIMEOUT 10)
endif()
