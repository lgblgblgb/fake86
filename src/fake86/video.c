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

/* video.c: many various functions to emulate bits of the video controller.
   a lot of this code is inefficient, and just plain ugly. i plan to rework
   large sections of it soon. */

#include <SDL/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include "mutex.h"
#include "cpu.h"

extern void set_port_write_redirector (uint16_t startport, uint16_t endport, void *callback);
extern void set_port_read_redirector (uint16_t startport, uint16_t endport, void *callback);

extern SDL_Surface *screen;
extern uint8_t verbose;
extern union _bytewordregs_ regs;
extern uint8_t RAM[0x100000], readonly[0x100000];
extern uint8_t portram[0x10000];
extern uint16_t segregs[4];

extern uint8_t read86 (uint32_t addr32);
extern uint8_t write86 (uint32_t addr32, uint8_t value);
extern uint8_t scrmodechange;

uint8_t VRAM[262144], vidmode, cgabg, blankattr, vidgfxmode, vidcolor;
uint16_t cursx, cursy, cols = 80, rows = 25, vgapage, cursorposition, cursorvisible;
uint8_t updatedscreen, clocksafe, port3da, port6, portout16;
uint16_t VGA_SC[0x100], VGA_CRTC[0x100], VGA_ATTR[0x100], VGA_GC[0x100];
uint32_t videobase= 0xB8000, textbase = 0xB8000, x, y;
uint8_t fontcga[32768];
uint32_t palettecga[16], palettevga[256];
uint32_t usefullscreen = 0, usegrabmode = SDL_GRAB_OFF;

uint8_t latchRGB = 0, latchPal = 0, VGA_latch[4], stateDAC = 0;
uint8_t latchReadRGB = 0, latchReadPal = 0;
uint32_t tempRGB;
uint16_t oldw, oldh; //used when restoring screen mode

uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
#ifdef __BIG_ENDIAN__
	return ( (r<<24) | (g<<16) | (b<<8) );
#else
	return (r | (g<<8) | (b<<16) );
#endif
}

