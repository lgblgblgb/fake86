CC		= gcc
CC_WIN		= x86_64-w64-mingw32-gcc
SRCFILES	= $(wildcard src/*.c)
OBJFILES	= $(addprefix bin/objs/, $(notdir $(SRCFILES:.c=.o)))
SRCFILES_WIN	= $(wildcard src/win32/*.c) $(SRCFILES)
OBJFILES_WIN	= $(addprefix bin/objs-win/, $(notdir $(SRCFILES_WIN:.c=.o)))
#HEADERS		= $(wildcard src/*.h)
BINPATH		= /usr/local/bin
DATAPATH	= /usr/local/share/fake86
CFLAGS		= -std=c11 -Ofast -Wall -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\" -D_DEFAULT_SOURCE
CFLAGS_WIN	= -std=c11 -Ofast -Wall -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\" -D_DEFAULT_SOURCE
GENFLAGS	= -fno-common
GENFLAGS_WIN	= -fno-common
INCLUDE		= -Isrc
LIBS		= -lpthread -lX11
LIBS_WIN	=
#SDLFLAGS	= $(shell sdl2-config --cflags --libs)
SDL_CFLAGS	= $(shell sdl2-config --cflags)
SDL_LIBS	= $(shell sdl2-config --libs)
#SDLFLAGS_WIN	= $(shell x86_64-w64-mingw32-sdl2-config --cflags --libs)
SDL_CFLAGS_WIN  = $(shell x86_64-w64-mingw32-sdl2-config --cflags)
SDL_LIBS_WIN	= $(shell x86_64-w64-mingw32-sdl2-config --libs)
BIN_FAKE86	= bin/fake86
BIN_IMAGEGEN	= bin/fake86-imagegen
BINS		= $(BIN_FAKE86) $(BIN_IMAGEGEN)
DLL_SOURCE	= $(shell x86_64-w64-mingw32-sdl2-config --prefix)/bin/SDL2.dll
DLL_TARGET	= bin/SDL2.dll
ALLDEP		=

all: $(BINS)

winall: $(BIN_FAKE86).exe $(BIN_IMAGEGEN).exe

clangstricttest:
	clang $(SRCFILES) -o $(BIN_FAKE86) $(CFLAGS) $(GENFLAGS) -Weverything $(INCLUDE) $(LIBS) $(SDL_CFLAGS) $(SDL_LIBS)
	ls -l $(BIN_FAKE86)
	rm $(BIN_FAKE86)

bin/objs/%.o: src/%.c $(ALLDEP)
	$(CC) $(CFLAGS) $(GENFLAGS) $(INCLUDE) $(SDL_CFLAGS) -o $@ -c $<

bin/objs-win/%.o: src/%.c $(ALLDEP)
	$(CC_WIN) $(CFLAGS_WIN) $(GENFLAGS_WIN) $(INCLUDE) $(SDL_CFLAGS_WIN) -o $@ -c $<
bin/objs-win/menus.o: src/win32/menus.c $(ALLDEP)
	$(CC_WIN) $(CFLAGS_WIN) $(GENFLAGS_WIN) $(INCLUDE) $(SDL_CFLAGS_WIN) -o $@ -c $<

#$(BIN_FAKE86): $(SRCFILES) $(HEADERS)
#	$(CC) $(SRCFILES) -o $@ $(CFLAGS) $(INCLUDE) $(LIBS) $(SDLFLAGS)

$(BIN_FAKE86): $(OBJFILES) $(ALLDEP)
	$(CC) $(GENFLAGS) -o $@ $(OBJFILES) $(LIBS) $(SDL_LIBS)

#$(BIN_FAKE86).exe: $(SRCFILES) $(SRCFILES_PWIN) $(HEADERS)
#	$(CC_WIN) $(SRCFILES) $(SRCFILES_PWIN) -o $@ $(CFLAGS_WIN) $(INCLUDE) $(LIBS_WIN) $(SDLFLAGS_WIN)
#	$(MAKE) $(DLL_TARGET)

$(BIN_FAKE86).exe: $(OBJFILES_WIN) $(ALLDEP)
	$(CC_WIN) $(GENFLAGS_WIN) -o $@ $(OBJFILES_WIN) $(LIBS_WIN) $(SDL_LIBS_WIN)
	$(MAKE) $(DLL_TARGET)

$(DLL_TARGET): $(DLL_SOURCE)
	cp $< $@

$(BIN_IMAGEGEN): src/imagegen/imagegen.c $(ALLDEP)
	$(CC) $< -o $@ $(CFLAGS) $(GENFLAGS)

$(BIN_IMAGEGEN).exe: src/imagegen/imagegen.c $(ALLDEP)
	$(CC_WIN) $< -o $@ $(CFLAGS_WIN) $(GENFLAGS_WIN)

test: $(BIN_FAKE86)
	$< -fd0 $(DATAPATH)/boot-floppy.img -speed 20000000 -boot 0

wintest: $(BIN_FAKE86).exe $(DLL_TARGET)
	wine $(BIN_FAKE86).exe -fd0 $(DATAPATH)/boot-floppy.img -speed 20000000 -boot 0

install: $(BINS)
	mkdir -p $(BINPATH) $(DATAPATH)
	cp $(BINS) $(BINPATH)/
	cp data/asciivga.dat data/pcxtbios.bin data/videorom.bin data/rombasic.bin $(DATAPATH)/

clean:
	rm -f src/*.o src/imagegen/*.o $(BINS) $(BIN_FAKE86).exe $(BIN_IMAGEGEN).exe $(DLL_TARGET) bin/objs/*.o bin/objs-win/*.o

uninstall:
	rm -f $(BINPATH)/fake86 $(BINPATH)/imagegen

.PHONY: all winall test wintest install clean clean uninstall clangstricttest
