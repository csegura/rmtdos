/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <iconv.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <locale.h>
#include <ncurses.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "client/curses.h"
#include "client/globals.h"
#include "client/hostlist.h"
#include "client/keyboard.h"
#include "client/network.h"
#include "client/util.h"
#include "common/protocol.h"

#define BUF_SIZE (ETH_FRAME_LEN)

#define MAX_EVENTS 16 /* epoll events */

#define MAX_HOSTS 16

enum AppMode {
  MODE_PROBING = 1,  // Probing for remote servers.
  MODE_RMT_CTRL = 2, // Remote control of a single server.
};

static const int epoll_timeout_ms = 1000;

// How often to send a broadcast probe, looking for servers.
static const struct timeval broadcast_probe_interval = {
    .tv_sec = 2,
    .tv_usec = 500000,
};

// How often client should send a 'V1_SESSION_START' message.
static const struct timeval session_start_interval = {
    .tv_sec = 2,
    .tv_usec = 0,
};

static uint8_t broadcast_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#define MIN(x, y) ((x) > (y) ? (y) : (x))

enum AppMode g_app_mode = MODE_PROBING;
int g_running = 1;
int g_show_debug_window = 0;

static struct timeval g_last_probe = {0};

// Non-NULL if we're actively controlling a server.
struct RemoteHost *g_active_host = NULL;

void update_probing_window(const struct RawSocket *rs) {
  struct timeval tv_now;
  char mac_tmp[MAC_ADDR_FMT_LEN];
  WINDOW *w = g_probe_window;
  int y = 0;

  wmove(w, y, 0);
  wclrtobot(w);

  box(w, 0, 0);
  mvwprintw(w, y, 2, "Probing LAN for Servers (EtherType: %04x)",
            rs->ethertype);
  ++y;
  mvwprintw(w, y, 1, RMTDOS_VERSION);
  mvwprintw(w, y, 50, "<ALT-ESC> to exit");
  ++y;
  mvwprintw(w, y, 1, "%s: %s", rs->if_name,
            fmt_mac_addr(mac_tmp, sizeof(mac_tmp), rs->if_addr));
  y += 2;

  wattron(w, COLOR_PAIR(MY_COLOR_HEADER));
  wattron(w, A_BOLD);
  mvwprintw(w, y, 1, "Id  Source            Mode    W   H  last_ping");
  wattroff(w, A_BOLD);
  wattroff(w, COLOR_PAIR(MY_COLOR_HEADER));

  gettimeofday(&tv_now, NULL);
  ++y;

  int iter = 0;
  struct RemoteHost *r;
  while (NULL != (r = hostlist_iter(&iter))) {
    if (timerisset(&(r->tv_last_resp))) {
      struct timeval tv_stale;
      timersub(&tv_now, &(r->tv_last_resp), &tv_stale);

      char stale[32];
      snprintf(stale, sizeof(stale), "%ld.%06ld", tv_stale.tv_sec,
               tv_stale.tv_usec);

      mvwprintw(w, y, 1, "%2d  %s   %2d  %3d %3d  %s", r->index,
                fmt_mac_addr(mac_tmp, sizeof(mac_tmp), r->if_addr),
                r->status.video_mode, r->status.text_cols, r->status.text_rows,
                stale);
      ++y;
    }
  }
}

void debug_show_incoming_packet(const uint8_t *buf, size_t received) {
  const struct ether_header *eh = (const struct ether_header *)buf;
  const struct ProtocolHeader *ph = (const struct ProtocolHeader *)(eh + 1);
  const uint8_t *p = (const uint8_t *)(ph + 1);
  WINDOW *w = g_debug_window;

  wmove(w, 0, 0);
  wclrtobot(w);

  box(w, 0, 0);
  mvwprintw(w, 0, 2, "Incoming Packet Dump");

  wattron(w, COLOR_PAIR(MY_COLOR_HEADER));
  wattron(w, A_BOLD);
  mvwprintw(w, 1, 1, "Source             Session   Type  Size  Payload");
  wattroff(w, A_BOLD);
  wattroff(w, COLOR_PAIR(MY_COLOR_HEADER));

  char tmp[MAC_ADDR_FMT_LEN];
  mvwprintw(w, 2, 1, "%s  %08x  %4d  %4d ",
            fmt_mac_addr(tmp, sizeof(tmp), eh->ether_shost),
            ntohl(ph->session_id), ntohs(ph->pkt_type), ntohs(ph->payload_len));

  received -= sizeof(*eh) + sizeof(*ph);
  received = MIN(received, ntohs(ph->payload_len));

  for (int i = 0; i < MIN(received, 12); ++i) {
    wprintw(w, " %02x", p[i]);
  }
}

