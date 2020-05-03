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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "bios.h"

#include "cpu.h"
#include "disk.h"


#define BIOS_TRAP_RESET	0x100
#define BIOS_TRAP_BASIC	0x101
#define BIOS_TRAP_HALT	0x102


int internalbios = 0;
int internalbiostrapseg = 0xFFFFFF;	// some impossible segment value by default!

static uint16_t return_segment, return_offset, return_flags;

#if defined(CPU_8086)
#	define CPU_TYPE_STR "8086"
#elif defined(CPU_186)
#	define CPU_TYPE_STR "80186"
#elif defined(CPU_V20)
#	define CPU_TYPE_STR "NEC-V20"
#elif defined(CPU_286)
#	define CPU_TYPE_STR "80286"
#elif defined(CPU_386)
#	define CPU_TYPE_STR "80386"
#else
#	define CPU_TYPE_STR "?????"
#endif


#define bios_printf(...)					\
	do {							\
		char _buf_[4096];				\
		snprintf(_buf_, sizeof(_buf_), __VA_ARGS__);	\
		bios_putstr(_buf_);				\
	} while (0)

#define pokeb(a,b) RAM[a]=(b)
#define peekb(a)   RAM[a]
static inline void pokew ( int a, uint16_t w )
{
	pokeb(a, w & 0xFF);
	pokeb(a + 1, w >> 8);
}
static inline uint16_t peekw ( int a )
{
	return peekb(a) + (peekb(a + 1) << 8);
}

static void bios_putchar ( const char c );

static void bios_putstr ( const char *s )
{
	while (*s)
		bios_putchar(*s++);
}

static void place_trap_vector ( int addr, int trap )
{
	RAM[addr + 0] = (trap * 2) & 0xFF;
	RAM[addr + 1] = (trap * 2) >> 8;
	RAM[addr + 2] = internalbiostrapseg & 0xFF;
	RAM[addr + 3] = internalbiostrapseg >> 8;
}

static void place_jmp_to_trap_vector ( int addr, int trap )
{
	RAM[addr] = 0xEA;	// opcode of "far jump"
	place_trap_vector(addr + 1, trap);
}


// This function should NOT touch any RAM! This is for building the *ROM*!
void bios_internal_install ( void )
{
	internalbios = 1;	// make sure to switch it on
	internalbiostrapseg = 0xF000;
	// Build our trap table
	for (int a = 0; a < 0x200; a++) {
		RAM[internalbiostrapseg * 16 + a * 2 + 0] = 0xCC;	// INT3 opcode
		RAM[internalbiostrapseg * 16 + a * 2 + 1] = 0xCF;	// IRET opcode
	}
	// Entry points
	place_jmp_to_trap_vector(0xFFFF0, BIOS_TRAP_RESET);	// CPU reset address will trap to our BIOS init routine
	place_jmp_to_trap_vector(0xF6000, BIOS_TRAP_BASIC);	// Common entry point for ROM BASIc.
	// put something "standard" at the end of our fake BIOS
	static const uint8_t bios_signature[] = {
		//31 30 2f-32 38 2f 31 37 00 fe ad
		//1  0  /  2  8  /  1   7    ?  ?
		'1','0','/','2','8','/','1','7',
		0x00, 0xFE, 0xAD
	};
	memcpy(RAM + 0xFFFF5, bios_signature, 11);
	printf("BIOS: installed, trap_segment = %04Xh\n", internalbiostrapseg);
}



// INT 19h
static void bios_boot_interrupt ( void )
{
	if (!disk[bootdrive].inserted) {
		fprintf(stderr, "BIOS: ERROR! Requested boot drive %02Xh is not inserted!", bootdrive);
		exit(1);
	}
	printf("BIOS: trying to boot from %02Xh ...\n", bootdrive);
	bios_printf("\nBooting from drive %02Xh ... ", bootdrive);
	bios_read_boot_sector(bootdrive, 0, 0x7C00);	// read MBR!
	if (cf) {
		fprintf(stderr, "BIOS: ERROR! System cannot boot (cannot read boot record)! AH=%02Xh\n", regs.byteregs[regah]);
		exit(1);
	}
	bios_putstr("OK.\n\n");
	// Initialize environment needed for the read OS boot loader.
	regs.wordregs[regsp] = 0x400;	// set some value to SP, this seems to be common in BIOSes
	// Do not fill CS here, since the trap returns for IRET ... Just the other segment registers
	segregs[regss] = 0;
	segregs[regds] = 0;
	segregs[reges] = 0;
	regs.wordregs[regdx] = bootdrive;	// DL only, but some BIOSes passes DH=0 too ...
	// Set execution parameters after returning
	return_segment = 0;
	return_offset = 0x7C00;
	return_flags = 0;
}

