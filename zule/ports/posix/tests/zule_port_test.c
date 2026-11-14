/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <time.h>

#include "zule_port.h"

static uint32_t counter;
static uint32_t ticks;

static zule_result_t tick_callback(void) {
    const zule_port_critical_state_t state = zule_port_critical_enter();
    ++ticks;
    zule_port_critical_exit(state);
    /* A scheduler fault must not turn healthy timer delivery into a port error. */
    return ZULE_RESULT_DISPATCH_MISSED;
}

static uint32_t read_ticks(void) {
    const zule_port_critical_state_t state = zule_port_critical_enter();
    const uint32_t count = ticks;
    zule_port_critical_exit(state);
    return count;
}

/* Idle is called inside one critical-section level, as the core does. */
static zule_port_result_t idle(void) {
    const zule_port_critical_state_t state = zule_port_critical_enter();
    const zule_port_result_t result = zule_port_idle();
    zule_port_critical_exit(state);
    return result;
}

static void *worker(void *argument) {
    (void)argument;
    for (uint32_t iteration = 0U; iteration < 1000U; ++iteration) {
        const zule_port_critical_state_t outer = zule_port_critical_enter();
        const zule_port_critical_state_t inner = zule_port_critical_enter();
        ++counter;
        zule_port_critical_exit(inner);
        zule_port_critical_exit(outer);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[4];
    for (size_t index = 0U; index < sizeof(threads) / sizeof(threads[0]); ++index) {
        assert(pthread_create(&threads[index], NULL, worker, NULL) == 0);
    }
    for (size_t index = 0U; index < sizeof(threads) / sizeof(threads[0]); ++index) {
        assert(pthread_join(threads[index], NULL) == 0);
    }
    assert(counter == 4000U);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
    assert(idle() == ZULE_PORT_RESULT_ERROR);
    assert(zule_port_tick_init(0U, tick_callback) == ZULE_PORT_RESULT_ERROR);
    assert(zule_port_tick_init(100000U, NULL) == ZULE_PORT_RESULT_ERROR);
    for (uint32_t restart = 0U; restart < 2U; ++restart) {
        const uint32_t before = read_ticks();
        assert(zule_port_tick_init(100000U, tick_callback) == ZULE_PORT_RESULT_OK);
        uint32_t idle_calls = 0U;
        while (read_ticks() < before + 2U) {
            assert(idle() == ZULE_PORT_RESULT_OK);
            ++idle_calls;
        }
        /* Idle waits for ticks rather than polling; allow for spurious wakeups. */
        assert(idle_calls <= 4U);
        assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
        const uint32_t stopped = read_ticks();
        assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
        assert(read_ticks() == stopped);
    }
    /* A tick that becomes due while idle's caller holds the critical section still
     * ends the wait, because the wait releases exclusion. */
    assert(zule_port_tick_init(200000U, tick_callback) == ZULE_PORT_RESULT_OK);
    {
        const zule_port_critical_state_t state = zule_port_critical_enter();
        const uint32_t before = ticks;
        const struct timespec hold = {0, 300000000L};
        assert(nanosleep(&hold, NULL) == 0);
        assert(ticks == before);
        while (ticks == before) {
            assert(zule_port_idle() == ZULE_PORT_RESULT_OK);
        }
        zule_port_critical_exit(state);
    }
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);

    /* Stop must wake the timer rather than wait for a very long period. */
    assert(zule_port_tick_init(UINT32_MAX, tick_callback) == ZULE_PORT_RESULT_OK);
    assert(zule_port_tick_stop() == ZULE_PORT_RESULT_OK);
    return 0;
}
