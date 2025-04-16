/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// server/globals.h

#ifndef __RMTDOS_SERVER_GLOBALS_H
#define __RMTDOS_SERVER_GLOBALS_H

#include "common/ethernet.h"
#include "lib16/types.h"
#include "server/config.h"
#include "server/pktdrv.h"

/* Count of BIOS timer ticks a session lives for unless refreshed from the
   client.  BIOS ticks at 18.2065/s
*/
extern uint32_t session_lifetime_bios_ticks;

// Checksum of the video memory.  Used to detect changes in the video
extern uint16_t video_checksum;
// The next row of the video memory to write to.  This is used to
extern uint16_t video_next_row;

// Our custom 'EtherType' that we use.
extern uint16_t g_ethertype;

// Common buffer used to composing and sending response packets.
extern uint8_t g_send_buffer[ETH_FRAME_LEN];

#if DEBUG
// Should we overlay some debugging data onto the text screen.
extern int g_show_debug_overlay;
#endif

extern uint32_t int08_ticks;
extern uint32_t int08_original_handler;

#if HAS_INT28
extern uint32_t int28_ticks;
extern uint32_t int28_original_handler;
#endif

extern uint32_t int2f_ticks;
extern uint32_t int2f_original_handler;

// We only ever bind to ONE driver during runtime, so we will just store
// the driver data in a global.  BCC will emit much more efficient ASM code
// for accessing globals than it does for chucking them onto the stack or heap.

// Which IRQ the packet driver is on.
extern PktDrvIrq g_pktdrv_irq;

// Handle returned from `access_type()` when we register to receive packets
// for our custom EtherType.
extern PktDrvHandle g_pktdrv_handle;

// Misc data returned from the driver (static)
extern struct PktDrvInfo g_pktdrv_info;

// Misc statistics returned from the driver (dynamic; not all drivers
// support this).
extern struct PktDrvStats g_pktdrv_stats;

#endif // __RMTDOS_SERVER_GLOBALS_H