static int color = 7;

#if 0
static struct biostime {
	time_t	uts;
	Uint32	ticks;
	Uint32	ticks_at_last_read;
	int64_t	time_offset;	// SIGNED, unix time specific!! 0, if hostOS time is the same as emulated (is user does not set time inside the emulator)
	int	secs, mins, hrs, day, mon, year, dst;
	int	since_midnight_secs;
} biostime;


static void get_time ( void )
{
	biostime.uts = time(NULL);
	time_t uts = (time_t)((int64_t)uts + biostime.time_offset);
	struct tm *t = localtime(&uts);
	biostime.secs = t->tm_sec;
	biostime.mins = t->tm_min;
	biostime.hrs  = t->tm_hour;
	biostime.day  = t->tm_mday;
	biostime.mon  = t->tm_mon + 1;
	biostime.year = t->tm_year + 1900;
	biostime.dst  = t->tm_isdst > 0;
	biostime.since_midnight_secs = biostime.hrs * 3600 + biostime.mins * 60 + biostime.secs;
	//biostime.since_midnight_ticks = (int)(biostime.since_midnight_secs * 18.2);
}


static void set_time ( int hour, int min, int sec )
{
	get_time();
	biostime.time_offset += hour * 3600 + min * 60 + sec - biostime.since_midnight_secs;
	biostime.hrs = hour;
	biostime.mins = min;
	biostime.secs = sec;
}


static void set_date ( int year, int month, int day )
{
}


static void update_ticks ( void )
{
	static uint32_t ticks = 0;
	ticks++;
	pokeb(0x46C, (uint8_t)(ticks      ));
	pokeb(0x46D, (uint8_t)(ticks >>  8));
	pokeb(0x46E, (uint8_t)(ticks >> 16));
	pokeb(0x46F, (uint8_t)(ticks >> 24));
	//if (last_tick_read - ticks >= 1572480UL)
}
#endif


// This is the function will be executed on the RESET vector.
// It will initialize RAM, also reading boot record from disk and executing it.
static void bios_reset ( void )
{
	puts("BIOS: cold reset");
	// Set initial video mode
	regs.wordregs[regax] = 0x0003;
	vidinterrupt();
	// Some stupid texts ...
	color = 0x4E;
	bios_putstr("Fake86 internal BIOS (C)2020 LGB G\240bor L\202n\240rt                              [WIP]\n");
	color = 7;
	bios_printf("CPU type : %s\nMemory   : %dK\n", CPU_TYPE_STR, (int)(RAM_SIZE - 0x60000) >> 10);
	for (int a = 0; a < 0x100; a++) {
		if (disk[a].inserted) {
			bios_printf("Drive %02X : %s CHS=%d/%d/%d SIZE=%u%c %s\n",
				a,
				a < 0x80 ? "FDD" : "HDD",
				disk[a].cyls,
				disk[a].heads,
				disk[a].sects,
				(unsigned int)(disk[a].filesize >= 10*1024*1024 ? disk[a].filesize >> 20 : disk[a].filesize >> 10),
				disk[a].filesize >= 10*1024*1024 ? 'M' : 'K',
				disk[a].readonly ? "RO" : "RW"
			);
		}
	}
	memset(RAM, 0, 0x1000);	// clear some part of the main RAM to be sure
	// Install fake interrupt table
	for (int a = 0; a < 0x100; a++)
		place_trap_vector(a * 4, a);
	// Configuration word. This word is returned by INT 11h
	pokew(0x410,
		((fdcount ? 1 : 0) << 0) +	// bit 0: "1" if one of more floppy drives is in the system
		((0) << 1) +			// bit 1: XT not used, AT -> 1 if there is FPU
		((3) << 2) +			// bit 2-3: XT = memory on motherboard (max 64K), AT = not used
		((2) << 4) +			// bit 4-5: video mode on startup: 0=not used, 1=40*25 colour, 2=80*25 colour, 3=80*25 mono
		((fdcount?fdcount-1:0) << 6) +	// bit 6-7: floppy drives in system (!! 0 means ONE, etc)
		((0) << 8) +			// bit 8: XT = zero if there is DMA, AT = not used
		((1) << 9) +			// bit 9-11: number of RS232 ports
		((0) << 12) +			// bit 12: XT 1 if game adapter presents, AT = not used
						// bit 13 is not used
		((0) << 14)			// bit 14-15: number of printers? (probably LPT ports in system)
	);
	pokew(0x413, 640);		// base memory (below 640K!), returned by int12h. ... 640Kbyte ~ "should be enough for everyone" - remember?
	pokeb(0x475, hdcount);		// number of HDDs in the system
	// Set BIOS ticks as number of 18.2/second ticks since midnight
	//get_time();
	//biostime.ticks = biostime.since_midnight_secs * 18.2;
	// *** SYSTEM BOOT ***
	bios_boot_interrupt();
}



