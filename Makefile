CC		= gcc
CC_WIN		= x86_64-w64-mingw32-gcc
WINDRES		= x86_64-w64-mingw32-windres
STRIP		= strip
STRIP_WIN	= x86_64-w64-mingw32-strip
SRCFILES	= $(wildcard src/*.c)
OBJFILES	= $(addprefix bin/objs/, $(notdir $(SRCFILES:.c=.o)))
SRCFILES_WIN	= $(wildcard src/win32/*.c) $(SRCFILES)
OBJFILES_WIN	= $(addprefix bin/objs-win/, $(notdir $(SRCFILES_WIN:.c=.o))) bin/objs-win/windres.o
BINPATH		= /usr/local/bin
DATAPATH	= /usr/local/share/fake86
#NETWORKING_ENABLED is not well tested. You can try to enable and put -lpcap into LIBS as well. No idea about the windows part ...
#NETWORKEMU	= -DNETWORKING_ENABLED
NETWORKEMU	=
CFLAGS		= -std=c11 -Ofast -Wall -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\" -D_DEFAULT_SOURCE $(NETWORKEMU)
CFLAGS_WIN	= -std=c11 -Ofast -Wall -pipe -DPATH_DATAFILES=\"$(DATAPATH)/\" -D_DEFAULT_SOURCE $(NETWORKEMU)
GENFLAGS	= -fno-common
GENFLAGS_WIN	= -fno-common
INCLUDE		= -Isrc
LIBS		= -lpthread -lX11
LIBS_WIN	=
SDL_CFLAGS	= $(shell sdl2-config --cflags)
SDL_LIBS	= $(shell sdl2-config --libs)
SDL_CFLAGS_WIN  = $(shell x86_64-w64-mingw32-sdl2-config --cflags)
SDL_LIBS_WIN	= $(shell x86_64-w64-mingw32-sdl2-config --libs)
BIN_FAKE86	= bin/fake86
BIN_IMAGEGEN	= bin/fake86-imagegen
BINS		= $(BIN_FAKE86) $(BIN_IMAGEGEN)
BINS_WIN	= $(BIN_FAKE86).exe $(BIN_IMAGEGEN).exe
DLL_SOURCE	= $(shell x86_64-w64-mingw32-sdl2-config --prefix)/bin/SDL2.dll
DLL_TARGET	= bin/SDL2.dll
ALLDEP		=
DEPFILE		= bin/objs/.depend
DEPFILE_WIN	= bin/objs-win/.depend

all: $(BINS)

winall: $(BINS_WIN)

clangstricttest:
	clang $(SRCFILES) -o $(BIN_FAKE86) $(CFLAGS) $(GENFLAGS) -Weverything $(INCLUDE) $(LIBS) $(SDL_CFLAGS) $(SDL_LIBS)
	ls -l $(BIN_FAKE86)
	rm $(BIN_FAKE86)

genbininclude:
	bin/tools/asciidump src/bindata.c src/bindata.h bin/data/asciivga.dat:mem_asciivga_dat

bin/objs/%.o: src/%.c $(ALLDEP)
	$(CC) $(CFLAGS) $(GENFLAGS) $(INCLUDE) $(SDL_CFLAGS) -o $@ -c $<

bin/objs-win/windres.o: src/win32/rc/fake86.rc
	$(WINDRES) $< -O coff -o $@

bin/objs-win/%.o: src/%.c $(ALLDEP)
	$(CC_WIN) $(CFLAGS_WIN) $(GENFLAGS_WIN) $(INCLUDE) $(SDL_CFLAGS_WIN) -o $@ -c $<
bin/objs-win/%.o: src/win32/%.c $(ALLDEP)
	$(CC_WIN) $(CFLAGS_WIN) $(GENFLAGS_WIN) $(INCLUDE) $(SDL_CFLAGS_WIN) -o $@ -c $<

$(BIN_FAKE86): $(OBJFILES) $(ALLDEP)
	$(CC) $(GENFLAGS) -o $@ $(OBJFILES) $(LIBS) $(SDL_LIBS)

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
	$(STRIP) $(BINS)
	cp $(BINS) $(BINPATH)/
	cp bin/data/asciivga.dat bin/data/pcxtbios.bin bin/data/videorom.bin bin/data/rombasic.bin $(DATAPATH)/

clean:
	rm -f src/*.o src/imagegen/*.o $(BINS) $(BINS_WIN) $(DLL_TARGET) bin/objs/*.o bin/objs-win/*.o $(DEPFILE) $(DEPFILE_WIN)

uninstall:
	rm -f $(BINPATH)/fake86 $(BINPATH)/imagegen

strip:
	@test -f $(BIN_FAKE86) && $(STRIP) $(BIN_FAKE86) || echo "Not found: $(BIN_FAKE86)"
	@test -f $(BIN_FAKE86).exe && $(STRIP_WIN) $(BIN_FAKE86).exe || echo "Not found: $(BIN_FAKE86).exe"
	@test -f $(BIN_IMAGEGEN) && $(STRIP) $(BIN_IMAGEGEN) || echo "Not found: $(BIN_IMAGEGEN)"
	@test -f $(BIN_IMAGEGEN).exe && $(STRIP_WIN) $(BIN_IMAGEGEN).exe || echo "Not found: $(BIN_IMAGEGEN).exe"

$(DEPFILE):
	$(CC) -MM $(CFLAGS) $(GENFLAGS) $(INCLUDE) $(SDL_CFLAGS) $(SRCFILES) | sed -E 's/^([^: ]+.o):/bin\/objs\/\0/' > $@
$(DEPFILE_WIN):
	$(CC_WIN) -MM $(CFLAGS_WIN) $(GENFLAGS_WIN) $(INCLUDE) $(SDL_CFLAGS_WIN) $(SRCFILES_WIN) | sed -E 's/^([^: ]+.o):/bin\/objs-win\/\0/' > $@

sdl2wininstall:
	bin/tools/install-cross-win-mingw-sdl-on-linux.sh /usr/local/bin

dep:
	rm -f $(DEPFILE)
	$(MAKE) $(DEPFILE)
windep:
	rm -f $(DEPFILE_WIN)
	$(MAKE) $(DEPFILE_WIN)

.PHONY: all winall clangstricttest genbininclude test wintest install clean uninstall strip sdl2wininstall dep windep

ifneq ($(wildcard $(DEPFILE)),)
include $(DEPFILE)
endif
ifneq ($(wildcard $(DEPFILE_WIN)),)
include $(DEPFILE_WIN)
endif