void process_incoming_video_text(const uint8_t *buf, size_t received) {
  const struct ether_header *eh = (const struct ether_header *)buf;
  const struct ProtocolHeader *ph = (const struct ProtocolHeader *)(eh + 1);
  const struct VideoText *video = (const struct VideoText *)(ph + 1);
  const uint8_t *data = (const uint8_t *)(video + 1);

  struct RemoteHost *rh = hostlist_find_by_mac(eh->ether_shost);
  if (!rh) {
    return;
  }

  gettimeofday(&(rh->tv_last_resp), NULL);

  const uint16_t offset = ntohs(video->offset);
  const uint16_t count = ntohs(video->count);
  if (count + offset > sizeof(rh->video_text_buffer)) {
    return;
  }

  memcpy(rh->video_text_buffer + offset, data, count);

  rh->text_rows = video->text_rows;
  rh->text_cols = video->text_cols;

  // we already have a place to store the cursor position
  rh->status.cursor_row = video->cursor_row;
  rh->status.cursor_col = video->cursor_col;

  update_session_window(rh, offset, count);
}

void process_socket_io(struct RawSocket *rs) {
  uint8_t buf[ETH_FRAME_LEN];
  const struct ether_header *eh = (const struct ether_header *)buf;
  const struct ProtocolHeader *ph = (const struct ProtocolHeader *)(eh + 1);
  ssize_t received;

  if (0 >= (received = recvfrom(rs->sock_fd, buf, BUF_SIZE, 0, NULL, NULL))) {
    return;
  }

  // Only accept packets sent directly to our host.
  // We send broadcasts to servers to find them, but a server already knows
  // our MAC address.  This way, we can safely run multiple servers on the
  // same broadcast domain.
  if (memcmp(eh->ether_dhost, rs->if_addr, ETH_ALEN)) {
    return;
  }

  // Skip packets without our signature.
  if (PACKET_SIGNATURE != ntohl(ph->signature)) {
    return;
  }

  // Only accept packets sent to OUR session_id
  // Allows me to test w/ multiple clients on the same host.
  if (rs->session_id != ntohl(ph->session_id)) {
    return;
  }

  if (g_show_debug_window) {
    debug_show_incoming_packet(buf, received);
  }

  switch (ntohs(ph->pkt_type)) {
    case V1_STATUS_RESP:
      hostlist_register(buf, received);
      break;
    case V1_VGA_TEXT:
      process_incoming_video_text(buf, received);
      break;
  }
}

void start_remote_control(struct RemoteHost *rh) {
  g_active_host = rh;
  rh->window = g_session_window;

  mvwprintw(rh->window, 0, 0, "Connecting...");
}

// Called when there is data on STDIN and the UI is in the "menu mode" (waiting
// for user to select a server to connect to).
void process_stdin_menu_mode() {
  int c = getch();

  if (c == EXIT_WCH_CODE || c == KEY_F(12) || c == 27) {
    g_running = 0;
    return;
  }

  if ((c >= '0') && (c <= '9')) {
    struct RemoteHost *rh = hostlist_find_by_index(c - '0');
    if (rh) {
      start_remote_control(rh);
    }
  }
}

void process_timers(struct RawSocket *rs) {
  struct timeval now;
  struct timeval diff;
  gettimeofday(&now, NULL);

  // Time to send another broadcast probe?
  timersub(&now, &g_last_probe, &diff);
  if (timercmp(&diff, &broadcast_probe_interval, >)) {
    send_status_req(rs, NULL);
    g_last_probe = now;
  }

  // Refresh session intention with each server under control.
  int iter = 0;
  struct RemoteHost *rh;
  while (NULL != (rh = hostlist_iter(&iter))) {
    if (rh->window) {
      timersub(&now, &(rh->tv_last_session_start), &diff);
      if (timercmp(&diff, &session_start_interval, >)) {
        send_session_start(rs, rh->if_addr);
        rh->tv_last_session_start = now;
      }

      update_hud(rh);
    }
  }
}

