add_library(zule_core STATIC src/zule.c)
add_library(zule::core ALIAS zule_core)

target_include_directories(zule_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_compile_features(zule_core PUBLIC c_std_99)
set_target_properties(zule_core PROPERTIES C_EXTENSIONS OFF)
