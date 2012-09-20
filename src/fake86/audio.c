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

/* audio.c: functions to mix the audio channels, and handle SDL's audio interface. */

#include "config.h"
#include <SDL/SDL.h>
#ifdef _WIN32
#include <Windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include "blaster.h"
#include "audio.h"

extern SDL_Surface *screen;
struct wav_hdr_s wav_hdr;
FILE *wav_file = NULL;

SDL_AudioSpec wanted;
int8_t audbuf[96000];
int32_t audbufptr, usebuffersize, usesamplerate = AUDIO_DEFAULT_SAMPLE_RATE, latency = AUDIO_DEFAULT_LATENCY;
uint8_t speakerenabled = 0;

extern uint64_t gensamplerate, sampleticks, hostfreq;
extern int16_t adlibgensample();
extern int16_t speakergensample();
extern int16_t getssourcebyte();
extern int16_t getBlasterSample();

void create_output_wav (uint8_t *filename) {
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
	sprintf (&wav_hdr.WAVE[0], "WAVE");
	sprintf (&wav_hdr.fmt[0], "fmt ");
	wav_hdr.NumOfChan = 1;
	wav_hdr.bytesPerSec = usesamplerate * (uint32_t) (wav_hdr.bitsPerSample >> 3) * (uint32_t) wav_hdr.NumOfChan;
	sprintf (&wav_hdr.RIFF[0], "RIFF");
	wav_hdr.Subchunk1Size = 16;
	wav_hdr.SamplesPerSec = usesamplerate;
	sprintf (&wav_hdr.Subchunk2ID[0], "data");
	wav_hdr.Subchunk2Size = 0;
	//fwrite((void *)&wav_hdr, 1, sizeof(wav_hdr), wav_file);
}

uint64_t doublesamplecount, cursampnum = 0, sampcount = 0, framecount = 0;
uint8_t bmpfilename[256];

void savepic() {
	SDL_SaveBMP (screen, &bmpfilename[0]);
}

int8_t samps[2400];

void tickaudio() {
	int16_t sample;
	if (audbufptr >= usebuffersize) return;
	sample = adlibgensample() >> 4;
	//sample += getssourcebyte();
	sample += getBlasterSample();
	if (speakerenabled) sample += (speakergensample() >> 1);
	if (audbufptr < sizeof(audbuf) ) audbuf[audbufptr++] = (uint8_t) 
( 
(uint16_t) sample+128);
	//if ((cursampnum % doublesamplecount) == 0) audbuf[audbufptr++] = (int8_t)sample;
	//cursampnum++;
}

extern uint64_t timinginterval;
extern void inittiming();
void fill_audio (void *udata, int8_t *stream, int len) {
	memcpy (stream, audbuf, len);
	memmove (audbuf, &audbuf[len], usebuffersize - len);

	/*sampcount += len;
	while (sampcount >= 2400) {
	sprintf(&bmpfilename[0], "j:\\bmp\\%08u.bmp", framecount++);
	_beginthread(savepic, 0, NULL);
	sampcount -= 2400;
	}*/

	audbufptr -= len;
	if (audbufptr < 0) audbufptr = 0;
}

void initaudio() {
	printf ("Initializing audio stream... ");

	if (usesamplerate < 4000) usesamplerate = 4000;
	else if (usesamplerate > 96000) usesamplerate = 96000;
	if (latency < 10) latency = 10;
	else if (latency > 1000) latency = 1000;
	audbufptr = usebuffersize = (usesamplerate / 1000) * latency;
	gensamplerate = usesamplerate;
	doublesamplecount = (uint32_t) ( (double) usesamplerate * (double) 0.01);

	wanted.freq = usesamplerate;
	wanted.format = AUDIO_U8;
	wanted.channels = 1;
	wanted.samples = (uint16_t) usebuffersize >> 1;
	wanted.callback = (void *) fill_audio;
	wanted.userdata = NULL;

	if (SDL_OpenAudio (&wanted, NULL) <0) {
			printf ("Error: %s\n", SDL_GetError() );
			return;
		}
	else {
			printf ("OK! (%lu Hz, %lu ms, %lu sample latency)\n", usesamplerate, latency, usebuffersize);
		}

	memset (audbuf, 128, sizeof (audbuf) );
	audbufptr = usebuffersize;
	//create_output_wav("fake86.raw");
	SDL_PauseAudio (0);
	return;
}

void killaudio() {
	SDL_PauseAudio (1);

	if (wav_file == NULL) return;
	//wav_hdr.ChunkSize = wav_hdr.Subchunk2Size + sizeof(wav_hdr) - 8;
	//fseek(wav_file, 0, SEEK_SET);
	//fwrite((void *)&wav_hdr, 1, sizeof(wav_hdr), wav_file);
	fclose (wav_file);
}
