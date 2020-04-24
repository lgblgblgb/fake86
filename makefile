CC		= gcc
CC_WIN		= x86_64-w64-mingw32-gcc
SRCFILES	= src/fake86/*.c
HEADERS		= src/fake86/*.h
BINPATH		= /usr/local/bin
DATAPATH	= /usr/local/share/fake86
CFLAGS		= -std=c11 -O3 -Wall -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\" -D_DEFAULT_SOURCE
INCLUDE		= -Isrc/fake86
LIBS		= -lpthread -lX11
SDLFLAGS	= `sdl-config --cflags --libs`
BIN_FAKE86	= bin/fake86
BIN_IMAGEGEN	= bin/fake86-imagegen
BINS		= $(BIN_FAKE86) $(BIN_IMAGEGEN)

all: $(BINS)

$(BIN_FAKE86): $(SRCFILES) $(HEADERS)
	$(CC) $(SRCFILES) -o $@ $(CFLAGS) $(INCLUDE) $(LIBS) $(SDLFLAGS)

#$(BIN_FAKE86).exe: $(SRCFILES) $(HEADERS)
#	$(CC_WIN) $(SRCFILES) -o $@ $(CFLAGS) $(INCLUDE) $(LIBS_WIN) $(SDLFLAGS_WIN)

$(BIN_IMAGEGEN): src/imagegen/imagegen.c
	$(CC) $< -o $@ $(CFLAGS)

#$(BIN_IMAGEGEN).exe: src/imagegen/imagegen.c
#	$(CC) $< -o $@ $(CFLAGS_WIN)

test: $(BIN_FAKE86)
	$< -fd0 $(DATAPATH)/boot-floppy.img -speed 4000000 -boot 0

install: $(BINS)
	mkdir -p $(BINPATH) $(DATAPATH)
	cp $(BINS) $(BINPATH)/
	cp data/asciivga.dat data/pcxtbios.bin data/videorom.bin data/rombasic.bin $(DATAPATH)/

clean:
	rm -f src/fake86/*.o src/imagegen/*.o $(BINS) $(BIN_FAKE86).exe

uninstall:
	rm -f $(BINPATH)/fake86 $(BINPATH)/imagegen

.PHONY: all test install clean clean uninstall
