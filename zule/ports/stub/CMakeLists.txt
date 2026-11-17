# Compile-check port: links, but does nothing. See README.md.
set(ZULE_STUB_PORT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/zule_port.c")
add_library(zule_port_stub STATIC ${ZULE_STUB_PORT_SOURCES})
add_library(zule::port ALIAS zule_port_stub)
target_include_directories(zule_port_stub PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zule_port_stub PUBLIC zule_port_api PRIVATE zule_warnings)
