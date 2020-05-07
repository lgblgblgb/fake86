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

#ifndef FAKE86_DISK_H_INCLUDED
#define FAKE86_DISK_H_INCLUDED

#include <stdint.h>
#include "hostfs.h"

struct struct_drive {
	HOSTFS_FILE	*diskfile;
	size_t		filesize;
	uint16_t	cyls;
	uint16_t	sects;
	uint16_t	heads;
	uint8_t		inserted;
	uint8_t		readonly;
	char 		*filename;
};

extern struct struct_drive disk[256];

extern uint8_t bootdrive, hdcount, fdcount;

extern uint8_t	insertdisk  ( uint8_t drivenum, const char *filename );
extern void	diskhandler ( void );
extern void	ejectdisk   ( uint8_t drivenum );

extern void	bios_read_boot_sector ( int drive, uint16_t dstseg, uint16_t dstofs );

#endif
