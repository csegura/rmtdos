#define _DEFAULT_SOURCE 1
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "keymaplib.h"

// Global keymap list; initially NULL.
KeyMapEntry_t *g_keymap = NULL;
// Global keymap filename; initially NULL.
char *g_keymap_filename = NULL;

// Trim leading and trailing whitespace.
char *_trim(char *str) {
  char *end;
  while (isspace((unsigned char)*str))
    str++;
  if (*str == '\0')
    return str;
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return str;
}

// Converts a hex string of the form "\xAB\xCD..." into a byte buffer.
// Returns the number of bytes parsed on success, or -1 on error.
// Caller must free *out_bytes.
int _parse_hex_sequence(const char *hex_str, unsigned char **out_bytes) {
  int len = (int)strlen(hex_str);
  if (len == 0) {
    *out_bytes = NULL;
    return 0;
  }
  if (len % 4 != 0 || strncmp(hex_str, "\\x", 2) != 0) {
    fprintf(stderr, "Invalid hex sequence format: '%s'\n", hex_str);
    return -1;
  }
  int num_bytes = len / 4;
  *out_bytes = malloc(num_bytes);
  if (!*out_bytes) {
    perror("Memory allocation for sequence bytes failed");
    return -1;
  }
  for (int i = 0; i < num_bytes; ++i) {
    const char *p = hex_str + i * 4;
    if (p[0] != '\\' || p[1] != 'x') {
      fprintf(stderr, "Invalid hex sequence format at byte %d: '%s'\n", i,
              hex_str);
      free(*out_bytes);
      *out_bytes = NULL;
      return -1;
    }
    char hex_pair[3] = {p[2], p[3], '\0'};
    char *endptr;
    long val = strtol(hex_pair, &endptr, 16);
    if (*endptr != '\0' || val < 0 || val > 0xFF) {
      fprintf(stderr, "Invalid hex digits '%s' in sequence: '%s'\n", hex_pair,
              hex_str);
      free(*out_bytes);
      *out_bytes = NULL;
      return -1;
    }
    (*out_bytes)[i] = (unsigned char)val;
  }
  return num_bytes;
}

// Load keymap from file into a linked list. Returns 0 on success, -1 on
// failure.
int keymap_load(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror("Error opening keymap file");
    return -1;
  }
  char line_buffer[1024];
  int line_num = 0;
  while (fgets(line_buffer, sizeof(line_buffer), fp)) {
    line_num++;
    char *line = _trim(line_buffer);
    if ((line[0] == '/' && line[1] == '/') || line[0] == '\0') {
      continue;
    }
    // Split the line by '|'
    char *desc_str = strtok(line, "|");
    char *seq_hex_str = strtok(NULL, "|");
    char *scan_str = strtok(NULL, "|");
    char *ascii_str = strtok(NULL, "|");
    char *flags_str = strtok(NULL, "|");

    // remove comments after the last field preceding for '//' or '#'
    if (flags_str) {
      char *comment = strchr(flags_str, '#');
      if (comment) {
        *comment = '\0';
      }
      comment = strchr(flags_str, '/');
      if (comment) {
        *comment = '\0';
      }
    }

    if (!desc_str || !seq_hex_str || !scan_str || !ascii_str || !flags_str) {
      fprintf(stderr, "Keymap Error: Line %d: Invalid or missing fields.\n",
              line_num);
      continue;
    }
    desc_str = _trim(desc_str);
    seq_hex_str = _trim(seq_hex_str);
    scan_str = _trim(scan_str);
    ascii_str = _trim(ascii_str);
    flags_str = _trim(flags_str);

    // Parse hex sequence.
    unsigned char *sequence_bytes = NULL;
    int sequence_len = _parse_hex_sequence(seq_hex_str, &sequence_bytes);
    if (sequence_len < 0) {
      fprintf(stderr, "Keymap Error: Line %d: Failed to parse sequence '%s'.\n",
              line_num, seq_hex_str);
      continue;
    }
    // Parse scan code, ASCII value and flags (base 0 to support "0x" notation).
    char *endptr;
    long scan_val = strtol(scan_str, &endptr, 0);
    if (*endptr != '\0' || scan_val < 0 || scan_val > 0xFF) {
      fprintf(stderr, "Keymap Error: Line %d: Invalid scan code '%s'.\n",
              line_num, scan_str);
      free(sequence_bytes);
      continue;
    }
    long ascii_val = strtol(ascii_str, &endptr, 0);
    if (*endptr != '\0' || ascii_val < 0 || ascii_val > 0xFF) {
      fprintf(stderr, "Keymap Error: Line %d: Invalid ASCII value '%s'.\n",
              line_num, ascii_str);
      free(sequence_bytes);
      continue;
    }
    long flags_val = strtol(flags_str, &endptr, 0);
    if (*endptr != '\0' || flags_val < 0 || flags_val > 0xFF) {
      fprintf(stderr, "Keymap Error: Line %d: Invalid flags value '%s'.\n",
              line_num, flags_str);
      free(sequence_bytes);
      continue;
    }

    // Allocate and initialize a new keymap entry.
    KeyMapEntry_t *kme = malloc(sizeof(KeyMapEntry_t));
    if (!kme) {
      perror("Keymap Error: Allocation failed for mapping entry");
      free(sequence_bytes);
      fclose(fp);
      return -1;
    }
    kme->description = strdup(desc_str);
    kme->sequence = sequence_bytes;
    kme->sequence_len = (size_t)sequence_len;
    kme->keydos.bios_scan_code = (uint8_t)scan_val;
    kme->keydos.ascii_value = (uint8_t)ascii_val;
    kme->keydos.flags_17 = (uint8_t)flags_val;
    // Insert at the head of the global keymap list.
    kme->next = g_keymap;
    g_keymap = kme;
  }
  fclose(fp);
  return 0;
}


