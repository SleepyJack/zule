add_executable(zule_posix_tests zule_port_test.c)
target_link_libraries(zule_posix_tests PRIVATE zule::port zule_warnings)
add_test(NAME zule_posix COMMAND zule_posix_tests)
set_tests_properties(zule_posix PROPERTIES TIMEOUT 10)
