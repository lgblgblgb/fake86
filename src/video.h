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

#ifndef FAKE86_VIDEO_H_INCLUDED
#define FAKE86_VIDEO_H_INCLUDED

#include <stdint.h>

extern uint16_t cols;
extern uint16_t cursorposition;
extern uint16_t cursorvisible;
extern uint16_t cursx;
extern uint16_t cursy;
extern uint16_t oldh;
extern uint16_t oldw;
extern uint16_t rows;
extern uint16_t VGA_ATTR[0x100];
extern uint16_t VGA_CRTC[0x100];
extern uint16_t VGA_GC[0x100];
extern uint16_t vgapage;
extern uint16_t VGA_SC[0x100];
extern uint16_t vtotal;
extern uint32_t palettecga[16];
extern uint32_t palettevga[256];
extern uint32_t textbase;
extern uint32_t usefullscreen;
extern uint32_t usegrabmode;
extern uint32_t videobase;
extern uint8_t blankattr;
extern uint8_t cgabg;
extern uint8_t clocksafe;
//extern uint8_t fontcga[32768];
extern const uint8_t *fontcga;
extern uint8_t port3da;
extern uint8_t port6;
extern uint8_t readVGA(uint32_t addr32);
extern uint8_t updatedscreen;
extern uint8_t vidcolor;
extern uint8_t vidgfxmode;
extern uint8_t vidmode;
extern uint8_t VRAM[262144];
extern void initVideoPorts(void);
extern void vidinterrupt(void);
extern void writeVGA(uint32_t addr32, uint8_t value);
extern int  initcga ( void );

#endif
