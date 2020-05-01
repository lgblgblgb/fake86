#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bios.h"

#include "cpu.h"
#include "disk.h"

// TODO: remove this
#include <SDL.h>

#define BIOS_TRAP_RESET	0x100


int internalbios = 0;
int internalbiostrapseg = 0xFFFFFF;	// some impossible segment value by default!

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


static void place_trap_vector ( int addr, int trap )
{
	RAM[addr + 0] = (trap * 2) & 0xFF;
	RAM[addr + 1] = (trap * 2) >> 8;
	RAM[addr + 2] = internalbiostrapseg & 0xFF;
	RAM[addr + 3] = internalbiostrapseg >> 8;
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
	// Put something to the CPU reset address
	RAM[0xFFFF0] = 0xEA;	// far jump opcode
	place_trap_vector(0xFFFF1, BIOS_TRAP_RESET);
	// put something "standard" at the end of our fake BIOS
	static const uint8_t bios_signature[] = {
		//31 30 2f-32 38 2f 31 37 00 fe ad
		//1  0  /  2  8  /  1   7    ?  ?
		'1','0','/','2','8','/','1','7',
		0x00, 0xFE, 0xAD
	};
	memcpy(RAM + 0xFFFF5, bios_signature, 11);
	printf("INTERNAL_BIOS: installed, trap_segment = %04Xh\n", internalbiostrapseg);
}



// This is the function will be executed on the RESET vector.
// It will initialize RAM, also reading boot record from disk and executing it.
static void bios_reset ( void )
{
	puts("INTERNAL_BIOS: @CPU_RESET vector");
	memset(RAM, 0, 0x1000);	// clear some part of the main RAM to be sure
	// Install fake interrupt table
	for (int a = 0; a < 0x100; a++)
		place_trap_vector(a * 4, a);
	RAM[0x413] = 640 & 0xFF;	// base RAM size, low byte
	RAM[0x414] = 640 >> 8;		// base RAM size, high byte
	regs.wordregs[regsp] = 0x400;
	// Do not fill CS here, since the trap returns for IRET ...
	segregs[regss] = 0;
	segregs[regds] = 0;
	segregs[reges] = 0;
	regs.wordregs[regdx] = bootdrive;	// DL only, but some BIOSes passes DH=0 too ...
	bios_read_boot_sector(bootdrive, 0, 0x7C00);	// read MBR!
	if (cf) {
		puts("ERROR! System cannot boot (cannot read boot record)!");
		exit(1);
	}
}

static uint16_t return_segment, return_offset, return_flags;


static void int_1a_gettime ( void )
{
	Uint32 ticks = SDL_GetTicks();
	ticks /= 55;
	regs.wordregs[regcx] = ticks >> 16;
	regs.wordregs[regdx] = ticks & 0xFFFF;
	regs.byteregs[regal] = 0;	// midnight stuff!
}


static void bios_putchar ( char c )
{
	static int x = 0, y = 0;
	if (c >= 32) {
		RAM[0xB8000 + (y * 160) + x * 2] = c;
		RAM[0xB8001 + (y * 160) + x * 2] = 7;
		if (x == 79) {
			x = 0;
			y++;
		} else
			x++;
	} else if (c == '\n') {
		x = 0;
		y++;
	}
	if (y == 25) {
		y = 24;
		memmove(RAM + 0xB8000, RAM + 0xB8000 + 160, 80 * 25 * 2);
		memset(RAM + 0xB8000 + 24 * 160, 32, 160);
	}
}


static void bios_putstr ( const char *s )
{
	while (*s)
		bios_putchar(*s++);
}


void bios_internal_trap ( unsigned int trap )
{
	if ((trap & 1) || trap > 0x3FE) {
		fprintf(stderr, "INTERNAL_BIOS: invalid trap offset, your system may crash! (%04Xh)\n", trap);
		return;
	}
	trap >>= 1;
	return_offset = cpu_pop();
	return_segment = cpu_pop();
	return_flags = cpu_pop();
	cf = 0;	// ??? FIXME
	//printf("BIOS_TRAP: %04Xh STACK_RET=%04X:%04X AX=%04Xh\n", trap, return_segment, return_offset, regs.wordregs[regax]);
	switch (trap) {
		case 0x10:		// Interrupt 10h: video services
			switch (regs.byteregs[regah]) {
				case 0x0E:
					bios_putchar(regs.byteregs[regal]);
					break;
				default:
					cf = 1;
					printf("INTERNAL_BIOS: unknown 1Ah interrupt function %02Xh\n", regs.byteregs[regah]);
					break;
			}
			break;
		case 0x12:		// Interrupt 12h: get memory size in Kbytes
			regs.wordregs[regax] = RAM[0x413] + (RAM[0x414] << 8);
			printf("INTERNAL_BIOS: int 12h answer (base RAM size), AX=%d\n", regs.wordregs[regax]);
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
					cf = 1;
					printf("INTERNAL_BIOS: unknown 1Ah interrupt function %02Xh\n", regs.byteregs[regah]);
					break;
			}
			break;
		case BIOS_TRAP_RESET:
			bios_putstr("Fake86 internal BIOS (C)2020 LGB\nWork-in-progress, do not use :)\n\n");
			bios_reset();
			return_segment = 0;
			return_offset = 0x7C00;
			return_flags = 0;
			printf("INTERNAL_BIOS: will return to %04X:%04X\n", return_segment, return_offset);
			//for (int a = 0; a < 0x200; a++)
			//	RAM[0xB8000 + a ] = a;
			break;
		default:
			if (trap < 0x100) {
				printf("INTERNALBIOS: unhandled interrupt %02Xh (AX=%04Xh) at %04X:%04X\n", trap, regs.wordregs[regax], return_segment, return_offset);
				cf = 1;
				regs.byteregs[regah] = 1;	// error code?
			} else {
				fprintf(stderr, "INTERNALBIOS: FATAL: invalid trap number %04Xh (stack frame: %04X:%04X)\n", trap, return_segment, return_offset);
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