extern uint32_t nw, nh;
void vidinterrupt() {
	uint32_t tempcalc, memloc, n;
	updatedscreen = 1;
	switch (regs.byteregs[regah]) { //what video interrupt function?
			case 0: //set video mode
				if (verbose) {
						printf ("Set video mode %02Xh\n", regs.byteregs[regal]);
					}
				VGA_SC[0x4] = 0; //VGA modes are in chained mode by default after a mode switch
				//regs.byteregs[regal] = 3;
				switch (regs.byteregs[regal] & 0x7F) {
						case 0: //40x25 mono text
							videobase = textbase;
							cols = 40;
							rows = 25;
							vidcolor = 0;
							vidgfxmode = 0;
							blankattr = 7;
							for (tempcalc = videobase; tempcalc<videobase+16384; tempcalc+=2) {
									RAM[tempcalc] = 0;
									RAM[tempcalc+1] = blankattr;
								}
							break;
						case 1: //40x25 color text
							videobase = textbase;
							cols = 40;
							rows = 25;
							vidcolor = 1;
							vidgfxmode = 0;
							blankattr = 7;
							for (tempcalc = videobase; tempcalc<videobase+16384; tempcalc+=2) {
									RAM[tempcalc] = 0;
									RAM[tempcalc+1] = blankattr;
								}
							portram[0x3D8] = portram[0x3D8] & 0xFE;
							break;
						case 2: //80x25 mono text
							videobase = textbase;
							cols = 80;
							rows = 25;
							vidcolor = 1;
							vidgfxmode = 0;
							blankattr = 7;
							for (tempcalc = videobase; tempcalc<videobase+16384; tempcalc+=2) {
									RAM[tempcalc] = 0;
									RAM[tempcalc+1] = blankattr;
								}
							portram[0x3D8] = portram[0x3D8] & 0xFE;
							break;
						case 3: //80x25 color text
							videobase = textbase;
							cols = 80;
							rows = 25;
							vidcolor = 1;
							vidgfxmode = 0;
							blankattr = 7;
							for (tempcalc = videobase; tempcalc<videobase+16384; tempcalc+=2) {
									RAM[tempcalc] = 0;
									RAM[tempcalc+1] = blankattr;
								}
							portram[0x3D8] = portram[0x3D8] & 0xFE;
							break;
						case 4:
						case 5: //80x25 color text
							videobase = textbase;
							cols = 40;
							rows = 25;
							vidcolor = 1;
							vidgfxmode = 1;
							blankattr = 7;
							for (tempcalc = videobase; tempcalc<videobase+16384; tempcalc+=2) {
									RAM[tempcalc] = 0;
									RAM[tempcalc+1] = blankattr;
								}
							if (regs.byteregs[regal]==4) portram[0x3D9] = 48;
							else portram[0x3D9] = 0;
							break;
						case 6:
							videobase = textbase;
							cols = 80;
							rows = 25;
							vidcolor = 0;
							vidgfxmode = 1;
							blankattr = 7;
							for (tempcalc = videobase; tempcalc<videobase+16384; tempcalc+=2) {
									RAM[tempcalc] = 0;
									RAM[tempcalc+1] = blankattr;
								}
							portram[0x3D8] = portram[0x3D8] & 0xFE;
							break;
						case 127:
							videobase = 0xB8000;
							cols = 90;
							rows = 25;
							vidcolor = 0;
							vidgfxmode = 1;
							for (tempcalc = videobase; tempcalc<videobase+16384; tempcalc++) {
									RAM[tempcalc] = 0;
								}
							portram[0x3D8] = portram[0x3D8] & 0xFE;
							break;
						case 0x9: //320x200 16-color
							videobase = 0xB8000;
							cols = 40;
							rows = 25;
							vidcolor = 1;
							vidgfxmode = 1;
							blankattr = 0;
							if ( (regs.byteregs[regal]&0x80) ==0) for (tempcalc = videobase; tempcalc<videobase+65535; tempcalc+=2) {
										RAM[tempcalc] = 0;
										RAM[tempcalc+1] = blankattr;
									}
							portram[0x3D8] = portram[0x3D8] & 0xFE;
							break;
						case 0xD: //320x200 16-color
						case 0x12: //640x480 16-color
						case 0x13: //320x200 256-color
							videobase = 0xA0000;
							cols = 40;
							rows = 25;
							vidcolor = 1;
							vidgfxmode = 1;
							blankattr = 0;
							for (tempcalc = videobase; tempcalc<videobase+65535; tempcalc+=2) {
									RAM[tempcalc] = 0;
									RAM[tempcalc+1] = blankattr;
								}
							portram[0x3D8] = portram[0x3D8] & 0xFE;
							break;
					}
				vidmode = regs.byteregs[regal] & 0x7F;
				RAM[0x449] = vidmode;
				RAM[0x44A] = (uint8_t) cols;
				RAM[0x44B] = 0;
				RAM[0x484] = (uint8_t) (rows - 1);
				cursx = 0;
				cursy = 0;
				if ( (regs.byteregs[regal] & 0x80) == 0x00) {
						memset (&RAM[0xA0000], 0, 0x1FFFF);
						memset (VRAM, 0, 262144);
					}
				switch (vidmode) {
						case 127: //hercules
							nw = oldw = 720;
							nh = oldh = 348;
							scrmodechange = 1;
							break;
						case 0x12:
							nw = oldw = 640;
							nh = oldh = 480;
							scrmodechange = 1;
							break;
						case 0x13:
							oldw = 640;
							oldh = 400;
							nw = 320;
							nh = 200;
							scrmodechange = 1;
							break;
						default:
							nw = oldw = 640;
							nh = oldh = 400;
							scrmodechange = 1;
							break;
					}
				break;
			case 0x10: //VGA DAC functions
				switch (regs.byteregs[regal]) {
						case 0x10: //set individual DAC register
							palettevga[getreg16 (regbx) ] = rgb((regs.byteregs[regdh] & 63) << 2, (regs.byteregs[regch] & 63) << 2, (regs.byteregs[regcl] & 63) << 2);
							break;
						case 0x12: //set block of DAC registers
							memloc = segregs[reges]*16+getreg16 (regdx);
							for (n=getreg16 (regbx); n< (uint32_t) (getreg16 (regbx) +getreg16 (regcx) ); n++) {
									palettevga[n] = rgb(read86(memloc) << 2, read86(memloc + 1) << 2, read86(memloc + 2) << 2);
									memloc += 3;
								}
					}
				break;
			case 0x1A: //get display combination code (ps, vga/mcga)
				regs.byteregs[regal] = 0x1A;
				regs.byteregs[regbl] = 0x8;
				break;
		}
}

