# Zule modules: optional hook consumers, each enabled by a core setting. The core
# links this library privately and reaches modules only through zule_modules.h.
add_library(zule_modules STATIC zule_modules.c)
target_include_directories(zule_modules PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(zule_modules PUBLIC zule_api PRIVATE zule_warnings)
