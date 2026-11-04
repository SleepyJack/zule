find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
if(NOT ZULE_PORT STREQUAL "posix")
    message(FATAL_ERROR "The native demo requires -DZULE_PORT=posix")
endif()

set(ZULE_SCHEDULE_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/schedule")
set(ZULE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(GLOB ZULE_GENERATOR_SOURCES CONFIGURE_DEPENDS "${ZULE_SCHEDULE_TOOLS_DIR}/zule_schedule/*.py")

add_custom_command(
    OUTPUT "${ZULE_GENERATED_DIR}/zule_schedule_definition.c"
    COMMAND "${Python3_EXECUTABLE}" -m zule_schedule generate
            "${CMAKE_CURRENT_SOURCE_DIR}/control-loop.json" "${ZULE_GENERATED_DIR}"
    WORKING_DIRECTORY "${ZULE_SCHEDULE_TOOLS_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/control-loop.json"
        "${ZULE_SCHEDULE_TOOLS_DIR}/zule_schedule/schedule.schema.json"
        ${ZULE_GENERATOR_SOURCES}
    COMMENT "Generating the Zule demo schedule"
    VERBATIM
)

add_executable(zule_demo main.c "${ZULE_GENERATED_DIR}/zule_schedule_definition.c")
target_link_libraries(zule_demo PRIVATE zule::core)
set_target_properties(zule_demo PROPERTIES C_EXTENSIONS OFF)
target_compile_definitions(zule_demo PRIVATE _POSIX_C_SOURCE=200809L)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zule_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
