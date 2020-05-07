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
#include "sermouse.h"
#include "ports.h"
// FIXME we don't need this:
#include "video.h"


#define BIOS_TRAP_EMUGW	0x100
#define BIOS_TRAP_RESET	0x101
#define BIOS_TRAP_BASIC	0x102
#define BIOS_TRAP_HALT	0x103


int internalbios = 0;

#define INTERNAL_BIOS_TRAP_SEG 0xF000

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
	pokew(addr, trap);			// offset
	pokew(addr + 2, INTERNAL_BIOS_TRAP_SEG);	// segment
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
	// Build our trap table
	for (int a = 0; a < 0x200; a++) {
		RAM[INTERNAL_BIOS_TRAP_SEG * 16 + a] = 0xF4;	// opcode of HLT
	}
	RAM[INTERNAL_BIOS_TRAP_SEG * 16 + 0x1FF] = 0xCF;		// an IRET to be used for various purposes (@ trapseg:0x1FF)
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
	printf("BIOS: installed, trap_segment = %04Xh\n", INTERNAL_BIOS_TRAP_SEG);
}


static int do_not_IRET;


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
	if (CPU_FL_CF) {
		fprintf(stderr, "BIOS: ERROR! System cannot boot (cannot read boot record)! AH=%02Xh\n", CPU_AH);
		exit(1);
	}
	bios_putstr("OK.\n\n");
	// Initialize environment needed for the read OS boot loader.
	CPU_SP = 0x400;		// set some value to SP, this seems to be common in BIOSes
	CPU_CS = 0;
	CPU_IP = 0x7C00;	// 0000:7C000 where we loaded boot record and what BIOSes do in general
	CPU_SS = 0;
	CPU_DS = 0;
	CPU_ES = 0;
	CPU_DX = bootdrive;	// DL only, but some BIOSes passes DH=0 too ...
	decodeflagsword(0);	// clear all flags just to be safe
	do_not_IRET = 1;
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
	CPU_AX = 0x0003;
	vidinterrupt();
	// Some stupid texts ...
	color = 0x4E;
	bios_putstr("Fake86 internal BIOS (C)2020 LGB G\240bor L\202n\240rt                              [WIP]\n");
	color = 7;
	bios_printf("CPU type : %s\nMemory   : %dK\nCOM1     : %Xh (mouse)\n", CPU_TYPE_STR, (int)(RAM_SIZE - 0x60000) >> 10, sermouse.baseport);
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
	memset(RAM, 0, 0x500);	// clear some part of the main RAM to be sure
	// Install fake interrupt table
	for (int a = 0; a < 0x100; a++)
		place_trap_vector(a * 4, a);
	// Override some of the vector table though
	pokew(0x1C * 4, 0x1FF);			// override, this interrupt points to an IRET
	// Configuration word. This word is returned by INT 11h
	pokew(0x410,
		((fdcount ? 1 : 0) << 0) +	// bit 0: "1" if one of more floppy drives is in the system
		((0) << 1) +			// bit 1: XT not used, AT -> 1 if there is FPU
		((0) << 2) +			// bit 2-3: XT = memory on motherboard (max 64K), AT = not used
		((0) << 4) +			// bit 4-5: video mode on startup: 0=VGA/EGA, 1=40*25 colour, 2=80*25 colour, 3=80*25 mono
		((fdcount?fdcount-1:0) << 6) +	// bit 6-7: floppy drives in system (!! 0 means ONE, etc)
		((0) << 8) +			// bit 8: XT = zero if there is DMA, AT = not used
		((1) << 9) +			// bit 9-11: number of RS232 ports
		((1) << 12) +			// bit 12: XT 1 if game adapter presents, AT = not used
						// bit 13 is not used
		((1) << 14)			// bit 14-15: number of printers? (probably LPT ports in system)
	);
	printf("BIOS: system configuration word: %04Xh\n", peekw(0x410));
	//pokew(0x488, 3);	// video mode WHAT IT SHOULD BE :-O
	printf("BIOS: installing COM1 on I/O port %Xh\n", sermouse.baseport);
	pokeb(0x412, 1);		// POST interrupt flag? no idea, but it seems some BIOS at least sets this to one
	pokeb(0x440, 0x26);		// floppy timeout, well just to have similar value here as usual BIOS does
	pokeb(0x449, 3);		// active video mode setting?
	pokew(0x44C, 0x1000);		// size of active video in page bytes ...
	pokew(0x400, sermouse.baseport); // COM1 (first COM port) base I/O address
	pokew(0x408, 0x3BC);		// I/O base for LPT ... we don't need this, but maybe some software goes crazy trying to interpet otherwise zero here as a valid port and output there ...
	pokew(0x413, 640);		// base memory (below 640K!), returned by int12h. ... 640Kbyte ~ "should be enough for everyone" - remember?
	pokeb(0x475, hdcount);		// number of HDDs in the system
	pokew(0x44A, 80);		// WORD: Number of textcolumns per row for the active video mode
	pokeb(0x484, 24);		// BYTE: Number of video rows - 1
	pokew(0x463, 0x3D4);		// WORD: I/O port of video display adapter, _OR_ the segment address of video RAM?!?!??!?!
	pokeb(0x465, 0x29);		// Video display adapter internal mode register
	pokeb(0x466, 0x30);		// colour palette??
	pokew(0x467, 3);		// adapter ROM offset
	pokew(0x469, 0xC000);		// adapter ROM segment
	pokew(0x4A8, 0x00A1);		// video parameter block offset
	pokew(0x4AA, 0xC000);		// video parameter block segment
	pokew(0x472, 0x1234);		// POST soft reset flag?
	// Init keyboard buffer vars
	pokew(0x41A, 0x1E);
	pokew(0x41C, 0x1E);
	pokew(0x480, 0x1E);
	pokew(0x482, 0x1E + 0x20);
	// FIXME: we must configure the interrupt controller to generate the priodic IRQs. Then we must write the IRQ handler (also does keyboard queue fill based on kbd ports)
	static const struct { uint16_t port; uint8_t data; } bios_port_init_seq[] = {
		{ 0xA0, 0x00 },		//{IO: unknown port OUT to 00A0h with value 00h
		//{ 0x3D8, 0x00 },
		//{ 0x3B8, 0x01 },
		{ 0x63, 0x99 },		// {IO: unknown port OUT to 0063h with value 99h
		{ 0x61, 0xA5 },
		{ 0x43, 0x54 },
		{ 0x41, 0x12 },
		{ 0x43, 0x40 },
		{ 0x81, 0x00 },
		{ 0x82, 0x00 },
		{ 0x83, 0x00 },
		{ 0xD, 0x00 },
		{ 0xB, 0x58 },
		{ 0xB, 0x41 },
		{ 0xB, 0x42 },
		{ 0xB, 0x43 },
		{ 0x1, 0xFF },
		{ 0x1, 0xFF },
		{ 0x8, 0x00 },
		{ 0xA, 0x00 },
		{ 0x43, 0x36 },
		{ 0x40, 0x00 },
		{ 0x40, 0x00 },
		//{ 0x213, 0x01 },	// {IO: unknown port OUT to 0213h with value 01h
		{ 0x20, 0x13 },
		{ 0x21, 0x08 },
		{ 0x21, 0x09 },
		{ 0x21, 0xFF },
		//{IO: reading BYTE port 61h
		{ 0x61, 0xB5 },
		{ 0x61, 0x85 },
		{ 0xA0, 0x80 },		// {IO: unknown port OUT to 00A0h with value 80h
		//{IO: reading BYTE port 62h
		{ 0x61, 0xAD },
		//{IO: reading BYTE port 62h
		{ 0x61, 0x08 },
		{ 0x61, 0xC8 },
		{ 0x61, 0x48 },
		//{ 0x3BC, 0xAA },
		{ 0xC0, 0xFF },		// {IO: unknown port OUT to 00C0h with value FFh
		// {IO: reading BYTE port 3BCh
		//{ 0x378, 0xAA },	// {IO: unknown port OUT to 0378h with value AAh
		{ 0xC0, 0xFF },		// {IO: unknown port OUT to 00C0h with value FFh
		//{IO: reading BYTE port 378h
		//{IO: unknown port IN to 0378h
		//{ 0x278, 0xAA },	// {IO: unknown port OUT to 0278h with value AAh
		{ 0xC0, 0xFF },		// {IO: unknown port OUT to 00C0h with value FFh
		//{IO: reading BYTE port 278h
		//{IO: unknown port IN to 0278h
		//{ 0x3FB, 0x1A },
		{ 0xC0, 0xFF },		// {IO: unknown port OUT to 00C0h with value FFh
		//{IO: reading BYTE port 3FBh
		//{ 0x2FB, 0x1A },	// {IO: unknown port OUT to 02FBh with value 1Ah
		{ 0xC0, 0xFF },		// {IO: unknown port OUT to 00C0h with value FFh
		//{IO: reading BYTE port 2FBh
		// NEW	!! this seems to achive the goal to generate our periodic intterupts :)
		{ 0x61, 0x44 },
		{ 0x21, 0xBC },
		{ 0x20, 0x20 },
		{ 0xFFFF, 0xFF }
	};
	//// //Init interrupt controller:
	//mov     al, 13h
	//out     20h, al
	//mov     al, 8
	//out     21h, al
	//mov     al, 9
	//out     21h, al
	//mov     al, 0FFh
	//out     21h, al
	//// Timer ...
	//mov     al, 00110110b                   ; Set up 8253 timer chip [value=0x36]
	//out     43h, al                         ;   chan 0 is time of day
	//mov     al, 0                           ; Request a divide by
	//out     40h, al                         ;   65536 decimal
	//out     40h, al                         ;   0000h or 18.2 tick/sec

	for (int a = 0; bios_port_init_seq[a].port != 0xFFFF; a++)
		portout(bios_port_init_seq[a].port, bios_port_init_seq[a].data);
