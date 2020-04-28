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

/* parsecl.c: Fake86 command line parsing for runtime options. */

#include "config.h"
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parsecl.h"

#include "disk.h"
#include "audio.h"
#include "video.h"
#include "render.h"
#include "cpu.h"
#include "packet.h"
#include "hostfs.h"

#ifndef _WIN32
#define strcmpi strcasecmp
#else
#define strcmpi _stricmp
#endif

uint8_t dohardreset = 0;

uint16_t constantw = 0, constanth = 0;
uint8_t slowsystem = 0;

char *biosfile = NULL;
uint32_t speed = 0;
uint8_t verbose = 0;
uint8_t useconsole = 0;
// uint8_t cgaonly = 0;
uint8_t usessource = 0;

static uint32_t hextouint(char *src) {
	// should be converted to std lib stuff.
	uint32_t tempuint = 0;
	sscanf(src, "%x", &tempuint);
	for (int i=0; i<strlen(src); i++) {
		uint32_t cc = src[i];
		if (cc == 0) break;
		if ((cc >= 'a') && (cc <= 'F')) cc = cc - 'a' + 10;
			else if ((cc >= 'A') && (cc <= 'F')) cc =  cc - 'A' + 10;
			else if ((cc >= '0') && (cc <= '9')) cc = cc - '0';
			else return 0;
		tempuint <<= 4;
		tempuint |= cc;
	}
	return tempuint;
}


static void showhelp ( void )
{
	puts(
		"Fake86 requires some command line parameters to run.\nValid options:\n"
		"  -fd0 filename    Specify a floppy disk image file to use as floppy 0.\n"
		"  -fd1 filename    Specify a floppy disk image file to use as floppy 1.\n"
		"  -hd0 filename    Specify a hard disk image file to use as hard drive 0.\n"
		"  -hd1 filename    Specify a hard disk image file to use as hard drive 1.\n"
		"  -boot #          Specify which BIOS drive ID should be the boot device in #.\n"
		"                   Examples: -boot 0 will boot from floppy 0.\n"
		"                             -boot 1 will boot from floppy 1.\n"
		"                             -boot 128 will boot from hard drive 0.\n"
		"                             -boot rom will boot to ROM BASIC if available.\n"
		"                   Default boot device is hard drive 0, if it exists.\n"
		"                   Otherwise, the default is floppy 0.\n"
		"  -bios filename   Specify alternate BIOS ROM image to use.\n"
#ifdef NETWORKING_ENABLED
#ifdef _WIN32
		"  -net #           Enable ethernet emulation via winpcap, where # is the\n"
#else
		"  -net #           Enable ethernet emulation via libpcap, where # is the\n"
#endif
		"                   numeric ID of your host's network interface to bridge.\n"
		"                   To get a list of possible interfaces, use -net list\n"
#endif
		"  -nosound         Disable audio emulation and output.\n"
		"  -fullscreen      Start Fake86 in fullscreen mode.\n"
		"  -verbose         Verbose mode. Operation details will be written to stdout.\n"
		"  -delay           Specify how many milliseconds the render thread should idle\n"
		"                   between drawing each frame. Default is 20 ms. (~50 FPS)\n"
		"  -slowsys         If your machine is very slow and you have audio dropouts,\n"
		"                   use this option to sacrifice audio quality to compensate.\n"
		"                   If you still have dropouts, then also decrease sample rate\n"
		"                   and/or increase latency.\n"
		"  -resw # -resh #  Force a constant window size in pixels.\n"
		"  -smooth          Apply smoothing to screen rendering.\n"
		"  -noscale         Disable 2x scaling of low resolution video modes.\n"
		"  -ssource         Enable Disney Sound Source emulation on LPT1.\n"
		"  -latency #       Change audio buffering and output latency. (default: 100 ms)\n"
		"  -samprate #      Change audio emulation sample rate. (default: 48000 Hz)\n"
		"  -console         Enable console on stdio during emulation.\n"
		"  -oprom addr rom  Inject a custom option ROM binary at an address in hex.\n"
		"                   Example: -oprom F4000 monitor.bin\n"
		"                            This loads the data from monitor.bin at 0xF4000.\n"
		"\nThis program is free software; you can redistribute it and/or\n"
		"modify it under the terms of the GNU General Public License\n"
		"as published by the Free Software Foundation; either version 2\n"
		"of the License, or (at your option) any later version.\n\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"GNU General Public License for more details."
	);
	exit(0);
}


