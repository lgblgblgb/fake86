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

/* adlib.c: very ugly Adlib OPL2 emulation for Fake86. very much a work in progress. :) */

#include "config.h"
#include <SDL/SDL.h>
#include <stdint.h>
#include <stdio.h>

extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

extern int32_t usesamplerate;

double samprateadjust = 1.0;
uint8_t optable[0x16] = { 0, 0, 0, 1, 1, 1, 255, 255, 0, 0, 0, 1, 1, 1, 255, 255, 0, 0, 0, 1, 1, 1 };
uint16_t adlibregmem[0xFF], adlibaddr = 0;

int8_t waveform[4][64] = {
	{ 1, 8, 13, 20, 26, 31, 37, 41, 47, 49, 54, 58, 58, 62, 63, 63, 64, 63, 62, 61, 58, 55, 52, 47, 45, 38, 34, 27, 23, 17, 10, 4,-2,-8,-15,-21,-26,-34,-36,-42,-48,-51,-54,-59,-60,-62,-64,-65,-65,-63,-64,-61,-59,-56,-53,-48,-46,-39,-36,-28,-24,-17,-11,-6 },
	{ 1, 8, 13, 20, 25, 32, 36, 42, 46, 50, 54, 57, 60, 61, 62, 64, 63, 65, 61, 61, 58, 55, 51, 49, 44, 38, 34, 28, 23, 16, 11, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 8, 13, 21, 25, 31, 36, 43, 45, 50, 54, 57, 59, 62, 63, 63, 63, 64, 63, 59, 59, 55, 52, 48, 44, 38, 34, 28, 23, 16, 10, 4, 2, 7, 14, 20, 26, 31, 36, 42, 45, 51, 54, 56, 60, 62, 62, 63, 65, 63, 62, 60, 58, 55, 52, 48, 44, 38, 34, 28, 23, 17, 10, 3 },
	{ 1, 8, 13, 20, 26, 31, 36, 42, 46, 51, 53, 57, 60, 62, 61, 66, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 13, 21, 25, 32, 36, 41, 47, 50, 54, 56, 60, 62, 61, 67, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

int8_t oplwave[4][256] = {
	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64, 63, 63, 63, 62, 62, 61, 61, 60,
		59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26, 24, 23, 22, 20, 18, 17, 15, 14,
		12, 11, 9, 7, 6, 4, 3, 1, 0, -1, -3, -4, -6, -7, -9, -11, -12, -14, -15, -17, -18, -20, -22, -23, -24, -26, -27, -29, -30, -31, -33, -34, -36, -37, -38, -40, -40, -42, -43, -44,
		-46, -46, -48, -49, -50, -51, -51, -53, -53, -54, -55, -56, -57, -57, -58, -59, -59, -60, -61, -61, -62, -62, -63, -63, -63, -64, -64, -64, -116, -116, -116, -116, -116, -116, -116, -116, -116, -64, -64, -64,
		-63, -63, -63, -62, -62, -61, -61, -60, -59, -59, -58, -57, -57, -56, -55, -54, -53, -53, -51, -51, -50, -49, -48, -46, -46, -44, -43, -42, -40, -40, -38, -37, -36, -34, -33, -31, -30, -29, -27, -26,
		-24, -23, -22, -20, -18, -17, -15, -14, -12, -11, -9, -7, -6, -4, -3, -1
	},

	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29,30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64, 63, 63, 63, 62, 62, 61, 61, 60,
		59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26, 24, 23, 22, 20, 18, 17, 15, 14,
		12, 11, 9, 7, 6, 4, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	},


	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64, 63, 63, 63, 62, 62, 61, 61, 60,
		59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26, 24, 23, 22, 20, 18, 17, 15, 14,
		12, 11, 9, 7, 6, 4, 3, 1, 0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44,
		46, 46, 48, 49, 50, 51, 51, 53, 53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 116, 116, 116, 116, 116, 64, 64, 64,
		63, 63, 63, 62, 62, 61, 61, 60, 59, 59, 58, 57, 57, 56, 55, 54, 53, 53, 51, 51, 50, 49, 48, 46, 46, 44, 43, 42, 40, 40, 38, 37, 36, 34, 33, 31, 30, 29, 27, 26,
		24, 23, 22, 20, 18, 17, 15, 14, 12, 11, 9, 7, 6, 4, 3, 1
	},


	{
		0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44, 46, 46, 48, 49, 50, 51, 51, 53,
		53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 18, 20, 22, 23, 24, 26, 27, 29, 30, 31, 33, 34, 36, 37, 38, 40, 40, 42, 43, 44,
		46, 46, 48, 49, 50, 51, 51, 53, 53, 54, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 63, 64, 64, 64, 116, 116, 116, 116, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	}

};

