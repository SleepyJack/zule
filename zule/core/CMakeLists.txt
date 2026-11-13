add_library(zule_core STATIC src/zule.c src/zule_scheduler.c src/zule_schedule.c)
add_library(zule::core ALIAS zule_core)

target_link_libraries(zule_core
    PUBLIC zule_api
    PRIVATE zule_schedule_internal zule_port_api zule::port zule_warnings)

if(ZULE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
