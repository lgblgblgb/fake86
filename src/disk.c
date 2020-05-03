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

/* disk.c: disk emulation routines for Fake86. works at the BIOS interrupt 13h level. */

#include <stdint.h>
#include <stdio.h>

#include "disk.h"

#include "cpu.h"
#include "hostfs.h"


struct struct_drive disk[256];

uint8_t bootdrive = 0, hdcount = 0, fdcount = 0;


static uint8_t sectorbuffer[512];


uint8_t insertdisk ( uint8_t drivenum, char *filename )
{
	const char *err = "?";
	HOSTFS_FILE *file = hostfs_open(filename, "?r+b");	// ? -> signal hostfs to use fallback mode "rb" (read-only) if the given mode (r/w here "r+b") fails
	if (!file) {
		err = SDL_GetError();
		goto error;
	}
	size_t size = hostfs_size(file);
	if (size < 0) {
		err = SDL_GetError();
		goto error;
	}
	if (size < 360*1024) {
		err = "Disk image is too small!";
		goto error;
	}
	if (size > 0x1f782000UL) {
		err = "Disk image is too large!";
		goto error;
	}
	if ((size & 511)) {
		err = "Disk image size is not multiple of 512 bytes!";
		goto error;
	}
	uint16_t cyls, heads, sects;
	if (drivenum >= 0x80) { //it's a hard disk image
		sects = 63;
		heads = 16;
		cyls = size / (sects * heads * 512);
	} else {   //it's a floppy image
		cyls = 80;
		sects = 18;
		heads = 2;
		if (size <= 1228800)
			sects = 15;
		if (size <= 737280)
			sects = 9;
		if (size <= 368640) {
			cyls = 40;
			sects = 9;
		}
		if (size <= 163840) {
			cyls = 40;
			sects = 8;
			heads = 1;
		}
	}
	if (cyls > 1023 || cyls * heads * sects * 512 != size) {
		err = "Cannot find some CHS geometry for this disk image file!";
		goto error;
	}
	// Seems to be OK. Let's validate (store params) and print message.
	ejectdisk(drivenum);	// close previous disk image for this drive if there is any
	disk[drivenum].diskfile = file;
	disk[drivenum].filesize = size;
	disk[drivenum].inserted = 1;
	disk[drivenum].readonly = hostfs_was_fallback_mode;
	disk[drivenum].cyls = cyls;
	disk[drivenum].heads = heads;
	disk[drivenum].sects = sects;
	if (drivenum >= 0x80)
		hdcount++;
	else
		fdcount++;
	printf(
		"DISK: Disk 0%02Xh has been attached %s from file %s size=%luK, CHS=%d,%d,%d\n",
		drivenum,
		hostfs_was_fallback_mode ? "R/O" : "R/W",
		filename,
		(unsigned long)(size >> 10),
		cyls,
		heads,
		sects
	);
	return 0;
error:
	if (file)
		hostfs_close(file);
	fprintf(stderr, "DISK: ERROR: cannot insert disk 0%02Xh as %s because: %s\n", drivenum, filename, err);
	return 1;
}


void ejectdisk ( uint8_t drivenum )
{
	if (disk[drivenum].inserted) {
		hostfs_close(disk[drivenum].diskfile);
		disk[drivenum].inserted = 0;
		if (drivenum >= 0x80)
			hdcount--;
		else
			fdcount--;
	}
}


// Call this ONLY if all parameters are valid! There is no check here!
static size_t chs2ofs ( int drivenum, int cyl, int head, int sect )
{
	return (((size_t)cyl * (size_t)disk[drivenum].heads + (size_t)head) * (size_t)disk[drivenum].sects + (size_t)sect - 1) * 512UL;
}