void refresh_windows() {
  refresh();

  if (g_active_host && g_active_host->window) {
    wrefresh(g_active_host->window);
  } else {
    wrefresh(g_probe_window);
  }

  if (g_show_debug_window) {
    wrefresh(g_debug_window);
  }
}

static const char *DEFAULT_ETH_DEV = "eth0";

static void print_usage(const char *progname) {
  printf("usage: %s [-d dest-addr] [-e type] [-i eth_dev] [-k]\n", progname);
  printf("  -d  Destination MAC address (xx:xx:xx:xx:xx:xx).\n");
  printf("  -e  Ethertype as 4 hexadecimal digits (default: %04x).\n",
         ETHERTYPE_RMTDOS);
  printf("  -i  Name of local ethernet device (default: %s).\n",
         DEFAULT_ETH_DEV);
  printf("  -k  Dump keyboard layout to text file for debugging.\n");
}

int main(int argc, char **argv) {
  const char *if_name = DEFAULT_ETH_DEV;
  uint16_t ethertype = ETHERTYPE_RMTDOS;
  uint8_t dest_addr[ETH_ALEN] = {0};
  int i;
  int opt;

  // http://yjlv.blogspot.com/2015/10/displaying-unicode-with-ncurses-in-c.html
  setlocale(LC_ALL, "");
  cp437_table_init();

  memcpy(dest_addr, broadcast_addr, ETH_ALEN);
  hostlist_create();

  while ((opt = getopt(argc, argv, "d:e:i:kl")) != -1) {
    switch (opt) {
      case 'i':
        if_name = optarg;
        break;

      case 'd': {
        int mac[ETH_ALEN];

        if (ETH_ALEN != sscanf(optarg, "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0],
                               &mac[1], &mac[2], &mac[3], &mac[4], &mac[5])) {
          print_usage(argv[0]);
          return EXIT_FAILURE;
        }
        for (i = 0; i < ETH_ALEN; i++) {
          dest_addr[i] = mac[i];
        }
      } break;

      case 'e':
        ethertype = strtoul(optarg, NULL, 16);
        break;

      case 'k':
        dump_keyboard_table(stdout);
        return EXIT_SUCCESS;

      default: /* '?' */
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
  }

  if (optind < argc) {
    //  msg = argv[optind];
  }

  struct RawSocket rs = {0};
  if (0 > create_socket(&rs, if_name, ethertype)) {
    return EXIT_FAILURE;
  }

  int epoll_fd;
  if (0 > (epoll_fd = epoll_create1(EPOLL_CLOEXEC))) {
    perror("epoll_create1()");
    return EXIT_FAILURE;
  }

  struct epoll_event ev, events[MAX_EVENTS];
  ev.events = EPOLLIN;
  ev.data.fd = rs.sock_fd;
  if (0 > epoll_ctl(epoll_fd, EPOLL_CTL_ADD, rs.sock_fd, &ev)) {
    perror("epoll_ctl(socket)");
    return EXIT_FAILURE;
  }

  struct epoll_event ev_stdin;
  ev_stdin.events = EPOLLIN;
  ev_stdin.data.fd = STDIN_FILENO;
  if (0 > epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev_stdin)) {
    perror("epoll_ctl(stdin)");
    return EXIT_FAILURE;
  }

  init_ncurses();

  // Ping broadcast address, to trigger a response from all clients.
  send_status_req(&rs, NULL);

  while (g_running) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, epoll_timeout_ms);
    if (nfds < 0) {
      if (errno == EINTR) {
        continue;
      }

      perror("epoll_wait()");

      // Exit cleanly, so that ncurses can restore the terminal.
      g_running = false;
      break;
    }

    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == STDIN_FILENO) {
        if (g_active_host) {
          process_stdin_session_mode(&rs);
        } else {
          process_stdin_menu_mode();
        }
      }

      if (events[n].data.fd == rs.sock_fd) {
        process_socket_io(&rs);
      }
    }

    process_timers(&rs);
    update_probing_window(&rs);
    refresh_windows();
  }

  shutdown_ncurses();

  close(epoll_fd);
  close_socket(&rs);

  hostlist_destroy();

  return 0;
}
