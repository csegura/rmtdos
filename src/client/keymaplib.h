/*
 * Copyright 2025 Carlos Segura <romheat@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KEYMAPLIB_H
#define KEYMAPLIB_H

#include <ctype.h>
#include <locale.h>
#include <ncursesw/ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// BIOS Flags (as used in the map file)
#ifndef KS_ALT
#define KS_RIGHT_SHIFT 0x01
#define KS_LEFT_SHIFT 0x02
#define KS_SHIFT 0x03
#define KS_CONTROL 0x04
#define KS_ALT 0x08
#endif

// rmtdos KeyDos
typedef struct {
  uint8_t bios_scan_code;
  uint8_t ascii_value;
  uint8_t flags_17;
} KeyDos_t;

typedef struct KeyMapEntry {
  char *description;        // Debugging description
  unsigned char *sequence;  // Raw byte sequence from terminal
  size_t sequence_len;      // Length of the sequence
  KeyDos_t keydos;          // DOS keystroke data
  struct KeyMapEntry *next; // Linked list pointer
} KeyMapEntry_t;

// Keymap hash table
#define KEYMAP_HASH_SIZE 256

typedef struct KeyMapNode {
    KeyMapEntry_t *entry;
    struct KeyMapNode *next;
} KeyMapNode_t;



// keymap head 
extern KeyMapEntry_t *g_keymap;
extern char *g_keymap_filename;


// Helper functions
char *_trim(char *str);
int _parse_hex_sequence(const char *hex_str, unsigned char **out_bytes);

// Keymap functions
int keymap_load(const char *filename);
int keymap_save(const char *filename);

KeyMapEntry_t *keymap_find(KeyMapEntry_t *list, unsigned char *captured_seq,
                           size_t captured_len);
void keymap_format_sequence(unsigned char *seq, int len, char *out_buf,
                            size_t out_size);
void keymap_format_sequence_hex(unsigned char *seq, int len, char *out_buf,
                                size_t out_size);

void keymap_free(void);

#define SEQ_BUFFER_SIZE 32 // Max length of escape sequence to read

#endif 