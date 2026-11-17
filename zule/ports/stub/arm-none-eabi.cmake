# Toolchain file for the stub port's target, bare-metal Arm Cortex-M, with the
# Arm GNU toolchain (arm-none-eabi-gcc). Select the core through compiler flags,
# for example -mcpu=cortex-m3 -mthumb; the cross check's presets set them.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)

# A bare-metal compiler cannot link a test program without startup code, so
# check the compiler by building a static library instead.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
