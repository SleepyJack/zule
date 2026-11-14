# Test a private build of the port sources with the optional timebase enabled,
# independently of the shared configuration.
set(ZULE_POSIX_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(zule_posix_tests zule_port_test.c
    "${ZULE_POSIX_PORT_DIR}/zule_port.c"
    "${ZULE_POSIX_PORT_DIR}/zule_port_tick.c"
    "${ZULE_POSIX_PORT_DIR}/zule_port_time.c")
target_include_directories(zule_posix_tests PRIVATE "${ZULE_POSIX_PORT_DIR}")
target_compile_definitions(zule_posix_tests PRIVATE _POSIX_C_SOURCE=200809L ZULE_CONFIG_TIMEBASE=1)
target_link_libraries(zule_posix_tests PRIVATE zule_port_api Threads::Threads zule_warnings)
add_test(NAME zule_posix COMMAND zule_posix_tests)
set_tests_properties(zule_posix PROPERTIES TIMEOUT 10)
