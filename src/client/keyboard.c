// Inspired by https://github.com/qemu/qemu/blob/master/ui/curses_keys.h
//
/*
 * Keycode and keysyms conversion tables for curses
 *
 * Copyright (c) 2005 Andrzej Zaborowski  <balrog@zabor.org>
 * Copyright (c) 2004 Johannes Schindelin
 * Copyright (c) 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 #include <bits/types/wint_t.h>
 #include <ctype.h>
 #include <ncurses.h>
 #include <stdlib.h>
 #include <string.h>
 
 #include "client/curses.h"
 #include "client/globals.h"
 #include "client/keyboard.h"
 
 #include "client/keymaplib.h"
 
 #include "common/protocol.h"
 #include "hostlist.h"
 #include "keymaplib.h"
 
 // Short version of `struct Keystroke`.
 struct Key {
   uint8_t bios;     // (AH) raw bios scan code
   uint8_t ascii;    // (AL) ascii value
   uint8_t flags;    // State of control key flags
   const char *name; // friendly name of non-modified key (4 chars max)
 };
 
 void process_stdin_session_mode(struct RawSocket *rs, struct RemoteHost *rh) {
 
   unsigned char seq_buf[SEQ_BUFFER_SIZE];
   char formatted_seq[SEQ_BUFFER_SIZE * 5]; 
   memset(seq_buf, 0, sizeof(seq_buf));
 
   int seq_len = 0;
   int y = rh->text_rows + 1;
 
   wtimeout(g_session_window, 0);
 
   while (1) {
     int ch = wgetch(g_session_window);
 
     // Store the initial ESC
     seq_buf[0] = ch; 
     seq_len = 1;     
 
     if (ch == ERR) {
       break;
     }
 
     // *** Check for Escape Character ***
     if (ch == 27) { // ESC detected
 
       // Set a short timeout (e.g., 40 milliseconds)
       // to wait for subsequent chars
       wtimeout(g_session_window, 40);
 
       int next_ch;
       while (seq_len < SEQ_BUFFER_SIZE - 1) {
         next_ch = wgetch(g_session_window);
         // Timeout expired - sequence finished
         if (next_ch == ERR) { 
           break;
         }
         seq_buf[seq_len++] = next_ch;
       }
 
       // clear status line
       mvwprintw(g_session_window, y, 0, "%*c", 80, ' ');
       mvwprintw(g_session_window, y + 1, 0, "%*c", 80, ' ');
 
       // Process the result
       if (seq_len == 1) {
         mvwprintw(g_session_window, y, 0, "ESC");
       } else {
         // Print the raw sequence (safely formatted)
         keymap_format_sequence(seq_buf, seq_len, formatted_seq,
                                sizeof(formatted_seq));
         mvwprintw(g_session_window, y, 0, "Seq: %s (len %d)", formatted_seq,
                   seq_len);
         keymap_format_sequence_hex(seq_buf, seq_len, formatted_seq,
                                    sizeof(formatted_seq));
         mvwprintw(g_session_window, y, 40, "Raw: %s", formatted_seq);
       }
     } else { // Not an ESC character
       if (ch >= ' ' && ch <= '~') {
         mvwprintw(g_session_window, y, 0, "Char: %c - %04x - %u", ch, ch, ch);
       } else {
         mvwprintw(g_session_window, y, 0, "Code: %d - %04x ", ch, ch);
       }
     }
 
     // CTRL+F12 to quit
     // Sequence: \x1b[24;5~ (len 7) Raw: \x1b\x5b\x32\x34\x3b\x35\x7e
     if (seq_len > 2) {
       if (strncmp((const char *)seq_buf, "\x1b\x5b\x32\x34\x5e", 7) == 0) {
         g_running = 0;
         return;
       }
       // CTRL+F11 reload keymap
       if (strncmp((const char *)seq_buf, "\x1b\x5b\x32\x33\x5e", 7) == 0) {
         // Reload keymap
         if (keymap_load(g_keymap_filename) < 0) {
           mvwprintw(g_session_window, y, 0, "KEYMAP: Failed to load keymap: %s", g_keymap_filename);
           fprintf(stderr, "Failed to load keymap\n");
           return;
         }
         mvwprintw(g_session_window, y, 0, "KEYMAP: Keymap %s reloaded", g_keymap_filename);
         return;
       }
     }
 
     ++y;
     // seq_buf[seq_len] = '\0'; // Null-terminate the sequence buffer
     KeyMapEntry_t *key = keymap_find(g_keymap, seq_buf, seq_len);
     if (key) {
       KeyDos_t keydos = key->keydos;
       // Found a key mapping
       mvwprintw(g_session_window, y, 0, "Key: %s - %04x - %u", key->description,
                 keydos.bios_scan_code, keydos.ascii_value);
       // Send the keystroke to the remote host
       const struct Keystroke ks = {
           .bios_scan_code = keydos.bios_scan_code,
           .ascii_value = keydos.ascii_value,
           .flags_17 = keydos.flags_17,
       };
       send_keystrokes(rs, g_active_host->if_addr, 1, &ks);
       break;
     } else {
       mvwprintw(g_session_window, y, 0, "Unmapped sequence: %s", seq_buf);
       break;
     }
   }
 }
 