static void bios_readdisk ( uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head, uint16_t sectcount, int is_verify )
{
	if (!disk[drivenum].inserted) {
		regs.byteregs[regah] = 0x31;	// no media in drive
		goto error;
	}
	if (!sect || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;
	}
	//uint32_t lba = ((uint32_t)cyl * (uint32_t)disk[drivenum].heads + (uint32_t)head) * (uint32_t)disk[drivenum].sects + (uint32_t)sect - 1;
	//size_t fileoffset = lba * 512;
	size_t fileoffset = chs2ofs(drivenum, cyl, head, sect);
	if (fileoffset > disk[drivenum].filesize) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;
	}
	if (hostfs_seek_set(disk[drivenum].diskfile, fileoffset) != fileoffset) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;
	}
	uint32_t memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
	// for the readdisk function, we need to use write86 instead of directly fread'ing into
	// the RAM array, so that read-only flags are honored. otherwise, a program could load
	// data from a disk over BIOS or other ROM code that it shouldn't be able to.
	uint32_t cursect;
	for (cursect = 0; cursect < sectcount; cursect++) {
		if (hostfs_read(disk[drivenum].diskfile, sectorbuffer, 512, 1) != 1)
			break;
		if (is_verify) {
			for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
				// FIXME: segment overflow condition?
				if (read86(memdest++) != sectorbuffer[sectoffset]) {
					// sector verify failed!
					regs.byteregs[regal] = cursect;
					cf = 1;
					regs.byteregs[regah] = 0xBB;	// error code?? what we should say in this case????
					return;
				}
			}
		} else {
			for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
				// FIXME: segment overflow condition?
				write86(memdest++, sectorbuffer[sectoffset]);
			}
		}
	}
	if (sectcount && !cursect) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;			// not even one sector could be read?
	}
	regs.byteregs[regal] = cursect;
	cf = 0;
	regs.byteregs[regah] = 0;
	return;
error:
	// AH must be set with the error code
	regs.byteregs[regal] = 0;
	cf = 1;
}


void bios_read_boot_sector ( int drive, uint16_t dstseg, uint16_t dstoff )
{
	bios_readdisk(drive, dstseg, dstoff, 0, 1, 0, 1, 0);
}


static void bios_writedisk ( uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head, uint16_t sectcount )
{
	if (!disk[drivenum].inserted) {
		regs.byteregs[regah] = 0x31;	// no media in drive
		goto error;
	}
	if (!sect || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;
	}
	//uint32_t lba = ((uint32_t)cyl * (uint32_t)disk[drivenum].heads + (uint32_t)head) * (uint32_t)disk[drivenum].sects + (uint32_t)sect - 1;
	//size_t fileoffset = lba * 512;
	size_t fileoffset = chs2ofs(drivenum, cyl, head, sect);
	if (fileoffset > disk[drivenum].filesize) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;
	}
	if (disk[drivenum].readonly) {
		regs.byteregs[regah] = 0x03;	// drive is read-only
		goto error;
	}
	if (hostfs_seek_set(disk[drivenum].diskfile, fileoffset) != fileoffset) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;
	}
	uint32_t memdest = ((uint32_t)dstseg << 4) + (uint32_t)dstoff;
	uint32_t cursect;
	for (cursect = 0; cursect < sectcount; cursect++) {
		for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
			// FIXME: segment overflow condition?
			sectorbuffer[sectoffset] = read86(memdest++);
		}
		if (hostfs_write(disk[drivenum].diskfile, sectorbuffer, 512, 1) != 1)
			break;
	}
	if (sectcount && !cursect) {
		regs.byteregs[regah] = 0x04;	// sector not found
		goto error;			// not even one sector could be written?
	}
	regs.byteregs[regal] = cursect;
	cf = 0;
	regs.byteregs[regah] = 0;
	return;
error:
	// AH must be set with the error code
	regs.byteregs[regal] = 0;
	cf = 1;
}


#if 0
static void formattrack ( uint8_t drivenum, int track )
{
}
#endif