void initcga() {
	FILE *fontfile;
	fontfile = fopen (PATH_DATAFILES "asciivga.dat", "rb");
	if (fontfile==NULL) {
			printf ("FATAL: Cannot open " PATH_DATAFILES "asciivga!\n");
			exit (1);
		}
	fread (&fontcga[0], 32768, 1, fontfile);
	fclose (fontfile);

	palettecga[0] = 0;
	palettecga[1] = rgb (0, 0, 0xAA);
	palettecga[2] = rgb (0, 0xAA, 0);
	palettecga[3] = rgb (0, 0xAA, 0xAA);
	palettecga[4] = rgb (0xAA, 0, 0);
	palettecga[5] = rgb (0xAA, 0, 0xAA);
	palettecga[6] = rgb (0xAA, 0x55, 0);
	palettecga[7] = rgb (0xAA, 0xAA, 0xAA);
	palettecga[8] = rgb (0x55, 0x55, 0x55);
	palettecga[9] = rgb (0x55, 0x55, 0xFF);
	palettecga[10] = rgb (0x55, 0xFF, 0x55);
	palettecga[11] = rgb (0x55, 0xFF, 0xFF);
	palettecga[12] = rgb (0xFF, 0x55, 0x55);
	palettecga[13] = rgb (0xFF, 0x55, 0xFF);
	palettecga[14] = rgb (0xFF, 0xFF, 0x55);
	palettecga[15] = rgb (0xFF, 0xFF, 0xFF);
	palettevga[0] = rgb (0, 0, 0);
	palettevga[1] = rgb (0, 0, 169);
	palettevga[2] = rgb (0, 169, 0);
	palettevga[3] = rgb (0, 169, 169);
	palettevga[4] = rgb (169, 0, 0);
	palettevga[5] = rgb (169, 0, 169);
	palettevga[6] = rgb (169, 169, 0);
	palettevga[7] = rgb (169, 169, 169);
	palettevga[8] = rgb (0, 0, 84);
	palettevga[9] = rgb (0, 0, 255);
	palettevga[10] = rgb (0, 169, 84);
	palettevga[11] = rgb (0, 169, 255);
	palettevga[12] = rgb (169, 0, 84);
	palettevga[13] = rgb (169, 0, 255);
	palettevga[14] = rgb (169, 169, 84);
	palettevga[15] = rgb (169, 169, 255);
	palettevga[16] = rgb (0, 84, 0);
	palettevga[17] = rgb (0, 84, 169);
	palettevga[18] = rgb (0, 255, 0);
	palettevga[19] = rgb (0, 255, 169);
	palettevga[20] = rgb (169, 84, 0);
	palettevga[21] = rgb (169, 84, 169);
	palettevga[22] = rgb (169, 255, 0);
	palettevga[23] = rgb (169, 255, 169);
	palettevga[24] = rgb (0, 84, 84);
	palettevga[25] = rgb (0, 84, 255);
	palettevga[26] = rgb (0, 255, 84);
	palettevga[27] = rgb (0, 255, 255);
	palettevga[28] = rgb (169, 84, 84);
	palettevga[29] = rgb (169, 84, 255);
	palettevga[30] = rgb (169, 255, 84);
	palettevga[31] = rgb (169, 255, 255);
	palettevga[32] = rgb (84, 0, 0);
	palettevga[33] = rgb (84, 0, 169);
	palettevga[34] = rgb (84, 169, 0);
	palettevga[35] = rgb (84, 169, 169);
	palettevga[36] = rgb (255, 0, 0);
	palettevga[37] = rgb (255, 0, 169);
	palettevga[38] = rgb (255, 169, 0);
	palettevga[39] = rgb (255, 169, 169);
	palettevga[40] = rgb (84, 0, 84);
	palettevga[41] = rgb (84, 0, 255);
	palettevga[42] = rgb (84, 169, 84);
	palettevga[43] = rgb (84, 169, 255);
	palettevga[44] = rgb (255, 0, 84);
	palettevga[45] = rgb (255, 0, 255);
	palettevga[46] = rgb (255, 169, 84);
	palettevga[47] = rgb (255, 169, 255);
	palettevga[48] = rgb (84, 84, 0);
	palettevga[49] = rgb (84, 84, 169);
	palettevga[50] = rgb (84, 255, 0);
	palettevga[51] = rgb (84, 255, 169);
	palettevga[52] = rgb (255, 84, 0);
	palettevga[53] = rgb (255, 84, 169);
	palettevga[54] = rgb (255, 255, 0);
	palettevga[55] = rgb (255, 255, 169);
	palettevga[56] = rgb (84, 84, 84);
	palettevga[57] = rgb (84, 84, 255);
	palettevga[58] = rgb (84, 255, 84);
	palettevga[59] = rgb (84, 255, 255);
	palettevga[60] = rgb (255, 84, 84);
	palettevga[61] = rgb (255, 84, 255);
	palettevga[62] = rgb (255, 255, 84);
	palettevga[63] = rgb (255, 255, 255);
	palettevga[64] = rgb (255, 125, 125);
	palettevga[65] = rgb (255, 157, 125);
	palettevga[66] = rgb (255, 190, 125);
	palettevga[67] = rgb (255, 222, 125);
	palettevga[68] = rgb (255, 255, 125);
	palettevga[69] = rgb (222, 255, 125);
	palettevga[70] = rgb (190, 255, 125);
	palettevga[71] = rgb (157, 255, 125);
	palettevga[72] = rgb (125, 255, 125);
	palettevga[73] = rgb (125, 255, 157);
	palettevga[74] = rgb (125, 255, 190);
	palettevga[75] = rgb (125, 255, 222);
	palettevga[76] = rgb (125, 255, 255);
	palettevga[77] = rgb (125, 222, 255);
	palettevga[78] = rgb (125, 190, 255);
	palettevga[79] = rgb (125, 157, 255);
	palettevga[80] = rgb (182, 182, 255);
	palettevga[81] = rgb (198, 182, 255);
	palettevga[82] = rgb (218, 182, 255);
	palettevga[83] = rgb (234, 182, 255);
	palettevga[84] = rgb (255, 182, 255);
	palettevga[85] = rgb (255, 182, 234);
	palettevga[86] = rgb (255, 182, 218);
	palettevga[87] = rgb (255, 182, 198);
	palettevga[88] = rgb (255, 182, 182);
	palettevga[89] = rgb (255, 198, 182);
	palettevga[90] = rgb (255, 218, 182);
	palettevga[91] = rgb (255, 234, 182);
	palettevga[92] = rgb (255, 255, 182);
	palettevga[93] = rgb (234, 255, 182);
	palettevga[94] = rgb (218, 255, 182);
	palettevga[95] = rgb (198, 255, 182);
	palettevga[96] = rgb (182, 255, 182);
	palettevga[97] = rgb (182, 255, 198);
	palettevga[98] = rgb (182, 255, 218);
	palettevga[99] = rgb (182, 255, 234);
	palettevga[100] = rgb (182, 255, 255);
	palettevga[101] = rgb (182, 234, 255);
	palettevga[102] = rgb (182, 218, 255);
	palettevga[103] = rgb (182, 198, 255);
	palettevga[104] = rgb (0, 0, 113);
	palettevga[105] = rgb (28, 0, 113);
	palettevga[106] = rgb (56, 0, 113);
	palettevga[107] = rgb (84, 0, 113);
	palettevga[108] = rgb (113, 0, 113);
	palettevga[109] = rgb (113, 0, 84);
	palettevga[110] = rgb (113, 0, 56);
	palettevga[111] = rgb (113, 0, 28);
	palettevga[112] = rgb (113, 0, 0);
	palettevga[113] = rgb (113, 28, 0);
	palettevga[114] = rgb (113, 56, 0);
	palettevga[115] = rgb (113, 84, 0);
	palettevga[116] = rgb (113, 113, 0);
	palettevga[117] = rgb (84, 113, 0);
	palettevga[118] = rgb (56, 113, 0);
	palettevga[119] = rgb (28, 113, 0);
	palettevga[120] = rgb (0, 113, 0);
	palettevga[121] = rgb (0, 113, 28);
	palettevga[122] = rgb (0, 113, 56);
	palettevga[123] = rgb (0, 113, 84);
	palettevga[124] = rgb (0, 113, 113);
	palettevga[125] = rgb (0, 84, 113);
	palettevga[126] = rgb (0, 56, 113);
	palettevga[127] = rgb (0, 28, 113);
	palettevga[128] = rgb (56, 56, 113);
	palettevga[129] = rgb (68, 56, 113);
	palettevga[130] = rgb (84, 56, 113);
	palettevga[131] = rgb (97, 56, 113);
	palettevga[132] = rgb (113, 56, 113);
	palettevga[133] = rgb (113, 56, 97);
	palettevga[134] = rgb (113, 56, 84);
	palettevga[135] = rgb (113, 56, 68);
	palettevga[136] = rgb (113, 56, 56);
	palettevga[137] = rgb (113, 68, 56);
	palettevga[138] = rgb (113, 84, 56);
	palettevga[139] = rgb (113, 97, 56);
	palettevga[140] = rgb (113, 113, 56);
	palettevga[141] = rgb (97, 113, 56);
	palettevga[142] = rgb (84, 113, 56);
	palettevga[143] = rgb (68, 113, 56);
	palettevga[144] = rgb (56, 113, 56);
	palettevga[145] = rgb (56, 113, 68);
	palettevga[146] = rgb (56, 113, 84);
	palettevga[147] = rgb (56, 113, 97);
	palettevga[148] = rgb (56, 113, 113);
	palettevga[149] = rgb (56, 97, 113);
	palettevga[150] = rgb (56, 84, 113);
	palettevga[151] = rgb (56, 68, 113);
	palettevga[152] = rgb (80, 80, 113);
	palettevga[153] = rgb (89, 80, 113);
	palettevga[154] = rgb (97, 80, 113);
	palettevga[155] = rgb (105, 80, 113);
	palettevga[156] = rgb (113, 80, 113);
	palettevga[157] = rgb (113, 80, 105);
	palettevga[158] = rgb (113, 80, 97);
	palettevga[159] = rgb (113, 80, 89);
	palettevga[160] = rgb (113, 80, 80);
	palettevga[161] = rgb (113, 89, 80);
	palettevga[162] = rgb (113, 97, 80);
	palettevga[163] = rgb (113, 105, 80);
	palettevga[164] = rgb (113, 113, 80);
	palettevga[165] = rgb (105, 113, 80);
	palettevga[166] = rgb (97, 113, 80);
	palettevga[167] = rgb (89, 113, 80);
	palettevga[168] = rgb (80, 113, 80);
	palettevga[169] = rgb (80, 113, 89);
	palettevga[170] = rgb (80, 113, 97);
	palettevga[171] = rgb (80, 113, 105);
	palettevga[172] = rgb (80, 113, 113);
	palettevga[173] = rgb (80, 105, 113);
	palettevga[174] = rgb (80, 97, 113);
	palettevga[175] = rgb (80, 89, 113);
	palettevga[176] = rgb (0, 0, 64);
	palettevga[177] = rgb (16, 0, 64);
	palettevga[178] = rgb (32, 0, 64);
	palettevga[179] = rgb (48, 0, 64);
	palettevga[180] = rgb (64, 0, 64);
	palettevga[181] = rgb (64, 0, 48);
	palettevga[182] = rgb (64, 0, 32);
	palettevga[183] = rgb (64, 0, 16);
	palettevga[184] = rgb (64, 0, 0);
	palettevga[185] = rgb (64, 16, 0);
	palettevga[186] = rgb (64, 32, 0);
	palettevga[187] = rgb (64, 48, 0);
	palettevga[188] = rgb (64, 64, 0);
	palettevga[189] = rgb (48, 64, 0);
	palettevga[190] = rgb (32, 64, 0);
	palettevga[191] = rgb (16, 64, 0);
	palettevga[192] = rgb (0, 64, 0);
	palettevga[193] = rgb (0, 64, 16);
	palettevga[194] = rgb (0, 64, 32);
	palettevga[195] = rgb (0, 64, 48);
	palettevga[196] = rgb (0, 64, 64);
	palettevga[197] = rgb (0, 48, 64);
	palettevga[198] = rgb (0, 32, 64);
	palettevga[199] = rgb (0, 16, 64);
	palettevga[200] = rgb (32, 32, 64);
	palettevga[201] = rgb (40, 32, 64);
	palettevga[202] = rgb (48, 32, 64);
	palettevga[203] = rgb (56, 32, 64);
	palettevga[204] = rgb (64, 32, 64);
	palettevga[205] = rgb (64, 32, 56);
	palettevga[206] = rgb (64, 32, 48);
	palettevga[207] = rgb (64, 32, 40);
	palettevga[208] = rgb (64, 32, 32);
	palettevga[209] = rgb (64, 40, 32);
	palettevga[210] = rgb (64, 48, 32);
	palettevga[211] = rgb (64, 56, 32);
	palettevga[212] = rgb (64, 64, 32);
	palettevga[213] = rgb (56, 64, 32);
	palettevga[214] = rgb (48, 64, 32);
	palettevga[215] = rgb (40, 64, 32);
	palettevga[216] = rgb (32, 64, 32);
	palettevga[217] = rgb (32, 64, 40);
	palettevga[218] = rgb (32, 64, 48);
	palettevga[219] = rgb (32, 64, 56);
	palettevga[220] = rgb (32, 64, 64);
	palettevga[221] = rgb (32, 56, 64);
	palettevga[222] = rgb (32, 48, 64);
	palettevga[223] = rgb (32, 40, 64);
	palettevga[224] = rgb (44, 44, 64);
	palettevga[225] = rgb (48, 44, 64);
	palettevga[226] = rgb (52, 44, 64);
	palettevga[227] = rgb (60, 44, 64);
	palettevga[228] = rgb (64, 44, 64);
	palettevga[229] = rgb (64, 44, 60);
	palettevga[230] = rgb (64, 44, 52);
	palettevga[231] = rgb (64, 44, 48);
	palettevga[232] = rgb (64, 44, 44);
	palettevga[233] = rgb (64, 48, 44);
	palettevga[234] = rgb (64, 52, 44);
	palettevga[235] = rgb (64, 60, 44);
	palettevga[236] = rgb (64, 64, 44);
	palettevga[237] = rgb (60, 64, 44);
	palettevga[238] = rgb (52, 64, 44);
	palettevga[239] = rgb (48, 64, 44);
	palettevga[240] = rgb (44, 64, 44);
	palettevga[241] = rgb (44, 64, 48);
	palettevga[242] = rgb (44, 64, 52);
	palettevga[243] = rgb (44, 64, 60);
	palettevga[244] = rgb (44, 64, 64);
	palettevga[245] = rgb (44, 60, 64);
	palettevga[246] = rgb (44, 52, 64);
	palettevga[247] = rgb (44, 48, 64);
	palettevga[248] = rgb (0, 0, 0);
	palettevga[249] = rgb (0, 0, 0);
	palettevga[250] = rgb (0, 0, 0);
	palettevga[251] = rgb (0, 0, 0);
	palettevga[252] = rgb (0, 0, 0);
	palettevga[253] = rgb (0, 0, 0);
	palettevga[254] = rgb (0, 0, 0);
	palettevga[255] = rgb (0, 0, 0);
}

