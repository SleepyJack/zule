#include "zule_port.h"
#include "zule_test_port.h"

#include <stdlib.h>

static size_t depth;

zule_port_critical_state_t zule_port_critical_enter(void) {
    const zule_port_critical_state_t state = {depth};
    ++depth;
    return state;
}

void zule_port_critical_exit(zule_port_critical_state_t previous_state) {
    if (depth == 0U || previous_state.previous_depth != depth - 1U) {
        abort();
    }
    depth = previous_state.previous_depth;
}

bool zule_test_port_is_critical(void) {
    return depth != 0U;
}
