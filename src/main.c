/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers
            (C)2020      Gabor Lenart "LGB"

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* main.c: functions to initialize the different components of Fake86,
   load ROM binaries, and kickstart the CPU emulator. */

#include "config.h"
//#ifdef __APPLE__      /* Memory leaks occur in OS X when the SDL window gets */
#include <SDL.h>      /* resized if SDL.h not included in file with main() */
//#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "mutex.h"
#ifdef _WIN32
	extern CRITICAL_SECTION screenmutex;
#	include "win32/menus.h"
#else
#	include <unistd.h>
#	ifndef __APPLE__
#		include <X11/Xlib.h>
#	endif
	static pthread_t consolethread;
#endif

#include "fake86_release.h"
#include "hostfs.h"
#include "adlib.h"
#include "audio.h"
#include "i8253.h"
#include "i8259.h"
#include "i8237.h"
#include "console.h"
#include "video.h"
#include "ports.h"
#include "cpu.h"
#include "render.h"
#include "timing.h"
#include "parsecl.h"
#include "sndsource.h"
#include "blaster.h"
#include "sermouse.h"
#include "input.h"
#include "bios.h"
#ifdef NETWORKING_ENABLED
#	include "packet.h"
#endif

//uint64_t lasttick;
static uint64_t starttick, endtick;
//uint8_t dohardreset = 0;
//uint8_t usessource = 0;

//uint8_t cgaonly = 0;
//uint8_t useconsole = 0;

#ifdef DO_NOT_FORCE_UNREACHABLE
void UNREACHABLE_FATAL_ERROR ( void )
{
	fprintf(stderr, "FATAL_CONTROL_FLOW: Code point hit marked as 'unreachable'!!\n");
	exit(1);
}
#endif

#if 0
static uint32_t loadbinary (uint32_t addr32, const char *filename, uint8_t roflag)
{
	return hostfs_load_binary(filename, RAM + addr32, 1, 0x10000, "ROM");

	HOSTFS_FILE *file = hostfs_open(filename, "rb");
	if (!file)
		return 0;
	size_t readsize = hostfs_size(file);
	if (readsize > 0x10000 || readsize <= 0) {
		hostfs_close(file);
		return 0;
	}
	size_t ret = hostfs_read(file, RAM + addr32, 1, readsize);
	hostfs_close(file);
	if (ret != readsize)
		return 0;
	memset(readonly + addr32, roflag, readsize);
	return readsize;
#if 0
	FILE *binfile = fopen (filename, "rb");
	if (binfile == NULL)
		return 0;
	fseek (binfile, 0, SEEK_END);
	long readsize = ftell (binfile);
	if (readsize > 0x10000 || readsize <= 0) {
		fclose(binfile);
		return 0;
	}
	fseek (binfile, 0, SEEK_SET);
	long ret = fread(RAM + addr32, 1, readsize, binfile);
	fclose (binfile);
	if (ret != readsize)
		return 0;
	memset(readonly + addr32, roflag, readsize);
	return readsize;
#endif
}
#endif

#if 0
uint32_t loadrom (uint32_t addr32, const char *filename, uint8_t failure_fatal) {
	int readsize = hostfs_load_binary(filename, RAM + addr32, 1, 0x10000, "ROM");
	if (readsize <= 0) {
		if(failure_fatal)
			fprintf(stderr, "FATAL: Unable to load %s\n", filename);
		else
			printf("WARNING: Unable to load %s\n", filename);
		return 0;
	} else {
		printf("Loaded %s at 0x%05X (%d KB)\n", filename, addr32, readsize >> 10);
		return readsize;
	}
}
#endif

static uint32_t loadbios ( const char *filename )
{
	uint8_t bios[0x10000];
	int readsize = hostfs_load_binary(filename, bios, 1, 0x10000, "BIOS");
	if (readsize <= 0)
		return 0;
	memcpy(RAM + 0x100000 - readsize, bios, readsize);
	printf("BIOS %s loaded at 0x%05X (%d KB)\n", filename, 0x100000 - readsize, readsize >> 10);
	memset(readonly + 0x100000 - readsize, 1, readsize);
	return readsize;
}

#if 0
static void printbinary (uint8_t value) {
	int8_t curbit;
	for (curbit=7; curbit>=0; curbit--) {
		if ((value >> curbit) & 1)
			printf ("1");
		else
			printf ("0");
	}
}
#endif