uint8_t oplstep[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

struct structadlibop {
	uint8_t wave;
} adlibop[9][2];

struct structadlibchan {
	uint16_t freq;
	double convfreq;
	uint8_t keyon;
	uint16_t octave;
	uint8_t wavesel;
} adlibch[9];

double attacktable[16] = { 1.0003, 1.00025, 1.0002, 1.00015, 1.0001, 1.00009, 1.00008, 1.00007, 1.00006, 1.00005, 1.00004, 1.00003, 1.00002, 1.00001, 1.000005 }; //1.003, 1.05, 1.01, 1.015, 1.02, 1.025, 1.03, 1.035, 1.04, 1.045, 1.05, 1.055, 1.06, 1.065, 1.07, 1.075 };
double decaytable[16] = { 0.99999, 0.999985, 0.99998, 0.999975, 0.99997, 0.999965, 0.99996, 0.999955, 0.99995, 0.999945, 0.99994, 0.999935, 0.99994, 0.999925, 0.99992, 0.99991 };
double adlibenv[9], adlibdecay[9], adlibattack[9];
uint8_t adlibdidattack[9], adlibpercussion = 0, adlibstatus = 0;

uint16_t adlibport = 0x388;

void outadlib (uint16_t portnum, uint8_t value) {
	if (portnum==adlibport) {
			adlibaddr = value;
			return;
		}
	portnum = adlibaddr;
	adlibregmem[portnum] = value;
	switch (portnum) {
			case 4: //timer control
				if (value&0x80) {
						adlibstatus = 0;
						adlibregmem[4] = 0;
					}
				break;
			case 0xBD:
				if (value & 0x10) adlibpercussion = 1;
				else adlibpercussion = 0;
				break;
		}
	if ( (portnum >= 0x60) && (portnum <= 0x75) ) { //attack/decay
			portnum &= 15;
			adlibattack[portnum] = attacktable[15- (value>>4) ]*1.006;
			adlibdecay[portnum] = decaytable[value&15];
		}
	else if ( (portnum >= 0xA0) && (portnum <= 0xB8) ) { //octave, freq, key on
			portnum &= 15;
			if (!adlibch[portnum].keyon && ( (adlibregmem[0xB0+portnum]>>5) &1) ) {
					adlibdidattack[portnum] = 0;
					adlibenv[portnum] = 0.0025;
				}
			adlibch[portnum].freq = adlibregmem[0xA0+portnum] | ( (adlibregmem[0xB0+portnum]&3) <<8);
			adlibch[portnum].convfreq = ( (double) adlibch[portnum].freq * 0.7626459);
			adlibch[portnum].keyon = (adlibregmem[0xB0+portnum]>>5) &1;
			adlibch[portnum].octave = (adlibregmem[0xB0+portnum]>>2) &7;
		}
	else if ( (portnum >= 0xE0) && (portnum <= 0xF5) ) { //waveform select
			portnum &= 15;
			if (portnum<9) adlibch[portnum].wavesel = value&3;
		}
}

uint8_t inadlib (uint16_t portnum) {
	if (!adlibregmem[4]) adlibstatus = 0;
	else adlibstatus = 0x80;
	adlibstatus = adlibstatus + (adlibregmem[4]&1) *0x40 + (adlibregmem[4]&2) *0x10;
	return (adlibstatus);
}

uint16_t adlibfreq (uint8_t chan) {
	//uint8_t downoct[4] = { 3, 2, 1, 0 };
	//uint8_t upoct[3] = { 1, 2, 3 };
	uint16_t tmpfreq;
	if (!adlibch[chan].keyon) return (0);
	tmpfreq = (uint16_t) adlibch[chan].convfreq;
	//if (adlibch[chan].octave<4) tmpfreq = tmpfreq>>1;
	//if (adlibch[chan].octave>4) tmpfreq = tmpfreq<<1;
	switch (adlibch[chan].octave) {
			case 0:
				tmpfreq = tmpfreq >> 4;
				break;
			case 1:
				tmpfreq = tmpfreq >> 3;
				break;
			case 2:
				tmpfreq = tmpfreq >> 2;
				break;
			case 3:
				tmpfreq = tmpfreq >> 1;
				break;
				//case 4: tmpfreq = tmpfreq >> 1; break;
			case 5:
				tmpfreq = tmpfreq << 1;
				break;
			case 6:
				tmpfreq = tmpfreq << 2;
				break;
			case 7:
				tmpfreq = tmpfreq << 3;
		}

	return (tmpfreq);
}

uint64_t fullstep, adlibstep[9];
double adlibenv[9], adlibdecay[9], adlibattack[9];
uint8_t adlibdidattack[9];

extern SDL_AudioSpec wanted;
int32_t adlibsample (uint8_t curchan) {
	int32_t tempsample;
	double tempstep;

	if (adlibpercussion && (curchan>=6) && (curchan<=8) ) return (0);

	fullstep = usesamplerate/adlibfreq (curchan);

	tempsample = (int32_t) oplwave[adlibch[curchan].wavesel][ (uint8_t) ( (double) adlibstep[curchan]/ ( (double) fullstep/ (double) 256) ) ];
	tempstep = adlibenv[curchan];
	if (tempstep>1.0) tempstep = 1;
	tempsample = (int32_t) ( (double) tempsample * tempstep * 2.0);

	adlibstep[curchan]++;
	if (adlibstep[curchan]>fullstep) adlibstep[curchan] = 0;
	return (tempsample);
}

int16_t adlibgensample() {
	uint8_t curchan;
	int16_t adlibaccum;
	adlibaccum = 0;
	for (curchan=0; curchan<9; curchan++) {
			if (adlibfreq (curchan) !=0) {
					adlibaccum += (int16_t) adlibsample (curchan);
				}
		}
	return (adlibaccum);
}

void tickadlib() {
	uint8_t curchan;
	for (curchan=0; curchan<9; curchan++) {
			if (adlibfreq (curchan) !=0) {
					if (adlibdidattack[curchan]) {
							adlibenv[curchan] *= adlibdecay[curchan];
						}
					else {
							adlibenv[curchan] *= adlibattack[curchan];
							if (adlibenv[curchan]>=1.0) adlibdidattack[curchan] = 1;
						}
				}
		}
}

void initadlib (uint16_t baseport) {
	//samprateadjust = (double)44100.0 / usesamplerate;
	set_port_write_redirector (baseport, baseport + 1, &outadlib);
	set_port_read_redirector (baseport, baseport + 1, &inadlib);
}
