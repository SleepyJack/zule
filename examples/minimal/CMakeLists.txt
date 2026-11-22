cmake_minimum_required(VERSION 3.31)
project(zule_minimal LANGUAGES C)

# The smallest integration: no build settings, so Zule uses its defaults. The
# target, a directory under targets/, selects the port; each preset builds for
# one target.
set(EXAMPLE_TARGET "" CACHE STRING "Target: a directory under targets/")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/targets/${EXAMPLE_TARGET}/target.cmake")
    message(FATAL_ERROR "Set EXAMPLE_TARGET to a directory under ${CMAKE_CURRENT_SOURCE_DIR}/targets")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/targets/${EXAMPLE_TARGET}/target.cmake")

add_subdirectory(../../zule zule)

add_executable(zule_minimal main.c)
set_target_properties(zule_minimal PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(zule_minimal PRIVATE zule::zule)
zule_add_schedule(zule_minimal SCHEDULE config/schedule.json)
# A target that needs its own sources or options defines this function.
if(COMMAND example_target_configure)
    example_target_configure(zule_minimal)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zule_minimal PRIVATE -Wall -Wextra -Wpedantic)
endif()
