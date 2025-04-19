;  SPDX-FileCopyrightText: 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
;  SPDX-License-Identifier: GPL-2.0-or-later

; // vga_write(int x, int y, uint8_t attr, const char *str)
.text
.global _vga_write_str
_vga_write_str:
  push     bp
  mov      bp, sp
  push     es

  mov      ax, #$0040           ; BIOS data area
  mov      es, ax

  seg      es
  mov      bx, [$4a]            ; Number of screen columns
  seg      es
  mov      cx, [$4e]            ; Offset of current video page.

  mov      ax, #$b800           ; VGA TEXT mode frame buffer segment address.
  mov      es, ax

  mov      ax, [bp + 6]         ; AX = y
  mul      bl                   ; AX = y * width
  mov      bx, [bp + 4]         ; BX = x
  add      ax, bx               ; AX = (y * width) + x
  add      ax, ax               ; AX = 2 * ((y * width) + x)
  mov      di, ax               ; ES:DI points into VGA text buffer
  add      di, cx               ; Adjust for alternate video pages.

  mov      ah, [bp + 8]         ; AH = attr
  mov      si, [bp + 10]        ; DS:SI = str

vga_write_str_loop:
  mov      al, [si]             ; AL = *str (AX = attr,*str)
  or       al, al
  jz       vga_write_str_done

  seg      es
  mov     [di], ax              ; write BOTH the char and attr.
  inc      di
  inc      di
  inc      si
  jmp      vga_write_str_loop

vga_write_str_done:
  pop      es
  pop      bp
  ret


; // void vga_clear_rows(int y1, int y2)
.text
.global _vga_clear_rows
_vga_clear_rows:
  push     bp
  mov      bp, sp
  push     es

  mov      ax, #$b800
  mov      es, ax

  mov      ax, [bp + 6]          ; AX = y2
  mov      dx, [bp + 4]          ; DX = y1
  sub      ax, dx                ; AX = (y2 - y1) = row count
  mov      dl, #80               ; Assume 80-column text mode
  mul      al, dl                ; AX = word count
  mov      cx, ax                ; CX = word count

  mov      ax, [bp + 4]          ; AX = y1
  mov      dl, #160              ; 80 columns of 2-byte words
  mul      al, dl                ; AX = starting offset
  mov      di, ax                ; ES:DI = vga buffer pos to start at

  mov      ax, #$0720            ; "space on light grey".

  rep
  stosw

  pop      es
  pop      bp
  ret


; // void vga_gotoxy(int x, int y);
.text
.global _vga_gotoxy
_vga_gotoxy:
#if __FIRST_ARG_IN_AX__
  mov      bx, sp
  mov      dl, al
  mov      ax, [bx+2]
  mov      dh, al
#else
  mov      bx, sp
  mov      ax, [bx+4]
  mov      dh, al
  mov      ax, [bx+2]
  mov      dl, al
#endif
  mov      ah, #$02
  mov      bx, #7
  int      $10
  ret



; // void vga_copy_from_frame_buffer(
; //  void *dest,
; //  uint16_t offset,  // Offset from [$b800:0000]
; //  uint16_t words
; // );

.text
.global _vga_copy_from_frame_buffer
_vga_copy_from_frame_buffer:
  push     bp
  mov      bp, sp
  push     ds
  push     es
  push     si
  push     di

  ; [bp + 0] = previous frame's BP
  ; [bp + 2] = return address
  ; [bp + 4] = dest
  ; [bp + 6] = offset
  ; [bp + 8] = words

  ; Point 'es:di' to '*dest' (for later use in movsw)
  mov      ax, ds
  mov      es, ax              ; Writing to [es:di]
  mov      di, [bp + 4]        ; es:di points to '*dest'

  ; Point 'ds:si' to '$b800:offset'
  mov      ax, #$b800
  mov      ds, ax
  mov      si, [bp + 6]

  cld
  mov      cx, [bp + 8]
  rep
      movsw

  pop      di
  pop      si
  pop      es
  pop      ds
  pop      bp
  ret


; TODO: Figure out how to determine 'state->text_rows' (25, 28, 43, 50)
; Maybe read CRTC register 9, look at lowest 5 bits?
; https://ardent-tool.com/video/VGA_Video_Modes.html

; // void vga_read_state(struct VgaState *state);
.text
.global _vga_read_state
_vga_read_state:
  push    bp
  mov     bp, sp
  push    es
  push    di
  mov     di, [bp + 4]                 ; di = state

  ; Get current video mode.
  mov     ah, #$0f                     ; "get video mode"
  int     $10
  mov     [di], al                     ; state->video_mode
  mov     [di + 1], bh                 ; state->active_page
  mov     [di + 2], #25                ; state->text_rows (assume 25 for now)
  mov     [di + 3], ah                 ; state->text_cols

  ; Get cursor info.
  mov     ah, #$03                     ; "get cursor info"
                                       ; BH = page number (already set)
  int     $10
  mov     [di + 4], dh                 ; state->cursor_row
  mov     [di + 5], dl                 ; state->cursor_col

  ; Determine if we have an EGA or VGA video card.
  ; http://www.ctyme.com/intr/rb-0219.htm
  mov     ax, #$1a00                   ; Canonical VGA adapter check
  int     $10
  cmp     al, #$1a
  jne     vga_read_state_no_vga        ; No VGA, check for EGA
  cmp     bl, #6
  jb      vga_read_state_no_vga        ; No VGA, check for EGA

vga_read_state_have_vga_or_ega:
  mov     ax, #$0040                   ; BIOS data area.
  mov     es, ax
  seg     es
  mov     al, [$84]                    ; Screen rows (minus one).
  inc     al
  mov     [di + 2], al                 ; state->text_rows
  jmp     vga_read_state_done

vga_read_state_no_vga:
  ; Test for EGA
  mov     ax, #$1200
  mov     bx, #$0010
  int     $10
  cmp     bl, #$10
  jne     vga_read_state_have_vga_or_ega     ; EGA detected

vga_read_state_done:
  pop     es
  pop     di
  pop     bp
  ret


; // void vga_disable_blink();
.text
.global _vga_disable_blink
_vga_disable_blink:
  push    ax
  push    dx

  mov     dx, #$03da
  in      al, dx                    ; Reset index/data flip-flop.

  mov     dx, #$03c0
  mov     al, #$30
  out     dx, al                    ; Index = $30

  inc     dx
  in      al, dx                    ; Read VGA register contents.

  and     al, #$f7                  ; Disable 'blink'.

  dec     dx                        ; Write VGA register.
  out     dx, al

  pop     dx
  pop     ax
  ret
