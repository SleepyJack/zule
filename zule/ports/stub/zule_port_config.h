#ifndef ZULE_PORT_CONFIG_H
#define ZULE_PORT_CONFIG_H

/* Stub port settings. The port has no settings. */
#ifndef ZULE_USE_USER_PORT_CONFIG
#define ZULE_USE_USER_PORT_CONFIG 0
#endif

#if ZULE_USE_USER_PORT_CONFIG
#include "zule_user_port_config.h"
#endif

#endif
