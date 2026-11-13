if(NOT UNIX)
    message(FATAL_ERROR "The POSIX port requires a POSIX host")
endif()

find_package(Threads REQUIRED)
add_library(zule_port_posix STATIC zule_port.c zule_port_tick.c)
add_library(zule::port ALIAS zule_port_posix)
target_include_directories(zule_port_posix PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(zule_port_posix PRIVATE _POSIX_C_SOURCE=200809L)
target_link_libraries(zule_port_posix PUBLIC zule_port_api Threads::Threads PRIVATE zule_warnings)

if(ZULE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
