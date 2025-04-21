/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __RMTDOS_CLIENT_CURSES_H
#define __RMTDOS_CLIENT_CURSES_H

#include <ncurses.h>

#include "client/hostlist.h"

// Maps VGA text mode characters to unicode strings.
#define CP437_CHARS 256
#define CP437_WIDTH 8
extern char g_cp437_table[CP437_CHARS][CP437_WIDTH];

// Maps VGA text mode attribute to ncurses color code.
#define VGA_ATTRS 256
extern int g_ncurses_colors[VGA_ATTRS];

// Custom color pairs for use in ncurses.
#define MY_COLOR_HEADER 32 //15 // (equiv to VGA attr 0x0f)
#define COLOR_CURSOR 15
#define SESSION_WIN_SEPARATOR 249 //0x4f

extern WINDOW *g_probe_window;
extern WINDOW *g_debug_window;
extern WINDOW *g_session_window;
extern WINDOW *g_status_window;

extern void cp437_table_init();

extern void update_hud(struct RemoteHost *rh);

extern void update_session_window(struct RemoteHost *rh, uint16_t vga_offset,
                                  uint16_t byte_count);

extern void init_ncurses();

extern void shutdown_ncurses();

extern void update_status_window(struct RemoteHost *rh);

#endif // __RMTDOS_CLIENT_CURSES_H