static void int_1a_gettime ( void )
{
	Uint32 ticks = SDL_GetTicks();
	ticks /= 55;
	regs.wordregs[regcx] = ticks >> 16;
	regs.wordregs[regdx] = ticks & 0xFFFF;
	regs.byteregs[regal] = 0;	// midnight stuff!
}


#define CURX RAM[0x450 + (bios_video_page << 1)]
#define CURY RAM[0x451 + (bios_video_page << 1)]


static void bios_putchar ( const char c )
{
	static int x = 0, y = 0;
	if ((unsigned)c >= 32) {
		RAM[0xB8000 + (y * 160) + x * 2] = c & 0xFF;
		RAM[0xB8001 + (y * 160) + x * 2] = color;
		if (x == 79) {
			x = 0;
			y++;
		} else
			x++;
	} else if (c == '\n') {
		x = 0;
		y++;
	} else if (c == '\r') {
		x = 0;
	}
	if (y == 25) {
		y = 24;
		memmove(RAM + 0xB8000, RAM + 0xB8000 + 160, 80 * 25 * 2);
		memset(RAM + 0xB8000 + 24 * 160, 32, 160);
	}
}






void bios_internal_trap ( unsigned int trap )
{
	if ((trap & 1) || trap > 0x3FE) {
		fprintf(stderr, "BIOS: invalid trap offset, your system may crash! (%04Xh)\n", trap);
		return;
	}
	trap >>= 1;
	return_offset = cpu_pop();
	return_segment = cpu_pop();
	return_flags = cpu_pop();
	if (trap < 0x100)
		cf = 0;	// by default we set carry flag to zero. INT handlers may set it '1' in case of error!
	//printf("BIOS_TRAP: %04Xh STACK_RET=%04X:%04X AX=%04Xh\n", trap, return_segment, return_offset, regs.wordregs[regax]);
	switch (trap) {
		case 0x00:
			bios_putstr("Division by zero.\n");
			cf = 1;
			return;
		case 0x10:		// Interrupt 10h: video services
			switch (regs.byteregs[regah]) {
				case 0x0E:
					bios_putchar(regs.byteregs[regal]);
					break;
				default:
					printf("BIOS: unknown 1Ah interrupt function %02Xh\n", regs.byteregs[regah]);
					cf = 1;
					regs.byteregs[regah] = 1;
					break;
			}
			break;
		case 0x11:		// Interrupt 11h: get system configuration
			regs.wordregs[regax] = peekw(0x410);
			break;
		case 0x12:		// Interrupt 12h: get memory size in Kbytes
			//regs.wordregs[regax] = RAM[0x413] + (RAM[0x414] << 8);
			regs.wordregs[regax] = peekw(0x413);
			//printf("BIOS: int 12h answer (base RAM size), AX=%d\n", regs.wordregs[regax]);
			break;
		case 0x13:		// Interrupt 13h: disk services
			diskhandler();
			break;
		case 0x1A:		// Interrupt 1Ah: time services
			switch (regs.byteregs[regah]) {
				case 0x00:	// get 18.2 ticks/sec since midnight and day change flag
					int_1a_gettime();
					break;
				default:
					printf("BIOS: unknown 1Ah interrupt function %02Xh\n", regs.byteregs[regah]);
					cf = 1;
					regs.byteregs[regah] = 1;
					break;
			}
			break;
		case BIOS_TRAP_RESET:
			bios_reset();
			//return_segment = 0;
			//return_offset = 0x7C00;
			//return_flags = 0;
			//printf("BIOS: will return to %04X:%04X\n", return_segment, return_offset);
			//for (int a = 0; a < 0x200; a++)
			//	RAM[0xB8000 + a ] = a;
			break;
		case 0x18:		// ROM BASIC interrupt :)
		case BIOS_TRAP_BASIC:	// the entry point of ROM basic.
			bios_putstr("No ROM-BASIC. System halted.\n");
			return_offset -= 1;
			break;
		case BIOS_TRAP_HALT:
			return_offset -= 1;
			break;
		default:
			if (trap < 0x100) {
				printf("BIOS: unhandled interrupt %02Xh (AX=%04Xh) at %04X:%04X\n", trap, regs.wordregs[regax], return_segment, return_offset);
				cf = 1;
				regs.byteregs[regah] = 1;	// error code?
			} else {
				fprintf(stderr, "BIOS: FATAL: invalid trap number %04Xh (stack frame: %04X:%04X)\n", trap, return_segment, return_offset);
				exit(1);
			}
			break;
	}
	// the next opcode after the "TRAP"-ing 0xCC will be IRET.
	// That's why we pop'ed at the beginning and pushing back now.
	// So every functions between at C level can modify return parameters this way.
	return_flags = (return_flags & 0xFFFE) | cf;  // modified CF, so after IRET, modification of "cf" in our fake-BIOS is preserved!
	cpu_push(return_flags);
	cpu_push(return_segment);
	cpu_push(return_offset);
}


