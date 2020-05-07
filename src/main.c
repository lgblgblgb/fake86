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
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "mutex.h"
#ifdef _WIN32
	//extern CRITICAL_SECTION screenmutex;
#	include "windows.h"
#	include "win32/menus.h"
#else
#	include <unistd.h>
#	if !defined(__APPLE__) && defined(USE_XINITTHREADS)
#		include <X11/Xlib.h>
#		warning "Using USE_XINITTHREADS"
#	endif
	//static pthread_t consolethread;
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
#ifdef USE_KVM
#	include "kvm.h"
#endif

static uint64_t starttick, endtick;


#ifdef DO_NOT_FORCE_UNREACHABLE
void UNREACHABLE_FATAL_ERROR ( void )
{
	fprintf(stderr, "FATAL_CONTROL_FLOW: Code point hit marked as 'unreachable'!!\n");
	exit(1);
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


static int inithardware( void )
{
#ifdef NETWORKING_ENABLED
	if (ethif != 254)
		initpcap();
#endif
	printf("Initializing emulated hardware:\n");
	ports_init();
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
	initsermouse(0x3F8, 4);
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



static int EmuThread(void *ptr)
{
	puts("CPU: starting to execute.");
#ifdef USE_KVM
	if (usekvm)
		while (running) {
			cpu_regs_to_kvm();
			if (kvm_run()) {
				fprintf(stderr, "FATAL ERROR: exiting because of KVM problem.\n");
				running = 0;
				break;
			}
			cpu_regs_from_kvm();
			switch (kvm.kvm_run->exit_reason) {
				case KVM_EXIT_IO:
					if (kvm.kvm_run->io.direction == KVM_EXIT_IO_OUT) {
						printf("KVM: output request, port %Xh IO-size %d\n",
							kvm.kvm_run->io.port,
							kvm.kvm_run->io.size
						);
						//if (kvm.kvm_run->io.size == 1)
						//	;
					} else if (kvm.kvm_run->io.direction == KVM_EXIT_IO_IN) {
						printf("KVM: input request, port %Xh IO-size %d\n",
							kvm.kvm_run->io.port,
							kvm.kvm_run->io.size
						);
					} else {
						printf("KVM: unknown I/O event requested! (%d)\n", kvm.kvm_run->io.direction);
					}
					exec86(1);	// execute a single opcode with the software emulator, hopefully the I/O one, which was the reason KVM exited from VM run mode.
					continue;
				case KVM_EXIT_HLT:
					// cpu_hlt_handler() is designed for the software x86 emulation
					// it expects the saveip (!) of the HLT, but it seems we passed it already with KVM, let's decrement IP ;)
					// And also populate it as 'saveip' what bios.c uses by cpu_hlt_handler()
					cpu.ip--;
					cpu.saveip = cpu.ip;
					if (cpu_hlt_handler() == 0) {	// Fake86 bios internal trap! :)
						continue;
					}
					puts("KVM: HLT condition?!");
					break;
			}
			printf("KVM is not ready yet :(\n");
			running = 0;
			break;
		}
	else
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
	return 0;
}


int main ( int argc, char *argv[] )
{
	puts(FAKE86_BANNER_STRING);
	// Initialize SDL with "nothing". So we delay audo/video/etc (whatever needed later) initialization.
	// However this initalize SDL for using its own functionality only, like SDL_GetPrefPath and other things!
        if (SDL_Init(0))
		return sdl_error("Cannot pre-initialize SDL2");
	atexit(sdl_shutdown);
	if (hostfs_init())
		return -1;
	parsecl(argc, argv);
#ifdef USE_KVM
	if (!usekvm) {
		RAM = SDL_malloc(RAM_SIZE);
		if (!RAM) {
			fprintf(stderr, "Cannot allocate memory!\n");
			return -1;
		}
		printf("MEM: allocated system memory (%uK) at %p for software CPU\n", (RAM_SIZE >> 10), RAM);
	} else {
		if (kvm_init(RAM_SIZE)) {
			fprintf(stderr, "Cannot initialize KVM!\n");
			return -1;
		}
		RAM = kvm.mem;
		printf("MEM: allocated system memory (%uK) at %p via mmap() for KVM\n", (RAM_SIZE >> 10), RAM);
		//fprintf(stderr, "KVM: yet unimplemented...\n");
		//return -1;
	}
#else
	printf("MEM: using static memory (%uK) for software CPU\n", (RAM_SIZE >> 10));
#endif
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
#if !defined(_WIN32) && !defined(__APPLE__) && defined(USE_XINITTHREADS)
	XInitThreads();
#endif
	if (inithardware())
		return -1;
#ifdef _WIN32
	initmenus();
	//InitializeCriticalSection(&screenmutex);
#endif
	if (useconsole) {
		if (!SDL_CreateThread(ConsoleThread, "Fake86ConsoleThread", NULL)) {
			fprintf(stderr, "WARNING: console thread cannot be created, console will be unavailable: %s\n", SDL_GetError());
		}
	}
	if (!SDL_CreateThread(EmuThread, "Fake86EmuThread", NULL)) {
		fprintf(stderr, "Could not create the main emuthread: %s\n", SDL_GetError());
		return -1;
	}
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
