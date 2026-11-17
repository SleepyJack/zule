# Sources come from zule/zule_sources.txt. Modules live in zule/modules/ but build
# into the core library, so they can use the core API without a link cycle.
add_library(zule_core STATIC ${ZULE_SOURCES})
add_library(zule::core ALIAS zule_core)

target_link_libraries(zule_core
    PUBLIC zule_api
    PRIVATE zule_schedule_internal zule_port_api zule::port zule_warnings)

if(ZULE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
