/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stddef.h>
#include <string.h>

#include "lib16/video.h"
#include "lib16/x86.h"
#include "server/bufmgr.h"
#include "server/debug.h"
#include "server/globals.h"
#include "server/session.h"
#include "server/util.h"

#define MAX_SESSIONS 4

static struct Session g_sessions[MAX_SESSIONS];
static struct Session *g_session_eof = g_sessions + MAX_SESSIONS;

static uint8_t null_if_addr[ETH_ALEN] = {0, 0, 0, 0, 0, 0};

static uint16_t video_chksum = 0;

void session_mgr_init() { memset(&g_sessions, 0, sizeof(g_sessions)); }

#if DEBUG
void session_mgr_debug() {
  struct Session *s;
  uint32_t now = x86_read_bios_tick_clock();
  int i;

  for (i = 0, s = g_sessions; i < MAX_SESSIONS; ++s, ++i) {
    char tmp[MAC_ADDR_FMT_LEN];
    video_printf(4, i + 3, 2, "%d  %08lx  %s  %9ld", i, s->session_id,
               fmt_mac_addr(tmp, s->mac_addr), now - s->t_last_recv);
  }
}
#endif // DEBUG

struct Session *session_mgr_find(const uint8_t *mac_addr, uint32_t session_id) {
  struct Session *s;

  for (s = g_sessions; s < g_session_eof; ++s) {
    if (!memcmp(s->mac_addr, mac_addr, ETH_ALEN) &&
        (s->session_id == session_id)) {
      return s;
    }
  }

  return NULL;
}

struct Session *session_mgr_start(const struct Buffer *buffer) {
  const struct EthernetHeader *in_eh =
      (const struct EthernetHeader *)(buffer->data);
  const struct ProtocolHeader *in_ph =
      (const struct ProtocolHeader *)(in_eh + 1);
  struct Session *s;
  const uint32_t session_id = ntohl(in_ph->session_id);

  // Do we have an existing session?
  if (NULL == (s = session_mgr_find(in_eh->src_mac_addr, session_id))) {
    // Nope.  Find an empty slot and start a new one?
    if (NULL == (s = session_mgr_find(null_if_addr, 0L))) {
      // All slots full.  Crap.
      return NULL;
    }
    memcpy(s->mac_addr, in_eh->src_mac_addr, ETH_ALEN);
    s->session_id = session_id;
  }

  s->t_last_recv = x86_read_bios_tick_clock();
  s->video_chksum = 0;

  return s;
}

void session_mgr_reclaim(struct Session *s) { memset(s, 0, sizeof(*s)); }

void session_mgr_update(struct Session *s) {}


void session_mgr_update_all() {
  struct EthernetHeader *out_eh = (struct EthernetHeader *)(g_send_buffer);
  struct ProtocolHeader *out_ph = (struct ProtocolHeader *)(out_eh + 1);
  struct VideoText *resp = (struct VideoText *)(out_ph + 1);
  uint8_t *video_data = (uint8_t *)(resp + 1);
  struct Session *s;
  struct VideoState video;
  uint32_t now = x86_read_bios_tick_clock();
  int rows;
  int active_sessions = 0;
  uint16_t payload_len;
  uint16_t offset = 0;
  uint16_t word_count = 0;
  uint16_t video_curr_chksum = 0;
  
  // Prune any stale sessions.
  for (s = g_sessions; s < g_session_eof; ++s) {
    // Is session in use?
    if (!memcmp(s->mac_addr, null_if_addr, ETH_ALEN)) {
      continue;
    }

    // Has it timed out?
    // Warning: overflow errors abound here...
    if (s->t_last_recv + session_lifetime_bios_ticks < now) {
      session_mgr_reclaim(s);
      continue;
    }

    active_sessions++;
  }

  if (!active_sessions) {
    video_next_row = 0;
    return;
  }

  // Determine next chunk of Video data to send to all clients.
  // Compute how many rows we can fit into a packet.
  video_read_state(&video);

#if DEBUG
  video_printf(40, 20, 15, "video: %d %d %d %d    ;", video.video_mode,
             video.active_page, video.text_rows, video.text_cols);
#endif

  // Need to wrap around and start over, but also the video mode could have
  // changed since our last cycle, and the screne is now SHORTER than it used
  // to be.
  if (video_next_row >= video.text_rows) {
    video_next_row = 0;
  }
  
  // We are at the begining?
  if (video_next_row == 0) {
    // Check if screen has been updated
    video_curr_chksum = video_checksum_frame_buffer(0, video.text_rows * video.text_cols * VIDEO_WORD);    
    if (video_curr_chksum == video_chksum) {
      // No changes to the video frame buffer.  Don't send anything.
      return;
    } else {
      // Go ahead
      video_chksum = video_curr_chksum;      
    }
  }


  // Compute how many rows we can fit into an Ethernet frame given the current
  // row width.
  rows = (ETH_FRAME_LEN -
          (sizeof(struct ether_header) + sizeof(struct ProtocolHeader) +
           sizeof(struct VideoText))) /
         (video.text_cols * VIDEO_WORD);
#if DEBUG
  video_printf(40, 21, 15, "rows: %d %d %d    ;", rows, video_next_row,
             video.text_rows);
#endif

  // Reduce rows if we're near the end of the current frame buffer.
  if (rows + video_next_row > video.text_rows) {
    rows = video.text_rows - video_next_row;
  }

#if DEBUG
  video_printf(40, 22, 11, "rows: %d    ;", rows);
#endif
 

  // How many words to copy, and from where?
  offset = video_next_row * video.text_cols * VIDEO_WORD;
  word_count = rows * video.text_cols;
  payload_len = sizeof(struct VideoText) + (word_count * VIDEO_WORD);

  // Advance to the next starting row for the next cycle.
  video_next_row += rows;

#if DEBUG
  video_printf(40, 23, 12, "of:%04x wc:%04x pl:%04x", offset, word_count,
             payload_len);
#endif

  // We'll set the dest_mac_addr and session when we loop through the sessions.
  memcpy(out_eh->src_mac_addr, g_pktdrv_info.mac_addr, ETH_ALEN);
  out_eh->ethertype = htons(g_ethertype);

  out_ph->signature = htonl(PACKET_SIGNATURE);
  out_ph->payload_len = htons(payload_len);
  out_ph->pkt_type = htons(V1_VGA_TEXT);

  resp->text_rows = video.text_rows;
  resp->text_cols = video.text_cols;
  resp->cursor_row = video.cursor_row;
  resp->cursor_col = video.cursor_col;
  resp->offset = htons(offset);
  resp->count = htons(word_count * VIDEO_WORD);

#if DEBUG
  video_printf(40, 24, 13, "%04x %04x %04x %d  ;", (uint16_t)video_data,
             (uint16_t)g_sessions, (uint16_t)g_session_eof, active_sessions);
#endif

  video_copy_from_frame_buffer(video_data, offset, word_count);
  // memset(video_data, 0x1f, word_count * 2);
    
  for (s = g_sessions; s < g_session_eof; ++s) {
    // Is session in use?
    if (!memcmp(s->mac_addr, null_if_addr, ETH_ALEN)) {
      continue;
    }
    memcpy(out_eh->dest_mac_addr, s->mac_addr, ETH_ALEN);
    out_ph->session_id = htonl(s->session_id);
    pktdrv_send(g_send_buffer, COMBINED_HEADER_LEN + payload_len);
  }
}
