/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

 #ifndef __RMTDOS_CLIENT_KEYBOARD_H
 #define __RMTDOS_CLIENT_KEYBOARD_H
 
 #include "client/globals.h"
 #include "client/network.h"
 
 #define EXIT_WCH_CODE 0x9b /* ALT-ESCAPE */
 
 // UI is in "session mode" (connected to a server).  Send the keystroke over
 // for server to inject it into the BIOS keyboard buffer.
 void process_stdin_session_mode(struct RawSocket *rs, struct RemoteHost *rh);
 
 
 #endif // __RMTDOS_CLIENT_KEYBOARD_H
 