#if 0
	// Alternative short version (DOES NOT WORK):
	portout(0x20, 0x13);
	portout(0x21, 0x08);
	portout(0x21, 0x09);
	portout(0x21, 0xFF);
	portout(0x43, 0x36);
	portout(0x40, 0x00);
	portout(0x40, 0x00);
	portout(0x20, 0x20);
#endif
	// Set BIOS ticks as number of 18.2/second ticks since midnight
	//get_time();
	//biostime.ticks = biostime.since_midnight_secs * 18.2;
	// *** SYSTEM BOOT ***
	printf("BIOS: END OF INIT\n");
	bios_boot_interrupt();
}




// FIXME: should be moved bios putchar etc routines to use the BDAT (bios data) area
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
	} else if (c == 8 && x > 0) {
		x--;
		RAM[0xB8000 + (y * 160) + x * 2] = 32;
	}
	if (y == 25) {
		y = 24;
		memmove(RAM + 0xB8000, RAM + 0xB8000 + 160, 80 * 25 * 2);
		for (int a = 0; a < 80; a++) {
			RAM[0xB8000 + 24 * 160 + a * 2 + 0] = 32;
			RAM[0xB8000 + 24 * 160 + a * 2 + 1] = color;
		}
	}
	cursx = x;
	cursy = y;
}


