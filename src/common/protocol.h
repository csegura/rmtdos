/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Values common to the client and server.
//
// NOTE: The values here MUST compile identically under 'bcc' for 16-bit
// real-mode, and under 'gcc' for 64-bit Linux-amd64.

#ifndef __RMTDOS_COMMON_PROTOCOL_H__
#define __RMTDOS_COMMON_PROTOCOL_H__

#include "common/ethernet.h"

#ifdef __GNUC__
// 'gcc' on Linux.
#include <stdint.h>
#define NEED_PRAGMA_PACK 1
#else
/* 'bcc' compiler (for 16-bit real-mode) */

#include "lib16/types.h"
#define NEED_PRAGMA_PACK 0
#endif

// Our version info, for display purposes.
#define RMTDOS_VERSION "RMTDOS v0.1"

// 32-bit signature sent in every packet (network byte order).
// Initially picked at random via `uuidgen`.  Has no meaning.
// Used to reject packets using the same EtherType but from other systems.
#define PACKET_SIGNATURE ((uint32_t)0x7b6e05b0)

// Values possible for 'ProtocolHeader.type' (see below).
enum PKT_TYPE {
  V1_NOOP = 0,

  // Client -> Server, send 'ping'.
  // Payload is arbitrary.
  // Client will respond if target is broadcast address.
  V1_PING = 1,

  // Server -> Client, send ping response.
  // Payload is echoed back.
  V1_PONG = 2,

  // Client -> Server.  Ask server for current status.
  // Payload is empty.
  // Client will respond if target is broadcast address.
  V1_STATUS_REQ = 3,

  // Server -> Client.  Response to V1_STATUS_REQ.
  // Payload is `struct StatusResponse`.
  V1_STATUS_RESP = 4,

  // Client -> Server
  // Request server to place itself under control of the client.
  // Server will start sending VGA text frames.
  // Client should send this packet every few seconds.
  // Server will stop sending data to client a few seconds after the last
  // time this packet was received.  This ensures that the server stops
  // a remote session if the client stops, and will handle network packet loss.
  V1_SESSION_START = 5,

  // Server -> Client
  // Contains VGA TEXT data.
  V1_VGA_TEXT = 6,

  // Client -> Server
  // Inserts keystroke into BIOS keyboard buffer.
  V1_INJECT_KEYSTROKE = 7,
};

#if NEED_PRAGMA_PACK
#pragma pack(push, 1)
#endif

// Assumes pure 802.3 Ethernet frames with no 802.1Q tagging.
struct EthernetHeader {
  uint8_t dest_mac_addr[ETH_ALEN];
  uint8_t src_mac_addr[ETH_ALEN];

  // Network Byte Order.
  // If 'length' > 1536, then value is interpreted as 'EtherType'.
  // If 'length' < 1501, then value is interpreted as 'octet length'.
  // https://en.wikipedia.org/wiki/EtherType
  uint16_t ethertype;
};

// All multi-byte integers are in network byte order.
struct ProtocolHeader {
  // Used to filter out packets not for our protocol.
  uint32_t signature;

  // Unique session ID, selected at random by the client.
  uint32_t session_id;

  // Byte size of payload (not counting this header).
  uint16_t payload_len;

  // PKT_TYPE'.
  uint16_t pkt_type;
};

#define COMBINED_HEADER_LEN                                                    \
  (sizeof(struct ether_header) + sizeof(struct ProtocolHeader))

#define MAX_PAYLOAD_LENGTH (ETH_FRAME_LEN - COMBINED_HEADER_LEN)

// V1_STATUS_RESP: Server -> Client
struct StatusResponse {
  uint8_t video_mode;
  uint8_t active_page;
  uint8_t text_rows;
  uint8_t text_cols;
  uint8_t cursor_row;
  uint8_t cursor_col;
};

// V1_VGA_TEXT: Server -> Client
// Followed by raw data, to end of packet.
struct VideoText {
  uint8_t text_rows; // Current height of the screen
  uint8_t text_cols; // Current width of the screen
  uint8_t cursor_row; // Current row of the cursor
  uint8_t cursor_col; // Current column of the cursor
  uint16_t offset;   // Byte offset from $b800:0
  uint16_t count;    // Count of BYTES of data in packet
};

// Bit flags for `Keystroke.flags`
// ncurses cannot distinguish between LEFT and RIGHT modifier keys,
// so we'll translate these as all "left" keys.
// These bits match the lower 4 bits of BIOS memory location 0040:0017.
#define KS_SHIFT 1
#define KS_CONTROL 4
#define KS_ALT 8

#define KS_MASK (~(KS_SHIFT | KS_CONTROL | KS_ALT))

// V1_INJECT_KEYSTROKE: Client -> Server
// Packet contains (possibly repeated) pairs of keyboard data in the same
// format that the BIOS int 16h AH=5 function will take.
// http://www.ctyme.com/intr/rb-1761.htm
struct Keystroke {
  // int 16h, AH=05, CH=bios scan code.
  uint8_t bios_scan_code;

  // int 16h, AH=05, CL=ASCII code.
  uint8_t ascii_value;

  // Bit-flags for which modifier keys were active.
  // https://stanislavs.org/helppc/kb_flags.html
  // To be jammed into 0040:0017.
  uint16_t flags_17;
};

#if NEED_PRAGMA_PACK
#pragma pack(pop)
#endif

#endif // __RMTDOS_COMMON_PROTOCOL_H__
