#ifndef ZULE_PORT_CONFIG_H
#define ZULE_PORT_CONFIG_H

/* POSIX port settings and capabilities. Apply the same settings to the core,
 * port, generated schedule, and application. */
#ifndef ZULE_USE_USER_PORT_CONFIG
#define ZULE_USE_USER_PORT_CONFIG 0
#endif

#if ZULE_USE_USER_PORT_CONFIG
#include "zule_user_port_config.h"
#endif

/* The port has no application settings yet. Capabilities are fixed by the
 * port and cannot be overridden. */
#ifdef ZULE_PORT_CONFIG_TIME
#error "ZULE_PORT_CONFIG_TIME is a port capability and cannot be configured"
#endif

/* CLOCK_MONOTONIC in microseconds; see zule_port_time.c. */
#define ZULE_PORT_CONFIG_TIME 1

#endif
