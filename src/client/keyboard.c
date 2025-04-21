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

#include <ctype.h>
#include <ncurses.h>
#include <stdlib.h>

#include "client/curses.h"
#include "client/globals.h"
#include "client/keyboard.h"
#include "client/keysyms.h"
#include "common/protocol.h"

// Short version of `struct Keystroke`.
struct Key {
  uint8_t bios;     // (AH) raw bios scan code
  uint8_t ascii;    // (AL) ascii value
  uint8_t flags;    // State of control key flags
  const char *name; // friendly name of non-modified key (4 chars max)
};

// Maximum value returned in `wch` from `wget_wch(&wch)`.
#define WCH_MAX 0x250

// Filler for ncurses codes that we've not mapped yet.
#define UNKNOWN                                                                \
  { 0, 0, 0, NULL }

// NOTES:
// 1. Many key combos return "0x9b", including "alt-escape".
// 2. I am unable to manually verify the "ALT-Fn" keys, as most of them are
//    caught by my window manager (fluxbox) or terminal (xterm).
// 3. ALT-SHIFT-F[4..12] are mapped differently?

// Maps output parameters of 'wget_wch()' to raw BIOS keyboard data.
static const struct Key keymap[WCH_MAX] = {
    [0 ...(WCH_MAX - 1)] = {0, 0, 0},

    [0x000] = {0x03, 0x00, KS_CONTROL | KS_SHIFT, "2"},
    [0x001] = {SCAN_A, 'A' - '@', KS_CONTROL, "a"},
    [0x002] = {SCAN_B, 'B' - '@', KS_CONTROL, "b"},
    [0x003] = {SCAN_C, 'C' - '@', KS_CONTROL, "c"}, // SIGINT
    [0x004] = {SCAN_D, 'D' - '@', KS_CONTROL, "d"},
    [0x005] = {SCAN_E, 'E' - '@', KS_CONTROL, "e"},
    [0x006] = {SCAN_F, 'F' - '@', KS_CONTROL, "f"},
    [0x007] = {SCAN_G, 'G' - '@', KS_CONTROL, "g"},
    [0x008] = {SCAN_H, 'H' - '@', KS_CONTROL, "h"}, // seen as 0x107 (bs)
    [0x009] = {SCAN_TAB, '\t', 0, "tab"},
    [0x00a] = {SCAN_J, 'J' - '@', KS_CONTROL, "j"},
    [0x00b] = {SCAN_K, 'K' - '@', KS_CONTROL, "k"},
    [0x00c] = {SCAN_L, 'L' - '@', KS_CONTROL, "l"},
    [0x00d] = {SCAN_M, 'M' - '@', KS_CONTROL, "m"},
    [0x00e] = {SCAN_N, 'N' - '@', KS_CONTROL, "n"},
    [0x00e] = {SCAN_RETURN, '\n', 0, "entr"},
    [0x00f] = {SCAN_O, 'O' - '@', KS_CONTROL, "o"},

    [0x010] = {SCAN_P, 'P' - '@', KS_CONTROL, "p"},
    [0x011] = {SCAN_Q, 'Q' - '@', KS_CONTROL, "q"},
    [0x012] = {SCAN_RETURN, '\r', 0, "entr"},
    [0x012] = {SCAN_R, 'R' - '@', KS_CONTROL, "r"},
    [0x013] = {SCAN_S, 'S' - '@', KS_CONTROL, "s"},
    [0x014] = {SCAN_T, 'T' - '@', KS_CONTROL, "t"},
    [0x015] = {SCAN_U, 'U' - '@', KS_CONTROL, "u"},
    [0x016] = {SCAN_V, 'V' - '@', KS_CONTROL, "v"},
    [0x017] = {SCAN_W, 'W' - '@', KS_CONTROL, "w"},
    [0x018] = {SCAN_X, 'X' - '@', KS_CONTROL, "x"},
    [0x019] = {SCAN_Y, 'Y' - '@', KS_CONTROL, "y"},
    [0x01a] = {SCAN_Z, 'Z' - '@', KS_CONTROL, "z"}, // Send to background
    [0x01b] = {SCAN_ESC, 0x1b, 0, "esc"},
    [0x01c] = {0x2b, 0x1c, KS_CONTROL, "\\"}, // SIGQUIT
    [0x01d] = {0x1d, 0x1b, KS_CONTROL, "]"},
    [0x01e] = {0x07, 0x1e, KS_CONTROL, "6"},
    [0x01f] = {0x0c, 0x1f, KS_CONTROL | KS_SHIFT, "-"},

    [0x020] = {SCAN_SPACE, ' ', 0, "sp"},
    [0x021] = {SCAN_1, '!', KS_SHIFT, "1"},
    [0x022] = {SCAN_QUOTE, '"', KS_SHIFT, "'"},
    [0x023] = {SCAN_3, '#', KS_SHIFT, "3"},
    [0x024] = {SCAN_4, '$', KS_SHIFT, "4"},
    [0x025] = {SCAN_5, '%', KS_SHIFT, "5"},
    [0x026] = {SCAN_7, '&', KS_SHIFT, "7"},
    [0x027] = {SCAN_QUOTE, '\'', 0, "'"},
    [0x028] = {SCAN_9, '(', KS_SHIFT, "9"},
    [0x029] = {SCAN_0, ')', KS_SHIFT, "0"},
    [0x02a] = {SCAN_8, '*', KS_SHIFT, "8"},
    [0x02b] = {SCAN_EQ, '+', KS_SHIFT, "="},
    [0x02c] = {SCAN_COMMA, ',', 0, ","},
    [0x02d] = {SCAN_DASH, '-', 0, "-"},
    [0x02e] = {SCAN_PERIOD, '.', 0, "."},
    [0x02f] = {SCAN_FSLASH, '/', 0, "/"},

    [0x030] = {SCAN_0, '0', 0, "0"},
    [0x031] = {SCAN_1, '1', 0, "1"},
    [0x032] = {SCAN_2, '2', 0, "2"},
    [0x033] = {SCAN_3, '3', 0, "3"},
    [0x034] = {SCAN_4, '4', 0, "4"},
    [0x035] = {SCAN_5, '5', 0, "5"},
    [0x036] = {SCAN_6, '6', 0, "6"},
    [0x037] = {SCAN_7, '7', 0, "7"},
    [0x038] = {SCAN_8, '8', 0, "8"},
    [0x039] = {SCAN_9, '9', 0, "9"},
    [0x03a] = {SCAN_SEMICOLON, ':', KS_SHIFT, ";"},
    [0x03b] = {SCAN_SEMICOLON, ';', 0, ";"},
    [0x03c] = {SCAN_COMMA, '<', KS_SHIFT, ","},
    [0x03d] = {SCAN_EQ, '=', 0, "="},
    [0x03e] = {SCAN_PERIOD, '>', KS_SHIFT, "."},
    [0x03f] = {SCAN_FSLASH, '?', KS_SHIFT, "/"},

    [0x040] = {SCAN_2, '@', KS_SHIFT, "2"},
    [0x041] = {SCAN_A, 'A', KS_SHIFT, "a"},
    [0x042] = {SCAN_B, 'B', KS_SHIFT, "b"},
    [0x043] = {SCAN_C, 'C', KS_SHIFT, "c"},
    [0x044] = {SCAN_D, 'D', KS_SHIFT, "d"},
    [0x045] = {SCAN_E, 'E', KS_SHIFT, "e"},
    [0x046] = {SCAN_F, 'F', KS_SHIFT, "f"},
    [0x047] = {SCAN_G, 'G', KS_SHIFT, "g"},
    [0x048] = {SCAN_H, 'H', KS_SHIFT, "h"},
    [0x049] = {SCAN_I, 'I', KS_SHIFT, "i"},
    [0x04a] = {SCAN_J, 'J', KS_SHIFT, "j"},
    [0x04b] = {SCAN_K, 'K', KS_SHIFT, "k"},
    [0x04c] = {SCAN_L, 'L', KS_SHIFT, "l"},
    [0x04d] = {SCAN_M, 'M', KS_SHIFT, "m"},
    [0x04e] = {SCAN_N, 'N', KS_SHIFT, "n"},
    [0x04f] = {SCAN_O, 'O', KS_SHIFT, "o"},

    [0x050] = {SCAN_P, 'P', KS_SHIFT, "p"},
    [0x051] = {SCAN_Q, 'Q', KS_SHIFT, "q"},
    [0x052] = {SCAN_R, 'R', KS_SHIFT, "r"},
    [0x053] = {SCAN_S, 'S', KS_SHIFT, "s"},
    [0x054] = {SCAN_T, 'T', KS_SHIFT, "t"},
    [0x055] = {SCAN_U, 'U', KS_SHIFT, "u"},
    [0x056] = {SCAN_V, 'V', KS_SHIFT, "v"},
    [0x057] = {SCAN_W, 'W', KS_SHIFT, "w"},
    [0x058] = {SCAN_X, 'X', KS_SHIFT, "x"},
    [0x059] = {SCAN_Y, 'Y', KS_SHIFT, "y"},
    [0x05a] = {SCAN_Z, 'Z', KS_SHIFT, "z"},
    [0x05b] = {SCAN_LBRACE, '[', 0, "["},
    [0x05c] = {SCAN_BSLASH, '\\', 0, "\\"},
    [0x05d] = {SCAN_RBRACE, ']', 0, "]"},
    [0x05e] = {SCAN_6, '^', KS_SHIFT, "6"},
    [0x05f] = {SCAN_DASH, '_', KS_SHIFT, "-"},

    [0x060] = {SCAN_TILDE, '`', 0, "`"},
    [0x061] = {SCAN_A, 'a', 0, "a"},
    [0x062] = {SCAN_B, 'b', 0, "b"},
    [0x063] = {SCAN_C, 'c', 0, "c"},
    [0x064] = {SCAN_D, 'd', 0, "d"},
    [0x065] = {SCAN_E, 'e', 0, "e"},
    [0x066] = {SCAN_F, 'f', 0, "f"},
    [0x067] = {SCAN_G, 'g', 0, "g"},
    [0x068] = {SCAN_H, 'h', 0, "h"},
    [0x069] = {SCAN_I, 'i', 0, "i"},
    [0x06a] = {SCAN_J, 'j', 0, "j"},
    [0x06b] = {SCAN_K, 'k', 0, "k"},
    [0x06c] = {SCAN_L, 'l', 0, "l"},
    [0x06d] = {SCAN_M, 'm', 0, "m"},
    [0x06e] = {SCAN_N, 'n', 0, "n"},
    [0x06f] = {SCAN_O, 'o', 0, "o"},

    [0x070] = {SCAN_P, 'p', 0, "p"},
    [0x071] = {SCAN_Q, 'q', 0, "q"},
    [0x072] = {SCAN_R, 'r', 0, "r"},
    [0x073] = {SCAN_S, 's', 0, "s"},
    [0x074] = {SCAN_T, 't', 0, "t"},
    [0x075] = {SCAN_U, 'u', 0, "u"},
    [0x076] = {SCAN_V, 'v', 0, "v"},
    [0x077] = {SCAN_W, 'w', 0, "w"},
    [0x078] = {SCAN_X, 'x', 0, "x"},
    [0x079] = {SCAN_Y, 'y', 0, "y"},
    [0x07a] = {SCAN_Z, 'z', 0, "z"},
    [0x07b] = {SCAN_LBRACE, '{', KS_SHIFT, "["},
    [0x07c] = {SCAN_BSLASH, '|', KS_SHIFT, "\\"},
    [0x07d] = {SCAN_RBRACE, '}', KS_SHIFT, "]"},
    [0x07e] = {SCAN_TILDE, '~', KS_SHIFT, "`"},
    [0x07f] = {SCAN_BS, 0x7f, KS_CONTROL, "bs"},

    [0x080] = {SCAN_TILDE, 0, KS_ALT | KS_CONTROL, "`"},
    [0x081] = {SCAN_A, 0, KS_ALT | KS_CONTROL, "a"},
    [0x082] = {SCAN_B, 0, KS_ALT | KS_CONTROL, "b"},
    [0x083] = {SCAN_C, 0, KS_ALT | KS_CONTROL, "c"},
    [0x084] = {SCAN_D, 0, KS_ALT | KS_CONTROL, "d"},
    [0x085] = {SCAN_E, 0, KS_ALT | KS_CONTROL, "e"},
    [0x086] = {SCAN_F, 0, KS_ALT | KS_CONTROL, "f"},
    [0x087] = {SCAN_G, 0, KS_ALT | KS_CONTROL, "g"},
    [0x088] = {SCAN_H, 0, KS_ALT | KS_CONTROL, "h"},
    [0x089] = {SCAN_I, 0, KS_ALT | KS_CONTROL, "i"},
    [0x08a] = {SCAN_J, 0, KS_ALT | KS_CONTROL, "j"},
    [0x08b] = {SCAN_K, 0, KS_ALT | KS_CONTROL, "k"},
    [0x08c] = {SCAN_L, 0, KS_ALT | KS_CONTROL, "l"},
    [0x08d] = {SCAN_M, 0, KS_ALT | KS_CONTROL, "m"},
    [0x08e] = {SCAN_N, 0, KS_ALT | KS_CONTROL, "n"},
    [0x08f] = {SCAN_O, 0, KS_ALT | KS_CONTROL, "o"},

    [0x090] = {SCAN_P, 0, KS_ALT | KS_CONTROL, "p"},
    [0x091] = {SCAN_Q, 0, KS_ALT | KS_CONTROL, "q"},
    [0x092] = {SCAN_R, 0, KS_ALT | KS_CONTROL, "r"},
    [0x093] = {SCAN_S, 0, KS_ALT | KS_CONTROL, "s"},
    [0x094] = {SCAN_T, 0, KS_ALT | KS_CONTROL, "t"},
    [0x095] = {SCAN_U, 0, KS_ALT | KS_CONTROL, "u"},
    [0x096] = {SCAN_V, 0, KS_ALT | KS_CONTROL, "v"},
    [0x097] = {SCAN_W, 0, KS_ALT | KS_CONTROL, "w"},
    [0x098] = {SCAN_X, 0, KS_ALT | KS_CONTROL, "x"},
    [0x099] = {SCAN_Y, 0, KS_ALT | KS_CONTROL, "y"},
    [0x09a] = {SCAN_Z, 0, KS_ALT | KS_CONTROL, "z"},
    [0x09b] = {SCAN_ESC, 0, KS_ALT | KS_CONTROL, "????"}, // See notes
    [0x09c] = {SCAN_BSLASH, 0, KS_ALT | KS_CONTROL, "\\"},
    [0x09d] = UNKNOWN,
    [0x09e] = {SCAN_6, 0, KS_ALT | KS_CONTROL | KS_SHIFT, "6"}, // C-A-S ^
    [0x09f] = {SCAN_FSLASH, 0, KS_ALT | KS_CONTROL},

    [0x0a0] = UNKNOWN,
    [0x0a1] = {0x78, 0, KS_ALT | KS_SHIFT, "1"},
    [0x0a2] = {SCAN_QUOTE, 0, KS_ALT | KS_SHIFT, "'"},
    [0x0a3] = {0x7a, 0, KS_ALT | KS_SHIFT, "3"},
    [0x0a4] = {0x7b, 0, KS_ALT | KS_SHIFT, "4"},
    [0x0a5] = {0x7c, 0, KS_ALT | KS_SHIFT, "5"},
    [0x0a6] = {0x7e, 0, KS_ALT | KS_SHIFT, "7"},
    [0x0a7] = {SCAN_QUOTE, 0, KS_ALT, "'"},
    [0x0a8] = {0x80, 0, KS_ALT | KS_SHIFT, "9"},
    [0x0a9] = {0x81, 0, KS_ALT | KS_SHIFT, "0"},
    [0x0aa] = {0x7f, 0, KS_ALT | KS_SHIFT, "8"},
    [0x0ab] = {0x83, 0, KS_ALT | KS_SHIFT, "="},
    [0x0ac] = {SCAN_COMMA, 0, KS_ALT, ","},
    [0x0ad] = {SCAN_DASH, 0, KS_ALT, "-"},
    [0x0ae] = {SCAN_PERIOD, 0, KS_ALT, "."},
    [0x0af] = UNKNOWN,

    [0x0b0] = {0x81, 0, KS_ALT, "0"},
    [0x0b1] = {0x78, 0, KS_ALT, "1"},
    [0x0b2] = {0x79, 0, KS_ALT, "2"},
    [0x0b3] = {0x7a, 0, KS_ALT, "3"},
    [0x0b4] = {0x7b, 0, KS_ALT, "4"},
    [0x0b5] = {0x7c, 0, KS_ALT, "5"},
    [0x0b6] = {0x7d, 0, KS_ALT, "6"},
    [0x0b7] = {0x7e, 0, KS_ALT, "7"},
    [0x0b8] = {0x7f, 0, KS_ALT, "8"},
    [0x0b9] = {0x80, 0, KS_ALT, "9"},
    [0x0ba] = {SCAN_SEMICOLON, 0, KS_ALT | KS_SHIFT, ";"},
    [0x0bb] = {SCAN_SEMICOLON, 0, KS_ALT, ";"}, // dup of 0x0ba
    [0x0bc] = {SCAN_COMMA, 0, KS_ALT | KS_SHIFT, ","},
    [0x0bd] = {0x83, 0, KS_ALT, "="},
    [0x0be] = {SCAN_PERIOD, 0, KS_ALT | KS_SHIFT, "."},
    [0x0bf] = {SCAN_FSLASH, 0, KS_ALT | KS_SHIFT, "/"},

    [0x0c0] = {0x79, 0, KS_ALT | KS_SHIFT, "2"},
    [0x0c1] = {SCAN_A, 0, KS_ALT | KS_SHIFT, "a"},
    [0x0c2] = {SCAN_B, 0, KS_ALT | KS_SHIFT, "b"},
    [0x0c3] = {SCAN_C, 0, KS_ALT | KS_SHIFT, "c"},
    [0x0c4] = {SCAN_D, 0, KS_ALT | KS_SHIFT, "d"},
    [0x0c5] = {SCAN_E, 0, KS_ALT | KS_SHIFT, "e"},
    [0x0c6] = {SCAN_F, 0, KS_ALT | KS_SHIFT, "f"},
    [0x0c7] = {SCAN_G, 0, KS_ALT | KS_SHIFT, "g"},
    [0x0c8] = {SCAN_H, 0, KS_ALT | KS_SHIFT, "h"},
    [0x0c9] = {SCAN_I, 0, KS_ALT | KS_SHIFT, "i"},
    [0x0ca] = {SCAN_J, 0, KS_ALT | KS_SHIFT, "j"},
    [0x0cb] = {SCAN_K, 0, KS_ALT | KS_SHIFT, "k"},
    [0x0cc] = {SCAN_L, 0, KS_ALT | KS_SHIFT, "l"},
    [0x0cd] = {SCAN_M, 0, KS_ALT | KS_SHIFT, "m"},
    [0x0ce] = {SCAN_N, 0, KS_ALT | KS_SHIFT, "n"},
    [0x0cf] = {SCAN_O, 0, KS_ALT | KS_SHIFT, "o"},

    [0x0d0] = {SCAN_P, 0, KS_ALT | KS_SHIFT, "p"},
    [0x0d1] = {SCAN_Q, 0, KS_ALT | KS_SHIFT, "q"},
    [0x0d2] = {SCAN_R, 0, KS_ALT | KS_SHIFT, "r"},
    [0x0d3] = {SCAN_S, 0, KS_ALT | KS_SHIFT, "s"},
    [0x0d4] = {SCAN_T, 0, KS_ALT | KS_SHIFT, "t"},
    [0x0d5] = {SCAN_U, 0, KS_ALT | KS_SHIFT, "u"},
    [0x0d6] = {SCAN_V, 0, KS_ALT | KS_SHIFT, "v"},
    [0x0d7] = {SCAN_W, 0, KS_ALT | KS_SHIFT, "w"},
    [0x0d8] = {SCAN_X, 0, KS_ALT | KS_SHIFT, "x"},
    [0x0d9] = {SCAN_Y, 0, KS_ALT | KS_SHIFT, "y"},
    [0x0da] = {SCAN_Z, 0, KS_ALT | KS_SHIFT, "z"},
    [0x0db] = {SCAN_LBRACE, 0, KS_ALT, "["},
    [0x0dc] = {SCAN_BSLASH, 0, KS_ALT, "\\"},
    [0x0dd] = {SCAN_RBRACE, 0, KS_ALT, "]"},
    [0x0de] = {0x7d, 0, KS_ALT | KS_SHIFT, "6"},
    [0x0df] = UNKNOWN,

    [0x0e0] = {SCAN_TILDE, 0, KS_ALT, "`"},
    [0x0e1] = {SCAN_A, 0, KS_ALT, "a"},
    [0x0e2] = {SCAN_B, 0, KS_ALT, "b"},
    [0x0e3] = {SCAN_C, 0, KS_ALT, "c"},
    [0x0e4] = {SCAN_D, 0, KS_ALT, "d"},
    [0x0e5] = {SCAN_E, 0, KS_ALT, "e"},
    [0x0e6] = {SCAN_F, 0, KS_ALT, "f"},
    [0x0e7] = {SCAN_G, 0, KS_ALT, "g"},
    [0x0e8] = {SCAN_H, 0, KS_ALT, "h"},
    [0x0e9] = {SCAN_I, 0, KS_ALT, "i"},
    [0x0ea] = {SCAN_J, 0, KS_ALT, "j"},
    [0x0eb] = {SCAN_K, 0, KS_ALT, "k"},
    [0x0ec] = {SCAN_L, 0, KS_ALT, "l"},
    [0x0ed] = {SCAN_M, 0, KS_ALT, "m"},
    [0x0ee] = {SCAN_N, 0, KS_ALT, "n"},
    [0x0ef] = {SCAN_O, 0, KS_ALT, "o"},

    [0x0f0] = {SCAN_P, 0, KS_ALT, "p"},
    [0x0f1] = {SCAN_Q, 0, KS_ALT, "q"},
    [0x0f2] = {SCAN_R, 0, KS_ALT, "r"},
    [0x0f3] = {SCAN_S, 0, KS_ALT, "s"},
    [0x0f4] = {SCAN_T, 0, KS_ALT, "t"},
    [0x0f5] = {SCAN_U, 0, KS_ALT, "u"},
    [0x0f6] = {SCAN_V, 0, KS_ALT, "v"},
    [0x0f7] = {SCAN_W, 0, KS_ALT, "w"},
    [0x0f8] = {SCAN_X, 0, KS_ALT, "x"},
    [0x0f9] = {SCAN_Y, 0, KS_ALT, "y"},
    [0x0fa] = {SCAN_Z, 0, KS_ALT, "z"},
    [0x0fb] = {SCAN_LBRACE, 0, KS_ALT | KS_SHIFT, "["},
    [0x0fc] = {SCAN_BSLASH, 0, KS_ALT | KS_SHIFT, "\\"},
    [0x0fd] = {SCAN_RBRACE, 0, KS_ALT | KS_SHIFT, "]"},
    [0x0fe] = UNKNOWN,
    [0x0ff] = UNKNOWN,

    [0x100] = UNKNOWN,
    [0x101] = UNKNOWN,
    [0x102] = {SCAN_KEYPAD_2, 0, 0, "down"}, // keypad "down"
    [0x103] = {SCAN_KEYPAD_8, 0, 0, "up"},   // keypad "up"
    [0x104] = {SCAN_KEYPAD_4, 0, 0, "left"}, // keypad "left"
    [0x105] = {SCAN_KEYPAD_6, 0, 0, "righ"}, // keypad "right"
    [0x106] = {SCAN_KEYPAD_7, 0, 0, "home"}, // keypad "home"
    [0x107] = {SCAN_BS, 0x08, 0, "bs"},
    [0x108] = UNKNOWN,

    [0x109] = {SCAN_F1, 0, 0, "f1"},
    [0x10a] = {SCAN_F2, 0, 0, "f2"},
    [0x10b] = {SCAN_F3, 0, 0, "f3"},
    [0x10c] = {SCAN_F4, 0, 0, "f4"},
    [0x10d] = {SCAN_F5, 0, 0, "f5"},
    [0x10e] = {SCAN_F6, 0, 0, "f6"},
    [0x10f] = {SCAN_F7, 0, 0, "f7"},
    [0x110] = {SCAN_F8, 0, 0, "f8"},
    [0x111] = {SCAN_F9, 0, 0, "f9"},
    [0x112] = {SCAN_F10, 0, 0, "f10"},
    [0x113] = {SCAN_F11, 0, 0, "f11"},
    [0x114] = {SCAN_F12, 0, 0, "f12"},

    [0x115] = {SCAN_F1, 0, KS_SHIFT, "f1"},
    [0x116] = {SCAN_F2, 0, KS_SHIFT, "f2"},
    [0x117] = {SCAN_F3, 0, KS_SHIFT, "f3"},
    [0x118] = {SCAN_F4, 0, KS_SHIFT, "f4"},
    [0x119] = {SCAN_F5, 0, KS_SHIFT, "f5"},
    [0x11a] = {SCAN_F6, 0, KS_SHIFT, "f6"},
    [0x11b] = {SCAN_F7, 0, KS_SHIFT, "f7"},
    [0x11c] = {SCAN_F8, 0, KS_SHIFT, "f8"},
    [0x11d] = {SCAN_F9, 0, KS_SHIFT, "f9"},
    [0x11e] = {SCAN_F10, 0, KS_SHIFT, "f10"},
    [0x11f] = {SCAN_F11, 0, KS_SHIFT, "f11"},
    [0x120] = {SCAN_F12, 0, KS_SHIFT, "f12"},

    [0x121] = {SCAN_F1, 0, KS_CONTROL, "f1"},
    [0x122] = {SCAN_F2, 0, KS_CONTROL, "f2"},
    [0x123] = {SCAN_F3, 0, KS_CONTROL, "f3"},
    [0x124] = {SCAN_F4, 0, KS_CONTROL, "f4"},
    [0x125] = {SCAN_F5, 0, KS_CONTROL, "f5"},
    [0x126] = {SCAN_F6, 0, KS_CONTROL, "f6"},
    [0x127] = {SCAN_F7, 0, KS_CONTROL, "f7"},
    [0x128] = {SCAN_F8, 0, KS_CONTROL, "f8"},
    [0x129] = {SCAN_F9, 0, KS_CONTROL, "f9"},
    [0x12a] = {SCAN_F10, 0, KS_CONTROL, "f10"},
    [0x12b] = {SCAN_F11, 0, KS_CONTROL, "f11"},
    [0x12c] = {SCAN_F12, 0, KS_CONTROL, "f12"},

    [0x12d] = {SCAN_F1, 0, KS_SHIFT | KS_CONTROL, "f1"},
    [0x12e] = {SCAN_F2, 0, KS_SHIFT | KS_CONTROL, "f2"},
    [0x12f] = {SCAN_F3, 0, KS_SHIFT | KS_CONTROL, "f3"},
    [0x130] = {SCAN_F4, 0, KS_SHIFT | KS_CONTROL, "f4"},
    [0x131] = {SCAN_F5, 0, KS_SHIFT | KS_CONTROL, "f5"},
    [0x132] = {SCAN_F6, 0, KS_SHIFT | KS_CONTROL, "f6"},
    [0x133] = {SCAN_F7, 0, KS_SHIFT | KS_CONTROL, "f7"},
    [0x134] = {SCAN_F8, 0, KS_SHIFT | KS_CONTROL, "f8"},
    [0x135] = {SCAN_F9, 0, KS_SHIFT | KS_CONTROL, "f9"},
    [0x136] = {SCAN_F10, 0, KS_SHIFT | KS_CONTROL, "f10"},
    [0x137] = {SCAN_F11, 0, KS_SHIFT | KS_CONTROL, "f11"},
    [0x138] = {SCAN_F12, 0, KS_SHIFT | KS_CONTROL, "f12"},

    [0x139] = {SCAN_F1, 0, KS_ALT, "f1"},
    [0x13a] = {SCAN_F2, 0, KS_ALT, "f2"},
    [0x13b] = {SCAN_F3, 0, KS_ALT, "f3"},
    [0x13c] = {SCAN_F4, 0, KS_ALT, "f4"},
    [0x13d] = {SCAN_F5, 0, KS_ALT, "f5"},
    [0x13e] = {SCAN_F6, 0, KS_ALT, "f6"},
    [0x13f] = {SCAN_F7, 0, KS_ALT, "f7"},
    [0x140] = {SCAN_F8, 0, KS_ALT, "f8"},
    [0x141] = {SCAN_F9, 0, KS_ALT, "f9"},
    [0x142] = {SCAN_F10, 0, KS_ALT, "f10"},
    [0x143] = {SCAN_F11, 0, KS_ALT, "f11"},
    [0x144] = {SCAN_F12, 0, KS_ALT, "f12"},

    [0x145] = {SCAN_F1, 0, KS_ALT | KS_SHIFT, "f1"},
    [0x146] = {SCAN_F2, 0, KS_ALT | KS_SHIFT, "f2"},
    [0x147] = {SCAN_F3, 0, KS_ALT | KS_SHIFT, "f3"},
    //  [0x148] = {SCAN_F4, 0, KS_ALT | KS_SHIFT, "f4"},   // unconfirmed
    //  [0x149] = {SCAN_F5, 0, KS_ALT | KS_SHIFT, "f5"},   // unconfirmed
    //  [0x14a] = {SCAN_F6, 0, KS_ALT | KS_SHIFT, "f6"},   // unconfirmed
    [0x14a] = {SCAN_KEYPAD_PERIOD, 0, 0, "del"}, // keypad "delete"
    //  [0x14b] = {SCAN_F7, 0, KS_ALT | KS_SHIFT, "f7"},   // unconfirmed
    [0x14b] = {SCAN_KEYPAD_0, 0, 0, "ins"}, // keypad "insert"
    //  [0x14c] = {SCAN_F8, 0, KS_ALT | KS_SHIFT, "f8"},   // unconfirmed
    //  [0x14d] = {SCAN_F9, 0, KS_ALT | KS_SHIFT, "f9"},   // unconfirmed
    //  [0x14e] = {SCAN_F10, 0, KS_ALT | KS_SHIFT, "f10"}, // unconfirmed
    //  [0x14f] = {SCAN_F11, 0, KS_ALT | KS_SHIFT, "f11"}, // unconfirmed
    //  [0x150] = {SCAN_F12, 0, KS_ALT | KS_SHIFT, "f12"}, // unconfirmed

    [0x150] = {0xa0, 0, KS_SHIFT, "down"},
    [0x151] = {0x8d, 0, KS_SHIFT, "up"},
    [0x152] = {SCAN_KEYPAD_3, 0, 0, "pgdn"},  // keypad "page down"
    [0x153] = {SCAN_KEYPAD_9, 0, 0, "pgup"},  // keypad "page up"
    [0x157] = {SCAN_RETURN, 0x0d, 0, "entr"}, // keypad "enter"

    [0x162] = {SCAN_KEYPAD_5, 0, 0, "5"},   // keypad "5"
    [0x168] = {SCAN_KEYPAD_1, 0, 0, "end"}, // keypad "end"

    [0x182] = {0x9f, 0, KS_SHIFT, "end"},
    [0x187] = {0x97, 0, KS_SHIFT, "home"},
    [0x189] = {0x73, 0, KS_SHIFT, "left"},
    [0x192] = {0x74, 0, KS_SHIFT, "righ"},

    [0x206] = {0xa3, 0, KS_ALT, "del"},
    [0x207] = {0xa3, 0, KS_ALT | KS_SHIFT, "del"},
    [0x208] = {0x93, 0, KS_CONTROL, "del"},
    [0x209] = {0x93, 0, KS_CONTROL | KS_SHIFT, "del"},

    [0x20c] = {0xa0, 0, KS_ALT, "down"},
    [0x20e] = {0x91, 0, KS_CONTROL, "down"},

    [0x211] = {0x9f, 0, KS_ALT, "end"},
    [0x212] = {0x9f, 0, KS_ALT | KS_SHIFT, "end"},
    [0x213] = {0x75, 0, KS_CONTROL, "end"},
    [0x214] = {0x75, 0, KS_CONTROL | KS_SHIFT, "end"},

    [0x216] = {0x97, 0, KS_ALT, "home"},
    [0x217] = {0x97, 0, KS_ALT | KS_SHIFT, "home"},
    [0x218] = {0x77, 0, KS_CONTROL, "home"},
    [0x219] = {0x77, 0, KS_CONTROL | KS_SHIFT, "home"},

    [0x21b] = {0xa2, 0, KS_ALT, "ins"},
    [0x21d] = {0x92, 0, KS_CONTROL, "ins"},
    [0x21f] = {0xa2, 0, KS_ALT | KS_CONTROL, "ins"},

    [0x220] = {0x9b, 0, KS_ALT, "left"},
    [0x222] = {0x73, 0, KS_CONTROL, "left"},

    [0x225] = {0xa1, 0, KS_ALT, "pgdn"},
    [0x227] = {0x76, 0, KS_CONTROL, "pgdn"},

    [0x22a] = {0x99, 0, KS_ALT, "pgup"},
    [0x22c] = {0x84, 0, KS_CONTROL, "pgup"},

    [0x22f] = {0x9d, 0, KS_ALT, "righ"},
    [0x231] = {0x74, 0, KS_CONTROL, "righ"},

    [0x235] = {0x98, 0, KS_ALT, "up"},
    [0x237] = {0x8d, 0, KS_CONTROL, "up"},

    [0x23f] = {SCAN_KEYPAD_PLUS, 0, 0, "add"}, // keypad "+"
    [0x241] = {SCAN_FSLASH, '/', 0, "div"},    // keypad "/"
    [0x243] = {SCAN_MUL, '*', 0, "mul"},       // keypad "*"
    [0x243] = {SCAN_MUL, '*', 0, "mul"},       // keypad '*'
    [0x244] = {0x4a, '-', 0, "sub"},           // keypad "-"
};

