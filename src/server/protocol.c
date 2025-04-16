/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib16/types.h"
#include "lib16/video.h"
#include "lib16/x86.h"

#include "server/bufmgr.h"
#include "server/debug.h"
#include "server/globals.h"
#include "server/pktdrv.h"
#include "server/protocol.h"
#include "server/session.h"
#include "server/util.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#if DEBUG
static void display_packet(const struct Buffer *buffer) {
  const struct EthernetHeader *eh =
      (const struct EthernetHeader *)(buffer->data);
  const struct ProtocolHeader *ph = (const struct ProtocolHeader *)(eh + 1);
  char tmp[MAC_ADDR_FMT_LEN];
  const uint16_t max_display_bytes = (2 * 16);

  video_gotoxy(0, 36);
  video_clear_rows(36, 50);

  printf("Raw Size:    %4d\n", buffer->bytes);
  printf("Dest:        %s\n", fmt_mac_addr(tmp, eh->dest_mac_addr));
  printf("Src:         %s\n", fmt_mac_addr(tmp, eh->src_mac_addr));
  printf("Ethertype:  $%04x\n", ntohs(eh->ethertype));
  printf("Session:    $%08lx\n", ntohl(ph->session_id));
  printf("pkt_type:   $%04x\n", ntohs(ph->pkt_type));
  printf("pay_size:    %4d\n", ntohs(ph->payload_len));

  hex_dump(stdout, buffer->data, MIN(buffer->bytes, max_display_bytes));
  printf("\n\n");
  fflush(stdout);
}
#endif // DEBUG

// Copies MAC addresses from incoming packet to g_send_buffer.
void prep_for_reply(const struct Buffer *buffer) {
  const struct EthernetHeader *in_eh =
      (const struct EthernetHeader *)(buffer->data);
  const struct ProtocolHeader *in_ph =
      (const struct ProtocolHeader *)(in_eh + 1);

  struct EthernetHeader *out_eh = (struct EthernetHeader *)(g_send_buffer);
  struct ProtocolHeader *out_ph = (struct ProtocolHeader *)(out_eh + 1);

  memcpy(out_eh->dest_mac_addr, in_eh->src_mac_addr, ETH_ALEN);
  memcpy(out_eh->src_mac_addr, g_pktdrv_info.mac_addr, ETH_ALEN);
  out_eh->ethertype = htons(g_ethertype);
  out_ph->session_id = in_ph->session_id;

  // Zero out the rest of the packet.
  memset((uint8_t *)(out_ph + 1), 0, MAX_PAYLOAD_LENGTH);
  out_ph->signature = htonl(PACKET_SIGNATURE);
}

void handle_ping(const struct Buffer *buffer) {
  const struct EthernetHeader *in_eh =
      (const struct EthernetHeader *)(buffer->data);
  const struct ProtocolHeader *in_ph =
      (const struct ProtocolHeader *)(in_eh + 1);
  const uint8_t *in_payload = (const uint8_t *)(in_ph + 1);

  struct EthernetHeader *out_eh = (struct EthernetHeader *)(g_send_buffer);
  struct ProtocolHeader *out_ph = (struct ProtocolHeader *)(out_eh + 1);
  uint8_t *out_payload = (uint8_t *)(out_ph + 1);

  // PONG just echos back PING, so packets should be the same size.
  // However, Linux will sent 'runt packets' (not 60 byte minimum), but
  // some PC packet drivers will pad the packet to 60 bytes, per the spec.
  const size_t out_len = buffer->bytes;
  const size_t payload_len = MIN(MAX_PAYLOAD_LENGTH, ntohs(in_ph->payload_len));

  prep_for_reply(buffer);
  out_ph->pkt_type = htons(V1_PONG);
  out_ph->payload_len = htons(payload_len);
  memcpy(out_payload, in_payload, payload_len);

  //  printf("PONG: \n");
  //  hex_dump(stdout, g_send_buffer, out_len);
  //  fflush(stdout);

  pktdrv_send(g_send_buffer, out_len);
}

void handle_status_req(const struct Buffer *buffer) {
  struct EthernetHeader *out_eh = (struct EthernetHeader *)(g_send_buffer);
  struct ProtocolHeader *out_ph = (struct ProtocolHeader *)(out_eh + 1);
  struct StatusResponse *resp = (struct StatusResponse *)(out_ph + 1);
  struct VideoState video;

  video_read_state(&video);

  prep_for_reply(buffer);
  out_ph->pkt_type = htons(V1_STATUS_RESP);
  out_ph->payload_len = htons(sizeof(*resp));

  resp->video_mode = video.video_mode;
  resp->active_page = video.active_page;
  resp->text_rows = video.text_rows;
  resp->text_cols = video.text_cols;
  resp->cursor_row = video.cursor_row;
  resp->cursor_col = video.cursor_col;

#define V1_STATUS_RESP_LEN (COMBINED_HEADER_LEN + sizeof(*resp))

#if DEBUG
  printf("V1_STATUS_RESP:\n");
  hex_dump(stdout, g_send_buffer, V1_STATUS_RESP_LEN);
  fflush(stdout);
#endif // DEBUg

  pktdrv_send(g_send_buffer, V1_STATUS_RESP_LEN);
}

void handle_inject_keystroke(const struct Buffer *buffer) {
  const struct EthernetHeader *in_eh =
      (const struct EthernetHeader *)(buffer->data);
  const struct ProtocolHeader *in_ph =
      (const struct ProtocolHeader *)(in_eh + 1);
  const struct Keystroke *in_keys = (const struct Keystroke *)(in_ph + 1);
  uint16_t key_count = ntohs(in_ph->payload_len) / sizeof(struct Keystroke);

  for (; key_count; ++in_keys, --key_count) {
#if DEBUG
    video_printf(0, 0, 15, "key: %02x %02x %02x ", in_keys->bios_scan_code,
               in_keys->ascii_value, in_keys->flags_17);
#endif

    // We are inside an interrupt handler (either 08h or 28h).
    // It is unclear if we can safely call the BIOS keyboard interrupt
    // (int 16h, AH=05) directly at this time.  However, it seems to work in
    // QEMU (SeaBIOS) w/ FreeDOS.  If this mechanic is problematic, then we'll
    // need to insert the keystrokes into our own ring-buffer, then process
    // that buffer during a safe time.
    x86_inject_keystroke(in_keys->bios_scan_code, in_keys->ascii_value,
                         in_keys->flags_17);
  }
}

void protocol_init() {}

void protocol_process() {
  struct Buffer *buffer;

  while (NULL != (buffer = buffer_get_ready())) {
    const struct EthernetHeader *eh =
        (const struct EthernetHeader *)(buffer->data);
    const struct ProtocolHeader *ph = (const struct ProtocolHeader *)(eh + 1);

    // Skip packets not meant for us.
    if (PACKET_SIGNATURE == ntohl(ph->signature)) {

#if DEBUG
      display_packet(buffer);
#endif // DEBUG

      switch (ntohs(ph->pkt_type)) {
        case V1_PING:
          handle_ping(buffer);
          break;
        case V1_STATUS_REQ:
          handle_status_req(buffer);
          break;
        case V1_SESSION_START:
          session_mgr_start(buffer);
          break;
        case V1_INJECT_KEYSTROKE:
          handle_inject_keystroke(buffer);
          break;
      }
    }

    buffer_release(buffer);
  }
}
