#ifndef ZULE_PORT_CONFIG_H
#define ZULE_PORT_CONFIG_H

/* POSIX port settings. Apply the same settings to the core, port, generated
 * schedule, and application. The port has no settings yet. */
#ifndef ZULE_USE_USER_PORT_CONFIG
#define ZULE_USE_USER_PORT_CONFIG 0
#endif

#if ZULE_USE_USER_PORT_CONFIG
#include "zule_user_port_config.h"
#endif

#endif
