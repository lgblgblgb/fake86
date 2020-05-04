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

/* console.c: functions for a simple interactive console on stdio. */

#include "config.h"
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#define strcmpi _strcmpi
#else
#define strcmpi strcasecmp
#endif

#include "console.h"

#include "disk.h"
#include "cpu.h"


static void waitforcmd ( char *dst, uint16_t maxlen, const char *ans_on_fail )
{
#ifdef _WIN32
	uint16_t inputptr;
	inputptr = 0;
	maxlen -= 2;
	dst[0] = 0;
	while (running) {
		if (_kbhit() ) {
			uint8_t cc = (uint8_t)_getch();
			switch (cc) {
				case 0:
				case 9:
				case 10:
					break;
				case 8: //backspace
					if (inputptr > 0) {
						printf("%c %c", 8, 8);
						dst[--inputptr] = 0;
					}
					break;
				case 13: //enter
					printf("\n");
					return;
				default:
					if (inputptr < maxlen) {
						dst[inputptr++] = cc;
						dst[inputptr] = 0;
						printf("%c",cc);
					}
					break;
			}
		}
		SDL_Delay(10); //don't waste CPU time while in the polling loop
	}
#else
	//gets (dst);
	if (!fgets(dst, maxlen, stdin)) {
		if (ans_on_fail) {
			puts(ans_on_fail);
			strcpy(dst, ans_on_fail);
		} else {
			puts("");
			dst[0] = 0;
		}
	} else {
		for (char *p = dst ;; p++)
			if (*p < 0x20) {
				*p = 0;
				break;
			}
	}
#endif
}


static inline void consolehelp ( void ) {
	puts(
		"Console command summary:\n"
		"  The console is not very robust yet. There are only a few commands:\n\n"
		"    change fd0        Mount a new image file on first floppy drive.\n"
		"                      Entering a blank line just ejects any current image file.\n"
		"    change fd1        Mount a new image file on first floppy drive.\n"
		"                      Entering a blank line just ejects any current image file.\n"
		"    help              This help display.\n"
		"    quit              Immediately abort emulation and quit Fake86."
	);
}


int ConsoleThread( void *ptr )
{
	char inputline[1024];
	puts("\nFake86 management console\nType \"help\" for a summary of commands.");
	while (running) {
		printf(">");
		waitforcmd(inputline, sizeof(inputline), "close");
		if (!strcmpi(inputline, "change fd0")) {
			printf("Path to new image file: ");
			waitforcmd(inputline, sizeof(inputline), NULL);
			if (strlen(inputline) > 0) {
				insertdisk(0, inputline);
			} else {
				ejectdisk(0);
				puts("Floppy image ejected from first drive.");
			}
		} else if (!strcmpi(inputline, "change fd1")) {
			printf("Path to new image file: ");
			waitforcmd(inputline, sizeof(inputline), NULL);
			if (strlen(inputline) > 0) {
				insertdisk(1, inputline);
			} else {
				ejectdisk(1);
				puts("Floppy image ejected from second drive.");
			}
		} else if (!strcmpi(inputline, "help")) {
			consolehelp();
		} else if (!strcmpi(inputline, "quit")) {
			running = 0;
		} else if (!strcmpi(inputline, "close")) {
			puts("Closing management console on request (or cannot read from console).");
			break;
		} else {
			printf("Invalid command was entered: [%s]\n", inputline);
		}
	}
	puts("Terminating console thread.");
	return 0;
}
