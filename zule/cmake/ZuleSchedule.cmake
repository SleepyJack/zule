# zule_add_schedule(<target> SCHEDULE <file.json> [OUTPUT_DIR <dir>])
#
# Generates the schedule definition and task IDs from a schedule JSON file at build
# time and links them into <target>. The generated zule_task_ids.h is available to
# <target> sources. A relative SCHEDULE is resolved against the calling directory.
# Requires Python 3.10 or later with zule/tools/schedule/requirements.txt installed;
# set Python3_EXECUTABLE to select the interpreter.

function(zule_add_schedule target)
    cmake_parse_arguments(PARSE_ARGV 1 ZULE "" "SCHEDULE;OUTPUT_DIR" "")
    if(NOT ZULE_SCHEDULE)
        message(FATAL_ERROR "zule_add_schedule(${target}) requires SCHEDULE <file.json>")
    endif()
    if(ZULE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "zule_add_schedule(${target}): unexpected arguments ${ZULE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "zule_add_schedule: ${target} is not a target")
    endif()

    get_filename_component(schedule "${ZULE_SCHEDULE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(ZULE_OUTPUT_DIR)
        get_filename_component(output_dir "${ZULE_OUTPUT_DIR}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_schedule")
    endif()

    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    cmake_path(SET tools_dir NORMALIZE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/schedule")
    file(GLOB generator_sources CONFIGURE_DEPENDS "${tools_dir}/zule_schedule/*.py")

    add_custom_command(
        OUTPUT "${output_dir}/zule_schedule_definition.c" "${output_dir}/zule_task_ids.h"
        COMMAND "${Python3_EXECUTABLE}" -m zule_schedule generate "${schedule}" "${output_dir}"
        WORKING_DIRECTORY "${tools_dir}"
        DEPENDS "${schedule}" "${tools_dir}/zule_schedule/schedule.schema.json" ${generator_sources}
        COMMENT "Generating Zule schedule for ${target}"
        VERBATIM
    )

    # Compile the definition separately so only generated code sees internal headers.
    # Object files link directly into the target, independent of library order.
    set(schedule_target "${target}_zule_schedule")
    add_library(${schedule_target} OBJECT
        "${output_dir}/zule_schedule_definition.c" "${output_dir}/zule_task_ids.h")
    target_include_directories(${schedule_target} PUBLIC "${output_dir}")
    set_target_properties(${schedule_target} PROPERTIES C_EXTENSIONS OFF)
    target_link_libraries(${schedule_target}
        PUBLIC zule::core PRIVATE zule_schedule_internal zule_warnings)
    target_link_libraries(${target} PRIVATE ${schedule_target})
endfunction()
