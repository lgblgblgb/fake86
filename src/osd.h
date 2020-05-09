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

#ifndef FAKE86_OSD_H_INCLUDED
#define FAKE86_OSD_H_INCLUDED

#include "config.h"

#ifdef USE_OSD
#include <SDL.h>
#include <stdint.h>
struct osd {
	uint32_t	fg, bg, cg;
	int		active, available, cursor;
	uint32_t	*pixels;
	const uint8_t	*font;
	int		x, y;
	int		curlastx, curlasty;
	int		width, height;
	int		texwidth, texheight;
	int		fontwidth, fontheight;
	SDL_Texture	*tex;
	SDL_PixelFormat	*pixfmt;
};
extern struct osd osd;

extern void osd_scroll ( void );
extern void osd_clearbox ( int x1, int y1, int x2, int y2, uint32_t color );
extern void osd_clearscr ( void );
extern void osd_setcolors ( uint32_t fg, uint32_t bg, uint32_t cg );
extern void osd_putchar ( const char c );
extern void osd_putstr ( const char *s );
extern int  osd_init ( SDL_Renderer *sdl_ren, SDL_PixelFormat *pixfmt, int texwidth, int texheight, const uint8_t *font );
#endif

#endif
