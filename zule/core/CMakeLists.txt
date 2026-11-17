# The core is built into the Zule library, defined in zule/CMakeLists.txt from
# zule/zule_sources.txt. This directory adds the core's tests.
if(ZULE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
