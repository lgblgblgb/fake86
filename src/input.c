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

/* input.c: functions for translation of SDL scancodes to BIOS scancodes,
   and handling of SDL events in general. */

#include <stdio.h>
#include <SDL.h>
#include <stdint.h>

#include "input.h"

#include "video.h"
#include "sermouse.h"
#include "cpu.h"
#include "ports.h"
#include "i8259.h"
#include "render.h"

uint8_t keyboardwaitack = 0;
static uint8_t keydown[0x100];

static int translatescancode ( /*uint16_t keyval*/ SDL_Keycode keyval )
{
	//printf("translatekey for 0x%04X %s\n", keyval, SDL_GetKeyName(keyval));
	switch (keyval) {
		case SDLK_ESCAPE:	return 0x01;
		case 0x30:		return 0x0B;		// zero
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x38:
		case 0x39:		return keyval - 0x2F;	//other number keys
		case 0x2D:		return 0x0C;
		case 0x3D:		return 0x0D;
		case SDLK_BACKSPACE:	return 0x0E;
		case SDLK_TAB:		return 0x0F;
		case 0x71:		return 0x10;
		case 0x77:		return 0x11;
		case 0x65:		return 0x12;
		case 0x72:		return 0x13;
		case 0x74:		return 0x14;
		case 0x79:		return 0x15;
		case 0x75:		return 0x16;
		case 0x69:		return 0x17;
		case 0x6F:		return 0x18;
		case 0x70:		return 0x19;
		case 0x5B:		return 0x1A;
		case 0x5D:		return 0x1B;
		case SDLK_KP_ENTER:
		case SDLK_RETURN:
		case SDLK_RETURN2:	return 0x1C;
		case SDLK_RCTRL:
		case SDLK_LCTRL:	return 0x1D;
		case 0x61:		return 0x1E;
		case 0x73:		return 0x1F;
		case 0x64:		return 0x20;
		case 0x66:		return 0x21;
		case 0x67:		return 0x22;
		case 0x68:		return 0x23;
		case 0x6A:		return 0x24;
		case 0x6B:		return 0x25;
		case 0x6C:		return 0x26;
		case 0x3B:		return 0x27;
		case 0x27:		return 0x28;
		case 0x60:		return 0x29;
		case SDLK_LSHIFT:	return 0x2A;
		case 0x5C:		return 0x2B;
		case 0x7A:		return 0x2C;
		case 0x78:		return 0x2D;
		case 0x63:		return 0x2E;
		case 0x76:		return 0x2F;
		case 0x62:		return 0x30;
		case 0x6E:		return 0x31;
		case 0x6D:		return 0x32;
		case 0x2C:		return 0x33;
		case 0x2E:		return 0x34;
		case 0x2F:		return 0x35;
		case SDLK_RSHIFT:	return 0x36;
		case SDLK_PRINTSCREEN:	return 0x37;
		case SDLK_RALT:
		case SDLK_LALT:		return 0x38;
		case SDLK_SPACE:	return 0x39;
		case SDLK_CAPSLOCK:	return 0x3A;
		case SDLK_F1:		return 0x3B;	// F1
		case SDLK_F2:		return 0x3C;	// F2
		case SDLK_F3:		return 0x3D;	// F3
		case SDLK_F4:		return 0x3E;	// F4
		case SDLK_F5:		return 0x3F;	// F5
		case SDLK_F6:		return 0x40;	// F6
		case SDLK_F7:		return 0x41;	// F7
		case SDLK_F8:		return 0x42;	// F8
		case SDLK_F9:		return 0x43;	// F9
		case SDLK_F10:		return 0x44;	// F10
		case SDLK_NUMLOCKCLEAR:	return 0x45;
		case SDLK_SCROLLLOCK:	return 0x46;
		case SDLK_KP_7:
		case SDLK_HOME:		return 0x47;
		case SDLK_KP_8:
		case SDLK_UP:		return 0x48;
		case SDLK_KP_9:
		case SDLK_PAGEUP:	return 0x49;
		case SDLK_KP_MINUS:	return 0x4A;
		case SDLK_KP_4:
		case SDLK_LEFT:		return 0x4B;
		case SDLK_KP_5:		return 0x4C;
		case SDLK_KP_6:
		case SDLK_RIGHT:	return 0x4D;
		case SDLK_KP_PLUS:	return 0x4E;
		case SDLK_KP_1:
		case SDLK_END:		return 0x4F;
		case SDLK_KP_2:
		case SDLK_DOWN:		return 0x50;
		case SDLK_KP_3:
		case SDLK_PAGEDOWN:	return 0x51;
		case SDLK_KP_0:
		case SDLK_INSERT:	return 0x52;
		case SDLK_KP_PERIOD:
		case SDLK_DELETE:	return 0x53;
		default:		return   -1;	// *** UNSUPPORTED KEY ***
	}
}

