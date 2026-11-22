cmake_minimum_required(VERSION 3.31)
project(zule VERSION 0.1.0 LANGUAGES C)

# Zule ownership domain: core, ports, and schedule tooling. Applications add this
# directory, select a port, and link zule::zule. See doc/application.md.

option(ZULE_BUILD_TESTS "Build the core and port tests" ${PROJECT_IS_TOP_LEVEL})
set(ZULE_PORT "" CACHE STRING
    "Port directory under ports/, or 'external' for an application-defined zule::port")

# Zule's own targets compile as strict C99, the MISRA C:2012 baseline, so any use
# of a later standard fails the build. Applications keep their own standard;
# zule_api only requires at least C99 of them.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(ZULE_BUILD_TESTS)
    enable_testing()
endif()

# Settings shared by every component that includes zule_config.h or the port's
# zule_port_config.h: the core, port, generated schedule, and application.
# ZULE_USER_CONFIG_DIR holds zule_user_config.h, zule_user_port_config.h, or both.
# ZULE_USER_PORT_CONFIG_DIR, when the port settings are kept separately, holds
# zule_user_port_config.h; only one of the directories may supply it.
add_library(zule_config INTERFACE)
set(zule_user_port_config_found OFF)
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
        set(zule_user_port_config_found ON)
        set(zule_user_config_found ON)
    endif()
    if(NOT zule_user_config_found)
        message(FATAL_ERROR "ZULE_USER_CONFIG_DIR must contain zule_user_config.h, "
                            "zule_user_port_config.h, or both")
    endif()
    target_include_directories(zule_config INTERFACE "${ZULE_USER_CONFIG_DIR}")
endif()
if(DEFINED ZULE_USER_PORT_CONFIG_DIR)
    if(NOT IS_ABSOLUTE "${ZULE_USER_PORT_CONFIG_DIR}" OR NOT IS_DIRECTORY "${ZULE_USER_PORT_CONFIG_DIR}")
        message(FATAL_ERROR "ZULE_USER_PORT_CONFIG_DIR must be an absolute directory")
    endif()
    if(NOT EXISTS "${ZULE_USER_PORT_CONFIG_DIR}/zule_user_port_config.h")
        message(FATAL_ERROR "ZULE_USER_PORT_CONFIG_DIR must contain zule_user_port_config.h")
    endif()
    if(zule_user_port_config_found)
        message(FATAL_ERROR "zule_user_port_config.h is in both ZULE_USER_CONFIG_DIR and "
                            "ZULE_USER_PORT_CONFIG_DIR; keep one")
    endif()
    set(zule_user_port_config_found ON)
    target_include_directories(zule_config INTERFACE "${ZULE_USER_PORT_CONFIG_DIR}")
endif()
if(zule_user_port_config_found)
    target_compile_definitions(zule_config INTERFACE ZULE_USE_USER_PORT_CONFIG=1)
endif()

# Public application API.
add_library(zule_api INTERFACE)
add_library(zule::api ALIAS zule_api)
target_include_directories(zule_api INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/core/api" "${CMAKE_CURRENT_SOURCE_DIR}/modules/api")
target_compile_features(zule_api INTERFACE c_std_99)
target_link_libraries(zule_api INTERFACE zule_config)

# Contract implemented by ports. The port target supplies zule_port_types.h.
add_library(zule_port_api INTERFACE)
add_library(zule::port_api ALIAS zule_port_api)
target_include_directories(zule_port_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/ports")
target_link_libraries(zule_port_api INTERFACE zule_api)

# Internal headers, for the Zule library, its tests, and generated schedules only:
# the schedule types, and the modules' per-task storage that generated schedules
# define.
add_library(zule_schedule_internal INTERFACE)
target_include_directories(zule_schedule_internal INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/core/inc" "${CMAKE_CURRENT_SOURCE_DIR}/modules/inc")
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
    message(FATAL_ERROR "Set ZULE_PORT to a directory under ${CMAKE_CURRENT_SOURCE_DIR}/ports, "
                        "or to 'external' and define a zule::port target")
endif()

# Zule's sources, read from the single list in zule_sources.txt, as absolute
# paths. Reconfigure when the list changes.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/zule_sources.txt" zule_source_lines REGEX "^[^#]")
set(ZULE_SOURCES "")
foreach(zule_source IN LISTS zule_source_lines)
    string(STRIP "${zule_source}" zule_source)
    if(NOT zule_source STREQUAL "")
        list(APPEND ZULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${zule_source}")
    endif()
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/zule_sources.txt")

# The Zule library that applications link: the core and the modules. Modules
# build into it, so they can use the core API without a link cycle.
add_library(zule STATIC ${ZULE_SOURCES})
add_library(zule::zule ALIAS zule)
target_link_libraries(zule
    PUBLIC zule_api
    PRIVATE zule_schedule_internal zule_port_api zule::port zule_warnings)

add_subdirectory(core)

if(ZULE_BUILD_TESTS)
    # The Cortex-M port's logic, tested on the host against fake registers.
    add_subdirectory(ports/cortex_m/tests/host cortex_m_port_tests)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZuleSchedule.cmake")
