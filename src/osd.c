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

/* render.c: functions for SDL initialization, as well as video scaling/rendering.
   it is a bit messy. i plan to rework much of this in the future. i am also
   going to add hardware accelerated scaling soon. */

#include "config.h"

#ifdef USE_OSD

#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "osd.h"
#include "render.h"

struct osd osd;



void osd_scroll ( void )
{
	memmove(osd.pixels, osd.pixels + osd.texwidth * 16, osd.texwidth * (osd.texheight - 16) * 4);
	osd_clearbox(0, osd.height - 1, osd.width - 1, osd.height - 1, osd.bg);
}


void osd_clearbox ( int x1, int y1, int x2, int y2, uint32_t color )
{
	x1 *= 9;
	y1 *= 16;
	x2 = x2 * 9 + 8;
	y2 = y2 * 16 + 15;
	while (y1 <= y2) {
		uint32_t *pix = osd.pixels + x1 + y1 * osd.texwidth;
		int x = x1;
		while (x <= x2) {
			*pix++ = color;
			x++;
		}
		y1++;
	}
}

void osd_clearscr ( void )
{
	osd_clearbox(0, 0, osd.width - 1, osd.height - 1, osd.bg);
	osd.x = 0;
	osd.y = 0;
	osd.curlastx = -1;
	osd.curlasty = -1;
}


void osd_setcolors ( uint32_t fg, uint32_t bg, uint32_t cg )
{
	osd.fg = 
		(((fg >> 24) & 0xFF) << osd.pixfmt->Rshift) +
		(((fg >> 16) & 0xFF) << osd.pixfmt->Gshift) +
		(((fg >>  8) & 0xFF) << osd.pixfmt->Bshift) +
		(((fg      ) & 0xFF) << osd.pixfmt->Ashift) ;
	osd.bg = 
		(((bg >> 24) & 0xFF) << osd.pixfmt->Rshift) +
		(((bg >> 16) & 0xFF) << osd.pixfmt->Gshift) +
		(((bg >>  8) & 0xFF) << osd.pixfmt->Bshift) +
		(((bg      ) & 0xFF) << osd.pixfmt->Ashift) ;
	osd.cg = 
		(((cg >> 24) & 0xFF) << osd.pixfmt->Rshift) +
		(((cg >> 16) & 0xFF) << osd.pixfmt->Gshift) +
		(((cg >>  8) & 0xFF) << osd.pixfmt->Bshift) +
		(((cg      ) & 0xFF) << osd.pixfmt->Ashift) ;
}


void osd_putchar ( const char c )
{
	if (!osd.available)
		return;
	uint32_t *pix = osd.pixels + osd.x * 9 + osd.y * 16 * osd.texwidth;
	const uint8_t *fnt = osd.font + (unsigned int)c * 128;
	for (int y = 0; y < 16; y++) {
		for (int x = 0; x < 8; x++)
			*pix++ = (*fnt++) ? osd.fg : osd.bg;
		*pix++ = osd.bg;
		pix += osd.texwidth - 9;
	}
	osd.active = 1;
}


void osd_putstr ( const char *s )
{
	if (!osd.available)
		return;
	if (osd.curlastx >= 0 && osd.curlasty >= 0) {
		osd_clearbox(osd.curlastx, osd.curlasty, osd.curlastx, osd.curlasty, osd.bg);
		osd.curlastx = -1;
		osd.curlasty = -1;
	}
	while (*s) {
		if (*s == 8 && (osd.x > 0 || osd.y > 0)) {
			osd.x--;
			if (osd.x < 0) {
				osd.x = osd.width - 1;
				osd.y--;
			}
			osd_putchar(' ');
		} else if (*s == '\n') {
			osd.x = 0;
			osd.y++;
		} else if (*s == '\r') {
			osd.x = 0;
		} else {
			osd_putchar(*s);
			osd.x++;
		}
		if (osd.x >= osd.width) {
			osd.x = 0;
			osd.y++;
		}
		if (osd.y >= osd.height) {
			osd.y = osd.height - 1;
			osd_scroll();
		}
		s++;
	}
	if (osd.cursor) {
		osd_clearbox(osd.x, osd.y, osd.x, osd.y, osd.cg);
		osd.curlastx = osd.x;
		osd.curlasty = osd.y;
	}
}


int osd_init ( SDL_Renderer *sdl_ren, SDL_PixelFormat *pixfmt, int texwidth, int texheight, const uint8_t *font )
{
	osd.texwidth = texwidth;
	osd.texheight = texheight;
	osd.pixfmt = pixfmt;
	osd.available = 0;
	osd.active = 0;
	osd.tex = SDL_CreateTexture(
		sdl_ren,
		pixfmt->format,
		SDL_TEXTUREACCESS_STREAMING,
		osd.texwidth, osd.texheight
	);
	if (!osd.tex)
		return sdl_error("Cannot create OSD texture");
	if (SDL_SetTextureBlendMode(osd.tex, SDL_BLENDMODE_BLEND))
		return sdl_error("Cannot set BLENDMODE for OSD texture");
	osd.pixels = SDL_malloc(osd.texwidth * osd.texheight * 4);
	if (!osd.pixels)
		return sdl_error("Cannot allocate OSD pixels data");
	memset(osd.pixels, 0, osd.texwidth * osd.texheight * 4);
	osd.available = 1;
	osd.fg = 0xFFFFFFFFU;
	osd.cg = 0xFFFFFFFFU;
	osd.bg = 0x00000000U;
	osd.cursor = 0;
	osd.curlastx = -1;
	osd.curlasty = -1;
	osd.font = font;
	osd.x = 0;
	osd.y = 0;
	osd.width = osd.texwidth / 9;
	osd.height = osd.texheight / 16;
	osd.cursor = 0;
	printf("OSD: subsystem available with %dx%d pixels and %dx%d characters\n", osd.texwidth, osd.texheight, osd.width, osd.height);
	return 0;
}
#endif
