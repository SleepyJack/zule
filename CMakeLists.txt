cmake_minimum_required(VERSION 3.16)
project(zule VERSION 0.1.0 LANGUAGES C)

option(ZULE_BUILD_DEMO "Build the host demo and generate its schedule" OFF)
option(ZULE_BUILD_TESTS "Build the core API tests" OFF)
set(ZULE_PORT "" CACHE STRING "Port implementation (posix), or provide a zule::port target")

if(ZULE_BUILD_DEMO)
    # Apply the demo configuration before creating the core and port targets.
    add_compile_definitions(ZULE_USE_USER_CONFIG=1)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/demo")
endif()

if(ZULE_BUILD_TESTS)
    enable_testing()
endif()

if(NOT TARGET zule::port)
    if(ZULE_PORT STREQUAL "posix")
        add_subdirectory(ports/posix)
    else()
        message(FATAL_ERROR "Select -DZULE_PORT=posix or provide a zule::port target before adding Zule")
    endif()
endif()

add_subdirectory(core)

if(ZULE_BUILD_DEMO)
    add_subdirectory(demo)
endif()
