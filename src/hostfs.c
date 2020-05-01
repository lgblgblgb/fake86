/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2020      Gabor Lenart "LGB"

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

#include <SDL.h>
#include <stdio.h>

#include "hostfs.h"

// We need this for sdl_error() only
#include "render.h"

static const char *app_basepath;
static const char *app_prefpath;


int hostfs_was_fallback_mode;


static HOSTFS_FILE *try_open ( const char *basedir, const char *fn, const char *mode, const char *mode_fallback )
{
	char fnbuf[basedir ? strlen(basedir) + strlen(fn) + 1 : 0];
	if (basedir) {
		strcpy(fnbuf, basedir);
		strcpy(fnbuf + strlen(basedir), fn);
		fn = fnbuf;
	}
	HOSTFS_FILE *file = SDL_RWFromFile(fn, mode);
	hostfs_was_fallback_mode = 0;
	if (file)
		goto ok;
	if (mode_fallback) {
		hostfs_was_fallback_mode = 1;
		mode = mode_fallback;
		file = SDL_RWFromFile(fn, mode);
		if (file)
			goto ok;
	}
	return NULL;
ok:
	printf("File %s has been opened with mode \"%s\" successfully.\n", fn, mode);
	return file;
}

#ifdef _WIN32
#	define TRY_PATH0	"bin\\data\\"
#	define TRY_PATH1	"data\\"
#else
#	define TRY_PATH0	"bin/data/"
#	define TRY_PATH1	"data/"
#endif


HOSTFS_FILE *hostfs_open ( const char *fn, const char *mode )
{
	const char *fallback_mode;
	if (mode[0] == '?') {
		mode++;
		fallback_mode = "rb";
	} else
		fallback_mode = NULL;
	const char *tries[10];
	int try_no = 0;
	switch (fn[0]) {
		case '#':
			tries[try_no++] = app_basepath;
#ifndef _WIN32
			tries[try_no++] = PATH_DATAFILES;
#endif
			tries[try_no++] = TRY_PATH0;
			tries[try_no++] = TRY_PATH1;
			tries[try_no++] = app_prefpath;
			fn++;
			break;
		case '@':
			tries[try_no++] = app_prefpath;
			fn++;
			break;
		default:
			tries[try_no++] = NULL;
			break;
	}
	do {
		HOSTFS_FILE *file = try_open(tries[--try_no], fn, mode, fallback_mode);
		if (file)
			return file;
	} while (try_no > 0);
	return NULL;
}


HOSTFS_FILE *hostfs_open_with_feedback ( const char *fn, const char *mode, const char *msg )
{
	HOSTFS_FILE *file = hostfs_open(fn, mode);
	if (!file && msg)
		fprintf(stderr, "FILE: cannot open file (%s) %s [%s]\n", msg, fn, SDL_GetError());
	return file;
}


int hostfs_load_binary ( const char *fn, void *buf, int min_size, int max_size, const char *msg )
{
	const char *errcat;
	HOSTFS_FILE *file = hostfs_open_with_feedback(fn, "rb", msg);
	if (!file)
		return -1;
	size_t fsize = hostfs_size(file);
	if (fsize < 0) {
		errcat = "Cannot determine file size";
		goto cry;
	}
	if (fsize < min_size || fsize > max_size) {
		errcat = "Invalid size for the file";
		goto cry;
	}
	if (hostfs_read(file, buf, fsize, 1) != 1) {
		errcat = "Cannot read enough data";
		goto cry;
	}
	hostfs_close(file);
	return fsize;
cry:
	if (msg && errcat)
		fprintf(stderr, "FILE: %s (%s) %s [%s]\n", errcat, msg, fn, SDL_GetError());
	hostfs_close(file);
	return -1;
}


int hostfs_init ( void )
{
	// Initialize SDL with "nothing". So we delay audo/video/etc (whatever needed later) initialization.
	// However this initalize SDL for using its own functionality only, like SDL_GetPrefPath and other things!
	if (SDL_Init(0))
		return sdl_error("Cannot pre-initialize SDL2");
	app_basepath = SDL_GetBasePath();
	if (!app_basepath)
		return sdl_error("Cannot determine base directory");
	app_prefpath = SDL_GetPrefPath("lgb.hu", "fake86");
	if (!app_prefpath)
		return sdl_error("Cannot determine preference directory");
	return 0;
}
