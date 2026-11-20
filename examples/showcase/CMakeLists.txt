cmake_minimum_required(VERSION 3.31)
project(zule_showcase LANGUAGES C)

# Reference integration: the application and its configuration (user domain)
# consume Zule (Zule domain) through the variables, targets, and function below.
# The target, a directory under targets/, selects the port and supplies the
# showcase's console; each preset builds for one target.
set(EXAMPLE_TARGET "" CACHE STRING "Target: a directory under targets/")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/targets/${EXAMPLE_TARGET}/target.cmake")
    message(FATAL_ERROR "Set EXAMPLE_TARGET to a directory under ${CMAKE_CURRENT_SOURCE_DIR}/targets")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/targets/${EXAMPLE_TARGET}/target.cmake")

set(ZULE_USER_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_subdirectory(../../zule zule)

add_executable(zule_showcase main.c)
set_target_properties(zule_showcase PROPERTIES C_EXTENSIONS OFF)
target_include_directories(zule_showcase PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zule_showcase PRIVATE zule::zule)
zule_add_schedule(zule_showcase SCHEDULE config/schedule.json)
example_target_configure(zule_showcase)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zule_showcase PRIVATE -Wall -Wextra -Wpedantic)
endif()
