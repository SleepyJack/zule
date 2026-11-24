# The core's test configurations and the tests in each, shared by every build of
# the core tests: on the host, from CMakeLists.txt here, and on targets, such as
# qemu/. Each includer defines zule_add_test_config(<config> [<definition>...])
# and zule_add_test(<name> <source> <config>), with <source> relative to this
# directory, then includes this file.

# Default settings: hooks, the timebase, and checks are compiled away.
zule_add_test_config(zule_test_config_default)
zule_add_test(zule_core zule_scheduler_test.c zule_test_config_default)
zule_add_test(zule_schedule zule_schedule_test.c zule_test_config_default)
zule_add_test(zule_runner zule_test.c zule_test_config_default)

# The timebase and every user hook.
zule_add_test_config(zule_test_config_hooks
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_USER_HOOK_TICK_START=1
    ZULE_CONFIG_USER_HOOK_TICK_END=1
    ZULE_CONFIG_USER_HOOK_TICK_TASK_RELEASE=1
    ZULE_CONFIG_USER_HOOK_TASK_START=1
    ZULE_CONFIG_USER_HOOK_TASK_END=1)
zule_add_test(zule_hooks zule_hook_test.c zule_test_config_hooks)

# The tick timing check, with a 10 us tolerance and the tick start hook, which
# shares the check's time read.
zule_add_test_config(zule_test_config_tick_check
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_TICK_CHECK=1
    ZULE_CONFIG_TICK_TOLERANCE_US=10
    ZULE_CONFIG_USER_HOOK_TICK_START=1)
zule_add_test(zule_tick_check zule_tick_check_test.c zule_test_config_tick_check)

# The tick timing module, alone and with the core tick timing check.
zule_add_test_config(zule_test_config_tick_timing
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TICK_TIMING=1)
zule_add_test(zule_tick_timing zule_tick_timing_test.c zule_test_config_tick_timing)
zule_add_test_config(zule_test_config_tick_timing_check
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TICK_TIMING=1
    ZULE_CONFIG_TICK_CHECK=1
    ZULE_CONFIG_TICK_TOLERANCE_US=10)
zule_add_test(zule_tick_timing_check zule_tick_timing_test.c
    zule_test_config_tick_timing_check)

# The task statistics module, alone and with the tick timing module, which shares
# the tick start entry point.
zule_add_test_config(zule_test_config_task_stats
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TASK_STATS=1)
zule_add_test(zule_task_stats zule_task_stats_test.c zule_test_config_task_stats)
zule_add_test_config(zule_test_config_task_stats_tick_timing
    ZULE_CONFIG_TIMEBASE=1
    ZULE_CONFIG_MODULE_TASK_STATS=1
    ZULE_CONFIG_MODULE_TICK_TIMING=1)
zule_add_test(zule_task_stats_tick_timing zule_task_stats_test.c
    zule_test_config_task_stats_tick_timing)
