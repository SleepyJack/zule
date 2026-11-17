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
