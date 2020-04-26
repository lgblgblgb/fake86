CC		= gcc
CC_WIN		= x86_64-w64-mingw32-gcc
SRCFILES	= src/fake86/*.c
SRCFILES_PWIN	= src/fake86/win32/*.c
HEADERS		= src/fake86/*.h
BINPATH		= /usr/local/bin
DATAPATH	= /usr/local/share/fake86
CFLAGS		= -std=c11 -Ofast -Wall -fno-common -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\" -D_DEFAULT_SOURCE
CFLAGS_WIN	= -std=c11 -Ofast -Wall -fno-common -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\" -D_DEFAULT_SOURCE
INCLUDE		= -Isrc/fake86
LIBS		= -lpthread -lX11
LIBS_WIN	=
SDLFLAGS	= $(shell sdl2-config --cflags --libs)
SDLFLAGS_WIN	= $(shell x86_64-w64-mingw32-sdl2-config --cflags --libs)
BIN_FAKE86	= bin/fake86
BIN_IMAGEGEN	= bin/fake86-imagegen
BINS		= $(BIN_FAKE86) $(BIN_IMAGEGEN)
DLL_SOURCE	= $(shell x86_64-w64-mingw32-sdl2-config --prefix)/bin/SDL2.dll
DLL_TARGET	= bin/SDL2.dll

all: $(BINS)

winall: $(BIN_FAKE86).exe $(BIN_IMAGEGEN).exe

clangstricttest:
	clang $(SRCFILES) -o $(BIN_FAKE86) $(CFLAGS) -Weverything $(INCLUDE) $(LIBS) $(SDLFLAGS)

$(BIN_FAKE86): $(SRCFILES) $(HEADERS)
	$(CC) $(SRCFILES) -o $@ $(CFLAGS) $(INCLUDE) $(LIBS) $(SDLFLAGS)

$(BIN_FAKE86).exe: $(SRCFILES) $(SRCFILES_PWIN) $(HEADERS)
	$(CC_WIN) $(SRCFILES) $(SRCFILES_PWIN) -o $@ $(CFLAGS_WIN) $(INCLUDE) $(LIBS_WIN) $(SDLFLAGS_WIN)
	$(MAKE) $(DLL_TARGET)

$(DLL_TARGET): $(DLL_SOURCE)
	cp $< $@

$(BIN_IMAGEGEN): src/imagegen/imagegen.c
	$(CC) $< -o $@ $(CFLAGS)

$(BIN_IMAGEGEN).exe: src/imagegen/imagegen.c
	$(CC_WIN) $< -o $@ $(CFLAGS_WIN)

test: $(BIN_FAKE86)
	$< -fd0 $(DATAPATH)/boot-floppy.img -speed 20000000 -boot 0

wintest: $(BIN_FAKE86).exe $(DLL_TARGET)
	wine $(BIN_FAKE86).exe -fd0 $(DATAPATH)/boot-floppy.img -speed 20000000 -boot 0

install: $(BINS)
	mkdir -p $(BINPATH) $(DATAPATH)
	cp $(BINS) $(BINPATH)/
	cp data/asciivga.dat data/pcxtbios.bin data/videorom.bin data/rombasic.bin $(DATAPATH)/

clean:
	rm -f src/fake86/*.o src/imagegen/*.o $(BINS) $(BIN_FAKE86).exe $(BIN_IMAGEGEN).exe $(DLL_TARGET)

uninstall:
	rm -f $(BINPATH)/fake86 $(BINPATH)/imagegen

.PHONY: all winall test wintest install clean clean uninstall clangstricttest
