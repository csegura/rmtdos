/*
 * Copyright 2025 <romheat@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __RMTDOS_LIB16_VIDEO_H
#define __RMTDOS_LIB16_VIDEO_H

#include "lib16/types.h"

#define VIDEO_ROWS 25
#define VIDEO_COLS 80
//#define VIDEO_WORD 2
#define VIDEO_SIZE (VIDEO_ROWS * VIDEO_COLS * VIDEO_WORD)

//extern void vga_mode_80x25();
//extern void vga_mode_80x43();
//extern void vga_mode_80x50();

// Initialize video functions
// internally store the segment base address
// MUST be called before any other video functions
extern uint16_t video_init();
extern uint16_t active_video_segment;

#define VIDEO_WORD (active_video_segment == 0xB800 ? 2 : 1)

// Returns video segment base address.
extern uint16_t video_get_segment();

// Copy 'words' count of WORDS (16-bit ints) from VGA Text Mode frame buffer,
// starting at offset from [$b800:0000] into dest.
extern void video_copy_from_frame_buffer(void *dest, uint16_t offset,
                                         uint16_t words);

// same function that is on vga.s via int 0x10, ah=0x0f
extern void video_gotoxy(int x, int y);

extern void video_write_str(int x, int y, uint8_t attr, const char *str);

extern int video_printf(int x, int y, uint8_t attr, const char *fmt, ...);

// Erase all screen text from "y1 <= y < y2".
extern void video_clear_rows(int y1, int y2);

struct VideoState {
  uint8_t video_mode;  // int $10, ah=$0f, al value
  uint8_t active_page; // int $10, ah=$0f, bh value
  uint8_t text_rows;   // (no direct query for this)
  uint8_t text_cols;   // int $10, ah=$0f, ah value
  uint8_t cursor_row;  // int $10, ah=$03, dh value
  uint8_t cursor_col;  // int $10, ah=$03, dl value
};

extern void video_read_state(struct VideoState *state);

extern void video_disable_blink();
extern uint16_t video_checksum_frame_buffer(uint16_t offset, uint16_t words);

#endif // __RMTDOS_LIB16_VIDEO_H
