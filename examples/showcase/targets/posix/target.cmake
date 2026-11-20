# POSIX target: the host, through Zule's POSIX port. The console is standard
# output.
set(ZULE_PORT posix CACHE STRING "Zule port")

# Add the target's parts of the showcase to its executable.
function(example_target_configure executable)
    target_sources(${executable} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/example_console.c")
endfunction()