// This is the crude part of cpu.c from original Fake86
// It seems most of this work is because some incompleteness
// in hw emulation level the used BIOS cannot handle well
// with integration of Fake86, thus the need to do extra
// things here. TODO: eliminate ALL of this!
#if 0
int bios_fake86_intcall86_dispatch ( uint8_t intnum )
{
	if (internalbios)
		return 0;	// not handled here
	static uint16_t lastint10ax;
	uint16_t oldregax;
	didintr = 1;

	if (intnum == 0x19)
		didbootstrap = 1;

	switch (intnum) {
	case 0x10:
		updatedscreen = 1;
		/*if (regs.byteregs[regah]!=0x0E) {
			printf("Int 10h AX = %04X\n", regs.wordregs[regax]);
		}*/
		if ((regs.byteregs[regah] == 0x00) ||
		    (regs.byteregs[regah] == 0x10)) {
			oldregax = regs.wordregs[regax];
			vidinterrupt();
			regs.wordregs[regax] = oldregax;
			if (regs.byteregs[regah] == 0x10)
				return 1;
			if (vidmode == 9)
				return 1;
		}
		if ((regs.byteregs[regah] == 0x1A) &&
		    (lastint10ax !=
		     0x0100)) { // the 0x0100 is a cheap hack to make it not do
				// this if DOS EDIT/QBASIC
			regs.byteregs[regal] = 0x1A;
			regs.byteregs[regbl] = 0x8;
			return 1;
		}
		lastint10ax = regs.wordregs[regax];
		if (regs.byteregs[regah] == 0x1B) {
			regs.byteregs[regal] = 0x1B;
			segregs[reges] = 0xC800;
			regs.wordregs[regdi] = 0x0000;
			writew86(0xC8000, 0x0000);
			writew86(0xC8002, 0xC900);
			write86(0xC9000, 0x00);
			write86(0xC9001, 0x00);
			write86(0xC9002, 0x01);
			return 1;
		}
		break;

#ifndef DISK_CONTROLLER_ATA
	case 0x19: // bootstrap
#ifdef BENCHMARK_BIOS
		running = 0;
#endif
		if (bootdrive < 255) { // read first sector of boot drive into
				       // 07C0:0000 and execute it
			regs.byteregs[regdl] = bootdrive;
			bios_read_boot_sector(bootdrive, 0, 0x7C00);
			if (cf) {
				fprintf(stderr, "BOOT: cannot read boot record of drive %02X! Trying ROM basic instead!\n", bootdrive);
				cpu_set_cs_ip(0xF600, 0);
			} else {
				cpu_set_cs_ip(0x0000, 0x7C00);
				puts("BOOT: executing boot record now");
			}
		} else {
			// start ROM BASIC at bootstrap if requested
			cpu_set_cs_ip(0xF600, 0x0000);
		}
		return 1;

	case 0x13:
	case 0xFD:
		diskhandler();
		return 1;
#endif
#ifdef NETWORKING_OLDCARD
	case 0xFC:
#ifdef NETWORKING_ENABLED
		nethandler();
#endif
		return 1;
#endif
	}
	return 0;	// not handled here
}
#endif
