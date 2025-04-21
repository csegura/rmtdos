/*
 * Copyright 2025 Carlos Segura <romheat@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <ctype.h>
#include <limits.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../client/keymaplib.h"

// UI State
int g_cur_itm_idx = 0;
int g_top_itm_idx = 0;
int g_map_entries = 0;
int g_dirty_flag  = 0; // 0 = clean, 1 = modified

// UI Style
#define HIGHLIGHT_PAIR 1
#define CAPTURE_PAIR 2
#define MODIFIED_PAIR 3

typedef enum {
    HIGHLIGHT_NORMAL,
    HIGHLIGHT_CAPTURE,
    HIGHLIGHT_MODIFIED
} HighlightStyle;


// Hold a copy of the original Keymaps

typedef struct {
    unsigned char* data;
    size_t len;
    bool is_modified;
} OriginalSeqData;

// Global array to store the original sequences, parallel to g_keymap list order
OriginalSeqData* g_org_seq = NULL;

char *KeyFlags[] = {
    "", "", "",
    "KS_SHIFT", // 0x03
    "KS_CONTROL", // 0x04
    "", "", 
    "KS_SHIFT + KS_CONTROL", // 0x07
    "KS_ALT", // 0x08
};


// Count entries in the global linked list
int count_map_entries() {
    int count              = 0;
    KeyMapEntry_t* current = g_keymap;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

// Get a pointer to the entry at a specific index (0-based)
KeyMapEntry_t* get_entry_at_index(int index) {
    if (index < 0)
        return NULL;
    KeyMapEntry_t* current = g_keymap;
    int i                  = 0;
    while (current && i < index) {
        current = current->next;
        i++;
    }
    return (i == index) ? current : NULL;
}


// --- ncurses UI Functions ---

void display_status(const char* msg) {
    move(LINES - 1, 1); // Bottom line
    clrtoeol();
    attron(A_REVERSE);
    mvprintw(LINES - 1, 1, "%s", msg);
    attroff(A_REVERSE);
    refresh();
}

void clear_status(void) {
    move(LINES - 1, 1);
    clrtoeol();
    refresh();
}

char *format_flags(uint8_t flags) {
    return KeyFlags[flags];
}

void display_map_list(HighlightStyle style) {
    erase();
    int term_width = getmaxx(stdscr);
    mvprintw(0, 1, "Keymap Config: %s %s", g_keymap_filename ? g_keymap_filename : "<No File>",
    g_dirty_flag ? "[MODIFIED]" : "");
    mvhline(1, 1, '-', term_width > 2 ? term_width - 2 : 0);
    
    
    attron(A_REVERSE);
    mvhline(2, 1, ' ', term_width > 2 ? term_width - 2 : 0);
    mvprintw(2, 1, " Description       | Original Sequence    | Current Sequence     | Scan | Ascii| Flags");
    mvhline(LINES - 2, 1, ' ', term_width > 2 ? term_width - 2 : 0);
    mvprintw(LINES - 2, 1, "Up/Down: Navigate | Enter: Edit KEY | S: Save | Q: Quit");
    attroff(A_REVERSE);

    int list_height = LINES - 5;
    if (list_height < 1)
        list_height = 1;
    int start_row = 3;
    char orig_seq_hex_buf[128];
    char curr_seq_hex_buf[128];

    for (int i = 0; i < list_height; ++i) {
        int map_index = g_top_itm_idx + i;
        if (map_index >= g_map_entries)
            break;

        KeyMapEntry_t* entry       = get_entry_at_index(map_index);
        OriginalSeqData* orig_info = &g_org_seq[map_index];
        if (!entry)
            continue;

        // Format sequences
        keymap_format_sequence_hex(orig_info->data, orig_info->len,
        orig_seq_hex_buf, sizeof(orig_seq_hex_buf));
        keymap_format_sequence_hex(entry->sequence, entry->sequence_len,
        curr_seq_hex_buf, sizeof(curr_seq_hex_buf));

        // default normal
        attr_t highlight_attr = A_NORMAL;
        
        // modified
        if (g_org_seq[map_index].is_modified) {
            highlight_attr = COLOR_PAIR(MODIFIED_PAIR);            
        }

        // selected
        if (map_index == g_cur_itm_idx) {
            highlight_attr = (style == HIGHLIGHT_CAPTURE && has_colors()) ?
            COLOR_PAIR(CAPTURE_PAIR) :
            COLOR_PAIR(HIGHLIGHT_PAIR);
        }

        attron(highlight_attr);

        
        // Use %02X to format uint8_t as 2-digit uppercase hex with leading zero if needed
        mvprintw(start_row + i, 1, "%-18.18s | %-20.20s | %-20.20s | 0x%02X | 0x%02X | 0x%02X | %s",
            entry->description ? entry->description : "<NoDesc>",
            orig_seq_hex_buf,       // Original sequence
            curr_seq_hex_buf,       // Current sequence
            entry->keydos.bios_scan_code, // Scan Code
            entry->keydos.ascii_value,    // ASCII Value
            entry->keydos.flags_17,       // Flags
            KeyFlags[entry->keydos.flags_17] 
        );  

        clrtoeol();
        attroff(highlight_attr);        
    }
}

unsigned char* capture_sequence(size_t* out_len) {
    *out_len = 0;
    move(LINES - 1, 1);
    clrtoeol();

    // prompt for capture
    attron(A_REVERSE | A_BLINK);
    mvprintw(LINES - 1, 1, " Press the key/sequence now... (Press JUST ESC to Cancel) ");
    attroff(A_REVERSE | A_BLINK);
    refresh();
    curs_set(1);

    unsigned char* buffer = NULL;
    size_t buffer_cap = 0, buffer_len = 0;
    int cancelled = 0;

    // Wait for first key
    nodelay(stdscr, FALSE);
    timeout(-1);
    int first_ch = getch();

    // Handle first key
    if (first_ch == ERR) {
        display_status("Capture timed out.");
        cancelled = 1;
    } else if (first_ch == 27) {
        nodelay(stdscr, TRUE);
        timeout(90);
        int next_ch = getch();
        nodelay(stdscr, FALSE);
        timeout(-1);

        if (next_ch == ERR) {
            display_status("Capture cancelled.");
            cancelled = 1;
        } 

            else { // next_ch was not ERR, so it's an escape sequence
                    buffer_cap = 8;
                    buffer     = malloc(buffer_cap);
                    if (!buffer) {
                        display_status("ERROR: Memory allocation failed!");
                        cancelled = 1;
                        // No need to free(buffer) here as it's NULL
                    } else {
                        buffer[0]  = (unsigned char)first_ch;
                        buffer[1]  = (unsigned char)next_ch; // Directly assign second char
                        buffer_len = 2;
                    }
                }

            // buffer_cap = 8;
            // buffer     = malloc(buffer_cap);
            // if (!buffer) {
            //     display_status("ERROR: Memory allocation failed!");
            //     cancelled = 1;
            // } else {
            //     buffer[0]  = (unsigned char)first_ch;
            //     buffer_len = 1;
                
            //     // Directly add the second character if buffer has space
            //     // Ensure space for 2nd char (unlikely needed but safe)
            //     if (buffer_cap < 2) { 
            //         // Realloc if initial cap was tiny
            //         buffer = realloc(buffer, 8); 
            //         if (!buffer) {
            //             perror("realloc failed");
            //             display_status("ERROR: Memory allocation failed!");
            //             cancelled = 1;
            //             free(buffer); 
            //             buffer = NULL;
            //         }
            //         buffer_cap = 8;
            //     }
            //     // Check if allocation succeeded
            //     if (buffer) { 
            //         // Add the second char read
            //         buffer[1] = (unsigned char)next_ch; 
            //         // Length is now 2
            //         buffer_len = 2;                     
            //     }
            // }
            // } }
            
        

    } else if (first_ch >= 0 && first_ch <= 255) {
        buffer_cap = 8;
        buffer     = malloc(buffer_cap);
        if (!buffer) {
            display_status("ERROR: Memory allocation failed!");
            cancelled = 1;
        } else {
            buffer[0]  = (unsigned char)first_ch;
            buffer_len = 1;
        }
    } else {
        display_status("Invalid starting key.");
        cancelled = 1;
    }

    // Read rest of sequence
    if (!cancelled) {
        nodelay(stdscr, TRUE);
        timeout(100);
        while (1) {
            int ch = getch();
            if (ch == ERR)
                break;
            if (ch < 0 || ch > 255)
                continue;

            if (buffer_len >= buffer_cap) {
                buffer_cap *= 2;
                buffer = realloc(buffer, buffer_cap);
                if (!buffer) {
                    display_status("ERROR: Memory allocation failed!");
                    cancelled = 1;
                    break;
                }
            }
            buffer[buffer_len++] = (unsigned char)ch;
        }
    }

    nodelay(stdscr, FALSE);
    timeout(-1);
    curs_set(0);
    clear_status();

    if (cancelled || !buffer) {
        free(buffer);
        *out_len = 0;
        return NULL;
    }

    *out_len = buffer_len;
    return buffer;
}

static int visible_count() {
    int h = LINES - 5;
    return (h > 0 ? h : 1); 
}

// clamp and ensure_visible remain the same
static int clamp(int idx, int max) { 
    if (idx < 0) return 0;
    if (idx > max) return max;
    return idx;
}

static void ensure_visible() {
    int vis = visible_count(); 
    if (g_cur_itm_idx < g_top_itm_idx) {
        g_top_itm_idx = g_cur_itm_idx;
    } else if (g_cur_itm_idx >= g_top_itm_idx + vis) {
        g_top_itm_idx = g_cur_itm_idx - vis + 1;
    }
    int max_top = g_map_entries - vis;
    // if g_map_entries < vis
    if (max_top < 0) max_top = 0; 
    g_top_itm_idx = clamp(g_top_itm_idx, max_top);
}




int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <keymap_file>\n", argv[0]);
        return 1;
    }

    char* filename = argv[1];

    printf("Loading keymap file '%s'...\n", filename);

    if (keymap_load(filename) != 0) {
        fprintf(stderr, "Failed to load keymap file '%s'.\n", filename);
        return 1;
    }

    g_map_entries = count_map_entries();
    if (g_map_entries == 0) {
        fprintf(stderr,
        "Warning: Keymap file '%s' loaded successfully but is empty.\n", filename);
        keymap_free();
        return 0;
    }

    g_keymap_filename = filename;

    // Allocate original‐sequence tracking
    g_org_seq = malloc(g_map_entries * sizeof *g_org_seq);
    if (!g_org_seq) {
        perror("malloc");
        keymap_free();
        endwin();
        return 1;
    }

    KeyMapEntry_t* current = g_keymap;
    for (int i = 0; i < g_map_entries; ++i) {
        if (!current) {
            fprintf(stderr, "Error: list count mismatch\n");
            // Free partial allocations
            for (int j = 0; j < i; ++j)
                free(g_org_seq[j].data);
            free(g_org_seq);
            keymap_free();
            endwin();
            return 1;
        }

        // Initialize entry
        g_org_seq[i].data        = NULL;
        g_org_seq[i].len         = 0;
        g_org_seq[i].is_modified = false;

        if (current->sequence_len > 0) {
            g_org_seq[i].data = malloc(current->sequence_len);
            if (!g_org_seq[i].data) {
                perror("malloc");
                // Free partial allocations
                for (int j = 0; j < i; ++j)
                    free(g_org_seq[j].data);
                free(g_org_seq);
                keymap_free();
                endwin();
                return 1;
            }
            memcpy(g_org_seq[i].data, current->sequence, current->sequence_len);
            g_org_seq[i].len = current->sequence_len;
        }

        current = current->next;
    }


    printf("Keymap file '%s' loaded successfully.\n", filename);


    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    // set_escdelay(25);

    // colors
    if (has_colors() == FALSE) {
        endwin();
        fprintf(stderr, "Error: Your terminal does not support color.\n");
    } else {
        start_color();
        // color pairs: (pair_id, foreground, background)        
        init_pair(HIGHLIGHT_PAIR, COLOR_WHITE, COLOR_BLUE); // Example: White text on Blue background
        // For consistency, maybe just use A_REVERSE as the 'normal' style
        // and only define the special green pair. Let's try that.
        init_pair(CAPTURE_PAIR, COLOR_BLACK, COLOR_GREEN); // White text on Green background
        init_pair(MODIFIED_PAIR, COLOR_YELLOW, COLOR_BLACK); // Yellow text on Black background
    }

    // go
    int ch;
    while (1) {
        display_map_list(HIGHLIGHT_NORMAL);
        clear_status();

        ch = getch();

        switch (ch) {
         case KEY_UP:
            if (g_cur_itm_idx > 0) {
                g_cur_itm_idx--;
                ensure_visible();
            } else {
                beep();
            }
            break;

        case KEY_DOWN:
            if (g_cur_itm_idx < g_map_entries - 1) {
                g_cur_itm_idx++;
                ensure_visible();
            } else {
                beep();
            }
            break;

        case KEY_HOME:
            g_cur_itm_idx = 0;
            // g_top_itm_idx = 0; // ensure_visible will correctly set this
            ensure_visible();
            break;

        case KEY_END: {
            // int vis = visible_count(); // No longer needed here, ensure_visible handles it
            // int max_top = g_map_entries - vis;
            // if (max_top < 0) max_top = 0;

            g_cur_itm_idx = g_map_entries - 1;
            // g_top_itm_idx = clamp(g_cur_itm_idx - vis + 1, max_top); // Old direct logic
            ensure_visible(); // Let ensure_visible handle g_top_itm_idx consistently
        } break;

        case KEY_PPAGE: {
            // int vis = visible_count(); // Already used inside ensure_visible
            g_cur_itm_idx = clamp(g_cur_itm_idx - visible_count(), g_map_entries -1); // Clamp cur_idx before ensure_visible
            if (g_cur_itm_idx < 0) g_cur_itm_idx = 0; // Ensure cur_idx is not negative
            ensure_visible();
        } break;

        case KEY_NPAGE: {
            // int vis = visible_count(); // Already used inside ensure_visible
            int old_idx = g_cur_itm_idx;
            g_cur_itm_idx = clamp(g_cur_itm_idx + visible_count(), g_map_entries - 1);
            ensure_visible();
            if (g_cur_itm_idx == old_idx && old_idx == g_map_entries - 1) { // Beep if already at the end
                beep();
            }
        } break;

        case '\n':      // Enter key
        case KEY_ENTER: // Enter key (keypad)
        {
            KeyMapEntry_t* entry = get_entry_at_index(g_cur_itm_idx);
            if (entry) {
                display_map_list(HIGHLIGHT_CAPTURE);
                // Force redraw with new highlight
                refresh();

                size_t new_seq_len     = 0;
                unsigned char* new_seq = capture_sequence(&new_seq_len);

                // Check if capture wasn't cancelled/failed
                if (new_seq != NULL && new_seq_len > 0) {
                    // Free the old sequence before assigning the new one
                    free(entry->sequence);
                    // Assign the newly malloc'd buffer
                    entry->sequence     = new_seq;
                    entry->sequence_len = new_seq_len;
                    g_dirty_flag        = 1;
                    char hex_buf[64];
                    keymap_format_sequence_hex(entry->sequence,
                    entry->sequence_len, hex_buf, sizeof(hex_buf));
                    display_status("Sequence updated!");
                    // modified
                    g_org_seq[g_cur_itm_idx].is_modified = true;
                    timeout(1500);
                    getch();
                    clear_status();
                } else if (new_seq == NULL && new_seq_len == 0) {
                    // Capture was cancelled or failed, new_seq might be NULL
                    display_status("Sequence Cancelled !!!");
                    timeout(1500);
                    free(new_seq);
                }
            }
        } break;

        case 's':
        case 'S': {
            display_status("Saving...");
            if (keymap_save(g_keymap_filename) == 0) {
                g_dirty_flag = 0;
                display_status("Saved successfully.");
            } else {
                // keymap_save should print specific errors
                display_status("ERROR SAVING FILE! Check console.");
                beep();
            }
            timeout(2000);
            getch();
            clear_status();
        } break;

        case 'q':
        case 'Q':
        // Also allow ESC to quit (maybe check if it's *just* ESC?)
        //case 27: 
        {
            if (g_dirty_flag) {
                display_status(
                "Unsaved changes! Save before exit? (Y/N/Cancel)");
                // Wait indefinitely for Y/N/C
                timeout(-1);
                int confirm_ch = getch();
                clear_status();
                confirm_ch = tolower(confirm_ch);
                if (confirm_ch == 'y') {
                    display_status("Saving...");
                    if (keymap_save(g_keymap_filename) == 0) {
                        // Saved, clean and exit
                        goto cleanup_and_exit;
                    } else {
                        display_status("ERROR SAVING! Exit anyway? (Y/N)");
                        timeout(-1);
                        confirm_ch = getch();
                        if (tolower(confirm_ch) == 'y') {
                            goto cleanup_and_exit;
                        } else {
                            clear_status();
                            // Don't exit, back to main loop
                            continue;
                        }
                    }
                } else if (confirm_ch == 'n') {
                    // Exit without saving
                    goto cleanup_and_exit;
                } else {
                    // Cancelled exit, continue loop
                    display_status("Exit cancelled.");
                    timeout(1000);
                    getch();
                    clear_status();
                    continue;
                }
            } else {
                // No changes, exit directly
                goto cleanup_and_exit;
            }
        } break;

        default:
            // beep();
            break;

        } 
    }     


cleanup_and_exit:
    endwin();

    // free original sequences
    if (g_org_seq) {
        for (int i = 0; i < g_map_entries; ++i) {
            free(g_org_seq[i].data);
        }
        free(g_org_seq);
        g_org_seq = NULL;
    }


    keymap_free();
    return 0;
}