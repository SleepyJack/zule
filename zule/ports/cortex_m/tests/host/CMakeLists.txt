# Host tests of the Cortex-M port's C, whichever port the build selects. They
# compile zule_port.c against fake registers and instructions, in place of the
# assembly file, so they check the port's logic, not the hardware.
set(zule_cortex_m_port_dir "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# zule_add_cortex_m_port_test(<name> <definition>...)
function(zule_add_cortex_m_port_test name)
    add_executable(${name}_tests zule_port_cortex_m_test.c "${zule_cortex_m_port_dir}/zule_port.c")
    target_include_directories(${name}_tests PRIVATE "${zule_cortex_m_port_dir}")
    target_compile_definitions(${name}_tests PRIVATE ${ARGN})
    target_link_libraries(${name}_tests PRIVATE zule_port_api zule_warnings)
    add_test(NAME ${name} COMMAND ${name}_tests)
    set_tests_properties(${name} PROPERTIES TIMEOUT 10)
endfunction()

# The processor clock at 1 MHz, so a slot of n microseconds is n counts; and the
# reference clock at 1.5 MHz, where only even slots are whole counts.
zule_add_cortex_m_port_test(zule_cortex_m_port ZULE_PORT_CORTEX_M_SYSTICK_HZ=1000000U)
zule_add_cortex_m_port_test(zule_cortex_m_port_reference_clock
    ZULE_PORT_CORTEX_M_SYSTICK_HZ=1500000U ZULE_PORT_CORTEX_M_SYSTICK_USE_REFERENCE_CLOCK=1)
