/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers

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

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include "mutex.h"

// For texture size we want to use the largest width and height for
// any emulated PC mode!! Since we will update only parts of this
// texture based on the actual PC video mode wanted to be emulated.
#define WINDOW_WIDTH	640
#define WINDOW_HEIGHT	480
#define TEXTURE_WIDTH	720
#define TEXTURE_HEIGHT	480
#define PIXEL_FORMAT	SDL_PIXELFORMAT_ARGB8888

#ifdef _WIN32
CRITICAL_SECTION screenmutex;
#else
pthread_t vidthread;
pthread_mutex_t screenmutex = PTHREAD_MUTEX_INITIALIZER;
#endif

//SDL_Surface *screen = NULL;
//uint32_t *scalemap = NULL;
uint8_t regenscalemap = 1;

extern uint8_t RAM[0x100000], portram[0x10000];
extern uint8_t VRAM[262144], vidmode, cgabg, blankattr, vidgfxmode, vidcolor, running;
extern uint16_t cursx, cursy, cols, rows, vgapage, cursorposition, cursorvisible;
extern uint8_t updatedscreen, clocksafe, port3da, port6, portout16;
extern uint16_t VGA_SC[0x100], VGA_CRTC[0x100], VGA_ATTR[0x100], VGA_GC[0x100];
extern uint32_t videobase, textbase, x, y;
extern uint8_t fontcga[32768];
extern uint32_t palettecga[16], palettevga[256];
extern uint32_t usefullscreen, usegrabmode;

uint64_t totalframes = 0;
uint32_t framedelay = 20;
uint8_t scrmodechange = 0, noscale = 0, nosmooth = 1, renderbenchmark = 0, doaudio = 1;
char windowtitle[128];

void initcga(void);
#ifdef _WIN32
void VideoThread (void *dummy);
#else
void *VideoThread (void *dummy);
#endif

SDL_Window   *sdl_win = NULL;
SDL_Renderer *sdl_ren = NULL;
SDL_Texture  *sdl_tex = NULL;
SDL_PixelFormat *sdl_pixfmt = NULL;


void setwindowtitle (const char *extra) {
	char temptext[256];
	sprintf(temptext, "%s%s", windowtitle, extra ? extra : "");
	SDL_SetWindowTitle(sdl_win, temptext);
}


int initscreen ( const char *ver )
{
	if (doaudio) {
		if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) )
			return 1;
	} else {
		if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER) )
			return 1;
	}
	//screen = SDL_SetVideoMode (640, 400, 32, SDL_HWSURFACE);
	sdl_win = SDL_CreateWindow(
		ver,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		WINDOW_WIDTH, WINDOW_HEIGHT,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
	);
	if (!sdl_win)
		return 1;
	sdl_ren = SDL_CreateRenderer(sdl_win, -1, SDL_RENDERER_ACCELERATED);
	if (!sdl_ren)
		return 1;
	SDL_RenderSetLogicalSize(sdl_ren, WINDOW_WIDTH, WINDOW_HEIGHT);
	sdl_tex = SDL_CreateTexture(
		sdl_ren,
		PIXEL_FORMAT,
		SDL_TEXTUREACCESS_STREAMING,
		TEXTURE_WIDTH, TEXTURE_HEIGHT
	);
	if (!sdl_tex)
		return 1;
	sdl_pixfmt = SDL_AllocFormat(PIXEL_FORMAT);
	if (!sdl_pixfmt)
		return 1;
	printf("Rmask=%08X Gmask=%08X Bmask=%08X Amask=%08X Rloss=%d Gloss=%d Bloss=%d Aloss=%d Rshift=%d Gshift=%d Bshift=%d Ashift=%d\n",
		sdl_pixfmt->Rmask,  sdl_pixfmt->Gmask,  sdl_pixfmt->Bmask,  sdl_pixfmt->Amask,
		sdl_pixfmt->Rloss,  sdl_pixfmt->Gloss,  sdl_pixfmt->Bloss,  sdl_pixfmt->Aloss,
		sdl_pixfmt->Rshift, sdl_pixfmt->Gshift, sdl_pixfmt->Bshift, sdl_pixfmt->Ashift
	);
	sprintf(windowtitle, "%s", ver);
	setwindowtitle(NULL);
	initcga();