// Save the current global keymap list to the specified file.
// Returns 0 on success, -1 on failure.
int keymap_save(const char *filename) {
  if (!g_keymap) {
      // Optionally handle saving an empty map if desired
      // For now, let's assume saving an empty list means creating an empty file
      // or maybe it's an error state depending on requirements.
      // Let's just write an empty file or a header.
  }
  if (!filename) {
      fprintf(stderr, "Error: No filename specified for saving.\n");
      return -1;
  }

  FILE *fp = fopen(filename, "w");
  if (!fp) {
      perror("Error opening file for writing");
      fprintf(stderr, "Filename: %s\n", filename);
      return -1;
  }

  // --- Optional: Write header comments ---
  fprintf(fp, "// Key Map File - Saved by Config Tool\n");
  fprintf(fp, "// Format: Description | Sequence (Hex) | Scan (Hex) | ASCII (Hex) | Flags (Hex)\n");
  fprintf(fp, "//\n");

  // --- Find max description width for alignment ---
  size_t max_desc_width = 0;
  KeyMapEntry_t *current = g_keymap;
  while (current) {
      size_t len = strlen(current->description);
      if (len > max_desc_width) {
          max_desc_width = len;
      }
      current = current->next;
  }
  // Add a minimum width if desired
  if (max_desc_width < 10) max_desc_width = 10;


  // --- Iterate through the list and write each entry ---
  current = g_keymap;
  char seq_hex_buf[512]; // Buffer for formatted hex sequence, adjust size if needed

  // Since the list was built by inserting at the head, it's reversed
  // relative to the original file order. To save in original order,
  // we either need to reverse the list first or iterate differently.
  // Easiest is often to reverse the list before saving.
  // Let's implement a simple reversal (can be optimized).

  KeyMapEntry_t *reversed_list = NULL;
  current = g_keymap;
  while(current) {
      KeyMapEntry_t *next = current->next;
      current->next = reversed_list;
      reversed_list = current;
      current = next;
  }
  // IMPORTANT: Update the global pointer AFTER reversal if you want
  // the main app to see the reversed list. Or, just use reversed_list
  // locally for saving and then reverse it back (or reload).
  // Let's just use it locally for saving.

  // Now iterate through the reversed list
  current = reversed_list;
  while (current) {
      // Format sequence to hex
      keymap_format_sequence_hex(current->sequence, current->sequence_len, seq_hex_buf, sizeof(seq_hex_buf));

      // Write the formatted line
      // fprintf returns negative on error
      int written = fprintf(fp, "%-*s | %-25s | 0x%02X | 0x%02X | 0x%02X\n",
                            (int)max_desc_width, // Width for description
                            current->description ? current->description : "",
                            seq_hex_buf,
                            current->keydos.bios_scan_code,
                            current->keydos.ascii_value,
                            current->keydos.flags_17);

      if (written < 0) {
           perror("Error writing to keymap file");
           fclose(fp);
           // Need to reverse the list back here if we modified g_keymap globally
           // Or handle error differently
           return -1;
      }

      current = current->next;
  }

  // --- Reverse the list back to original order (important!) ---
  current = reversed_list;
  g_keymap = NULL; // Reset global head
  while(current) {
      KeyMapEntry_t *next = current->next;
      current->next = g_keymap; // Insert at head of g_keymap
      g_keymap = current;
      current = next;
  }


  if (fclose(fp) != 0) {
      perror("Error closing keymap file after writing");
      return -1; // File might be corrupt
  }

  return 0; // Success
}


// Find a mapping based on the captured sequence.
KeyMapEntry_t *keymap_find(KeyMapEntry_t *list, unsigned char *captured_seq,
                           size_t captured_len) {
  KeyMapEntry_t *current = list;
  while (current) {
    if (current->sequence_len == captured_len &&
        memcmp(current->sequence, captured_seq, captured_len) == 0) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

// Helper to represent sequence safely for printing
void keymap_format_sequence(unsigned char *seq, int len, char *out_buf,
                            size_t out_size) {
  out_buf[0] = '\0';
  for (int i = 0; i < len && strlen(out_buf) < out_size - 5;
       ++i) { // -5 for safety: \xHH + space
    if (seq[i] >= ' ' && seq[i] <= '~') {
      char tmp[2] = {seq[i], '\0'};
      strncat(out_buf, tmp, out_size - strlen(out_buf) - 1);
    } else {
      snprintf(out_buf + strlen(out_buf), out_size - strlen(out_buf), "\\x%02x",
               (unsigned char)seq[i]);
    }
  }
}

void keymap_format_sequence_hex(unsigned char *seq, int len, char *out_buf,
                                size_t out_size) {
  out_buf[0] = '\0';
  for (int i = 0; i < len && strlen(out_buf) < out_size - 5;
       ++i) { // -5 for safety: \xHH + space
    snprintf(out_buf + strlen(out_buf), out_size - strlen(out_buf), "\\x%02x",
             (unsigned char)seq[i]);
  }
}

// Free all allocated keymap entries.
void keymap_free(void) {
  KeyMapEntry_t *current = g_keymap;
  while (current) {
    KeyMapEntry_t *next = current->next;
    free(current->sequence);
    free(current->description);
    free(current);
    current = next;
  }
}