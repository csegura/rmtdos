/*
 * Copyright 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef __BCC__
#include <bios.h>
#include <dos.h>
#endif

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "lib16/hex2int.h"
#include "lib16/video.h"
#include "lib16/x86.h"
#include "server/bufmgr.h"
#include "server/config.h"
#include "server/debug.h"
#include "server/globals.h"
#include "server/int08.h"
#if HAS_INT28
#include "server/int28.h"
#endif
#include "server/int2f.h"
#include "server/pktdrv.h"
#include "server/protocol.h"
#include "server/resident.h"
#include "server/session.h"
#include "server/util.h"

#ifdef __GNUC__
extern long __getvect(int);
extern void __setvect(int, long);
extern uint16_t __envseg, __psp;
extern void *sbrk();
#endif

extern uint8_t _etext, _edata, _end;

void install_interrupt_handlers() {
  uint16_t cs = __get_cs();

  // Debugging aide.  Used to determine how much of each stack we are using.
  // We can examine the machine's memory (DEBUG.COM or a core-dump from
  // QEMU) for the regions occupied by these stacks.
#if DEBUG
  memset(&int08_stack_bottom, 0x55, &int08_stack_top - &int08_stack_bottom);
#if HAS_INT28
  memset(&int28_stack_bottom, 0x55, &int28_stack_top - &int28_stack_bottom);
#endif
  memset(&int2f_stack_bottom, 0x55, &int2f_stack_top - &int2f_stack_bottom);
#endif

  int08_original_handler = __getvect(0x08);
  __setvect(0x08, MK_FP(cs, int08_isr));
#if DEBUG
  printf("int28:   orig: %08lx, new: %08lx\n", int08_original_handler,
         __getvect(0x08));
#endif

#if HAS_INT28
  int28_original_handler = __getvect(0x28);
  __setvect(0x28, MK_FP(cs, int28_isr));
#if DEBUG
  printf("int28:   orig: %08lx, new: %08lx\n", int28_original_handler,
         __getvect(0x28));
#endif
#endif

  int2f_original_handler = __getvect(0x2f);
  __setvect(0x2f, MK_FP(cs, int2f_isr));
#if DEBUG
  printf("int2f:   orig: %08lx, new: %08lx\n", int2f_original_handler,
         __getvect(0x2f));
#endif
}

int unload_other_tsr() {
  struct CpuRegs regs;

#if DEBUG
  printf("Unloading previously resident instance.\n");
#endif
  x86_reset_regs(&regs);
  regs.w.ax = MULTIPLEX_MAGIC_AX;
  regs.w.bx = MULTIPLEX_MAGIC_BX;
  regs.w.dx = MULTIPLEX_CMD_UNINSTALL;

#if DEBUG
  printf("int 2fh.  AX:%04x BX:%04x DX:%04x\n", regs.w.ax, regs.w.bx,
         regs.w.dx);
#endif
  x86_call(0x2f, &regs);

  return regs.w.ax;
}

void print_usage(const char *prog) {
  printf("Usage: %s [-b #] [-d] [-e type] [-i irq#] [-u]\n", prog);
  printf("  -b  Count of Ethernet receive buffers (decimal).\n");
#if DEBUG
  printf("  -d  Show debug overlay.\n");
#endif
  printf("  -e  Override EtherType (4 hex digits).\n");
  printf("  -i  IRQ for packet driver.  Omit to auto-probe. (decimal)\n");
  printf("  -u  Uninstall resident TSR.\n");
}

int main(int argc, char *argv[]) {
  enum PktDrvResultCode r = 0;
  int opt = 0;
  int irq = 0;
  int unload = 0;
  int installed = 0;
  int safe_to_remove = 0;
  size_t buffers = DEFAULT_BUFFERS;
  struct CpuRegs regs;
  char tmp[32];
  void *keep_ptr = NULL;
  struct VideoState video_state;

  while (-1 != (opt = getopt(argv, argv, "b:de:hi:u"))) {
    switch (opt) {
      case 'b':
        buffers = atoi(optarg);
        break;

      case 'd':
#if DEBUG
        g_show_debug_overlay = 1;
#endif
        break;

      case 'e':
        // Using custom function, b/c `strtoul()` adds 500+ bytes to .text
        g_ethertype = hex_to_uint16(optarg);
        break;

      case 'h':
        print_usage(argv[0]);
        return EXIT_SUCCESS;

      case 'i':
        irq = atoi(optarg);
        break;

      case 'u':
        unload = 1;
        break;

      default:
        // Under "bcc", "argv[0]" is broken.  Instead of the program name,
        // it contains a pointer to "C" (assuming the first char of "C:\").
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
  }

  if ((optind < argc)) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (buffers < 1) {
    buffers = 1;
  } else if (buffers > MAX_BUFFERS) {
    buffers = MAX_BUFFERS;
  }

  video_init();
  
  // video_read_state(&video_state);
  // printf("Video State: %02x %02x %02x %02x\n", video_state.video_mode,
  //        video_state.active_page, video_state.text_rows, video_state.text_cols);

  // Check to see if a previous incarnation is already installed / "resident".
  x86_reset_regs(&regs);
  regs.w.ax = MULTIPLEX_MAGIC_AX;
  regs.w.bx = MULTIPLEX_MAGIC_BX;
  regs.w.dx = MULTIPLEX_CMD_INSTALL_CHECK;

  x86_call(0x2f, &regs);
  installed = !regs.w.bx;
  safe_to_remove = !regs.w.ax;

  if (unload) {
    if (!installed) {
      printf("Unable to uninstall, not installed.\n");
      return EXIT_FAILURE;
    }

    if (safe_to_remove) {
      unload_other_tsr();
      printf("Resident program removed. PSP was %04x\n", regs.w.cx);
      return EXIT_SUCCESS;
    } else {
      printf("Unable to uninstall, someone else hooked interrupt %02xh.\n",
             regs.w.ax);
      return EXIT_FAILURE;
    }
  }

  if (installed) {
    printf("Already installed.  PSP:%04x\n", regs.w.cx);
    return EXIT_SUCCESS;
  }

  // We must be loading and will "go TSR".
  buffer_init(buffers);
  protocol_init();
  session_mgr_init();

  r = pktdrv_init(irq);
  if (r) {
    printf("Packet Driver init failed, aborting.  Error %d\n", r);
    return r;
  }

  printf("Packet Driver Initialized.  irq:0x%02x, %s, %s, et:%04x\n",
         g_pktdrv_irq, g_pktdrv_info.name,
         fmt_mac_addr(tmp, g_pktdrv_info.mac_addr), g_ethertype);

  install_interrupt_handlers();

  keep_ptr = ((uint16_t)sbrk() + 15) & 0xfff0;
  printf("Going resident.  PSP:%04x, Last Addr: %04x\n", __psp, keep_ptr);

  // NOTE: Currently, this is a "tiny memory model" ("COM") program.
  // CS=DS=ES=SS=PSP.  If we convert to an EXE, then our PSP will be different.

  // Free our PSP's environment block, or we'll leak memory later.
  x86_reset_regs(&regs);
  regs.w.ax = 0x4900; // Free memory block.
  regs.es = __envseg;
  x86_call(0x21, &regs);

  // Zap the "envseg" from the PSP so that DOS won't try to free it later.
  *(uint16_t *)(__psp + 0x2c) = 0;

  // Tell DOS that we are going resident.
  // AH=31h (TSR), AL=0 (return code).
  x86_reset_regs(&regs);
  regs.w.ax = 0x3100;
  regs.w.dx = (uint16_t)keep_ptr >> 4;

  // If successful, this int 21h call will not return.
  x86_call(0x21, &regs);

  // Well crud...
  printf("TSR failed. AX = %04x\n", regs.w.ax);

  // We failed to go TSR, so we MUST unhook any interrupts that we've
  // captured, and tell the packet driver to not call into us either.
  restore_interrupt_handlers();
  pktdrv_done();
  return EXIT_FAILURE;
}
