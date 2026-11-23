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
    add_executable(${name}_tests "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${source}")
    target_link_libraries(${name}_tests PRIVATE ${config} zule_warnings)
    add_test(NAME ${name} COMMAND ${name}_tests)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

include(tests.cmake)
