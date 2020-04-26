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

static char inputline[1024];

static void waitforcmd (char *dst, uint16_t maxlen) {
#ifdef _WIN32
	uint16_t inputptr;
	uint8_t cc;
	inputptr = 0;
	maxlen -= 2;
	inputline[0] = 0;
	while (running) {
		if (_kbhit() ) {
			cc = (uint8_t)_getch();
			switch (cc) {
				case 0:
				case 9:
				case 10:
					break;
				case 8: //backspace
					if (inputptr > 0) {
						printf("%c %c", 8, 8);
						inputline[--inputptr] = 0;
					}
					break;
				case 13: //enter
					printf("\n");
					return;
				default:
					if (inputptr < maxlen) {
						inputline[inputptr++] = cc;
						inputline[inputptr] = 0;
						printf("%c",cc);
					}
					break;
			}
		}
		SDL_Delay(10); //don't waste CPU time while in the polling loop
	}
#else
	//gets (dst);
	if (!fgets(dst, maxlen, stdin))
		dst[0] = '\0';
#endif
}

static void consolehelp (void) {
	printf("\nConsole command summary:\n");
	printf("  The console is not very robust yet. There are only a few commands:\n\n");
	printf("    change fd0        Mount a new image file on first floppy drive.\n");
	printf("                      Entering a blank line just ejects any current image file.\n");
	printf("    change fd1        Mount a new image file on first floppy drive.\n");
	printf("                      Entering a blank line just ejects any current image file.\n");
	printf("    help              This help display.\n");
	printf("    quit              Immediately abort emulation and close Fake86.\n");
}

#ifdef _WIN32
void runconsole (void *dummy) {
#else
void *runconsole (void *dummy) {
#endif
	printf ("\nFake86 management console\n");
	printf ("Type \"help\" for a summary of commands.\n");
	while (running) {
		printf("\n>");
		waitforcmd(inputline, sizeof(inputline));
		if (strcmpi(inputline, "change fd0") == 0) {
			printf("Path to new image file: ");
			waitforcmd(inputline, sizeof(inputline));
			if (strlen(inputline) > 0) {
				insertdisk(0, inputline);
			} else {
				ejectdisk(0);
				printf("Floppy image ejected from first drive.\n");
			}
		} else if (strcmpi(inputline, "change fd1") == 0) {
			printf("Path to new image file: ");
			waitforcmd(inputline, sizeof(inputline));
			if (strlen(inputline) > 0) {
				insertdisk(1,inputline);
			} else {
				ejectdisk(1);
				printf("Floppy image ejected from second drive.\n");
			}
		} else if (strcmpi(inputline, "help") == 0) {
			consolehelp();
		} else if (strcmpi(inputline, "quit") == 0) {
			running = 0;
		} else {
			printf("Invalid command was entered.\n");
		}
	}
#ifndef _WIN32
	return NULL;
#endif
}