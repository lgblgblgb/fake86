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

/* ssource.c: functions to emulate the Disney Sound Source's 16-byte FIFO buffer. */

#include "config.h"
#include <stdint.h>

extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

extern uint8_t portram[0x10000];
uint8_t ssourcebuf[16], ssourceptr = 0, ssourceactive = 0;
int16_t ssourcecursample = 0;

int16_t getssourcebyte() {
	return (ssourcecursample);
}

void tickssource() {
	uint8_t rotatefifo;
	if ( (ssourceptr==0) || (!ssourceactive) ) {
			ssourcecursample = 0;
			return;
		}
	ssourcecursample = ssourcebuf[0];
	for (rotatefifo=1; rotatefifo<16; rotatefifo++)
		ssourcebuf[rotatefifo-1] = ssourcebuf[rotatefifo];
	ssourceptr--;
	portram[0x379] = 0;
}

void putssourcebyte (uint8_t value) {
	if (ssourceptr==16) return;
	ssourcebuf[ssourceptr++] = value;
	if (ssourceptr==16) portram[0x379] = 0x40;
}

uint8_t ssourcefull() {
	if (ssourceptr==16) return (0x40);
	else return (0x00);
}

void outsoundsource (uint16_t portnum, uint8_t value) {
	static uint8_t last37a = 0;
	switch (portnum) {
			case 0x378:
				putssourcebyte (value);
				break;
			case 0x37A:
				if ( (value & 4) && ! (last37a & 4) ) putssourcebyte (portram[0x378]);
				last37a = value;
				break;
		}
}

uint8_t insoundsource (uint16_t portnum) {
	return (ssourcefull() );
}

void initsoundsource() {
	set_port_write_redirector (0x378, 0x378, &outsoundsource);
	set_port_write_redirector (0x37A, 0x37A, &outsoundsource);
	set_port_read_redirector (0x379, 0x379, &insoundsource);
	ssourceactive = 1;
}
