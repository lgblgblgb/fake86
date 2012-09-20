/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers

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
#include <SDL/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "disk.h"

extern struct struct_drive disk[256];
#ifndef _WIN32
#define strcmpi strcasecmp
#else
#define strcmpi _strcmpi
#endif

extern uint8_t bootdrive, ethif, verbose, cgaonly, *biosfile, usessource, noscale, nosmooth, renderbenchmark, useconsole, doaudio;
extern uint32_t framedelay, textbase, usefullscreen, speed;
extern int32_t usesamplerate, latency;
uint16_t constantw = 0, constanth = 0;
uint8_t slowsystem = 0;

extern uint8_t insertdisk (uint8_t drivenum, char *filename);

void showhelp () {
	printf ("Fake86 requires some command line parameters to run.\nValid options:\n");

	printf ("  -fd0 filename    Specify a floppy disk image file to use as floppy 0.\n");
	printf ("  -fd1 filename    Specify a floppy disk image file to use as floppy 1.\n");
	printf ("  -hd0 filename    Specify a hard disk image file to use as hard drive 0.\n");
	printf ("  -hd1 filename    Specify a hard disk image file to use as hard drive 1.\n");
	printf ("  -boot #          Specify which BIOS drive ID should be the boot device in #.\n");
	printf ("                   Examples: -boot 0 will boot from floppy 0.\n");
	printf ("                             -boot 1 will boot from floppy 1.\n");
	printf ("                             -boot 128 will boot from hard drive 0.\n");
	printf ("                             -boot rom will boot to ROM BASIC if available.\n");
	printf ("                   Default boot device is hard drive 0, if it exists.\n");
	printf ("                   Otherwise, the default is floppy 0.\n");
	printf ("  -bios filename   Specify alternate BIOS ROM image to use.\n");
#ifdef NETWORKING_ENABLED
#ifdef _WIN32
	printf ("  -net #           Enable ethernet emulation via winpcap, where # is the\n");
#else
	printf ("  -net #           Enable ethernet emulation via libpcap, where # is the\n");
#endif
	printf ("                   numeric ID of your host's network interface to bridge.\n");
	printf ("                   To get a list of possible interfaces, use -net list\n");
#endif
	printf ("  -nosound         Disable audio emulation and output.\n");
	printf ("  -fullscreen      Start Fake86 in fullscreen mode.\n");
	printf ("  -verbose         Verbose mode. Operation details will be written to stdout.\n");
	printf ("  -delay           Specify how many milliseconds the render thread should idle\n");
	printf ("                   between drawing each frame. Default is 20 ms. (~50 FPS)\n");
	printf ("  -slowsys         If your machine is very slow and you have audio dropouts,\n");
	printf ("                   use this option to sacrifice audio quality to compensate.\n");
	printf ("                   If you still have dropouts, then also decrease sample rate\n");
	printf ("                   and/or increase latency.\n");
	printf ("  -resw # -resh #  Force a constant window size in pixels.\n");
	printf ("  -smooth          Apply smoothing to screen rendering.\n");
	printf ("  -noscale         Disable 2x scaling of low resolution video modes.\n");
	printf ("  -ssource         Enable Disney Sound Source emulation on LPT1.\n");
	printf ("  -latency #       Change audio buffering and output latency. (default: 100 ms)\n");
	printf ("  -samprate #      Change audio emulation sample rate. (default: 48000 Hz)\n");
	printf ("  -console         Enable console on stdio during emulation.\n");

	printf ("\nThis program is free software; you can redistribute it and/or\n");
	printf ("modify it under the terms of the GNU General Public License\n");
	printf ("as published by the Free Software Foundation; either version 2\n");
	printf ("of the License, or (at your option) any later version.\n\n");

	printf ("This program is distributed in the hope that it will be useful,\n");
	printf ("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
	printf ("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
	printf ("GNU General Public License for more details.\n");

	exit (0);
}

void parsecl (int argc, char *argv[]) {
	int i, abort = 0;

	if (argc<2) {
            printf ("Invoke Fake86 with the parameter -h for help and usage information.\n");
			exit (0);
		}

	bootdrive = 254;
	textbase = 0xB8000;
	ethif = 254;
	usefullscreen = 0;
	biosfile = PATH_DATAFILES "pcxtbios.bin";
	for (i=1; i<argc; i++) {
            if (strcmpi (argv[i], "-h") ==0) showhelp ();
            else if (strcmpi (argv[i], "-?") ==0) showhelp ();
            else if (strcmpi (argv[i], "-help") ==0) showhelp ();
			else if (strcmpi (argv[i], "-fd0") ==0) {
					i++;
					if (insertdisk (0, argv[i]) ) {
							printf ("ERROR: Unable to open image file %s\n", argv[i]);
						}
				}
			else if (strcmpi (argv[i], "-fd1") ==0) {
					i++;
					if (insertdisk (1, argv[i]) ) {
							printf ("ERROR: Unable to open image file %s\n", argv[i]);
						}
				}
			else if (strcmpi (argv[i], "-hd0") ==0) {
					i++;
					if (insertdisk (0x80, argv[i]) ) {
							printf ("ERROR: Unable to open image file %s\n", argv[i]);
						}
				}
			else if (strcmpi (argv[i], "-hd1") ==0) {
					i++;
					if (insertdisk (0x81, argv[i]) ) {
							printf ("ERROR: Unable to open image file %s\n", argv[i]);
						}
				}
			else if (strcmpi (argv[i], "-net") ==0) {
					i++;
					if (strcmpi (argv[i], "list") ==0) ethif = 255;
					else ethif = atoi (argv[i]);
				}
			else if (strcmpi (argv[i], "-boot") ==0) {
					i++;
					if (strcmpi (argv[i], "rom") ==0) bootdrive = 255;
					else bootdrive = atoi (argv[i]);
				}
			else if (strcmpi (argv[i], "-ssource") ==0) {
					i++;
					usessource = 1;
				}
			else if (strcmpi (argv[i], "-latency") ==0) {
					i++;
					latency = atol (argv[i]);
				}
			else if (strcmpi (argv[i], "-samprate") ==0) {
					i++;
					usesamplerate = atol (argv[i]);
				}
			else if (strcmpi (argv[i], "-bios") ==0) {
					i++;
					biosfile = argv[i];
				}
			else if (strcmpi (argv[i], "-resw") ==0) {
					i++;
					constantw = (uint16_t) atoi (argv[i]);
				}
			else if (strcmpi (argv[i], "-resh") ==0) {
					i++;
					constanth = (uint16_t) atoi (argv[i]);
				}
			else if (strcmpi (argv[i], "-noscale") ==0) noscale = 1;
			else if (strcmpi (argv[i], "-verbose") ==0) verbose = 1;
			else if (strcmpi (argv[i], "-smooth") ==0) nosmooth = 0;
			else if (strcmpi (argv[i], "-fps") ==0) renderbenchmark = 1;
			else if (strcmpi (argv[i], "-nosound") ==0) doaudio = 0;
			else if (strcmpi (argv[i], "-fullscreen") ==0) usefullscreen = SDL_FULLSCREEN;
			else if (strcmpi (argv[i], "-delay") ==0) framedelay = atol (argv[++i]);
			else if (strcmpi (argv[i], "-console") ==0) useconsole = 1;
			else if (strcmpi (argv[i], "-slowsys") ==0) slowsystem = 1;
			else {
					printf ("Unrecognized parameter: %s\n", argv[i]);
					exit (1);
				}
		}

	if (bootdrive==254) {
			if (disk[0x80].inserted) bootdrive = 0x80;
			else bootdrive = 0;
		}
}


