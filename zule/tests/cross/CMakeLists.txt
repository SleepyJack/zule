cmake_minimum_required(VERSION 3.31)
project(zule_cross_check LANGUAGES C)

# Link check for cross-compiled builds, a standalone project like the demo. It
# links the Zule library, the stub port, and a generated schedule into a
# freestanding program with no C library, so problems that only appear with the
# target compiler fail the build. Nothing runs it. The presets choose the core
# and the features; the settings below apply to every build.
set(ZULE_PORT stub CACHE STRING "Zule port")
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
string(APPEND CMAKE_C_FLAGS " -ffreestanding -Wconversion -Wshadow -Wstrict-prototypes")
add_subdirectory(../.. zule)

add_executable(zule_cross_check zule_cross_check.c zule_cross_runtime.c)
zule_add_schedule(zule_cross_check SCHEDULE schedule.json)
# No startup files or C library. libgcc supplies compiler support routines, such
# as 64-bit division, and comes after the libraries that need it.
target_link_options(zule_cross_check PRIVATE -nostdlib)
target_link_libraries(zule_cross_check PRIVATE zule::zule zule_warnings gcc)

# Static analysis: Cppcheck runs on each source of the Zule library, the selected
# port, and the generated schedule as it compiles, with the same definitions and
# include paths. A finding fails the build; fix it, or suppress it inline with a
# comment giving the reason. The check program itself is not analysed.
find_program(ZULE_CPPCHECK cppcheck REQUIRED)
set(zule_cppcheck_command "${ZULE_CPPCHECK}"
    --std=c99
    # 32-bit int, long, and pointers, as on Cortex-M.
    --platform=unix32
    --enable=warning,portability
    --error-exitcode=1
    --inline-suppr
    # Cppcheck knows the standard headers; it cannot see the compiler's own.
    --suppress=missingIncludeSystem
    --quiet)
get_target_property(zule_port_target zule::port ALIASED_TARGET)
set_target_properties(zule ${zule_port_target} zule_cross_check_zule_schedule
    PROPERTIES C_CPPCHECK "${zule_cppcheck_command}")