static uint8_t buttons = 0;

static void mousegrabtoggle ( void )
{
	if (usegrabmode) {
		usegrabmode = 0;
		//SDL_WM_GrabInput(SDL_GRAB_OFF);
		//SDL_ShowCursor(SDL_ENABLE);
		SDL_SetRelativeMouseMode(SDL_FALSE);
		setwindowtitle(NULL);
	} else {
		usegrabmode = 1;
		//SDL_WM_GrabInput(SDL_GRAB_ON);
		//SDL_ShowCursor(SDL_DISABLE);
		SDL_SetRelativeMouseMode(SDL_TRUE);
		setwindowtitle(" (press Ctrl + Alt to release mouse)");
	}
}

void handleinput ( void )
{
	SDL_Event event;
	int mx = 0, my = 0;
	uint8_t tempbuttons;
	int translated_key;
	if (SDL_PollEvent (&event) ) {
		switch (event.type) {
			case SDL_KEYDOWN:
				translated_key = translatescancode(event.key.keysym.sym);
				if (translated_key >= 0) {
					portram[0x60] = translated_key;
					portram[0x64] |= 2;
					doirq(1);
					//printf("%02X\n", translatescancode(event.key.keysym.sym));
					keydown[translated_key] = 1;
					if (keydown[0x38] && keydown[0x1D] && (SDL_GetRelativeMouseMode() == SDL_TRUE)) {
						keydown[0x1D] = 0;
						keydown[0x32] = 0;
						mousegrabtoggle();
						break;
					}
					if (keydown[0x38] && keydown[0x1C]) {
						keydown[0x1D] = 0;
						keydown[0x38] = 0;
						usefullscreen = !usefullscreen;
						scrmodechange = 1;
						break;
					}
				} else
					printf("Unsupported key: %s [%d]\n", SDL_GetKeyName(event.key.keysym.sym), event.key.keysym.sym);
				break;
			case SDL_KEYUP:
				translated_key = translatescancode(event.key.keysym.sym);
				if (translated_key >= 0) {
					portram[0x60] = translated_key | 0x80;
					portram[0x64] |= 2;
					doirq(1);
					keydown[translated_key] = 0;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
					mousegrabtoggle();
					break;
				}
				tempbuttons = SDL_GetMouseState(NULL, NULL);
				if (tempbuttons & 1)
					buttons = 2;
				else
					buttons = 0;
				if (tempbuttons & 4)
					buttons |= 1;
				sermouseevent(buttons, 0, 0);
				break;
			case SDL_MOUSEBUTTONUP:
				if (SDL_GetRelativeMouseMode() == SDL_FALSE)
					break;
				tempbuttons = SDL_GetMouseState(NULL, NULL);
				if (tempbuttons & 1)
					buttons = 2;
				else
					buttons = 0;
				if (tempbuttons & 4)
					buttons |= 1;
				sermouseevent(buttons, 0, 0);
				break;
			case SDL_MOUSEMOTION:
				if (SDL_GetRelativeMouseMode() == SDL_FALSE)
					break;
				SDL_GetRelativeMouseState(&mx, &my);
				sermouseevent(buttons, (int8_t)mx, (int8_t)my);
				//SDL_WarpMouse (screen->w / 2, screen->h / 2);
				// TODO: what was this one -> SDL_WarpMouse  port to SDL2
				while (1) {
					SDL_PollEvent(&event);
					SDL_GetRelativeMouseState(&mx, &my);
					if ((mx == 0) && (my == 0))
						break;
				}
				break;
			case SDL_QUIT:
				running = 0;
				break;
			default:
				break;
		}
	}
}
