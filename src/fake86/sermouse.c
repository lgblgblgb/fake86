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

/* sermouse.c: functions to emulate a standard Microsoft-compatible serial mouse. */

#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sermouse.h"

extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void doirq (uint8_t irqnum);

struct sermouse_s sermouse;

void bufsermousedata (uint8_t value) {
	if (sermouse.bufptr == 16) return;
	if (sermouse.bufptr == 0 ) doirq (4);
	sermouse.buf[sermouse.bufptr++] = value;
}

void outsermouse (uint16_t portnum, uint8_t value) {
	uint8_t oldreg;
	//printf("[DEBUG] Serial mouse, port %X out: %02X\n", portnum, value);
	portnum &= 7;
	oldreg = sermouse.reg[portnum];
	sermouse.reg[portnum] = value;
	switch (portnum) {
			case 4: //modem control register
				if ( (value & 1) != (oldreg & 1) ) { //software toggling of this register
						sermouse.bufptr = 0; //causes the mouse to reset and fill the buffer
						bufsermousedata ('M'); //with a bunch of ASCII 'M' characters.
						bufsermousedata ('M'); //this is intended to be a way for
						bufsermousedata ('M'); //drivers to verify that there is
						bufsermousedata ('M'); //actually a mouse connected to the port.
						bufsermousedata ('M');
						bufsermousedata ('M');
					}
				break;
		}
}

uint8_t insermouse (uint16_t portnum) {
	uint8_t temp;
	//printf("[DEBUG] Serial mouse, port %X in\n", portnum);
	portnum &= 7;
	switch (portnum) {
			case 0: //data receive
				temp = sermouse.buf[0];
				memmove (sermouse.buf, &sermouse.buf[1], 15);
				sermouse.bufptr--;
				if (sermouse.bufptr < 0) sermouse.bufptr = 0;
				if (sermouse.bufptr > 0) doirq (4);
				sermouse.reg[4] = ~sermouse.reg[4] & 1;
				return (temp);
			case 5: //line status register (read-only)
				if (sermouse.bufptr > 0) temp = 1;
				else temp = 0;
				return (0x1);
				return (0x60 | temp);
		}
	return (sermouse.reg[portnum & 7]);
}

void initsermouse (uint16_t baseport, uint8_t irq) {
	sermouse.bufptr = 0;
	set_port_write_redirector (baseport, baseport + 7, &outsermouse);
	set_port_read_redirector (baseport, baseport + 7, &insermouse);
}

void sermouseevent (uint8_t buttons, int8_t xrel, int8_t yrel) {
	uint8_t highbits = 0;
	if (xrel < 0) highbits = 3;
	else highbits = 0;
	if (yrel < 0) highbits |= 12;
	bufsermousedata (0x40 | (buttons << 4) | highbits);
	bufsermousedata (xrel & 63);
	bufsermousedata (yrel & 63);
}
