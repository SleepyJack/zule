cmake_minimum_required(VERSION 3.31)
project(zule_cross_check LANGUAGES C)

# Link check for cross-compiled builds, a standalone project like the examples. It
# links the Zule library, a port, and a generated schedule into a freestanding
# program with no C library, so problems that only appear with the target
# compiler fail the build. Nothing runs it. The presets choose the port, the
# core, and the features; the settings below apply to every build.
set(ZULE_PORT stub CACHE STRING "Zule port")
# Static analysis below reads the compile database; enable it before any target.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
string(APPEND CMAKE_C_FLAGS " -ffreestanding -Wconversion -Wshadow -Wstrict-prototypes")
add_subdirectory(../.. zule)

add_executable(zule_cross_check zule_cross_check.c zule_cross_runtime.c)
zule_add_schedule(zule_cross_check SCHEDULE schedule.json)
# No startup files or C library. libgcc supplies compiler support routines, such
# as 64-bit division, and comes after the libraries that need it.
target_link_options(zule_cross_check PRIVATE -nostdlib)
# The port is listed explicitly so it comes before libgcc too.
target_link_libraries(zule_cross_check PRIVATE zule::zule zule::port zule_warnings gcc)

# Static analysis: after the build, Cppcheck with its MISRA C:2012 addon analyses
# the Zule library, the selected port, and the generated schedule as one
# program, from the compile database, so it sees each source's definitions and
# include paths, and project-wide rules see every use. The check program is
# analysed only so its calls count as uses of the public API; findings in its own
# sources are suppressed. A finding fails the build; fix it, or suppress it
# inline with a comment giving the reason. The compile database is enabled above.
# There is no --cppcheck-build-dir: with one, Cppcheck 2.13 reports unused
# functions but leaves them out of its exit status.
find_program(ZULE_CPPCHECK cppcheck REQUIRED)
add_custom_target(zule_cppcheck ALL
    COMMAND "${ZULE_CPPCHECK}"
        "--project=${CMAKE_BINARY_DIR}/compile_commands.json"
        "--suppress=*:${CMAKE_CURRENT_SOURCE_DIR}/zule_cross_check.c"
        "--suppress=*:${CMAKE_CURRENT_SOURCE_DIR}/zule_cross_runtime.c"
        --std=c99
        # 32-bit int, long, and pointers, as on Cortex-M.
        --platform=unix32
        # The MISRA addon reports at style severity, so style must be enabled.
        --enable=warning,style,portability,unusedFunction
        --addon=misra
        --error-exitcode=1
        --inline-suppr
        # Cppcheck knows the standard headers; it cannot see the compiler's own.
        --suppress=missingIncludeSystem
        --quiet
    COMMENT "Analysing Zule with Cppcheck and its MISRA C:2012 addon"
    VERBATIM)
# The generated schedule must exist before analysis.
add_dependencies(zule_cppcheck zule_cross_check)
