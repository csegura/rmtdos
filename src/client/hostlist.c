/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Routines for managing a list of known remote hosts.

#include <arpa/inet.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "client/hostlist.h"

// This would be so much easier if I were using C++17 w/ the STL...
#define MAX_HOSTS 16
static struct RemoteHost *g_remote_hosts[MAX_HOSTS] = {NULL};

static struct RemoteHost *host_create() {
  struct RemoteHost *rh =
      (struct RemoteHost *)malloc(sizeof(struct RemoteHost));
  memset(rh, 0, sizeof(struct RemoteHost));
  return rh;
}

static void host_destroy(struct RemoteHost *rh) { free(rh); }

void hostlist_create() {}

void hostlist_destroy() {
  for (int i = 0; i < MAX_HOSTS; ++i) {
    if (g_remote_hosts[i]) {
      host_destroy(g_remote_hosts[i]);
      g_remote_hosts[i] = NULL;
    }
  }
}

struct RemoteHost *hostlist_find_by_mac(const uint8_t *if_addr) {
  for (int i = 0; i < MAX_HOSTS; ++i) {
    if (g_remote_hosts[i] &&
        !memcmp(g_remote_hosts[i]->if_addr, if_addr, ETH_ALEN)) {
      return g_remote_hosts[i];
    }
  }
  return NULL;
}

struct RemoteHost *hostlist_find_by_index(int index) {
  if ((index >= 0) && (index < MAX_HOSTS) && g_remote_hosts[index]) {
    return g_remote_hosts[index];
  }

  return NULL;
}

struct RemoteHost *hostlist_allocate() {
  for (int i = 0; i < MAX_HOSTS; ++i) {
    if (!g_remote_hosts[i]) {
      g_remote_hosts[i] = host_create();
      g_remote_hosts[i]->index = i;
      return g_remote_hosts[i];
    }
  }
  return NULL;
}

extern struct RemoteHost *hostlist_iter(int *iter) {
  while (*iter < MAX_HOSTS) {
    if (g_remote_hosts[*iter]) {
      struct RemoteHost *rh = g_remote_hosts[*iter];
      ++(*iter);
      return rh;
    }
    ++(*iter);
  }

  *iter = -1;
  return NULL;
}

void hostlist_register(const uint8_t *packet, size_t length) {
  const struct ether_header *eh = (const struct ether_header *)packet;
  const struct ProtocolHeader *ph = (const struct ProtocolHeader *)(eh + 1);

  struct RemoteHost *rh = hostlist_find_by_mac(eh->ether_shost);
  if (!rh) {
    if (NULL == (rh = hostlist_allocate())) {
      // No more space, drop host.
      return;
    }

    memcpy(rh->if_addr, eh->ether_shost, ETH_ALEN);
  }

  gettimeofday(&(rh->tv_last_resp), NULL);

  if (ntohs(ph->pkt_type) == V1_STATUS_RESP) {
    const struct StatusResponse *s = (const struct StatusResponse *)(ph + 1);

    rh->status.video_mode = s->video_mode;
    rh->status.active_page = s->active_page;
    rh->status.text_rows = s->text_rows;
    rh->status.text_cols = s->text_cols;
    rh->status.cursor_row = s->cursor_row;
    rh->status.cursor_col = s->cursor_col;
  }
}
