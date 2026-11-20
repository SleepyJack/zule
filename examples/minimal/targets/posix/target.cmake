# POSIX target: the host, through Zule's POSIX port.
set(ZULE_PORT posix CACHE STRING "Zule port")

# Add the target's parts of the example to its executable; the POSIX target needs
# none.
function(example_target_configure executable)
endfunction()
