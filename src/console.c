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
#include "input.h"


#ifdef USE_OSD
#include "osd.h"
#endif


static volatile char inp_key = 0;

// Warning, this is called from input.c and another thread than the console thread!!!
void input_text_event_cb ( const char *s )
{
	if ((unsigned)*s > 0 && (unsigned)*s < 127 && s[1] == '\0' && inp_key == 0) {
		inp_key = s[0];
		//printf("Event: [%d]\n", s[0]);
	}
}



#ifdef USE_OSD
static int osd_console = 0;
static void console_write ( const char *s )
{
	if (osd_console)
		osd_putstr(s);
	else
		printf("%s", s);
}
static void console_writeln ( const char *s )
{
	if (osd_console) {
		osd_putstr(s);
		osd_putstr("\n");
	} else
		puts(s);
}
#define console_printf(...) do {					\
	char _b_u_f_f_e_r_[1040];					\
	snprintf(_b_u_f_f_e_r_, sizeof _b_u_f_f_e_r_, __VA_ARGS__);	\
	if (osd_console)						\
		osd_putstr(_b_u_f_f_e_r_);				\
	else								\
		printf("%s", _b_u_f_f_e_r_);				\
} while(0)
#else

#define console_write(n)	printf("%s",n)
#define console_writeln(n)	puts(n)
#define console_printf 		printf
#endif




static void waitforcmd ( const char *prompt, char *dst, uint16_t maxlen, const char *ans_on_fail )
{
#ifdef USE_OSD
	if (osd_console) {
		dst[0] = 0;
		int n = 0;
		inp_key = 0;
		osd.cursor = 1;
		console_write(prompt);
		hijacked_input = 1;
		while (running) {
			char key = inp_key;
			inp_key = 0;
			if (key) {
				if (key == 13)
					break;
				else if (key == 8 && n > 0) {
					dst[--n] = 0;
					console_write("\010");
				} else if (key >= 0x20 && n < maxlen - 1) {
					console_printf("%c", key);
					dst[n] = key;
					dst[++n] = 0;
				}
			} else
				SDL_Delay(10);
		}
		osd.cursor = 0;
		console_write("\n");
		return;
	}
#endif
	console_write(prompt);
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
			console_writeln(ans_on_fail);
			strcpy(dst, ans_on_fail);
		} else {
			console_writeln("");
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


static void hexdump ( uint16_t seg, uint16_t ofs, unsigned int *put_seg, unsigned int *put_ofs )
{
	int skip = ofs & 15;
	ofs &= 0xFFF0;
	uint8_t *p = RAM + (seg <<4) + ofs;
	for (int a = 0; a < 0x100; a++) {
		char ascii[17];
		if ((a & 15) == 0)
			console_printf("%04X:%04X ", seg, ofs);
		if (skip) {
			ascii[a & 15] = ' ';
			console_write("   ");
			skip--;
		} else {
			ascii[a & 15] = (p[a] >= 0x20 && p[a] < 127) ? p[a] : '.';
			console_printf(" %02X", *p);
		}
		p++;
		ofs++;
		if (!ofs)
			seg += 0x1000;
		if ((a & 15) == 15) {
			ascii[16] = '\0';
			console_printf("  %s\n", ascii);
		}
	}
	if (put_seg)
		*put_seg = seg;
	if (put_ofs)
		*put_ofs = ofs;
}


static inline void consolehelp ( void ) {
	console_writeln(
		"Console command summary:\n"
		"  The console is not very robust yet. There are only a few commands:\n\n"
		"    chdisk drv fn     Attach/remove drive 'drv' (fd0,fd1,hd0,hd1) to image file 'fn' (or - to remove)\n"
		"    reset             Reset machine\n"
		"    dump seg ofs      Show memory dump at seg ofs (ofs is optional). All numbers are in hex\n"
		"    help              This help display.\n"
		"    quit              Immediately abort emulation and quit Fake86."
	);
}


static const char parameter_separator_chars[] = "\t\n\r ";
static const char console_prompt[] = "FAKE86> ";
#define NEXT_TOKEN()	strtok(NULL, parameter_separator_chars)


int ConsoleThread ( void *ptr )
{
	char inputline[1024];
	int last_cmd_dump = 0;
#ifdef USE_OSD
	if (osd_console)
		osd_setcolors(0xFFFFFFFFU, 0x0000FFFFU, 0xFF0000FFU);
#endif
	console_writeln("\nFake86 management console\nType \"help\" for a summary of commands.");
	while (running) {
		static const char dump_cmd_name[] = "dump";
		waitforcmd(console_prompt, inputline, sizeof(inputline), "close");
		const char *cmd = strtok(inputline, parameter_separator_chars);
		if (!cmd) {
			if (last_cmd_dump)
				cmd = dump_cmd_name;
			else
				continue;
		}
		last_cmd_dump = 0;
		if (!strcmpi(cmd, "chdisk")) {
			const char *drivestr = NEXT_TOKEN();
			const char *fn = NEXT_TOKEN();
			if (!drivestr || !fn || NEXT_TOKEN()) {
				console_writeln("Bad usage, two parameters needed, drive (fd0, fd1, hd0, hd1) and file name (or - to remove)");
				continue;
			}
			int drive;
			if      (!strcmpi(drivestr, "fd0")) drive = 0x00;
			else if (!strcmpi(drivestr, "fd1")) drive = 0x01;
			else if (!strcmpi(drivestr, "hd0")) drive = 0x80;
			else if (!strcmpi(drivestr, "hd1")) drive = 0x81;
			else {
				console_printf("Invalid drive: %s\n", drivestr);
				continue;
			}
			if (strcmpi(fn, "-")) {
				if (disk[drive].inserted)
					console_printf("Disk %s (%02Xh) is not inserted yet\n", drivestr, drive);
				else
					ejectdisk(drive);
			} else {
				insertdisk(drive, fn);
			}
		} else if (!strcmpi(cmd, "regs")) {
			console_printf(
				"CS:IP=%04X:%04X SS:SP=%04X:%04X DS=%04X ES=%04X\n"
				"AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X\n",
				CPU_CS, CPU_IP, CPU_SS, CPU_SP, CPU_DS, CPU_ES,
				CPU_AX, CPU_BX, CPU_CX, CPU_DX, CPU_SI, CPU_DI, CPU_BP
			);
		} else if (!strcmpi(cmd, "help")) {
			consolehelp();
		} else if (!strcmpi(cmd, "quit")) {
			running = 0;
		} else if (!strcmpi(cmd, "close")) {
			console_writeln("Closing management console on request (or cannot read from console).");
			break;
		} else if (!strcmpi(cmd, dump_cmd_name)) {
			static unsigned int seg = 0, ofs = 0;
			const char *seg_str = NEXT_TOKEN();
			const char *ofs_str = NEXT_TOKEN();
			if (seg_str)
				sscanf(seg_str, "%x", &seg);
			if (ofs_str)
				sscanf(ofs_str, "%x", &ofs);
			hexdump(seg, ofs, &seg, &ofs);
			last_cmd_dump = 1;
		} else {
			console_printf("Invalid command was entered: [%s]\n", cmd);
		}
	}
	console_writeln("Terminating console thread.");
	hijacked_input = 0;
#ifdef USE_OSD
	if (osd_console)
		osd.active = 0;
#endif
	return 0;
}
