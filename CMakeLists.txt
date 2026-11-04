cmake_minimum_required(VERSION 3.16)
project(zule VERSION 0.1.0 LANGUAGES C)

option(ZULE_BUILD_DEMO "Build the host demo and generate its schedule" OFF)
option(ZULE_BUILD_TESTS "Build the core API tests" OFF)

if(ZULE_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)

if(ZULE_BUILD_DEMO)
    add_subdirectory(demo)
endif()
