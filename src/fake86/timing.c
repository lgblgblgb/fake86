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

/* timing.c: critical functions to provide accurate timing for the
   system timer interrupt, and to generate new audio output samples. */

#include "config.h"
#include <SDL/SDL.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
LARGE_INTEGER queryperf;
#else
#include <sys/time.h>
struct timeval tv;
#endif
#include "i8253.h"
#include "blaster.h"

extern struct blaster_s blaster;
extern struct i8253_s i8253;
extern void doirq (uint8_t irqnum);
extern void tickaudio();
extern void tickssource();
extern void tickadlib();
extern void tickBlaster();

uint64_t hostfreq = 1000000, lasttick = 0, curtick = 0, tickgap, i8253tickgap, lasti8253tick, scanlinetiming, lastscanlinetick, curscanline = 0;
uint64_t sampleticks, lastsampletick, ssourceticks, lastssourcetick, adlibticks, lastadlibtick, lastblastertick, gensamplerate;

uint16_t pit0counter = 65535;
extern uint64_t totalexec;
extern uint32_t speed;
extern uint8_t port3da, doaudio, slowsystem;

void inittiming() {
#ifdef _WIN32
	QueryPerformanceFrequency (&queryperf);
	hostfreq = queryperf.QuadPart;
	QueryPerformanceCounter (&queryperf);
	curtick = queryperf.QuadPart;
#else
	hostfreq = 1000000;
	gettimeofday (&tv, NULL);
	curtick = (uint64_t) tv.tv_sec * (uint64_t) 1000000 + (uint64_t) tv.tv_usec;
#endif
	lasti8253tick = lastblastertick = lastadlibtick = lastssourcetick = lastsampletick = lastscanlinetick = lasttick = curtick;
	scanlinetiming = hostfreq / 31500;
	ssourceticks = hostfreq / 8000;
	adlibticks = hostfreq / 48000;
	if (doaudio) sampleticks = hostfreq / gensamplerate;
	else sampleticks = -1;
	i8253tickgap = hostfreq / 119318;
}

void timing() {
	uint8_t i8253chan;

#ifdef _WIN32
	QueryPerformanceCounter (&queryperf);
	curtick = queryperf.QuadPart;
#else
	gettimeofday (&tv, NULL);
	curtick = (uint64_t) tv.tv_sec * (uint64_t) 1000000 + (uint64_t) tv.tv_usec;
#endif

	if (curtick >= (lastscanlinetick + scanlinetiming) ) {
			curscanline = (curscanline + 1) % 525;
			if (curscanline > 479) port3da = 8;
			else port3da = 0;
			if (curscanline & 1) port3da |= 1;
			pit0counter++;
			lastscanlinetick = curtick;
		}

	if (i8253.active[0]) { //timer interrupt channel on i8253
			if (curtick >= (lasttick + tickgap) ) {
					lasttick = curtick;
					doirq (0);
				}
		}

	if (curtick >= (lasti8253tick + i8253tickgap) ) {
			for (i8253chan=0; i8253chan<3; i8253chan++) {
					if (i8253.active[i8253chan]) {
							if (i8253.counter[i8253chan] < 10) i8253.counter[i8253chan] = i8253.chandata[i8253chan];
							i8253.counter[i8253chan] -= 10;
						}
				}
			lasti8253tick = curtick;
		}

	if (curtick >= (lastssourcetick + ssourceticks) ) {
			tickssource();
			lastssourcetick = curtick - (curtick - (lastssourcetick + ssourceticks) );
		}

	if (blaster.samplerate > 0) {
			if (curtick >= (lastblastertick + blaster.sampleticks) ) {
					tickBlaster();
					lastblastertick = curtick - (curtick - (lastblastertick + blaster.sampleticks) );
				}
		}

	if (curtick >= (lastsampletick + sampleticks) ) {
			tickaudio();
			if (slowsystem) {
					tickaudio();
					tickaudio();
					tickaudio();
				}
			lastsampletick = curtick - (curtick - (lastsampletick + sampleticks) );
		}

	if (curtick >= (lastadlibtick + adlibticks) ) {
			tickadlib();
			lastadlibtick = curtick - (curtick - (lastadlibtick + adlibticks) );
		}
}