static int inithardware( void )
{
#ifdef NETWORKING_ENABLED
	if (ethif != 254)
		initpcap();
#endif
	printf("Initializing emulated hardware:\n");
	memset(port_write_callback, 0, sizeof(port_write_callback));
	memset(port_read_callback, 0, sizeof(port_read_callback));
	memset(port_write_callback16, 0, sizeof(port_write_callback16));
	memset(port_read_callback16, 0, sizeof(port_read_callback16));
	printf("  - Intel 8253 timer: ");
	init8253();
	puts("OK");
	printf("  - Intel 8259 interrupt controller: ");
	init8259();
	puts("OK");
	printf("  - Intel 8237 DMA controller: ");
	init8237();
	puts("OK");
	initVideoPorts();
	if (usessource) {
		printf("  - Disney Sound Source: ");
		initsoundsource();
		puts("OK");
	}
#ifndef NETWORKING_OLDCARD
	printf("  - Novell NE2000 ethernet adapter: ");
	isa_ne2000_init(0x300, 6);
	puts("OK");
#endif
	printf("  - Adlib FM music card: ");
	initadlib(0x388);
	puts("OK");
	printf("  - Creative Labs Sound Blaster 2.0: ");
	initBlaster(0x220, 7);
	puts("OK");
	printf("  - Serial mouse (Microsoft compatible): ");
	initsermouse (0x3F8, 4);
	puts("OK");
	if (doaudio)
		initaudio();
	inittiming();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | (doaudio ? SDL_INIT_AUDIO : 0)))
		return sdl_error("Cannot initialize SDL2");
	if (initscreen(FAKE86_RELEASE_STRING))
		return 1;
	return 0;
}

//uint8_t audiobufferfilled(void);

#ifdef _WIN32
//void initmenus(void);
static void EmuThread (void *dummy) {
#else
static pthread_t emuthread;
static void *EmuThread (void *dummy) {
#endif
	while (running) {
		if (!speed)
			exec86(10000);
		else {
			exec86(speed / 100);
			while (!audiobufferfilled()) {
				timing();
				tickaudio();
			}
#ifdef _WIN32
			Sleep(10);
#else
			usleep(10000);
#endif
		}
		if (scrmodechange)
			doscrmodechange();
		if (dohardreset) {
			reset86();
			dohardreset = 0;
		}
	}
#ifndef _WIN32
	return NULL;
#endif
}


int main ( int argc, char *argv[] )
{
	puts(FAKE86_BANNER_STRING);
	if (hostfs_init())
		return -1;
	parsecl(argc, argv);
	memset(readonly, 0, RAM_SIZE);
	memset(RAM, 0, RAM_SIZE);
	if (!internalbios) {
		uint32_t biossize = loadbios(biosfile);
		if (!biossize)
			return -1;
		if (biossize <= 8192) {
			loadrom(0xF6000UL, DEFAULT_ROMBASIC_FILE, 0);
			if (!loadrom(0xC0000UL, DEFAULT_VIDEOROM_FILE, 1))
				return -1;
		}
	} else {
		memset(readonly + 0xC0000, 1, 0x40000);
		bios_internal_install();
	}
#ifdef DISK_CONTROLLER_ATA
	if (!loadrom(0xD0000UL, DEFAULT_IDEROM_FILE, 1))
		return -1;
#endif
	printf("\nInitializing CPU... ");
	running = 1;
	reset86();
	puts("OK!");
#if !defined(_WIN32) && !defined(__APPLE__)
	XInitThreads();
#endif
	if (inithardware())
		return -1;
#ifdef _WIN32
	initmenus();
	InitializeCriticalSection(&screenmutex);
#endif
	if (useconsole) {
#ifdef _WIN32
		_beginthread(runconsole, 0, NULL);
#else
		pthread_create(&consolethread, NULL, (void*)runconsole, NULL);
#endif
	}
#ifdef _WIN32
		_beginthread(EmuThread, 0, NULL);
#else
		pthread_create(&emuthread, NULL, (void*)EmuThread, NULL);
#endif
	lasttick = starttick = SDL_GetTicks();
	while (running) {
		handleinput();
#ifdef NETWORKING_ENABLED
		if (ethif < 254)
			dispatch();
#endif
#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif
	}
	endtick = (SDL_GetTicks() - starttick) / 1000;
	if (endtick == 0)
		endtick = 1; //avoid divide-by-zero exception in the code below, if ran for less than 1 second
	killaudio();
	if (renderbenchmark) {
		printf("\n%lu frames rendered in %lu seconds.\n", (long unsigned int)totalframes, (long unsigned int)endtick);
		printf("Average framerate: %lu FPS.\n", (long unsigned int)(totalframes / endtick));
	}
	printf("\n%lu instructions executed in %lu seconds.\n", (long unsigned int)totalexec, (long unsigned int)endtick);
	printf("Average speed: %lu instructions/second.\n", (long unsigned int)(totalexec / endtick));
#ifdef CPU_ADDR_MODE_CACHE
	printf("\n  Cached modregrm data access count: %lu\n", (long unsigned int)cached_access_count);
	printf("Uncached modregrm data access count: %lu\n", (long unsigned int)uncached_access_count);
#endif
	if (useconsole)
		exit(0); //makes sure console thread quits even if blocking
	return 0;
}