uint32_t loadrom ( uint32_t addr32, const char *filename, uint8_t failure_fatal )
{
	int readsize = hostfs_load_binary(filename, RAM + addr32, 1, 0x10000, "ROM");
	if (readsize <= 0) {
		if (failure_fatal)
			fprintf(stderr, "FATAL: Unable to load %s\n", filename);
		else
			printf("WARNING: Unable to load %s\n", filename);
		return 0;
	} else {
		printf("ROM %s loaded at 0x%05X (%d KB)\n", filename, addr32, readsize >> 10);
		return readsize;
	}
}


void parsecl ( int argc, char *argv[] )
{
	// TODO !! this should be refactored for something more clever
	if (argc < 2) {
		printf ("Invoke Fake86 with the parameter -h for help and usage information.\n");
#ifndef _WIN32
		exit (0);
#endif
	}
	bootdrive = 254;
	textbase = 0xB8000;
	ethif = 254;
	usefullscreen = 0;
	biosfile = PATH_DATAFILES "pcxtbios.bin";
	for (int i = 1; i < argc; i++) {
		if (!strcmpi(argv[i], "-h") || !strcmpi(argv[i], "-?") || !strcmpi(argv[i], "-help")) {
			showhelp();
		} else if (!strcmpi(argv[i], "-fd0")) {
			i++;
			if (insertdisk(0, argv[i]))
				printf("ERROR: Unable to open image file %s\n", argv[i]);
		} else if (!strcmpi(argv[i], "-fd1")) {
			i++;
			if (insertdisk(1, argv[i]))
				printf("ERROR: Unable to open image file %s\n", argv[i]);
		} else if (!strcmpi(argv[i], "-hd0")) {
			i++;
			if (insertdisk(0x80, argv[i]))
				printf("ERROR: Unable to open image file %s\n", argv[i]);
		} else if (!strcmpi(argv[i], "-hd1")) {
			i++;
			if (insertdisk(0x81, argv[i]))
				printf("ERROR: Unable to open image file %s\n", argv[i]);
		}
		else if (!strcmpi(argv[i], "-net")) {
			i++;
			if (!strcmpi(argv[i], "list"))
				ethif = 255;
			else
				ethif = atoi(argv[i]);
		} else if (!strcmpi(argv[i], "-boot")) {
			i++;
			if (!strcmpi(argv[i], "rom"))
				bootdrive = 255;
			else
				bootdrive = atoi(argv[i]);
		} else if (!strcmpi(argv[i], "-ssource")) {
			i++;
			usessource = 1;
		} else if (!strcmpi(argv[i], "-latency")) {
			i++;
			latency = atol(argv[i]);
		} else if (!strcmpi(argv[i], "-samprate")) {
			i++;
			usesamplerate = atol(argv[i]);
		} else if (!strcmpi(argv[i], "-bios")) {
			i++;
			biosfile = argv[i];
		} else if (!strcmpi(argv[i], "-resw")) {
			i++;
			constantw = (uint16_t)atoi(argv[i]);
		} else if (!strcmpi(argv[i], "-resh")) {
			i++;
			constanth = (uint16_t)atoi(argv[i]);
		} else if (!strcmpi(argv[i], "-speed")) {
			i++;
			speed = (uint32_t)atol(argv[i]);
		} else if (!strcmpi(argv[i], "-noscale"))	noscale = 1;
		else if (!strcmpi(argv[i], "-verbose"))		verbose = 1;
		else if (!strcmpi(argv[i], "-smooth"))		nosmooth = 0;
		else if (!strcmpi(argv[i], "-fps"))		renderbenchmark = 1;
		else if (!strcmpi(argv[i], "-nosound"))		doaudio = 0;
		else if (!strcmpi(argv[i], "-fullscreen"))	usefullscreen = 1;
		else if (!strcmpi(argv[i], "-delay"))		framedelay = atol(argv[++i]);
		else if (!strcmpi(argv[i], "-console"))		useconsole = 1;
		else if (!strcmpi(argv[i], "-slowsys"))		slowsystem = 1;
		else if (!strcmpi(argv[i], "-oprom")) {
			i++;
			uint32_t tempuint = hextouint(argv[i++]);
			loadrom(tempuint, argv[i], 0);
		} else {
			printf("Unrecognized parameter: %s\n", argv[i]);
			exit (1);
		}
	}
	if (bootdrive == 254) {
		if (disk[0x80].inserted)
			bootdrive = 0x80;
		else if (disk[0x00].inserted)
			bootdrive = 0;
		else
			bootdrive = 0xFF; //ROM BASIC fallback
	}
}
