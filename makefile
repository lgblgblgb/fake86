CC		= gcc
SRCFILES	= src/fake86/*.c
HEADERS		= src/fake86/*.h
BINPATH		= /usr/local/bin
DATAPATH	= /usr/local/share/fake86
CFLAGS		= -std=c11 -O3 -Wall -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\"
INCLUDE		= -Isrc/fake86
LIBS		= -lpthread -lX11
SDLFLAGS	= `sdl-config --cflags --libs`
BIN_FAKE86	= bin/fake86
BIN_IMAGEGEN	= bin/fake86-imagegen
BINS		= $(BIN_FAKE86) $(BIN_IMAGEGEN)

all: $(BINS)

$(BIN_FAKE86): $(SRCFILES) $(HEADERS)
	$(CC) $(SRCFILES) -o $@ $(CFLAGS) $(INCLUDE) $(LIBS) $(SDLFLAGS)

$(BIN_IMAGEGEN): src/imagegen/imagegen.c
	$(CC) $< -o $@ $(CFLAGS)

test: $(BIN_FAKE86)
	$< -fd0 $(DATAPATH)/boot-floppy.img -boot 0

install: $(BINS)
	mkdir -p $(BINPATH) $(DATAPATH)
	cp $(BINS) $(BINPATH)/
	cp data/asciivga.dat data/pcxtbios.bin data/videorom.bin data/rombasic.bin $(DATAPATH)/

clean:
	rm -f src/fake86/*.o src/imagegen/*.o $(BINS)

uninstall:
	rm -f $(BINPATH)/fake86 $(BINPATH)/imagegen

.PHONY: all test install clean clean uninstall
