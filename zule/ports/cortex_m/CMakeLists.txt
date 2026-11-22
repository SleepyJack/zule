# Generic Arm Cortex-M port. See README.md.
enable_language(ASM)

add_library(zule_port_cortex_m STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/zule_port.c" "${CMAKE_CURRENT_SOURCE_DIR}/zule_port_cortex_m.S")
add_library(zule::port ALIAS zule_port_cortex_m)
target_include_directories(zule_port_cortex_m PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zule_port_cortex_m PUBLIC zule_port_api PRIVATE zule_warnings)
