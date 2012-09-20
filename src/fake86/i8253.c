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

/* i8253.c: functions to emulate the Intel 8253 programmable interval timer.
   these are required for the timer interrupt and PC speaker to be
   properly emulated! */

#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include "i8253.h"
#include "mutex.h"

extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

struct i8253_s i8253;

extern uint64_t hostfreq, lasttick, curtick, tickgap, totalexec;

void out8253 (uint16_t portnum, uint8_t value) {
	uint8_t curbyte;
	portnum &= 3;
	switch (portnum) {
			case 0:
			case 1:
			case 2: //channel data
				if ( (i8253.accessmode[portnum] == PIT_MODE_LOBYTE) || ( (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0) ) ) curbyte = 0;
				else if ( (i8253.accessmode[portnum] == PIT_MODE_HIBYTE) || ( (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1) ) ) curbyte = 1;
				if (curbyte == 0) { //low byte
						i8253.chandata[portnum] = (i8253.chandata[portnum] & 0xFF00) | value;
					}
				else {   //high byte
						i8253.chandata[portnum] = (i8253.chandata[portnum] & 0x00FF) | ( (uint16_t) value << 8);
					}
				if (i8253.chandata[portnum] == 0) i8253.effectivedata[portnum] = 65536;
				else i8253.effectivedata[portnum] = i8253.chandata[portnum];
				i8253.active[portnum] = 1;
				tickgap = (uint64_t) ( (float) hostfreq / (float) ( (float) 1193182 / (float) i8253.effectivedata[0]) );
				if (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) i8253.bytetoggle[portnum] = (~i8253.bytetoggle[portnum]) & 1;
				i8253.chanfreq[portnum] = (float) ( (uint32_t) ( ( (float) 1193182.0 / (float) i8253.effectivedata[portnum]) * (float) 1000.0) ) / (float) 1000.0;
				//printf("[DEBUG] PIT channel %u counter changed to %u (%f Hz)\n", portnum, i8253.chandata[portnum], i8253.chanfreq[portnum]);
				break;
			case 3: //mode/command
				i8253.accessmode[value>>6] = (value >> 4) & 3;
				if (i8253.accessmode[value>>6] == PIT_MODE_TOGGLE) i8253.bytetoggle[value>>6] = 0;
				break;
		}
}

uint8_t in8253 (uint16_t portnum) {
	uint8_t curbyte;
	portnum &= 3;
	switch (portnum) {
			case 0:
			case 1:
			case 2: //channel data
				if ( (i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_LOBYTE) || ( (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0) ) ) curbyte = 0;
				else if ( (i8253.accessmode[portnum] == PIT_MODE_HIBYTE) || ( (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1) ) ) curbyte = 1;
				if ( (i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_LOBYTE) || ( (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0) ) ) curbyte = 0;
				else if ( (i8253.accessmode[portnum] == PIT_MODE_HIBYTE) || ( (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1) ) ) curbyte = 1;
				if ( (i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) ) i8253.bytetoggle[portnum] = (~i8253.bytetoggle[portnum]) & 1;
				if (curbyte == 0) { //low byte
						return ( (uint8_t) i8253.counter[portnum]);
					}
				else {   //high byte
						return ( (uint8_t) (i8253.counter[portnum] >> 8) );
					}
				break;
		}
	return (0);
}

void init8253() {
	memset (&i8253, 0, sizeof (i8253) );
	set_port_write_redirector (0x40, 0x43, &out8253);
	set_port_read_redirector (0x40, 0x43, &in8253);
}
