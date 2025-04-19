/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <iconv.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client/curses.h"
#include "client/util.h"

char g_cp437_table[CP437_CHARS][CP437_WIDTH];

int g_ncurses_colors[VGA_ATTRS];

WINDOW *g_probe_window = NULL;
WINDOW *g_debug_window = NULL;
WINDOW *g_session_window = NULL;

static const char *control_chars[32] = {
    " ",      "\u263a", "\u263b", "\u2665", "\u2666", "\u2663", "\u2660",
    "\u2022", "\u25d8", "\u25cb", "\u25d9", "\u2642", "\u2640", "\u266a",
    "\u266b", "\u263c", "\u25ba", "\u25c4", "\u2195", "\u203c", "\u00b6",
    "\u00a7", "\u25ac", "\u21a8", "\u2191", "\u2193", "\u2192", "\u2190",
    "\u221f", "\u2194", "\u25b2", "\u25bc",
};

void cp437_table_init() {
  iconv_t cnv = iconv_open("UTF-8", "CP437");
  if (!cnv) {
    fprintf(stderr, "iconv_open(\"UTF-8\", \"CP437\") failed.\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < 32; i++) {
    strncpy(g_cp437_table[i], control_chars[i], CP437_WIDTH);
  }

  for (int i = 32; i < 256; i++) {
    char src[2];
    src[0] = i;
    src[1] = 0;
    char *c = src;
    size_t in_size = sizeof(src);

    char *d = g_cp437_table[i];
    size_t out_size = CP437_WIDTH;

    int r = iconv(cnv, &c, &in_size, &d, &out_size);
    if (r == -1) {
      fprintf(stderr, "iconv() for char 0x%02x failed.\n", i);
      exit(EXIT_FAILURE);
    }
  }

  strncpy(g_cp437_table[127], "\u2302", CP437_WIDTH);

  iconv_close(cnv);
}

// VGA bits (lsb to msb) are swapped from ncurses color bits.
// VGA bits are: "I, R, G, B"  (0x04 = red)
// Ncurses color bits are ANSI std: "B, G, R" (0x01 = red).
static uint8_t color_bit_swap[8] = {
    COLOR_BLACK, COLOR_BLUE,    COLOR_GREEN,  COLOR_CYAN,
    COLOR_RED,   COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE,
};

void color_table_init() {
  const char *term = getenv("TERM") ? getenv("TERM") : "<empty>";

  if (!has_colors()) {
    endwin();
    fprintf(stderr, "Your terminal (%s) does not support color.\n", term);
    exit(EXIT_FAILURE);
  }

  start_color();

  // "xterm" has 8 colors and 64 color pairs :(
  // "xterm-256color" has 256 colors and 65536 color pairs.

  if (can_change_color()) {
    // NOTE: VGA bits (lsb to msb) are swapped from ncurses color bits.
    for (int color = 0; color < 16; color++) {
      int i = (color & 8) ? 1000 : 500;
      int r = (color & 4) ? i : 0;
      int g = (color & 2) ? i : 0;
      int b = (color & 1) ? i : 0;

      init_color(color, r, g, b);
    }
  }

  if ((COLORS >= 256) && (COLOR_PAIRS >= 65536)) {
    // Create a 1:1 mapping from VGA attr to ncurses COLOR_PAIR.
    // Manually tested with 'TERM=xterm-256color' on Gentoo Linux with
    // `x11-terms/xterm-375` on a true-color visual.  Bugs:
    // 1. Brown shows as dark yellow.
    // 2. High-intensity background seems too bright?
    for (int index = 0; index < 256; index++) {
      int fg = index & 15;
      int bg = (index >> 4) & 15;
      init_pair(index, fg, bg);
      g_ncurses_colors[index] = index;
    }
  } else if ((COLORS == 8) && (COLOR_PAIRS >= 64)) {
    // TODO: This produces incorrect results in "TERM=xterm".
    // Any "(attr & 0xe0)" has the wrong FG and BG brightness.
    //
    // (bug) Ignore the "intensity" bit and map the 64 color pairs as RRGGBB.
    for (int index = 0; index < 64; index++) {
      int fg = index & 7;
      int bg = (index >> 3) & 7;
      init_pair(index, fg, bg);
    }

    for (int index = 0; index < 256; index++) {
      int fg = color_bit_swap[index & 7];
      int bg = color_bit_swap[(index >> 4) & 7];
      g_ncurses_colors[index] = fg | (bg << 3);
    }
  } else {
    fprintf(stderr, "Your terminal sucks.  TERM=%s\n", term);
    fprintf(stderr, "COLORS = %d\n", COLORS);
    fprintf(stderr, "COLOR_PAIRS = %d\n", COLOR_PAIRS);
    endwin();
    exit(EXIT_FAILURE);
  }
}

void update_hud(struct RemoteHost *rh) {
  if (OK != wmove(rh->window, rh->text_rows, 0)) {
    return;
  }

  int y = rh->text_rows + 1;

  char mac_addr[MAC_ADDR_FMT_LEN];
  fmt_mac_addr(mac_addr, sizeof(mac_addr), rh->if_addr);

  // Compute time delta to last received packet.
  struct timeval tv_now, tv_stale;
  gettimeofday(&tv_now, NULL);
  timersub(&tv_now, &(rh->tv_last_resp), &tv_stale);

  char stale[32];
  snprintf(stale, sizeof(stale), "%ld.%06ld", tv_stale.tv_sec,
           tv_stale.tv_usec);

  mvwprintw(rh->window, ++y, 0, "addr: %s, rows: %d, cols:%d, latency:%s",
            mac_addr, rh->text_rows, rh->text_cols, stale);

  mvwprintw(rh->window, ++y, 0, "<ALT-ESC> to Exit");
}

void update_session_window(struct RemoteHost *rh, uint16_t video_offset,
                           uint16_t byte_count) {

  for (uint16_t i = video_offset; i < (video_offset + byte_count); i += 2) {
    const uint16_t y = (i >> 1) / rh->text_cols;
    const uint16_t x = (i >> 1) % rh->text_cols;
    const uint8_t ch = rh->video_text_buffer[i];
    const uint8_t attr = rh->video_text_buffer[i + 1];

    wattron(g_session_window, COLOR_PAIR(g_ncurses_colors[attr]));
    //  wattron(g_session_window, PAIR_NUMBER(attr));

    // https://en.wikipedia.org/wiki/Code_page_437
    mvwaddstr(g_session_window, y, x, g_cp437_table[ch]);

    //  wattroff(g_session_window, PAIR_NUMBER(attr));
    wattroff(g_session_window, COLOR_PAIR(g_ncurses_colors[attr]));
  }

  // Clear any section to the right.
  // Typical VGA text mode widths are 40, 80, 90, 132.  If we switch from a
  // higher horizontal resolution to a lower one, we want to clear the now
  // vacant right margin.
  for (int y = 0; y < rh->text_rows; ++y) {
    if (OK == wmove(g_session_window, y, rh->text_cols)) {
      wclrtoeol(g_session_window);
    }
  }

  // If our ncurses window is larger than the remote screen resolution,
  // then clear the area that is outside of the remote end's screen resolution.
  // Then display connection stats below it.
  if (OK == wmove(g_session_window, rh->text_rows, 0)) {
    int y = rh->text_rows;
    wclrtobot(g_session_window);

    wattron(g_session_window, COLOR_PAIR(g_ncurses_colors[0x4f]));
    for (int x = 0; x < rh->text_cols; ++x) {
      mvwaddstr(g_session_window, y, x, g_cp437_table[0xcd]);
    }
    wattroff(g_session_window, COLOR_PAIR(g_ncurses_colors[0x4f]));
  }

  // show a "cursor" at your current position
  if (OK == wmove(g_session_window, rh->status.cursor_row, rh->status.cursor_col)) {
    wattron(g_session_window, COLOR_PAIR(MY_COLOR_HEADER));
    waddch(g_session_window, ' ' | A_REVERSE);
    wattroff(g_session_window, COLOR_PAIR(MY_COLOR_HEADER));
  }
}

void init_ncurses() {
  initscr();
  nonl();
  raw();
  noecho();
  curs_set(0);
  timeout(0);
  color_table_init();

  g_probe_window = newwin(18, 70, 2, 5);
  g_debug_window = newwin(5, 80, 20, 0);
  g_session_window = newwin(0, 0, 0, 0);

  keypad(g_session_window, TRUE);
  meta(g_session_window, TRUE);
  nodelay(g_session_window, TRUE);
  nonl();

  refresh();
}

void shutdown_ncurses() {
  delwin(g_session_window);
  delwin(g_debug_window);
  delwin(g_probe_window);
  endwin();

  g_session_window = g_debug_window = g_probe_window = NULL;
}
