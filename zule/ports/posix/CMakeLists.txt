if(NOT UNIX)
    message(FATAL_ERROR "The POSIX port requires a POSIX host")
endif()

find_package(Threads REQUIRED)
# The port's sources, listed once for the library and its tests.
set(ZULE_POSIX_PORT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/zule_port.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/zule_port_tick.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/zule_port_time.c")
add_library(zule_port_posix STATIC ${ZULE_POSIX_PORT_SOURCES})
add_library(zule::port ALIAS zule_port_posix)
target_include_directories(zule_port_posix PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(zule_port_posix PRIVATE _POSIX_C_SOURCE=200809L)
target_link_libraries(zule_port_posix PUBLIC zule_port_api Threads::Threads PRIVATE zule_warnings)

if(ZULE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
