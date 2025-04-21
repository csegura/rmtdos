// dont remove defines below, they are needed for ncursesw & strdup
/*
 * Copyright 2025 Carlos Segura <romheat@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define _DEFAULT_SOURCE 1
#define _XOPEN_SOURCE_EXTENDED 1
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <stdint.h>
#include <ncursesw/ncurses.h>

#include "client/keymaplib.h"


// --- Main Program ---
int main() {
    setlocale(LC_ALL, "");
    setenv("ESCDELAY", "25", 1);

    if (initscr() == NULL) {
        fprintf(stderr, "Error initializing ncurses.\n");
        return 1;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    g_keymap_filename = "keymap.map";
    if (keymap_load(g_keymap_filename) != 0) {
        endwin();
        fprintf(stderr, "Failed to load keymap %s.\n",
                g_keymap_filename);
        return 1;
    }

    // (Optional) You may add an interactive loop here to capture sequences.
    // For now we simply print all loaded keymap entries once ncurses is ended.
    endwin();

    printf("Loaded Keymap Entries:\n");
    KeyMapEntry_t *current = g_keymap;
    while (current) {
        printf(" Entry: '%-15.15s' -> Seq[%02zu]  | Scan: 0x%02x Ascii: 0x%02x Flags: 0x%02x | ",
               current->description, current->sequence_len, 
               current->keydos.bios_scan_code, current->keydos.ascii_value,
               current->keydos.flags_17);        
        for (size_t i = 0; i < current->sequence_len; ++i) {
            printf("\\x%02x", current->sequence[i]);
        }
        printf("\n");
        current = current->next;
    }

    keymap_free();
    return 0;
}