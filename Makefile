#  SPDX-FileCopyrightText: 2022 Dennis Jenkins <dennis.jenkins.75@gmail.com>
#  SPDX-License-Identifier: GPL-2.0-or-later

# Using "dev86" (https://github.com/lkundrak/dev86)
AR=ar
AS86=as86
BCC=bcc
LD86=ld86

# Where to place the final deliverables.
OUTDIR=out

# Where to place intermediate compilation products.
TMPDIR=tmp

# TODO: Why is this needed?  Why is "ld86" unable to find "crt0.o" on its own?
LD86_LIBDIR=/usr/lib64/bcc

# The "client" runs on Linux.
CLIENT_BIN=$(OUTDIR)/rmtdos-client
NCURSESW_FLAGS += $(shell pkg-config ncursesw --cflags --libs)

# "rmtdos.com" runs on DOS.
RMTDOS_BIN=$(OUTDIR)/rmtdos.com
RMTDOS_MAP=$(OUTDIR)/rmtdos.map

# DOS Program to run VGA text mode tests.
VGADEMO_BIN=$(OUTDIR)/vga_demo.com
VGADEMO_MAP=$(OUTDIR)/vga_demo.map

# Library of functions used in more than one 16-bit build target.
LIB16_LIB=$(TMPDIR)/lib16.a

TOOLS=tools
KEYMAP_CONFIG_BIN=$(OUTDIR)/keymap_config
KEYMAP_TEST_BIN=$(OUTDIR)/keymap_test


all:	dirs $(CLIENT_BIN) $(RMTDOS_BIN) $(VGADEMO_BIN) $(KEYMAP_CONFIG_BIN) list

clean:
	@rm -rf $(OUTDIR) $(TMPDIR)
	@find ./src -type f -name "*.o" | xargs rm -f

dirs:
	@mkdir -p $(OUTDIR) $(TMPDIR)

list:
	ls -l $(OUTDIR)

format:
	@clang-format -i $$(find . -name "*.[ch]" | sort)
	-(test -x ~/.local/bin/mdformat && ~/.local/bin/mdformat README.md)

# Requires root privs to run.
# https://stackoverflow.com/a/46466642
setcap: $(CLIENT_BIN)
	setcap cap_net_raw,cap_net_admin=eip $(CLIENT_BIN)

typos:
	-(test -x ~/.cargo/bin/typos && ~/.cargo/bin/typos src/ README.md)

$(KEYMAP_CONFIG_BIN): src/tools/keymap_config.c src/client/keymaplib.c src/client/keymaplib.h
	@echo "Building keymap_config... $@  - $^"
	$(CC) -std=c99 -Wall -Isrc -ggdb -o $@ $^ $(NCURSESW_FLAGS)
	@echo "Copying keyboard maps to $(OUTDIR)..."
	cp -v src/tools/*.kmap $(OUTDIR)

# "client" runs on Linux in a terminal (ncurses).
$(CLIENT_BIN): src/client/*.c
	$(CC) -std=c99 -Wall -Isrc -ggdb -o $@ $^ $(NCURSESW_FLAGS)

# "lib16.a"
LIB16_C_SRC:=	$(sort $(basename $(wildcard src/lib16/*.c)))

LIB16_C_OBJ:=	$(LIB16_C_SRC:=.o)

$(LIB16_C_OBJ):		src/lib16/*.c src/lib16/*.h
	$(BCC) -O -Isrc -ansi -0 -Md -c -o $@ $(*:=.c)

LIB16_ASM_SRC:=	$(sort $(basename $(wildcard src/lib16/*.s)))

LIB16_ASM_OBJ:=	$(LIB16_ASM_SRC:=.o)

$(LIB16_ASM_OBJ):	src/lib16/*.s
	$(AS86) -0 -o $@ $(*:=.s)

$(LIB16_LIB):	$(LIB16_ASM_OBJ) $(LIB16_C_OBJ)
	rm -f $@
	$(AR) -r $@ $^

# "server.com" runs on DOS in real-mode.

RMTDOS_C_SRC:=	$(sort $(basename $(wildcard src/server/*.c)))

RMTDOS_C_OBJ:=	$(RMTDOS_C_SRC:=.o)

$(RMTDOS_C_OBJ):	src/server/*.c src/server/*.h src/common/*.h
	$(BCC) -O -Isrc -ansi -0 -Md -c -o $@ $(*:=.c)

RMTDOS_ASM_SRC:=	$(sort $(basename $(wildcard src/server/*.s)))

RMTDOS_ASM_OBJ:=	$(RMTDOS_ASM_SRC:=.o)

$(RMTDOS_ASM_OBJ):	src/server/*.s
	$(AS86) -0 -o $@ $(*:=.s)

$(RMTDOS_BIN):	$(LIB16_LIB)  $(RMTDOS_C_OBJ) $(RMTDOS_ASM_OBJ)
	$(LD86) -y -d -T100 -L$(LD86_LIBDIR) -0 -C0 -o $@ -ldos $^
	$(LD86) -y -d -T100 -L$(LD86_LIBDIR) -0 -C0 -o $@ -ldos -M $^ > $(RMTDOS_MAP)


bcclint:	src/common/*.h src/server/*.h src/server/*.c
	$(CC) -Wall -Wno-format -Wno-pointer-to-int-cast \
              -Wno-int-conversion -Wno-int-to-pointer-cast \
              -Isrc -m16 -o /dev/null $^

# "vga_demo.com"
$(VGADEMO_BIN):	$(LIB16_LIB) src/vga_demo/*.c
	$(BCC) -O -Isrc -ansi -0 -Md -o $@ $^
	$(BCC) -O -Isrc -ansi -0 -Md -o $@ -M $^ > $(VGADEMO_MAP)
