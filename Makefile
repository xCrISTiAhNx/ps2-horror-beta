EE_BIN = bomberman.elf
EE_OBJS = main.o
EE_LIBS = -lpad -lps2ip -lgskit -ldmakit -lgs -lm

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal