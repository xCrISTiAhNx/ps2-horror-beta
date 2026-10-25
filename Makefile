# BombFPS - Makefile para PS2SDK
#   make            -> bombfps.elf con online
#   make ONLINE=0   -> bombfps.elf solo local (para probar sin red)
#   make clean

ONLINE ?= 1

EE_BIN  = bombfps.elf
EE_OBJS = src/main.o shared/game.o
EE_INCS += -Ishared -Isrc
EE_CFLAGS += -O2 -Wall -DONLINE=$(ONLINE)
EE_LIBS = -lgskit -ldmakit -lpad -lm

ifeq ($(ONLINE),1)
EE_OBJS += shared/proto.o src/net_ps2.o src/ps2dev9_irx.o src/netman_irx.o src/smap_irx.o
EE_LIBS += -lps2ip -lnetman -lpatches -ldebug
endif

# IRX de red embebidos dentro del ELF
BIN2C ?= $(PS2SDK)/bin/bin2c
src/%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$*.irx $@ $*_irx
.PRECIOUS: src/%_irx.c

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) src/*_irx.c

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