// This is the IRQ handler for the periodic interrupt of BIOS
static void bios_irq0_handler ( void )
{
	// Increment the BIOS counter.
	uint32_t cnt = (peekw(0x46C) + (peekw(0x46E) << 16)) + 1;
	pokew(0x46C, cnt & 0xFFFF);
	pokew(0x46E, cnt >> 16);
	//puts("BIOS: IRQ0!");
	portout(0x20, 0x20);	// send end-of-interrupt command to the interrupt controller
}




static int kbd_push_buffer ( uint16_t data )
{
	uint16_t buftail = peekw(0x41C);
	uint16_t buftail_adv = buftail + 2;
	if (buftail_adv == peekw(0x482))
		buftail_adv = peekw(0x480);
	if (buftail_adv == peekw(0x41A))
		return 1;		// buffer is full!
	pokew(0x400 + buftail, data);
	pokew(0x41C, buftail_adv);
	return 0;
}


static uint16_t kbd_get_buffer ( int to_remove )
{
	uint16_t bufhead = peekw(0x41A);
	if (bufhead == peekw(0x41C))
		return 0;	// no character is available in the buffer
	uint16_t data = peekw(0x400 + bufhead);
	if (!to_remove)
		return data;
	bufhead += 2;
	if (bufhead == peekw(0x482))
		pokew(0x41A, peekw(0x480));
	else
		pokew(0x41A, bufhead);
	return data;
}


