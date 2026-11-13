cmake_minimum_required(VERSION 3.31)
project(zule_demo LANGUAGES C)

# Reference integration: the application and its configuration (user domain)
# consume Zule (Zule domain) through the variables, targets, and function below.
set(ZULE_PORT posix CACHE STRING "Zule port")
set(ZULE_USER_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
add_subdirectory(../zule zule)

add_executable(zule_demo main.c)
set_target_properties(zule_demo PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(zule_demo PRIVATE zule::core)
zule_add_schedule(zule_demo SCHEDULE config/control-loop.json)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zule_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
