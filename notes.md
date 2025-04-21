# Custom make file for building the project

`Makefile_rh`

- My system needs `LD86_LIBDIR=/usr/lib/bcc` instead of lib64
- Added a `copy` target to copy the out files to the tests machines
   - COPYDIR=~/prj/vintage/share

- Generate debug objects


# cursor

struct VgaState vga;
vga_read_state(&vga);

// V1_VGA_TEXT: Server -> Client
// Followed by raw data, to end of packet.
struct VgaText {
  uint8_t text_rows; // Current height of the screen
  uint8_t text_cols; // Current width of the screen
  //--- added 
  uint8_t cursor_row; // Current row of the cursor NEW
  uint8_t cursor_col; // Current column of the cursor NEW
  //---
  uint16_t offset;   // Byte offset from $b800:0
  uint16_t count;    // Count of BYTES of data in packet
};

- session send cursor possition to client

Create a pull request for 'cursor' on GitHub by visiting:
remote:      https://github.com/csegura/rmtdos/pull/new/cursor

# VGA / MDA
https://pdos.csail.mit.edu/6.828/2018/readings/hardware/vgadoc/VGABIOS.TXT

Video page 03
Video mode 00
rows 19
cols 50

# Protocol

sudo tcpdump 'ether proto 0x80ab'

# Todo

- When cursor moves without change screen, is not updated on client



