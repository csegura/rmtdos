/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Contains main entry points for the "resident" portion of the server.

#ifdef __BCC__
#include <dos.h>
#endif

#include "lib16/x86.h"
#include "lib16/video.h"
#include "server/config.h"
#include "server/globals.h"
#include "server/int08.h"
#include "server/int28.h"
#include "server/int2f.h"
#include "server/protocol.h"
#include "server/resident.h"
#include "server/session.h"
#include "server/util.h"

#ifdef __GNUC__
extern long __getvect(int);
extern void __setvect(int, long);
#endif

// Called from int2f when we are "resident" (TSR).
// Return value:
// 0 = Can be uninstalled.
// >0 = Interrupt # that someone else hooked, blocking us from uninstalling.
int resident_uninstall_check() {
  uint16_t cs = __get_cs();

  if (MK_FP(cs, int08_isr) != __getvect(0x08)) {
    return 0x08;
  }

#if HAS_INT28
  if (MK_FP(cs, int28_isr) != __getvect(0x28)) {
    return 0x28;
  }
#endif

  if (MK_FP(cs, int2f_isr) != __getvect(0x2f)) {
    return 0x2f;
  }

  return 0;
}

// Performs no safety checks.  Called from transient portion if it fails to
// "go TSR".  Typically called from the resident portion when unloading.
void restore_interrupt_handlers() {
  __setvect(0x08, int08_original_handler);
#if HAS_INT28
  __setvect(0x28, int28_original_handler);
#endif
  __setvect(0x2f, int2f_original_handler);
}

int resident_do_uninstall() {
  struct CpuRegs regs;

  int x = resident_uninstall_check();
  if (x) {
    return x;
  }

  restore_interrupt_handlers();
  pktdrv_done();

  x86_reset_regs(&regs);
  regs.w.ax = 0x4900;   // DOS Free Memory.
  regs.es = __get_cs(); // Block to free (our PSP)
  x86_call(0x21, &regs);

  return -regs.w.ax;
}

#if DEBUG
void isr_show_debug_stats() {
  video_printf(64, 0, 15, "int 08: %8lu", int08_ticks);
#if HAS_INT28
  video_printf(64, 1, 15, "int 28: %8lu", int28_ticks);
#endif
  video_printf(64, 2, 15, "int 2f: %8lu", int2f_ticks);
  video_printf(64, 3, 15, "Bios:  %9lu", x86_read_bios_tick_clock());
  video_printf(64, 4, 15, "Recv:  %9lu", g_pktdrv_stats.packets_recv);
  video_printf(64, 5, 15, "Drop:  %9lu", g_pktdrv_stats.packets_dropped);
}
#endif

void int08_handler() {
  ++int08_ticks;

#if DEBUG
  if (g_show_debug_overlay) {
    isr_show_debug_stats();
  }
#endif

  // Process any inbound network traffic.
  protocol_process();

  // Send out any session updates.
  session_mgr_update_all();
}

void int2f_handler(struct CpuRegs *regs) {
  ++int2f_ticks;

#if DEBUG
  if (g_show_debug_overlay) {
    isr_show_debug_stats();
  }
#endif

  switch (regs->w.dx) {
    case MULTIPLEX_CMD_INSTALL_CHECK:
      // Can we uninstall safely?
      regs->w.ax = resident_uninstall_check();

      // Indicate that our ISR handled the interrupt.
      regs->w.bx = 0;

      // Not strictly required, but meh.
      regs->w.cx = __get_cs();

      // Unused in response, but clear it anyway.
      regs->w.dx = 0;
      return;

    case MULTIPLEX_CMD_UNINSTALL:
      regs->w.ax = resident_do_uninstall();
      regs->w.bx = 0;
      regs->w.cx = __get_cs();
      regs->w.dx = 0;
      break;
  }
}

/* int 28h is called by DOS when idle.
   We are free to call any DOS service EXCEPT for AH=01 through AH=0d.
*/
#if HAS_INT28
void int28_handler() {
  ++int28_ticks;

#if DEBUG
  if (g_show_debug_overlay) {
    isr_show_debug_stats();
  }
#endif
}
#endif
