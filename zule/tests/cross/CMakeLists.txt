# Link check for cross-compiled builds. Links the Zule library, the selected port,
# and a generated schedule into a freestanding executable with no C library, so
# problems that only appear with the target compiler fail the build. Nothing runs
# it. The CMake presets select the target, flags, and feature configuration.
add_executable(zule_cross_check zule_cross_check.c zule_cross_runtime.c)
zule_add_schedule(zule_cross_check SCHEDULE schedule.json)
# No startup files or C library. libgcc supplies compiler support routines, such
# as 64-bit division, and comes after the libraries that need it.
target_link_options(zule_cross_check PRIVATE -nostdlib)
target_link_libraries(zule_cross_check PRIVATE zule::zule zule_warnings gcc)
