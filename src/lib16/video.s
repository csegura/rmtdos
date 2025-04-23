;  SPDX-FileCopyrightText: 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
;  SPDX-FileCopyrightText: 2025 r0mheat <romheat@gmail.com>
;  SPDX-License-Identifier: GPL-2.0-or-later

; Will store the detected video segment (0xB800 or 0xB000)
; here .data = .text 
.data

SEG_COLOR = $B800
SEG_MDA   = $B000

.global _video_address
_video_address:
  dw      0                   

; uint16_t detect_video_segment(void);
; Returns 0xB800 for VGA/CGA or 0xB000 for MDA/Hercules
.text
.global _video_get_segment
_video_get_segment:
  push     bp
  mov      bp, sp

  mov      ax, #$0040               ; BIOS data area
  mov      es, ax

  seg      es
  mov      al, [$10]                ; Active display mode
  and      al, #$30                 ; Check bits 4-5 for adapter type
  cmp      al, #$30                 ; MDA mode?
  jne      not_mda

  mov      ax, #$b000               ; Return MDA segment
  jmp      vgs_done

not_mda:
  mov      ax, #$b800               ; Return VGA/CGA segment

vgs_done:
  pop      bp
  ret

; void video_init(void);
; Detects and stores the video segment for later use. Must be called first.
.text
.global _video_init
_video_init:
    push    bp
    mov     bp, sp
    call    _video_get_segment      ; Detect segment (result in AX)
    mov     [_video_address], ax    ; Store it globally
    pop     bp
    ret

; void video_write_str(int x, int y, uint8_t attr, const char *str)
; Writes string using the globally stored video segment.
.text
.global _video_write_str
_video_write_str:
  push     bp
  mov      bp, sp
  push     es                       ; Save ES

  ;  Get video segment from global variable 
  mov      ax, [_video_address]
  mov      es, ax                   ; ES = Video Segment

  ;  Calculate screen offset 
  push     ds                       ; Need DS temporarily for BIOS data
  mov      ax, #$0040               ; BIOS data area
  mov      ds, ax   
  mov      bx, [$4a]                ; Number of screen columns (width)
  pop      ds                       ; Restore original DS

  mov      ax, [bp + 6]             ; AX = y  (Stack: [bp+2]=ret, [bp+4]=x, [bp+6]=y, [bp+8]=attr, [bp+10]=str)
  mul      bl                       ; AX = y * width
  mov      bx, [bp + 4]             ; BX = x
  add      ax, bx                   ; AX = (y * width) + x
  add      ax, ax                   ; AX = 2 * ((y * width) + x) ; byte offset
  mov      di, ax                   ; ES:DI points into text buffer start

  ; Handle page offset if needed (for VGA only)
  cmp      word ptr [_video_address], #$b800
  jne      skip_page_offset         ; Skip for MDA

  push     ds                       ; Need DS again for BIOS data
  mov      ax, #$0040
  mov      ds, ax
  mov      cx, [$4e]                ; Offset of current video page (word offset)
  pop      ds                       ; Restore original DS
  add      di, cx                   ; Adjust DI for alternate video pages (byte offset)

skip_page_offset:
  ;  Write the string 
  mov      ah, [bp + 8]             ; AH = attr
  mov      si, [bp + 10]            ; DS:SI = str (Assume near pointer)

video_write_str_loop:
  lodsb                             ; AL = [DS:SI], SI++ (Get char)
  or       al, al
  jz       video_write_str_done

  seg      es
  stosw                             ; Store AX (attr, char) to [ES:DI], DI += 2

  jmp      video_write_str_loop

video_write_str_done:
  pop      es                       ; Restore original ES
  pop      bp
  ret                               ; Return (8 bytes of params popped by caller if cdecl)


; void video_gotoxy(int x, int y);
; Same vga_gotoxy using interrupt
.text
.global _video_gotoxy
_video_gotoxy:
#if __FIRST_ARG_IN_AX__
  mov      bx, sp
  mov      dl, al
  mov      ax, [bx+2]
  mov      dh, al
#else
  mov      bx, sp
  mov      ax, [bx+4]               ; y
  mov      dh, al
  mov      ax, [bx+2]               ; x
  mov      dl, al
#endif
  mov      ah, #$02                 ; Set cursor position BIOS call
  mov      bh, #0                   ; Assume page 0 (can be read from VgaState if needed)
  int      $10
  ret


; void video_read_state(struct VgaState *state);
; Read video info handling for MDA, CGA, EGA, and VGA.
; struct VgaState {
;   unsigned char video_mode;   // offset 0
;   unsigned char active_page;  // offset 1
;   unsigned char text_rows;    // offset 2
;   unsigned char text_cols;    // offset 3
;   unsigned char cursor_row;   // offset 4
;   unsigned char cursor_col;   // offset 5
; };