#ifdef _WIN32
	InitializeCriticalSection (&screenmutex);
	_beginthread (VideoThread, 0, NULL);
#else
	pthread_create (&vidthread, NULL, (void *) VideoThread, NULL);
#endif
	return 0;
}

//uint32_t prestretch[1024][1024];
//uint32_t nw, nh; //native width and height, pre-stretching (i.e. 320x200 for mode 13h)
void createscalemap(void) {
#if 0
	uint32_t srcx, srcy, dstx, dsty, scalemapptr;
	double xscale, yscale;

	xscale = (double) nw / (double) screen->w;
	yscale = (double) nh / (double) screen->h;
	if (scalemap != NULL) free(scalemap);
	scalemap = (void *)malloc( ((uint32_t)screen->w + 1) * (uint32_t)screen->h * 4);
	if (scalemap == NULL) {
		fprintf(stderr, "\nFATAL: Unable to allocate memory for scalemap!\n");
		exit(1);
	}
	scalemapptr = 0;
	for (dsty=0; dsty<(uint32_t)screen->h; dsty++) {
			srcy = (uint32_t) ( (double) dsty * yscale);
			scalemap[scalemapptr++] = srcy;
			for (dstx=0; dstx<(uint32_t)screen->w; dstx++) {
					srcx = (uint32_t) ( (double) dstx * xscale);
					scalemap[scalemapptr++] = srcx;
				}
		}

	regenscalemap = 0;
#endif
}

