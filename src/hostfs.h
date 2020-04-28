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

#ifndef FAKE86_HOSTFS_H_INCLUDED
#define FAKE86_HOSTFS_H_INCLUDED

#include <SDL.h>

#define hostfs_read			SDL_RWread
#define hostfs_close			SDL_RWclose
#define hostfs_size			SDL_RWsize
#define hostfs_seek_set(file,ofs)	SDL_RWseek(file, ofs, RW_SEEK_SET)
#define hostfs_seek_end(file,ofs)	SDL_RWseek(file, ofs, RW_SEEK_END)
#define hostfs_seek_cur(file,ofs)	SDL_RWseek(file, ofs, RW_SEEK_CUR)
#define hostfs_tell			SDL_RWtell

typedef SDL_RWops HOSTFS_FILE;

extern int hostfs_init ( void );
extern HOSTFS_FILE *hostfs_open ( const char *fn, const char *mode );
extern HOSTFS_FILE *hostfs_open_with_feedback ( const char *fn, const char *mode, const char *msg );
extern int hostfs_load_binary ( const char *fn, void *buf, int min_size, int max_size, const char *msg );

#endif