static void kbd_set_mod0 ( int mask, int scan )
{
	if ((scan & 0x80))
		RAM[0x417] &= ~mask;
	else
		RAM[0x417] |= mask;
}


static const uint8_t scan2ascii[] = {
	//0    1     2      3     4     5     6     7     8    9     A      B     C    D     E     F
	0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09, 
	0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x5B, 0x5D, 0x0D, 0x00, 0x61, 0x73, 
	0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x7A, 0x78, 0x63, 0x76, 
	0x62, 0x6E, 0x6D, 0x2C, 0x2E, 0x2F, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x7F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 


	0x00, 0x37, 0x2E, 0x20, 0x2F, 0x30, 0x31, 0x21, 0x32, 0x33, 0x34, 0x35, 0x22, 0x36, 0x38, 0x3E,
	0x11, 0x17, 0x05, 0x12,	0x14, 0x19, 0x15, 0x09,	0x0F, 0x10, 0x39, 0x3A,	0x3B, 0x84, 0x61, 0x13,
	0x04, 0x06, 0x07, 0x08,	0x0A, 0x0B, 0x0C, 0x3F,	0x40, 0x41, 0x82, 0x3C,	0x1A, 0x18, 0x03, 0x16,
	0x02, 0x0E, 0x0D, 0x42,	0x43, 0x44, 0x81, 0x3D,	0x88, 0x2D, 0xC0, 0x23,	0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B,	0x2C, 0xA0, 0x90
#if 0
	0x00, 0x37, 0x2E, 0x20, 0x2F, 0x30, 0x31, 0x21, 0x32, 0x33, 0x34, 0x35, 0x22, 0x36, 0x38, 0x3E,
	0x11, 0x17, 0x05, 0x12,	0x14, 0x19, 0x15, 0x09,	0x0F, 0x10, 0x39, 0x3A,	0x3B, 0x84, 0x01, 0x13,
	0x04, 0x06, 0x07, 0x08,	0x0A, 0x0B, 0x0C, 0x3F,	0x40, 0x41, 0x82, 0x3C,	0x1A, 0x18, 0x03, 0x16,
	0x02, 0x0E, 0x0D, 0x42,	0x43, 0x44, 0x81, 0x3D,	0x88, 0x2D, 0xC0, 0x23,	0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B,	0x2C, 0xA0, 0x90
#endif
};