uint16_t vtotal = 0;
void outVGA (uint16_t portnum, uint8_t value) {
	static uint8_t oldah, oldal;
	uint8_t flip3c0 = 0;
	updatedscreen = 1;
	switch (portnum) {
			case 0x3B8: //hercules support
				if ( ( (value & 2) == 2) && (vidmode != 127) ) {
						oldah = regs.byteregs[regah];
						oldal = regs.byteregs[regal];
						regs.byteregs[regah] = 0;
						regs.byteregs[regal] = 127;
						vidinterrupt();
						regs.byteregs[regah] = oldah;
						regs.byteregs[regal] = oldal;
					}
				if (value & 0x80) videobase = 0xB8000;
				else videobase = 0xB0000;
				break;
			case 0x3C0:
				if (flip3c0) {
						flip3c0 = 0;
						portram[0x3C0] = value & 255;
						return;
					}
				else {
						flip3c0 = 1;
						VGA_ATTR[portram[0x3C0]] = value & 255;
						return;
					}
			case 0x3C4: //sequence controller index
				portram[0x3C4] = value & 255;
				//if (portout16) VGA_SC[value & 255] = value >> 8;
				break;
			case 0x3C5: //sequence controller data
				VGA_SC[portram[0x3C4]] = value & 255;
				/*if (portram[0x3C4] == 2) {
				printf("VGA_SC[2] = %02X\n", value);
				}*/
				break;
			case 0x3D4: //CRT controller index
				portram[0x3D4] = value & 255;
				//if (portout16) VGA_CRTC[value & 255] = value >> 8;
				break;
			case 0x3C7: //color index register (read operations)
				latchReadPal = value & 255;
				latchReadRGB = 0;
				stateDAC = 0;
				break;
			case 0x3C8: //color index register (write operations)
				latchPal = value & 255;
				latchRGB = 0;
				tempRGB = 0;
				stateDAC = 3;
				break;
			case 0x3C9: //RGB data register
				value = value & 63;
				switch (latchRGB) {
#ifdef __BIG_ENDIAN__
						case 0: //red
							tempRGB = value << 26;
							break;
						case 1: //green
							tempRGB |= value << 18;
							break;
						case 2: //blue
							tempRGB |= value << 10;
							palettevga[latchPal] = tempRGB;
							latchPal = latchPal + 1;
							break;
#else
						case 0: //red
							tempRGB = value << 2;
							break;
						case 1: //green
							tempRGB |= value << 10;
							break;
						case 2: //blue
							tempRGB |= value << 18;
							palettevga[latchPal] = tempRGB;
							latchPal = latchPal + 1;
							break;
#endif
					}
				latchRGB = (latchRGB + 1) % 3;
				break;
			case 0x3D5: //cursor position latch
				VGA_CRTC[portram[0x3D4]] = value & 255;
				if (portram[0x3D4]==0xE) cursorposition = (cursorposition&0xFF) | (value<<8);
				else if (portram[0x3D4]==0xF) cursorposition = (cursorposition&0xFF00) |value;
				cursy = cursorposition/cols;
				cursx = cursorposition%cols;
				if (portram[0x3D4] == 6) {
						vtotal = value | ( ( (uint16_t) VGA_GC[7] & 1) << 8) | ( ( (VGA_GC[7] & 32) ? 1 : 0) << 9);
						//printf("Vertical total: %u\n", vtotal);
					}
				break;
			case 0x3CF:
				VGA_GC[portram[0x3CE]] = value;
				break;
			default:
				portram[portnum] = value;
		}
}