void process_stdin_session_mode(struct RawSocket *rs) {
  wint_t wch = 0;

  switch (wget_wch(g_session_window, &wch)) {
    case KEY_CODE_YES:
      mvwprintw(g_session_window, 52, 1, "KEY_CODE_YES: %04x", wch);
      break;

    case OK:
      mvwprintw(g_session_window, 52, 1, "OK:           %04x", wch);
      break;

    case ERR:
      return;

    default:
      abort();
  }

  if (wch == EXIT_WCH_CODE || wch == KEY_F(12)) {
    g_running = 0;
    return;
  }

  if ((wch > 0) && (wch < WCH_MAX)) {
    const struct Keystroke ks = {
        .bios_scan_code = keymap[wch].bios,
        .ascii_value = keymap[wch].ascii,
        .flags_17 = keymap[wch].flags,
    };

    if (keymap[wch].bios || keymap[wch].ascii) {
      send_keystrokes(rs, g_active_host->if_addr, 1, &ks);

      mvwprintw(g_session_window, 53, 1, "%*c", 30, ' ');
      return;
    }
  }

  mvwprintw(g_session_window, 53, 1, "Unmapped wch: %04x", wch);
}

void dump_keyboard_table(FILE *fp) {
  static const char GREEN[] = "\x1b[32m";
  static const char YELLOW[] = "\x1b[33m";
  static const char CLEAR[] = "\x1b[0m";

  int row = 0;
  int col = 0;

  for (int i = 0; i < WCH_MAX; ++i) {
    if (row > 3) {
      fprintf(fp, "\n");
      row = 0;
      col = 0;
    }

    // Print row header every line.
    if (col == 0) {
      fprintf(fp, "%04x: ", i);
    }

    if (keymap[i].bios || keymap[i].ascii) {
      fprintf(fp, "  %02x %02x %1x", keymap[i].bios, keymap[i].ascii,
              keymap[i].flags);

      fprintf(fp, " %s%c", GREEN,
              isprint(keymap[i].ascii) ? keymap[i].ascii : ' ');

      fprintf(fp, " %s%-4.4s", YELLOW, keymap[i].name ? keymap[i].name : "");
      fprintf(fp, "%s", CLEAR);
    } else {
      fprintf(fp, "  -- -- - - ----");
    }
    ++col;

    if (col >= 4) {
      fprintf(fp, "\n");
      col = 0;
      ++row;
    }
  }

  fprintf(fp, "\n");
}
