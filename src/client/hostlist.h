/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Routines for managing a list of known remote hosts.

#ifndef __RMTDOS_CLIENT_HOSTLIST_H
#define __RMTDOS_CLIENT_HOSTLIST_H

#include <ncurses.h>
#include <stdint.h>
#include <sys/time.h>

#include "common/protocol.h"

// Used to keep track of all servers seen, so we can present a selection
// to the user.
struct RemoteHost {
  // Human-friendly (small integer) ID attached to this host, for menu
  // seection usage.
  int index;

  // Network identify of the host.
  uint8_t if_addr[ETH_ALEN];

  // Absolute timestamp of last packet received for this host.
  struct timeval tv_last_resp;

  // Absolute timestamp of last sent V1_SESSION_START.
  struct timeval tv_last_session_start;

  // Misc status flags from the host.
  // Captured even when not actively under remote control.
  struct StatusResponse status;

  // Non-NULL if host is being remotely controlled (ncurses WINDOW).
  WINDOW *window;

  // Last known VGA text mode resolution.
  uint8_t text_rows;
  uint8_t text_cols;

  // Raw VGA text buffer, as sent from client in V1_VGA_TEXT packets.
  // VGA text buffer is from $000b8000 to $000bffff (32KiB).
  uint8_t video_text_buffer[32768];
};

extern void hostlist_create();
extern void hostlist_destroy();
extern void hostlist_register(const uint8_t *packet, size_t length);

// To iterate through the known remote hosts, set *iter to 0.  Call
// `hostlist_iter()` until it returns NULL.
extern struct RemoteHost *hostlist_iter(int *iter);

// Returns NULL is mac_addr is not found.
extern struct RemoteHost *hostlist_find_by_mac(const uint8_t *if_addr);

// Returns NULL is mac_addr is not found.
extern struct RemoteHost *hostlist_find_by_index(int index);

#endif // __RMTDOS_CLIENT_HOSTLIST_H
