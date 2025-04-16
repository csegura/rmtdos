/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "server/globals.h"

uint32_t session_lifetime_bios_ticks = 182; // ~10s

//uint16_t video_segment = 0;
uint16_t video_next_row = 0;

uint16_t g_ethertype = ETHERTYPE_RMTDOS;

uint8_t g_send_buffer[ETH_FRAME_LEN];

PktDrvIrq g_pktdrv_irq = 0;
PktDrvHandle g_pktdrv_handle = 0;
struct PktDrvInfo g_pktdrv_info = {0};
struct PktDrvStats g_pktdrv_stats = {0};

#if DEBUG
int g_show_debug_overlay = 0;
#endif

uint32_t int08_ticks = 0;
uint32_t int08_original_handler = 0;

#if HAS_INT28
uint32_t int28_ticks = 0;
uint32_t int28_original_handler = 0;
#endif

uint32_t int2f_ticks = 0;
uint32_t int2f_original_handler = 0;