uint8_t inVGA (uint16_t portnum) {
	switch (portnum) {
			case 0x3C1:
				return ( (uint8_t) VGA_ATTR[portram[0x3C0]]);
			case 0x3C5:
				return ( (uint8_t) VGA_SC[portram[0x3C4]]);
			case 0x3D5:
				return ( (uint8_t) VGA_CRTC[portram[0x3D4]]);
			case 0x3C7: //DAC state
				return (stateDAC);
			case 0x3C8: //palette index
				return (latchReadPal);
			case 0x3C9: //RGB data register
				switch (latchReadRGB++) {
#ifdef __BIG_ENDIAN__
						case 0: //blue
							return ( (palettevga[latchReadPal] >> 26) & 63);
						case 1: //green
							return ( (palettevga[latchReadPal] >> 18) & 63);
						case 2: //red
							latchReadRGB = 0;
							return ( (palettevga[latchReadPal++] >> 10) & 63);
#else
						case 0: //blue
							return ( (palettevga[latchReadPal] >> 2) & 63);
						case 1: //green
							return ( (palettevga[latchReadPal] >> 10) & 63);
						case 2: //red
							latchReadRGB = 0;
							return ( (palettevga[latchReadPal++] >> 18) & 63);
#endif
					}
			case 0x3DA:
				return (port3da);
		}
	return (portram[portnum]); //this won't be reached, but without it the compiler gives a warning
}