extern uint16_t oldw, oldh, constantw, constanth;
static void draw(void);
extern void handleinput(void);
#ifdef _WIN32
void VideoThread (void *dummy) {
#else
void *VideoThread (void *dummy) {
#endif
	uint32_t cursorprevtick, cursorcurtick, delaycalc;
	cursorprevtick = SDL_GetTicks();
	cursorvisible = 0;

	while (running) {
		cursorcurtick = SDL_GetTicks();
		if ( (cursorcurtick - cursorprevtick) >= 250) {
			updatedscreen = 1;
			cursorvisible = ~cursorvisible & 1;
			cursorprevtick = cursorcurtick;
		}
		if (updatedscreen || renderbenchmark) {
			updatedscreen = 0;
			//if (screen != NULL) {
				MutexLock (screenmutex);
				if (regenscalemap)
					createscalemap();
				draw();
				MutexUnlock(screenmutex);
			//}
			totalframes++;
		}
		if (!renderbenchmark) {
			delaycalc = framedelay - (SDL_GetTicks() - cursorcurtick);
			if (delaycalc > framedelay)
				delaycalc = framedelay;
			SDL_Delay(delaycalc);
		}
	}
#ifndef _WIN32
	return NULL;
#endif
}

#ifdef _WIN32
void ShowMenu(void);
void HideMenu(void);
#endif

void doscrmodechange(void) {
#if 0
	MutexLock (screenmutex);
	if (scrmodechange) {
		if (screen != NULL)
			SDL_FreeSurface (screen);
#ifdef _WIN32
		if (usefullscreen)
			HideMenu();
		else
			ShowMenu();
#endif
		if (constantw && constanth)
			screen = SDL_SetVideoMode (constantw, constanth, 32, SDL_HWSURFACE | usefullscreen);
		else if (noscale)
			screen = SDL_SetVideoMode (nw, nh, 32, SDL_HWSURFACE | usefullscreen);
		else {
			if ( (nw >= 640) || (nh >= 400) ) screen = SDL_SetVideoMode (nw, nh, 32, SDL_HWSURFACE | usefullscreen);
			else screen = SDL_SetVideoMode (640, 400, 32, SDL_HWSURFACE | usefullscreen);
		}
		if (usefullscreen)
			SDL_WM_GrabInput (SDL_GRAB_ON); //always have mouse grab turned on for full screen mode
		else
			SDL_WM_GrabInput (usegrabmode);
		SDL_ShowCursor (SDL_DISABLE);
		if (!usefullscreen) {
			if (usegrabmode == SDL_GRAB_ON)
				setwindowtitle (" (press Ctrl + Alt to release mouse)");
			else
				setwindowtitle (NULL);
		}
		regenscalemap = 1;
		createscalemap();
	}
	MutexUnlock (screenmutex);
	scrmodechange = 0;
#endif
}

#if 0
void stretchblit (SDL_Surface *target) {
	uint32_t srcx, srcy, dstx, dsty, lastx, lasty, r, g, b;
	uint32_t consecutivex, consecutivey = 0, limitx, limity, scalemapptr;
	uint32_t ofs;
	uint8_t *pixelrgb;

	limitx = (uint32_t)((double) nw / (double) target->w);
	limity = (uint32_t)((double) nh / (double) target->h);

	if (SDL_MUSTLOCK (target) )
		if (SDL_LockSurface (target) < 0)
			return;

	lasty = 0;
	scalemapptr = 0;
	for (dsty=0; dsty<(uint32_t)target->h; dsty++) {
			srcy = scalemap[scalemapptr++];
			ofs = dsty*target->w;
			consecutivex = 0;
			lastx = 0;
			if (srcy == lasty) consecutivey++;
			else consecutivey = 0;
			for (dstx=0; dstx<(uint32_t)target->w; dstx++) {
					srcx = scalemap[scalemapptr++];
					pixelrgb = (uint8_t *) &prestretch[srcy][srcx];
					r = pixelrgb[0];
					g = pixelrgb[1];
					b = pixelrgb[2];
					if (srcx == lastx) consecutivex++;
					else consecutivex = 0;
					if ( (consecutivex > limitx) && (consecutivey > limity) ) {
							pixelrgb = (uint8_t *) &prestretch[srcy][srcx+1];
							r += pixelrgb[0];
							g += pixelrgb[1];
							b += pixelrgb[2];
							pixelrgb = (uint8_t *) &prestretch[srcy+1][srcx];
							r += pixelrgb[0];
							g += pixelrgb[1];
							b += pixelrgb[2];
							pixelrgb = (uint8_t *) &prestretch[srcy+1][srcx+1];
							r += pixelrgb[0];
							g += pixelrgb[1];
							b += pixelrgb[2];
							r = r >> 2;
							g = g >> 2;
							b = b >> 2;
							//r = 255; g = 0; b = 0;
						}
					else if (consecutivex > limitx) {
							pixelrgb = (uint8_t *) &prestretch[srcy][srcx+1];
							r += pixelrgb[0];
							r = r >> 1;
							g += pixelrgb[1];
							g = g >> 1;
							b += pixelrgb[2];
							b = b >> 1;
							//r = 0; g = 255; b = 0;
						}
					else if (consecutivey > limity) {
							pixelrgb = (uint8_t *) &prestretch[srcy+1][srcx];
							r += pixelrgb[0];
							r = r >> 1;
							g += pixelrgb[1];
							g = g >> 1;
							b += pixelrgb[2];
							b = b >> 1;
							//r = 0; g = 0; b = 255;
						}
					( (uint32_t *) target->pixels) [ofs++] = SDL_MapRGB (target->format, (uint8_t) r, (uint8_t) g, (uint8_t) b);
					lastx = srcx;
				}
			lasty = srcy;
		}

	if (SDL_MUSTLOCK (target) )
		SDL_UnlockSurface (target);
	SDL_UpdateRect (target, 0, 0, target->w, target->h);
}
#endif

#if 0
void roughblit (SDL_Surface *target) {
	uint32_t srcx, srcy, dstx, dsty, scalemapptr;
	int32_t ofs;
	uint8_t *pixelrgb;

	if (SDL_MUSTLOCK (target) )
		if (SDL_LockSurface (target) < 0)
			return;

	scalemapptr = 0;
	for (dsty=0; dsty<(uint32_t)target->h; dsty++) {
			srcy = scalemap[scalemapptr++];
			ofs = dsty*target->w;
			for (dstx=0; dstx<(uint32_t)target->w; dstx++) {
					srcx = scalemap[scalemapptr++];
					pixelrgb = (uint8_t *) &prestretch[srcy][srcx];
					( (uint32_t *) target->pixels) [ofs++] = SDL_MapRGB (target->format, pixelrgb[0], pixelrgb[1], pixelrgb[2]);
				}
		}

	if (SDL_MUSTLOCK (target) )
		SDL_UnlockSurface (target);
	SDL_UpdateRect (target, 0, 0, target->w, target->h);
}
#endif



/* NOTE: doubleblit is only used when smoothing is not enabled, and the SDL window size
         is exactly double of native resolution for the current video mode. we can take
         advantage of the fact that every pixel is simply doubled both horizontally and
         vertically. this way, we do not need to waste mountains of CPU time doing
         floating point multiplication for each and every on-screen pixel. it makes the
         difference between games being smooth and playable, and being jerky on my old
         400 MHz PowerPC G3 iMac.
*/
#if 0
void doubleblit (SDL_Surface *target) {
	uint32_t srcx, srcy, dstx, dsty, curcolor;
	int32_t ofs; //, startofs;
	uint8_t *pixelrgb;

	if (SDL_MUSTLOCK (target) )
		if (SDL_LockSurface (target) < 0)
			return;

	for (dsty=0; dsty<(uint32_t)target->h; dsty += 2) {
			srcy = (uint32_t) (dsty >> 1);
			//startofs = ofs = dsty*target->w;
			ofs = dsty*target->w;
			for (dstx=0; dstx<(uint32_t)target->w; dstx += 2) {
					srcx = (uint32_t) (dstx >> 1);
					pixelrgb = (uint8_t *) &prestretch[srcy][srcx];
					curcolor = SDL_MapRGB (target->format, pixelrgb[0], pixelrgb[1], pixelrgb[2]);
					( (uint32_t *) target->pixels) [ofs+target->w] = curcolor;
					( (uint32_t *) target->pixels) [ofs++] = curcolor;
					( (uint32_t *) target->pixels) [ofs+target->w] = curcolor;
					( (uint32_t *) target->pixels) [ofs++] = curcolor;
				}
		}

	if (SDL_MUSTLOCK (target) )
		SDL_UnlockSurface (target);
	SDL_UpdateRect (target, 0, 0, target->w, target->h);
}
#endif


static struct pixel_access {
	SDL_Rect	rect;
	int		texture_pitch;
	int		tail;
	uint32_t	*pix;
} pia;

static uint32_t *start_pixel_access ( int nw, int nh )
{
	pia.rect.x = 0;
	pia.rect.y = 0;
	pia.rect.w = nw;
	pia.rect.h = nh;
	if (nw > TEXTURE_WIDTH) {
		fprintf(stderr, "FATAL: Texture width (%d) is too small for the needed emulated video mode width %d!\n", TEXTURE_WIDTH, nw);
		exit(1);
	}
	if (nh > TEXTURE_HEIGHT) {
		fprintf(stderr, "FATAL: Texture height (%d) is too small for the needed emulated video mode height %d!\n", TEXTURE_HEIGHT, nh);
		exit(1);
	}
	void *pixels;
	if (SDL_LockTexture(sdl_tex, &pia.rect, &pixels, &pia.texture_pitch)) {
		fprintf(stderr, "FATAL: Cannot lock texture: %s\n", SDL_GetError());
		exit(1);
	}
	// "tail" is in DWORDS, which must be added at every end of line to a uint32 pointer
	pia.tail = (pia.texture_pitch - 4 * nw) / 4;
	pia.pix = pixels;
	//printf("WIDTH=%d pitch=%d tail=%d\n", nw, pia.texture_pitch, pia.tail);
	return pixels;
}


extern uint16_t vtotal;
static void draw (void) {
	uint32_t planemode, vgapage, color, chary, charx, vidptr, divx, divy, curchar, curpixel, usepal, intensity, blockw, curheight, x1, y1;
	// Nice. Now time to render madness.
	Uint32 *pix;
	switch (vidmode) {
		case 0:
		case 1:
		case 2: //text modes
		case 3:
		case 7:
		case 0x82:
			pix = start_pixel_access(640, 400);
			vgapage = ( (uint32_t) VGA_CRTC[0xC]<<8) + (uint32_t) VGA_CRTC[0xD];
			for (y=0; y<400; y++) {
				for (x=0; x<640; x++) {
					if (cols==80) {
						charx = x/8;
						divx = 1;
					} else {
						charx = x/16;
						divx = 2;
					}
					if ( (portram[0x3D8]==9) && (portram[0x3D4]==9) ) {
						chary = y/4;
						vidptr = vgapage + videobase + chary*cols*2 + charx*2;
						curchar = RAM[vidptr];
						color = fontcga[curchar*128 + (y%4) *8 + ( (x/divx) %8) ];
					} else {
						chary = y/16;
						vidptr = videobase + chary*cols*2 + charx*2;
						curchar = RAM[vidptr];
						color = fontcga[curchar*128 + (y%16) *8 + ( (x/divx) %8) ];
					}
					if (vidcolor) {
						/*if (!color) if (portram[0x3D8]&128) color = palettecga[ (RAM[vidptr+1]/16) &7];
							else*/
						if (!color)
							color = palettecga[RAM[vidptr+1]/16]; //high intensity background
						else
							color = palettecga[RAM[vidptr+1]&15];
					} else {
						if ( (RAM[vidptr+1] & 0x70) ) {
							if (!color)
								color = palettecga[7];
							else
								color = palettecga[0];
						} else {
							if (!color)
								color = palettecga[0];
							else
								color = palettecga[7];
						}
					}
					//prestretch[y][x] = color;
					*pix++ = color;
				}
				pix += pia.tail;
			}
			break;
		case 4:
		case 5:
			pix = start_pixel_access(320, 200);
			usepal = (portram[0x3D9]>>5) & 1;
			intensity = ( (portram[0x3D9]>>4) & 1) << 3;
			for (y=0; y<200; y++) {
				for (x=0; x<320; x++) {
					charx = x;
					chary = y;
					vidptr = videobase + ( (chary>>1) * 80) + ( (chary & 1) * 8192) + (charx >> 2);
					curpixel = RAM[vidptr];
					switch (charx & 3) {
						case 3:
							curpixel = curpixel & 3;
							break;
						case 2:
							curpixel = (curpixel>>2) & 3;
							break;
						case 1:
							curpixel = (curpixel>>4) & 3;
							break;
						case 0:
							curpixel = (curpixel>>6) & 3;
							break;
					}
					if (vidmode==4) {
						curpixel = curpixel * 2 + usepal + intensity;
						if (curpixel == (usepal + intensity) )
							curpixel = cgabg;
						color = palettecga[curpixel];
						//prestretch[y][x] = color;
						*pix++ = color;
					} else {
						curpixel = curpixel * 63;
						color = palettecga[curpixel];
						//prestretch[y][x] = color;
						*pix++ = color;
					}
				}
				pix += pia.tail;
			}
			break;
		case 6:
			pix = start_pixel_access(640, 200);
			for (y=0; y<200; y++) {
				for (x=0; x<640; x++) {
					charx = x;
					chary = y;
					vidptr = videobase + ( (chary>>1) * 80) + ( (chary&1) * 8192) + (charx>>3);
					curpixel = (RAM[vidptr]>> (7- (charx&7) ) ) &1;
					color = palettecga[curpixel*15];
					//prestretch[y][x] = color;
					//prestretch[y+1][x] = color;
					*pix++ = color;
				}
				pix += pia.tail;
			}
			break;
		case 127:
			pix = start_pixel_access(720, 348);
			for (y=0; y<348; y++) {
				for (x=0; x<720; x++) {
					charx = x;
					chary = y>>1;
					vidptr = videobase + ( (y & 3) << 13) + (y >> 2) *90 + (x >> 3);
					curpixel = (RAM[vidptr]>> (7- (charx&7) ) ) &1;
#if 0
#ifdef __BIG_ENDIAN__
					if (curpixel)
						color = 0xFFFFFF00;
#else
					if (curpixel)
						color = 0x00FFFFFF;
#endif
					else color = 0x00000000;
#endif
					//prestretch[y][x] = color;
					*pix++ = curpixel ? palettevga[15] : palettevga[0];	// FIXME: hercules "colors" :) [no, no the colorhercules which really existed ...]
				}
				pix += pia.tail;
			}
			break;
		case 0x8: //160x200 16-color (PCjr)
			//nw = 640; //fix this
			//nh = 400; //part later
			pix = start_pixel_access(640, 400);
			for (y=0; y<400; y++) {
				for (x=0; x<640; x++) {
					vidptr = 0xB8000 + (y>>2) *80 + (x>>3) + ( (y>>1) &1) *8192;
					if ( ( (x>>1) &1) ==0)
						color = palettecga[RAM[vidptr] >> 4];
					else
						color = palettecga[RAM[vidptr] & 15];
					//prestretch[y][x] = color;
					*pix++ = color;
				}
				pix += pia.tail;
			}
			break;
		case 0x9: //320x200 16-color (Tandy/PCjr)
			// nw = 640; //fix this
			// nh = 400; //part later
			pix = start_pixel_access(640, 400);
			for (y=0; y<400; y++) {
				for (x=0; x<640; x++) {
					vidptr = 0xB8000 + (y>>3) *160 + (x>>2) + ( (y>>1) &3) *8192;
					if ( ( (x>>1) &1) ==0)
						color = palettecga[RAM[vidptr] >> 4];
					else
						color = palettecga[RAM[vidptr] & 15];
					//prestretch[y][x] = color;
					*pix++ = color;
				}
				pix += pia.tail;
			}
			break;
		case 0xD:
		case 0xE:
			//nw = 640; //fix this
			//nh = 400; //part later
			pix = start_pixel_access(640, 400);
			for (y=0; y<400; y++) {
				for (x=0; x<640; x++) {
					divx = x>>1;
					divy = y>>1;
					vidptr = divy*40 + (divx>>3);
					x1 = 7 - (divx & 7);
					color = (VRAM[vidptr] >> x1) & 1;
					color += ( ( (VRAM[0x10000 + vidptr] >> x1) & 1) << 1);
					color += ( ( (VRAM[0x20000 + vidptr] >> x1) & 1) << 2);
					color += ( ( (VRAM[0x30000 + vidptr] >> x1) & 1) << 3);
					color = palettevga[color];
					//prestretch[y][x] = color;
					*pix++ = color;
				}
				pix += pia.tail;
			}
			break;
		case 0x10:
			//nw = 640;
			//nh = 350;
			pix = start_pixel_access(640, 350);
			for (y=0; y<350; y++) {
				for (x=0; x<640; x++) {
					vidptr = y*80 + (x>>3);
					x1 = 7 - (x & 7);
					color = (VRAM[vidptr] >> x1) & 1;
					color |= ( ( (VRAM[0x10000 + vidptr] >> x1) & 1) << 1);
					color |= ( ( (VRAM[0x20000 + vidptr] >> x1) & 1) << 2);
					color |= ( ( (VRAM[0x30000 + vidptr] >> x1) & 1) << 3);
					color = palettevga[color];
					//prestretch[y][x] = color;
					*pix++ = color;
				}
				pix += pia.tail;
			}
			break;
		case 0x12:
			//nw = 640;
			//nh = 480;
			pix = start_pixel_access(640, 480);
			vgapage = ( (uint32_t) VGA_CRTC[0xC]<<8) + (uint32_t) VGA_CRTC[0xD];
			for (y=0; y<pia.rect.h; y++) {
				for (x=0; x<pia.rect.w; x++) {
					vidptr = y*80 + (x/8);
					color  = (VRAM[vidptr] >> (~x & 7) ) & 1;
					color |= ( (VRAM[vidptr+0x10000] >> (~x & 7) ) & 1) << 1;
					color |= ( (VRAM[vidptr+0x20000] >> (~x & 7) ) & 1) << 2;
					color |= ( (VRAM[vidptr+0x30000] >> (~x & 7) ) & 1) << 3;
					//prestretch[y][x] = palettevga[color];
					*pix++ = palettevga[color];
				}
				pix += pia.tail;
			}
			break;
		case 0x13:
			if (vtotal == 11) { //ugly hack to show Flashback at the proper resolution
				//nw = 256;
				//nh = 224;
				pix = start_pixel_access(256, 224);
			} else {
				//nw = 320;
				//nh = 200;
				pix = start_pixel_access(320, 200);
			}
			if (VGA_SC[4] & 6)
				planemode = 1;
			else
				planemode = 0;
			vgapage = ( (uint32_t) VGA_CRTC[0xC]<<8) + (uint32_t) VGA_CRTC[0xD];
			for (y = 0; y < pia.rect.h; y++) {
				for (x = 0; x < pia.rect.w; x++) {
					if (!planemode) {
						color = palettevga[RAM[videobase + ((vgapage + y*pia.rect.w + x) & 0xFFFF) ]];
					//if (!planemode) {
					//	color = palettevga[RAM[videobase + y*nw + x]];
					} else {
						vidptr = y*pia.rect.w + x;
						vidptr = vidptr/4 + (x & 3) *0x10000;
						vidptr = vidptr + vgapage - (VGA_ATTR[0x13] & 15);
						color = palettevga[VRAM[vidptr]];
					}
					//prestretch[y][x] = color;
					*pix++ = color;
				}
				pix += pia.tail;
			}
	}
	if (vidgfxmode==0) {
		if (cursorvisible) {
			curheight = 2;
			if (cols == 80)
				blockw = 8;
			else
				blockw = 16;
			x1 = cursx * blockw;
			y1 = cursy * 8 + 8 - curheight;
			for (y = y1 * 2; y <= y1 * 2 + curheight - 1; y++)
				for (x=x1; x <= x1 + blockw - 1; x++) {
					color = palettecga[RAM[videobase + cursy * cols * 2 + cursx * 2 + 1] & 15];
					//prestretch[y & 1023][x & 1023] = color;
					pia.pix[y * TEXTURE_WIDTH + x] = color;

				}
		}
	}
	SDL_UnlockTexture(sdl_tex);
	SDL_RenderClear(sdl_ren);
	SDL_RenderCopy(sdl_ren, sdl_tex, &pia.rect, NULL);
	SDL_RenderPresent(sdl_ren);
#if 0
	if (nosmooth) {
			if ( ((nw << 1) == screen->w) && ((nh << 1) == screen->h) ) doubleblit (screen);
			else roughblit (screen);
		}
	else stretchblit (screen);
#endif
}

