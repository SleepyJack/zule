/* Keep test assertions active in release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>

#include "zule_port.h"

static uint32_t counter;

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
    return 0;
}
