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

#include <stdint.h>

struct blaster_s {
	uint8_t mem[1024];
	uint16_t memptr;
	uint16_t samplerate;
	uint8_t dspmaj;
	uint8_t dspmin;
	uint8_t speakerstate;
	uint8_t lastresetval;
	uint8_t lastcmdval;
	uint8_t lasttestval;
	uint8_t waitforarg;
	uint8_t paused8;
	uint8_t paused16;
	uint8_t sample;
	uint8_t sbirq;
	uint8_t sbdma;
	uint8_t usingdma;
	uint8_t maskdma;
	uint8_t useautoinit;
	uint32_t blocksize;
	uint32_t blockstep;
	uint64_t sampleticks;
	struct mixer_s {
		uint8_t index;
		uint8_t reg[256];
	} mixer;
};
