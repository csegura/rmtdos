/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdarg.h>
#include <stdio.h>

#include "lib16/video.h"
#include "server/debug.h"

void hex_dump(FILE *fp, const void *buffer, size_t bytes) {
  size_t i = 0;
  const uint8_t *p = (const uint8_t *)buffer;

  while (bytes) {
    if (!i) {
      fprintf(fp, "%04x: ", (p - (const uint8_t *)buffer));
    }
    fprintf(fp, " %02x", *p);
    --bytes;
    ++p;
    ++i;
    if (i == 8) {
      fputc(' ', fp);
    }
    if (i == 16) {
      fputc('\n', fp);
      i = 0;
    }
  }
  if (i) {
    fputc('\n', fp);
  }
}

void log_checkpoint(const char *file, int line) {
  video_printf(0, 0, 0x1f, "CP: %s:%d     ", file, line);
}
