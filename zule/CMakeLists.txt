cmake_minimum_required(VERSION 3.31)
project(zule VERSION 0.1.0 LANGUAGES C)

# Zule ownership domain: core, ports, and schedule tooling. Applications add this
# directory, select a port, and link zule::core. See doc/application.md.

option(ZULE_BUILD_TESTS "Build the core and port tests" ${PROJECT_IS_TOP_LEVEL})
set(ZULE_PORT "" CACHE STRING
    "Port directory under ports/ (posix), or 'external' for an application-defined zule::port")

set(CMAKE_C_EXTENSIONS OFF)

if(ZULE_BUILD_TESTS)
    enable_testing()
endif()

# Settings shared by every component that includes zule_config.h or the port's
# zule_port_config.h: the core, port, generated schedule, and application.
add_library(zule_config INTERFACE)
if(DEFINED ZULE_USER_CONFIG_DIR)
    if(NOT IS_ABSOLUTE "${ZULE_USER_CONFIG_DIR}" OR NOT IS_DIRECTORY "${ZULE_USER_CONFIG_DIR}")
        message(FATAL_ERROR "ZULE_USER_CONFIG_DIR must be an absolute directory")
    endif()
    set(zule_user_config_found OFF)
    if(EXISTS "${ZULE_USER_CONFIG_DIR}/zule_user_config.h")
        target_compile_definitions(zule_config INTERFACE ZULE_USE_USER_CONFIG=1)
        set(zule_user_config_found ON)
    endif()
    if(EXISTS "${ZULE_USER_CONFIG_DIR}/zule_user_port_config.h")
        target_compile_definitions(zule_config INTERFACE ZULE_USE_USER_PORT_CONFIG=1)
        set(zule_user_config_found ON)
    endif()
    if(NOT zule_user_config_found)
        message(FATAL_ERROR "ZULE_USER_CONFIG_DIR must contain zule_user_config.h, "
                            "zule_user_port_config.h, or both")
    endif()
    target_include_directories(zule_config INTERFACE "${ZULE_USER_CONFIG_DIR}")
endif()

# Public application API.
add_library(zule_api INTERFACE)
add_library(zule::api ALIAS zule_api)
target_include_directories(zule_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/core/api")
target_compile_features(zule_api INTERFACE c_std_99)
target_link_libraries(zule_api INTERFACE zule_config)

# Contract implemented by ports. The port target supplies zule_port_types.h.
add_library(zule_port_api INTERFACE)
add_library(zule::port_api ALIAS zule_port_api)
target_include_directories(zule_port_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/ports")
target_link_libraries(zule_port_api INTERFACE zule_api)

# Internal schedule types, for the core, its tests, and generated schedules only.
add_library(zule_schedule_internal INTERFACE)
target_include_directories(zule_schedule_internal INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/core/inc")
target_link_libraries(zule_schedule_internal INTERFACE zule_api)

# Warning flags for Zule-owned targets; linked privately and not imposed on applications.
add_library(zule_warnings INTERFACE)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zule_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(TARGET zule::port OR ZULE_PORT STREQUAL "external")
    # The application defines zule::port, before or after adding Zule.
elseif(NOT ZULE_PORT STREQUAL "" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ports/${ZULE_PORT}/CMakeLists.txt")
    add_subdirectory("ports/${ZULE_PORT}")
else()
    message(FATAL_ERROR "Set ZULE_PORT to a port under ${CMAKE_CURRENT_SOURCE_DIR}/ports "
                        "(posix), or to 'external' and define a zule::port target")
endif()

add_subdirectory(core)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZuleSchedule.cmake")