#define shiftVGA(value) {\
	for (cnt=0; cnt<(VGA_GC[3] & 7); cnt++) {\
		value = (value >> 1) | ((value & 1) << 7);\
	}\
}

#define logicVGA(curval, latchval) {\
	switch ((VGA_GC[3]>>3) & 3) {\
		   case 1: curval &= latchval; break;\
		   case 2: curval |= latchval; break;\
		   case 3: curval ^= latchval; break;\
	}\
}

uint8_t lastmode = 0;
void writeVGA (uint32_t addr32, uint8_t value) {
	uint32_t planesize;
	uint8_t curval, tempand, cnt;
	updatedscreen = 1;
	planesize = 0x10000;
	shiftVGA (value);
	switch (VGA_GC[5] & 3) { //get write mode
			case 0:
				if (VGA_SC[2] & 1) {
						if (VGA_GC[1] & 1)
							if (VGA_GC[0] & 1) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[0]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[0]);
						VRAM[addr32] = curval;
					}
				if (VGA_SC[2] & 2) {
						if (VGA_GC[1] & 2)
							if (VGA_GC[0] & 2) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[1]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[1]);
						VRAM[addr32+planesize] = curval;
					}
				if (VGA_SC[2] & 4) {
						if (VGA_GC[1] & 4)
							if (VGA_GC[0] & 4) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[2]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[2]);
						VRAM[addr32+planesize*2] = curval;
					}
				if (VGA_SC[2] & 8) {
						if (VGA_GC[1] & 8)
							if (VGA_GC[0] & 8) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[3]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[3]);
						VRAM[addr32+planesize*3] = curval;
					}
				break;
			case 1:
				if (VGA_SC[2] & 1) VRAM[addr32] = VGA_latch[0];
				if (VGA_SC[2] & 2) VRAM[addr32+planesize] = VGA_latch[1];
				if (VGA_SC[2] & 4) VRAM[addr32+planesize*2] = VGA_latch[2];
				if (VGA_SC[2] & 8) VRAM[addr32+planesize*3] = VGA_latch[3];
				break;
			case 2:
				if (VGA_SC[2] & 1) {
						if (VGA_GC[1] & 1)
							if (value & 1) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[0]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[0]);
						VRAM[addr32] = curval;
					}
				if (VGA_SC[2] & 2) {
						if (VGA_GC[1] & 2)
							if (value & 2) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[1]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[1]);
						VRAM[addr32+planesize] = curval;
					}
				if (VGA_SC[2] & 4) {
						if (VGA_GC[1] & 4)
							if (value & 4) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[2]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[2]);
						VRAM[addr32+planesize*2] = curval;
					}
				if (VGA_SC[2] & 8) {
						if (VGA_GC[1] & 8)
							if (value & 8) curval = 255;
							else curval = 0;
						else curval = value;
						logicVGA (curval, VGA_latch[3]);
						curval = (~VGA_GC[8] & curval) | (VGA_GC[8] & VGA_latch[3]);
						VRAM[addr32+planesize*3] = curval;
					}
				break;
			case 3:
				tempand = value & VGA_GC[8];
				if (VGA_SC[2] & 1) {
						if (VGA_GC[0] & 1) curval = 255;
						else curval = 0;
						logicVGA (curval, VGA_latch[0]);
						curval = (~tempand & curval) | (tempand & VGA_latch[0]);
						VRAM[addr32] = curval;
					}
				if (VGA_SC[2] & 2) {
						if (VGA_GC[0] & 2) curval = 255;
						else curval = 0;
						logicVGA (curval, VGA_latch[1]);
						curval = (~tempand & curval) | (tempand & VGA_latch[1]);
						VRAM[addr32+planesize] = curval;
					}
				if (VGA_SC[2] & 4) {
						if (VGA_GC[0] & 4) curval = 255;
						else curval = 0;
						logicVGA (curval, VGA_latch[2]);
						curval = (~tempand & curval) | (tempand & VGA_latch[2]);
						VRAM[addr32+planesize*2] = curval;
					}
				if (VGA_SC[2] & 8) {
						if (VGA_GC[0] & 8) curval = 255;
						else curval = 0;
						logicVGA (curval, VGA_latch[3]);
						curval = (~tempand & curval) | (tempand & VGA_latch[3]);
						VRAM[addr32+planesize*3] = curval;
					}
				break;
		}
}

uint8_t readVGA (uint32_t addr32) {
	uint32_t planesize;
	planesize = 0x10000;
	VGA_latch[0] = VRAM[addr32];
	VGA_latch[1] = VRAM[addr32+planesize];
	VGA_latch[2] = VRAM[addr32+planesize*2];
	VGA_latch[3] = VRAM[addr32+planesize*3];
	if (VGA_SC[2] & 1) return (VRAM[addr32]);
	if (VGA_SC[2] & 2) return (VRAM[addr32+planesize]);
	if (VGA_SC[2] & 4) return (VRAM[addr32+planesize*2]);
	if (VGA_SC[2] & 8) return (VRAM[addr32+planesize*3]);
	return (0); //this won't be reached, but without it some compilers give a warning
}

void initVideoPorts() {
	set_port_write_redirector (0x3B0, 0x3DA, &outVGA);
	set_port_read_redirector (0x3B0, 0x3DA, &inVGA);
}
