/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers
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

#ifndef FAKE86_SERMOUSE_H_INCLUDED
#define FAKE86_SERMOUSE_H_INCLUDED

#include <stdint.h>

struct sermouse_s {
	uint8_t	reg[8];
	uint8_t	buf[16];
	int8_t	bufptr;
	int	baseport;
};

extern struct sermouse_s sermouse;

extern void sermouseevent ( uint8_t buttons, int8_t xrel, int8_t yrel );
extern void initsermouse  ( uint16_t baseport, uint8_t irq );

#endif