// This is the IRQ handler for the keyboard interrupt of BIOS
static void bios_irq1_handler ( void )
{
	puts("BIOS: IRQ1!");
	// keyboard
//	if ((portram[0x64] & 2)) {	// is input buffer full
		//uint8_t scan = portram[0x60];	// read the scancode
		uint8_t scan = portin(0x60);
		uint8_t ctrlst = portin(0x61);
		portout(0x61, ctrlst | 0x80);
		portout(0x61, ctrlst);
		//portram[0x64] &= ~2;		// empty the buffer
		printf("BIOS: scan got: %Xh\n", scan);
		switch (scan & 0x7F) {
			case 0x36: kbd_set_mod0(0x01, scan); break;	// rshift
			case 0x2A: kbd_set_mod0(0x02, scan); break;	// lshift
			case 0x1D: kbd_set_mod0(0x04, scan); break;	// ctrl
			case 0x38: kbd_set_mod0(0x08, scan); break;	// alt
			case 0x46: kbd_set_mod0(0x10, scan); break;	// scroll lock
			case 0x45: kbd_set_mod0(0x20, scan); break;	// num lock
			case 0x3A: kbd_set_mod0(0x40, scan); break;	// caps lock
			case 0x52: kbd_set_mod0(0x80, scan); break;	// ins
			default:
				if (scan < 0x80) {
					uint8_t ascii;
					if (scan <= sizeof(scan2ascii))
						ascii = scan2ascii[scan];
					else
						ascii = 0;
					if ((RAM[0x417] & 3)) {
						if (ascii == ';')
							ascii = ':';
					}
					kbd_push_buffer(ascii | (scan << 8));
				}
				break;
		}
//	}
	portout(0x20, 0x20);	// send end-of-interrupt command to the interrupt controller
}





