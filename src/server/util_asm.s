;  SPDX-FileCopyrightText: 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
;  SPDX-License-Identifier: GPL-2.0-or-later

; // uint16_t htons(uint16_t x);
.text
.global _htons
_htons:
  push    bp
  mov     bp, sp
  mov     ax, [bp + 4]          ; Input arg (word)
  xchg    al, ah
  pop     bp
  ret                           ; Result in AX.


; // uint32_t htonl(uint32_t x);
.text
.global _htonl
_htonl:
  push    bp
  mov     bp, sp
  mov     ax, [bp + 6]          ; Pre-swap 2 words of the 4-byte input.
  mov     dx, [bp + 4]
  xchg    al, ah
  xchg    dl, dh
  pop     bp
  ret                           ; DX:AX = htonl(x)


;// extern int is_broadcast_mac_addr(uint8_t *mac_addr);
.text
.global _is_broadcast_mac_addr
_is_broadcast_mac_addr:
  push    bp
  mov     bp, sp
  mov     bx, [bp + 4]           ; BX = mac_addr
  mov     ax, [bx]
  and     ax, [bx + 2]
  and     ax, [bx + 4]           ; AX = bitwise AND of 3 words.
  clc
  add     ax, #1                 ; AX = 0 IFF all bits in mac_addr are set.
  xor     ax, #1                 ; AX = (logical) not AX
  pop     bp
  ret


; // extern void copy_mac_addr(uint8_t *dest, uint8_t *src);
.text
.global _copy_mac_addr
_copy_mac_addr:
  push    bp
  mov     bp, sp
  mov     di, [bp + 4]           ; DI = dest
  mov     si, [bp + 6]           ; SI = src
  mov     ax, [si]
  mov     [di], ax
  mov     ax, [si + 2]
  mov     [di + 2], ax
  mov     ax, [si + 4]
  mov     [di + 4], ax
  pop     bp
  ret
