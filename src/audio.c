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

/* audio.c: functions to mix the audio channels, and handle SDL's audio interface. */

#include "config.h"
#include <SDL.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <memory.h>

#include "audio.h"

#include "blaster.h"
#include "adlib.h"
#include "sndsource.h"
#include "speaker.h"
#include "main.h"
#include "timing.h"

uint8_t doaudio = 1;

static struct wav_hdr_s wav_hdr;
static FILE *wav_file = NULL;

static SDL_AudioSpec wanted;
static int8_t audbuf[96000];
static int32_t audbufptr, usebuffersize;
int32_t usesamplerate = AUDIO_DEFAULT_SAMPLE_RATE;
int32_t latency = AUDIO_DEFAULT_LATENCY;

#if 0
static inline void putmemstr ( uint8_t *dest, const char *str )
{
	while (*str) {
		*dest++ = *str++;
	}
}

static void create_output_wav (const char *filename) {
	printf ("Creating %s for audio logging... ", filename);
	wav_file = fopen (filename, "wb");
	if (wav_file == NULL) {
			printf ("failed!\n");
			return;
		}
	printf ("OK!\n");

	wav_hdr.AudioFormat = 1; //PCM
	wav_hdr.bitsPerSample = 8;
	wav_hdr.blockAlign = 1;
	wav_hdr.ChunkSize = sizeof (wav_hdr) - 4;
	//sprintf ((char*)&wav_hdr.WAVE[0], "WAVE");
	putmemstr(wav_hdr.WAVE, "WAVE");
	//sprintf ((char*)&wav_hdr.fmt[0], "fmt ");
	putmemstr(wav_hdr.fmt, "fmt ");
	wav_hdr.NumOfChan = 1;
	wav_hdr.bytesPerSec = usesamplerate * (uint32_t) (wav_hdr.bitsPerSample >> 3) * (uint32_t) wav_hdr.NumOfChan;
	//sprintf ((char*)&wav_hdr.RIFF[0], "RIFF");
	putmemstr(wav_hdr.RIFF, "RIFF");
	wav_hdr.Subchunk1Size = 16;
	wav_hdr.SamplesPerSec = usesamplerate;
	//sprintf ((char*)&wav_hdr.Subchunk2ID[0], "data");
	putmemstr(wav_hdr.Subchunk2ID, "data");
	wav_hdr.Subchunk2Size = 0;
	//fwrite((void *)&wav_hdr, 1, sizeof(wav_hdr), wav_file);
}
#endif

static uint64_t doublesamplecount;

#if 0
static uint64_t cursampnum = 0, sampcount = 0, framecount = 0;
static char     bmpfilename[256];

static void savepic(void) {
	// FIXME: implement for SDL2
	//SDL_SaveBMP (screen, bmpfilename);
}

static int8_t samps[2400];
#endif

uint8_t audiobufferfilled(void)
{
	if (audbufptr >= usebuffersize) return(1);
	return 0;
}


void tickaudio(void) {
	int16_t sample;
	if (audbufptr >= usebuffersize)
		return;
	sample = adlibgensample() >> 4;
	if (usessource)
		sample += getssourcebyte();
	sample += getBlasterSample();
	if (speakerenabled)
		sample += (speakergensample() >> 1);
	if (audbufptr < sizeof(audbuf))
		audbuf[audbufptr++] = (uint8_t)((uint16_t)sample+128);
}


static void fill_audio ( void *udata, int8_t *stream, int len )
{
	memcpy (stream, audbuf, len);
	memmove (audbuf, &audbuf[len], usebuffersize - len);

	audbufptr -= len;
	if (audbufptr < 0) audbufptr = 0;
}


void initaudio ( void )
{
	printf ("Initializing audio stream... ");
	if (usesamplerate < 4000)
		usesamplerate = 4000;
	else if (usesamplerate > 96000)
		usesamplerate = 96000;
	if (latency < 10)
		latency = 10;
	else if (latency > 1000)
		latency = 1000;
	audbufptr = usebuffersize = (usesamplerate / 1000) * latency;
	gensamplerate = usesamplerate;
	doublesamplecount = (uint32_t) ((double)usesamplerate * (double)0.01);
	wanted.freq = usesamplerate;
	wanted.format = AUDIO_U8;
	wanted.channels = 1;
	wanted.samples = (uint16_t) usebuffersize >> 1;
	wanted.callback = (void*)fill_audio;
	wanted.userdata = NULL;
	if (SDL_OpenAudio (&wanted, NULL) <0) {
		printf ("Error: %s\n", SDL_GetError());
		return;
	} else {
		printf ("OK! (%lu Hz, %lu ms, %lu sample latency)\n", (long unsigned int)usesamplerate, (long unsigned int)latency, (long unsigned int)usebuffersize);
	}
	memset(audbuf, 128, sizeof(audbuf));
	audbufptr = usebuffersize;
	//create_output_wav("fake86.wav");
	SDL_PauseAudio(0);
	return;
}


void killaudio ( void )
{
	SDL_PauseAudio (1);
	if (wav_file == NULL)
		return;
	wav_hdr.ChunkSize = wav_hdr.Subchunk2Size + sizeof(wav_hdr) - 8;
	fseek(wav_file, 0, SEEK_SET);
	fwrite((void*)&wav_hdr, 1, sizeof(wav_hdr), wav_file);
	fclose (wav_file);
}
