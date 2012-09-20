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

/* blaster.c: functions to emulate a Creative Labs Sound Blaster Pro. */

#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "blaster.h"

extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void doirq (uint8_t irqnum);
extern uint8_t read8237 (uint8_t channel);

extern void outadlib (uint16_t portnum, uint8_t value); //on the Sound Blaster Pro, ports (base+0) and (base+1) are for
extern uint8_t inadlib (uint16_t portnum); //the OPL FM music chips, and are also mirrored at (base+8) (base+9)
//as well as 0x388 and 0x389 to remain compatible with the older adlib cards

struct blaster_s blaster;

void bufNewData (uint8_t value) {
	if (blaster.memptr >= sizeof (blaster.mem) ) return;
	blaster.mem[blaster.memptr] = value;
	blaster.memptr++;
}

extern uint64_t hostfreq;
void setsampleticks() {
	if (blaster.samplerate == 0) {
			blaster.sampleticks = 0;
			return;
		}
	blaster.sampleticks = hostfreq / (uint64_t) blaster.samplerate;
}

void cmdBlaster (uint8_t value) {
	if (blaster.waitforarg) {
			switch (blaster.lastcmdval) {
					case 0x10: //direct 8-bit sample output
						blaster.sample = value;
						break;
					case 0x14: //8-bit single block DMA output
					case 0x24:
						if (blaster.waitforarg == 2) {
								blaster.blocksize = (blaster.blocksize & 0xFF00) | (uint32_t) value;
								blaster.waitforarg = 3;
								return;
							}
						else {
								blaster.blocksize = (blaster.blocksize & 0x00FF) | ( (uint32_t) value << 8);
#ifdef DEBUG_BLASTER
								printf ("[NOTICE] Sound Blaster DSP block transfer size set to %u\n", blaster.blocksize);
#endif
								blaster.usingdma = 1;
								blaster.blockstep = 0;
								blaster.useautoinit = 0;
								blaster.paused8 = 0;
								blaster.speakerstate = 1;
							}
						break;
					case 0x40: //set time constant
						blaster.samplerate = (uint16_t) ( (uint32_t) 1000000 / (uint32_t) (256 - (uint32_t) value) );
						setsampleticks();
#ifdef DEBUG_BLASTER
						printf ("[DEBUG] Sound Blaster time constant received, sample rate = %u\n", blaster.samplerate);
#endif
						break;
					case 0x48: //set DSP block transfer size
						if (blaster.waitforarg == 2) {
								blaster.blocksize = (blaster.blocksize & 0xFF00) | (uint32_t) value;
								blaster.waitforarg = 3;
								return;
							}
						else {
								blaster.blocksize = (blaster.blocksize & 0x00FF) | ( (uint32_t) value << 8);
								//if (blaster.blocksize == 0) blaster.blocksize = 65536;
								blaster.blockstep = 0;
#ifdef DEBUG_BLASTER
								printf ("[NOTICE] Sound Blaster DSP block transfer size set to %u\n", blaster.blocksize);
#endif
							}
						break;
					case 0xE0: //DSP identification for Sound Blaster 2.0 and newer (invert each bit and put in read buffer)
						bufNewData (~value);
						break;
					case 0xE4: //DSP write test, put data value into read buffer
						bufNewData (value);
						blaster.lasttestval = value;
						break;
				}
			blaster.waitforarg = 0;
			return;
		}

	switch (value) {
			case 0x10:
			case 0x40:
			case 0xE0:
			case 0xE4:
				blaster.waitforarg = 1;
				break;

			case 0x14: //8-bit single block DMA output
			case 0x24:
			case 0x48:
				blaster.waitforarg = 2;
				break;

			case 0x1C: //8-bit auto-init DMA output
			case 0x2C:
				blaster.usingdma = 1;
				blaster.blockstep = 0;
				blaster.useautoinit = 1;
				blaster.paused8 = 0;
				blaster.speakerstate = 1;
				break;

			case 0xD0: //pause 8-bit DMA I/O
				blaster.paused8 = 1;
			case 0xD1: //speaker output on
				blaster.speakerstate = 1;
				break;
			case 0xD3: //speaker output off
				blaster.speakerstate = 0;
				break;
			case 0xD4: //continue 8-bit DMA I/O
				blaster.paused8 = 0;
				break;
			case 0xD8: //get speaker status
				if (blaster.speakerstate) bufNewData (0xFF);
				else bufNewData (0x00);
				break;
			case 0xDA: //exit 8-bit auto-init DMA I/O mode
				blaster.usingdma = 0;
				break;
			case 0xE1: //get DSP version info
				blaster.memptr = 0;
				bufNewData (blaster.dspmaj);
				bufNewData (blaster.dspmin);
				break;
			case 0xE8: //DSP read test
				blaster.memptr = 0;
				bufNewData (blaster.lasttestval);
				break;
			case 0xF2: //force 8-bit IRQ
				doirq (blaster.sbirq);
				break;
			case 0xF8: //undocumented command, clears in-buffer and inserts a null byte
				blaster.memptr = 0;
				bufNewData (0);
				break;
			default:
				printf ("[NOTICE] Sound Blaster received unhandled command %02Xh\n", value);
				break;
		}
}