static void bios_internal_trap ( unsigned int trap )
{
	int do_override_some_flags = 1;
	// get return CS:IP from stack WITHOUT POP'ing them!
	// this are used only for debug purposes! [know the top element of our stack frame @ x86 level]
	uint16_t stack_ip = peekw(CPU_SS * 16 + CPU_SP);
	uint16_t stack_cs = peekw(CPU_SS * 16 + ((CPU_SP + 2) & 0xFFFF));
	do_not_IRET = 0;
	if (trap < 0x100)
		CPU_FL_CF = 0;	// by default we set carry flag to zero. INT handlers may set it '1' in case of error!
	//printf("BIOS_TRAP: %04Xh STACK_RET=%04X:%04X AX=%04Xh\n", trap, return_segment, return_offset, CPU_AX);
	switch (trap) {
		case 0x00:
			bios_putstr("Division by zero.\n");
			do_override_some_flags = 0;
			break;
		case 0x08:
			bios_irq0_handler();
			do_override_some_flags = 0;
			break;
		case 0x09:
			bios_irq1_handler();
			do_override_some_flags = 0;
			break;
		case 0x10:		// Interrupt 10h: video services
			// FIXME! Some time video.c (vidinterrupt function) and these things must be unified here!
			// the problem: in non-internal BIOS mode, Fake86 uses ugly hacks in cpu.c involving vidinterrupt() directly.
			switch (CPU_AH) {
				case 0x0E:
					bios_putchar(CPU_AL);
					break;
				default:
					vidinterrupt();
					//printf("BIOS: unknown 10h interrupt function %02Xh\n", CPU_AH);
					//CPU_FL_CF = 1;
					//CPU_AH = 1;
					break;
			}
			break;
		case 0x11:		// Interrupt 11h: get system configuration
			CPU_AX = peekw(0x410);
			break;
		case 0x12:		// Interrupt 12h: get memory size in Kbytes
			//CPU_AX = RAM[0x413] + (RAM[0x414] << 8);
			CPU_AX = peekw(0x413);
			//printf("BIOS: int 12h answer (base RAM size), AX=%d\n", CPU_AX);
			break;
		case 0x13:		// Interrupt 13h: disk services
			diskhandler();
			break;
		case 0x14:		// Interrupt 14h: serial stuffs
			switch (CPU_AH) {
				case 0x00:	// Serial - initialize port
					printf("BIOS: int 14h serial initialization request for port %u\n", CPU_DX);
					if (CPU_DX == 0) {
						CPU_AH = 64 + 32;	// tx buffer and shift reg is empty
						CPU_AL = 0;
					} else {
						CPU_AH = 128;	// timeout
						CPU_AL = 0;
					}
					break;
				default:
					printf("BIOS: unknown 14h interrupt function %02Xh\n", CPU_AH);
					CPU_FL_CF = 1;
					CPU_AH = 1;
					break;
			}
			break;
		case 0x15:
			CPU_FL_CF = 1;
			CPU_AH = 0x86;
			printf("BIOS: unknown 15h AT ALL interrupt function, AX=%04Xh\n", CPU_AX);
			break;
		case 0x16:		// Interrupt 16h: keyboard functions TODO+FIXME !
			switch (CPU_AH) {
				case 0:	// get keypress, with waiting.
					// output: AL=0 -> extended code got, AH=the extended code
					//         AL!=0 -> AL=ASCII code, AH=scancode
					//CPU_AX = bios_getkey(1);
					CPU_AX = kbd_get_buffer(1);
					if (!CPU_AX) {
						// FIXME: fake!!!!
						//CPU_AL='A';
						//CPU_AH='A';
						//break;
						// if no key, re-execute the trap!
						CPU_CS = INTERNAL_BIOS_TRAP_SEG;
						CPU_IP = 0x16;
						do_not_IRET = 1;
						puts("Waiting for keypress!");
					}
					break;
				case 1:	// check kbd buffer (without waiting!)
					// output: ZF=1: no char
					// 	   ZF=0: there is char (AL/AH like with function 0)
					//CPU_AX = bios_getkey(0);
					CPU_AX = kbd_get_buffer(0);
					if (CPU_AX)
						CPU_FL_ZF = 0;
					else
						CPU_FL_ZF = 1;
					break;
				case 2:	// get kbd status
					CPU_AL = RAM[0x417];	// shift, etc status
					break;
				case 5:				// PUSH into kbd buffer by user call!!
					CPU_AL = kbd_push_buffer(CPU_CX);
					break;
				default:
					printf("BIOS: unknown 16h interrupt function %02Xh\n", CPU_AH);
					CPU_FL_CF = 1;
					CPU_AH = 1;
					break;
			}
			break;
		case 0x1A:		// Interrupt 1Ah: time services
			switch (CPU_AH) {
				case 0x00:	// get 18.2 ticks/sec since midnight and day change flag
					CPU_DX = peekw(0x46C);
					CPU_CX = peekw(0x46E);
					CPU_AL = peekb(0x470);
					break;
				case 0x01:
					pokew(0x46C, CPU_DX);
					pokew(0x46E, CPU_CX);
					break;
				case 0x02:	// read real-time clock
					{
					time_t uts = time(NULL);
					struct tm *t = localtime(&uts);
					CPU_DH = t->tm_sec;
					CPU_CL = t->tm_min;
					CPU_CH = t->tm_hour;
					CPU_DL  = t->tm_isdst > 0;
					printf("BIOS: RTC time requested, answer: %02u:%02u:%02u DST=%u\n", CPU_CH, CPU_CL, CPU_DH, CPU_DL);
					}
					CPU_FL_CF = 0;
					break;
				case 0x04:	// read real-time clock's date
					{
					time_t uts = time(NULL);
					struct tm *t = localtime(&uts);
					CPU_DL = t->tm_mday;
					CPU_DH = t->tm_mon + 1;
					CPU_CL = t->tm_year % 100;
					CPU_CH = t->tm_year / 100 + 19;
					printf("BIOS: RTC date requested, answer: %02u%02u.%02u.%02u\n", CPU_CH, CPU_CL, CPU_DH, CPU_DL);
					}
					CPU_FL_CF = 0;
					break;
				default:
					printf("BIOS: unknown 1Ah interrupt function %02Xh\n", CPU_AH);
					CPU_FL_CF = 1;
					CPU_AH = 1;
					break;
			}
			break;
		case 0xE6:	// "Filesystem server" invented by Mach, allow to map host FS to DOS drives!
			switch (CPU_AH) {
				case 0x00:	// Installation check
					CPU_AX = 0xAA55;	// magic number to return
					CPU_BX = 0x0101;	// high/low byte: major/minor version number
					CPU_CX = 0x0001;	// patch level
					break;
				case 0x01:	// Register dump
					puts("HOSTFS: register dump: TODO");
					break;
				case 0xFF:
					puts("HOSTFS: requested terminate");
					running = 0;
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
			do_not_IRET = 1;
			CPU_CS = INTERNAL_BIOS_TRAP_SEG;
			CPU_IP = BIOS_TRAP_HALT;
			break;
		case BIOS_TRAP_HALT:
			do_not_IRET = 1;
			CPU_CS = INTERNAL_BIOS_TRAP_SEG;
			CPU_IP = BIOS_TRAP_HALT;
			break;
		case BIOS_TRAP_EMUGW:
			// emulation gateway functionality can be used by special tools running inside Fake86
			do_not_IRET = 1;
			// do our FAR-RET here instead!
			CPU_IP = cpu_pop();
			CPU_CS = cpu_pop();
			break;
		default:
			if (trap < 0x100) {
				printf("BIOS: unhandled interrupt %02Xh (AX=%04Xh) at %04X:%04X\n", trap, CPU_AX, stack_cs, stack_ip);
				CPU_FL_CF = 1;
				CPU_AH = 1;	// error code?
			} else {
				fprintf(stderr, "BIOS: FATAL: invalid trap number %04Xh (stack frame: %04X:%04X)\n", trap, stack_cs, stack_ip);
				exit(1);
			}
			break;
	}
	if (!do_not_IRET) {
		int zf_to_set = CPU_FL_ZF;
		int cf_to_set = CPU_FL_CF;
		// Simulate an IRET by our own
		CPU_IP = cpu_pop();
		CPU_CS = cpu_pop();
		decodeflagsword(cpu_pop());
		// Override some flags, if needed
		if (do_override_some_flags) {
			CPU_FL_ZF = zf_to_set;
			CPU_FL_CF = cf_to_set;
		}
	} else {
		printf("BIOS: returning from trap without IRET (trap=%Xh)!\n", trap);
	}
}


int cpu_hlt_handler ( void )
{
	if (!internalbios || CPU_CS != INTERNAL_BIOS_TRAP_SEG || cpu.saveip >= 0x1FF) {
		puts("BIOS: critical warning, HLT outside of trap area?!");
		return 1;	// Yes, it was really a halt, since it does not fit into our trap area
	}
	bios_internal_trap(cpu.saveip);
	return 0;	// no, it wasn't a HLT, it's our trap!
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
		/*if (CPU_AH!=0x0E) {
			printf("Int 10h AX = %04X\n", CPU_AX);
		}*/
		if ((CPU_AH == 0x00) ||
		    (CPU_AH == 0x10)) {
			oldregax = CPU_AX;
			vidinterrupt();
			CPU_AX = oldregax;
			if (CPU_AH == 0x10)
				return 1;
			if (vidmode == 9)
				return 1;
		}
		if ((CPU_AH == 0x1A) &&
		    (lastint10ax !=
		     0x0100)) { // the 0x0100 is a cheap hack to make it not do
				// this if DOS EDIT/QBASIC
			CPU_AL = 0x1A;
			CPU_BL = 0x8;
			return 1;
		}
		lastint10ax = CPU_AX;
		if (CPU_AH == 0x1B) {
			CPU_AL = 0x1B;
			CPU_ES = 0xC800;
			CPU_DI = 0x0000;
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
			CPU_DL = bootdrive;
			bios_read_boot_sector(bootdrive, 0, 0x7C00);
			if (CPU_FL_CF) {
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