void diskhandler ( void )
{
	static uint8_t lastdiskah[256], lastdiskcf[256];
	//printf("DISK interrupt function %02Xh\n", regs.byteregs[regah]);
	switch (regs.byteregs[regah]) {
		case 0: //reset disk system
			regs.byteregs[regah] = 0;
			cf = 0; //useless function in an emulator. say success and return.
			break;
		case 1: //return last status
			regs.byteregs[regah] = lastdiskah[regs.byteregs[regdl]];
			cf = lastdiskcf[regs.byteregs[regdl]];
			return;
		case 2: //read sector(s) into memory
			bios_readdisk(
				regs.byteregs[regdl],						// drivenum
				segregs[reges], getreg16(regbx),				// segment & offset
				regs.byteregs[regch] + (regs.byteregs[regcl] / 64) * 256,	// cylinder
				regs.byteregs[regcl] & 63,					// sector
				regs.byteregs[regdh],						// head
				regs.byteregs[regal],						// sectcount
				0								// is verify (!=0) or read (==0) operation?
			);
			break;
		case 3: //write sector(s) from memory
			bios_writedisk(
				regs.byteregs[regdl],						// drivenum
				segregs[reges], getreg16(regbx),				// segment & offset
				regs.byteregs[regch] + (regs.byteregs[regcl] / 64) * 256,	// cylinder
				regs.byteregs[regcl] & 63,					// sector
				regs.byteregs[regdh],						// head
				regs.byteregs[regal]						// sectcount
			);
			break;
		case 4:	// verify sectors ...
			bios_readdisk(
				regs.byteregs[regdl],						// drivenum
				segregs[reges], getreg16(regbx),				// segment & offset
				regs.byteregs[regch] + (regs.byteregs[regcl] / 64) * 256,	// cylinder
				regs.byteregs[regcl] & 63,					// sector
				regs.byteregs[regdh],						// head
				regs.byteregs[regal],						// sectcount
				1								// is verify (!=0) or read (==0) operation?
			);
			break;
		case 5: //format track
			// pretend success ...
			// TODO: at least fill area (ie, the whole track) with zeroes or something, pretending the formatting was happened :)
			cf = 0;
			regs.byteregs[regah] = 0;
			break;
		case 8: //get drive parameters
			if (disk[regs.byteregs[regdl]].inserted) {
				cf = 0;
				regs.byteregs[regah] = 0;
				regs.byteregs[regch] = disk[regs.byteregs[regdl]].cyls - 1;
				regs.byteregs[regcl] = disk[regs.byteregs[regdl]].sects & 63;
				regs.byteregs[regcl] = regs.byteregs[regcl] + (disk[regs.byteregs[regdl]].cyls/256) *64;
				regs.byteregs[regdh] = disk[regs.byteregs[regdl]].heads - 1;
				if (regs.byteregs[regdl]<0x80) {
					regs.byteregs[regbl] = 4; //else regs.byteregs[regbl] = 0;
					regs.byteregs[regdl] = 2;
				} else
					regs.byteregs[regdl] = hdcount;
			} else {
				cf = 1;
				regs.byteregs[regah] = 0xAA;
			}
			break;
#if 0
		case 0x15:	// get disk type
			if (disk[regs.byteregs[regdl]].inserted) {
				int drivenum = regs.byteregs[regdl];
				printf("Requesting int 13h / function 15h for drive %02Xh\n", drivenum);
				regs.byteregs[regah] = (drivenum & 0x80) ? 3 : 1;		// either "floppy without change line support" (1) or "harddisk" (3)
				regs.byteregs[regcx] = (disk[drivenum].filesize >> 9) >> 16;	// number of blocks, high word
				regs.byteregs[regdx] = (disk[drivenum].filesize >> 9) & 0xFFFF;	// number of blocks, low word
				regs.byteregs[regal] = regs.byteregs[regah];
				cf = 0;
			} else {
				printf("Requesting int 13h / function 15h for drive %02Xh\n", regs.byteregs[regdl]);
				/*regs.byteregs[regah] = 0;	// no such device
				regs.byteregs[regal] = 0;
				regs.wordregs[regcx] = 0;
				regs.wordregs[regdx] = 0;*/
				regs.wordregs[regax] = 0x0101;
				cf = 1;
			}
			break;
#endif
		default:
			printf("BIOS: unknown Int 13h service was requested: %02Xh\n", regs.byteregs[regah]);
			cf = 1;	// unknown function was requested?
			regs.byteregs[regah] = 1;
			break;
	}
	lastdiskah[regs.byteregs[regdl]] = regs.byteregs[regah];
	lastdiskcf[regs.byteregs[regdl]] = cf;
	if (regs.byteregs[regdl] & 0x80)
		RAM[0x474] = regs.byteregs[regah];
}
