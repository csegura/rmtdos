/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// server/session.h
//
// Manages sessions (connects to a client).

#include "common/protocol.h"
#include "server/bufmgr.h"

// Data that we track per session (connect to a client).
struct Session {
  // Timestamp, `x86_read_bios_tick_clock()`, of last time we received a
  // packet on this connection.
  uint32_t t_last_recv;

  // Session ID sent by the client.
  uint32_t session_id;

  // MAC address of the client.
  uint8_t mac_addr[ETH_ALEN];

  // Checksum of the video data that we sent to this client.
  uint16_t video_chksum;
};

extern void session_mgr_init();

extern struct Session *session_mgr_find(const uint8_t *mac_addr,
                                        uint32_t session_id);

extern struct Session *session_mgr_start(const struct Buffer *buffer);

#if DEBUG
extern void session_mgr_debug();
#endif // DEBUG

extern void session_mgr_update_all();
