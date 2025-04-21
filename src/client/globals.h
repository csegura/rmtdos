/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __RMTDOS_CLIENT_GLOBALS_H
#define __RMTDOS_CLIENT_GLOBALS_H

#include "client/hostlist.h"

extern int g_running;
extern int g_show_debug_window;
extern int g_show_status;

// Non-NULL if we're actively controlling a server.
extern struct RemoteHost *g_active_host;

#endif // __RMTDOS_CLIENT_GLOBALS_H