.text
.global _video_read_state
_video_read_state:
  push    bp
  mov     bp, sp
  push    es
  push    di
  mov     di, [bp + 4]              ; di = state

  ; Get current video mode.
  mov     ah, #$0f                  ; "get video mode"
  int     $10
  mov     [di], al                  ; state->video_mode
  mov     [di + 1], bh              ; state->active_page
  mov     [di + 2], #25             ; state->text_rows (assume 25 for now)
  mov     [di + 3], ah              ; state->text_cols

  ; Get cursor info.
  mov     ah, #$03                  ; "get cursor info"
                                    ; BH = page number (already set)
  int     $10
  mov     [di + 4], dh              ; state->cursor_row
  mov     [di + 5], dl              ; state->cursor_col

  ; Determine if we have an EGA or VGA video card.
  ; http://www.ctyme.com/intr/rb-0219.htm
  mov     ax, #$1a00                ; Canonical VGA adapter check
  int     $10
  cmp     al, #$1a
  jne     video_rs_no_vga           ; No VGA, check for EGA
  cmp     bl, #6
  jb      video_rs_no_vga           ; No VGA, check for EGA

video_rs_ega_vga:
  mov     ax, #$0040                ; BIOS data area.
  mov     es, ax
  seg     es
  mov     al, [$84]                 ; Screen rows (minus one).
  inc     al
  mov     [di + 2], al              ; state->text_rows
  jmp     video_rs_done

video_rs_no_vga:
  ; Test for EGA
  mov     ax, #$1200
  mov     bx, #$0010
  int     $10
  cmp     bl, #$10
  jne     video_rs_ega_vga          ; EGA detected

video_rs_done:
  pop     es
  pop     di
  pop     bp
  ret

; void video_clear_rows(int y1, int y2)
; Clears rows using the globally stored video segment.
.text
.global _video_clear_rows
_video_clear_rows:
  push     bp
  mov      bp, sp
  push     es
  push     di

  ;  Get video segment 
  mov      ax, [_video_address]
  mov      es, ax                   ; ES = Video Segment

  ;  Calculate count and starting offset 
  mov      ax, [bp + 6]             ; AX = y2 
  mov      dx, [bp + 4]             ; DX = y1
  inc      ax                       ;
  sub      ax, dx                   ; AX = (y2 - y1 + 1) = row count
  mov      dl, #80                  ; Assume 80-column text mode width
  mul      dl                       ; AX = row count * 80 columns
  mov      cx, ax                   ; CX = word count (chars+attrs)

  mov      ax, [bp + 4]             ; AX = y1 (starting row)
  mov      dl, #160                 ; 80 columns * 2 bytes/char = 160 bytes per row
  mul      dl                       ; AX = starting byte offset
  mov      di, ax                   ; ES:DI = buffer pos to start clearing

  ;  Determine fill character/attribute 
  cmp      word ptr [_video_address], #$b800
  jne      mda_attribute

  mov      ax, #$0720               ; VGA "space on light grey" 
  jmp      fill_screen

mda_attribute:
  mov      ax, #$0720               ; MDA "space"

fill_screen:
  
  cld                               ; Ensure direction is forward
  rep 
  stosw                             ; Store AX (char+attr) CX times to ES:DI, incrementing DI

  pop      di                   
  pop      es                   
  pop      bp
  ret                               

; void video_copy_from_frame_buffer(
;  void *dest,        Destination buffer (near pointer in DS)
;  uint16_t offset,   Source offset in video segment
;  uint16_t words     Number of words to copy
; );
; Copies data FROM video memory TO program memory using global segment.
.text
.global _video_copy_from_frame_buffer
_video_copy_from_frame_buffer:
  push     bp
  mov      bp, sp
  push     ds           
  push     es           
  push     si          
  push     di        

  ; [bp + 4] = dest 
  ; [bp + 6] = offset
  ; [bp + 8] = words
  
  mov      ax, ds 
  mov      es, ax  
  mov      di, [bp + 4]             ; dest into di

  mov      ax, [_video_address]      
  mov      ds, ax                   
  mov      si, [bp + 6]             ; offset into si

  ;  Perform Copy Operation 
  cld                               ; flag is forward for movsw
  mov      cx, [bp + 8]             ; words to copy
  rep 
  movsw                             

  ; restore & return
  pop      di
  pop      si
  pop      es
  pop      ds           
  pop      bp
  ret                  

; uint16_t video_checksum_frame_buffer(
;  uint16_t offset,      Offset from frame buffer start
;  uint16_t words        Number of words to checksum
; );
; Calculate screen checksum 

.text
.global _video_checksum_frame_buffer
_video_checksum_frame_buffer:
  push     bp
  mov      bp, sp
  push     ds           
  push     si           
  push     bx           
  push     dx           

  ; [bp + 0] = previous frame's BP
  ; [bp + 2] = return address
  ; [bp + 4] = offset
  ; [bp + 6] = words
  
  mov      ax, [_video_address] 
  mov      ds, ax
  mov      si, [bp + 4]             ; offset into si

  mov      cx, [bp + 6]             ; words (always 2)
  xor      dx, dx                   ; init 0

checksum_loop:
  seg      ds                       
  mov      bx, [si]                 ; load ds:si
  xchg     bh, bl                   ; swap bytes for checksum
  add      dx, bx                   ; add
  rol      dx, #1                   ; rotate checksum left by 1 bit 
  add      si, #2                   ; sum
  loop     checksum_loop            

  mov      ax, dx                   ; return checksum  

  pop      dx           
  pop      bx
  pop      si
  pop      ds
  pop      bp
  ret                 