uint8_t mixer[256], mixerindex = 0;
void outBlaster (uint16_t portnum, uint8_t value) {
#ifdef DEBUG_BLASTER
	printf ("[DEBUG] outBlaster: port %Xh, value %02X\n", portnum, value);
#endif
	portnum &= 0xF;
	switch (portnum) {
			case 0x0:
			case 0x8:
				outadlib (0x388, value);
				break;
			case 0x1:
			case 0x9:
				outadlib (0x389, value);
				break;
			case 0x4: //mixer address port
				mixerindex = value;
				break;
			case 0x5: //mixer data
				mixer[mixerindex] = value;
				break;
			case 0x6: //reset port
				if ( (value == 0x00) && (blaster.lastresetval == 0x01) ) {
						blaster.speakerstate = 0;
						blaster.sample = 128;
						blaster.waitforarg = 0;
						blaster.memptr = 0;
						blaster.usingdma = 0;
						blaster.blocksize = 65535;
						blaster.blockstep = 0;
						bufNewData (0xAA);
						memset (mixer, 0xEE, sizeof (mixer) );
#ifdef DEBUG_BLASTER
						printf ("[DEBUG] Sound Blaster received reset!\n");
#endif
					}
				blaster.lastresetval = value;
				break;
			case 0xC: //write command/data
				cmdBlaster (value);
				if (blaster.waitforarg != 3) blaster.lastcmdval = value;
				break;
		}
}

uint8_t inBlaster (uint16_t portnum) {
	uint8_t ret = 0;
#ifdef DEBUG_BLASTER
	static uint16_t lastread = 0;
#endif
	portnum &= 0xF;
#ifdef DEBUG_BLASTER
	if (lastread != portnum) printf ("[DEBUG] inBlaster: port %Xh, value ", 0x220 + portnum);
#endif
	switch (portnum) {
			case 0x0:
			case 0x8:
				ret = inadlib (0x388);
				break;
			case 0x1:
			case 0x9:
				ret = inadlib (0x389);
				break;
			case 0x5: //mixer data
				ret = mixer[mixerindex];
				break;
			case 0xA: //read data
				if (blaster.memptr == 0) {
						ret = 0;
					}
				else {
						ret = blaster.mem[0];
						memmove (&blaster.mem[0], &blaster.mem[1], sizeof (blaster.mem) - 1);
						blaster.memptr--;
					}
				break;
			case 0xE: //read-buffer status
				if (blaster.memptr > 0) ret = 0x80;
				else ret = 0x00;
				break;
			default:
				ret = 0x00;
		}
#ifdef DEBUG_BLASTER
	if (lastread != portnum) printf ("%02X\n", ret);
	lastread = portnum;
#endif
	return (ret);
}

//FILE *sbout = NULL;
void tickBlaster() {
	if (!blaster.usingdma) return;
	/*if (blaster.paused8) {
	blaster.sample = 128;
	return;
	}*/
	//printf("tickBlaster();\n");
	blaster.sample = read8237 (blaster.sbdma);
	//if (sbout != NULL) fwrite(&blaster.sample, 1, 1, sbout);
	blaster.blockstep++;
	if (blaster.blockstep > blaster.blocksize) {
			doirq (blaster.sbirq);
#ifdef DEBUG_BLASTER
			printf ("[NOTICE] Sound Blaster did IRQ\n");
#endif
			if (blaster.useautoinit) {
					blaster.blockstep = 0;
				}
			else {
					blaster.usingdma = 0;
				}
		}
}

int16_t getBlasterSample() {
	if (blaster.speakerstate == 0) return (0);
	else return ( (int16_t) blaster.sample - 128);
}

void mixerReset() {
	memset (blaster.mixer.reg, 0, sizeof (blaster.mixer.reg) );
	blaster.mixer.reg[0x22] = blaster.mixer.reg[0x26] = blaster.mixer.reg[0x04] = (4 << 5) | (4 << 1);
}

void initBlaster (uint16_t baseport, uint8_t irq) {
	//sbout = fopen("sbout.raw", "wb");
	memset (&blaster, 0, sizeof (blaster) );
	blaster.dspmaj = 3; //emulate a Sound Blaster Pro
	blaster.dspmin = 0;
	blaster.sbirq = irq;
	blaster.sbdma = 1;
	mixerReset();
	set_port_write_redirector (baseport, baseport + 0xE, &outBlaster);
	set_port_read_redirector (baseport, baseport + 0xE, &inBlaster);
}
