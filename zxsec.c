 // ######  #    #   ####   ######   ####    ----------------------- //
//  """"#"  "#  #"  #""""   #"""""  #""""#  ZXSEC, barebones Sinclair //
//     #"    "##"   "####   #####   #    "  Spectrum emulator written //
//    #"      ##     """"#  #""""   #       on top of CPCEC's modules //
//   #"      #""#   #    #  #       #    #  by Cesar Nicolas-Gonzalez //
//  ######  #"  "#  "####"  ######  "####"  since 2019-02-24 till now //
 // """"""  "    "   """"   """"""   """"    ----------------------- //

#define MY_CAPTION "ZXSEC"
#define MY_VERSION "20200331"//"2255"
#define MY_LICENSE "Copyright (C) 2019-2020 Cesar Nicolas-Gonzalez"

/* This notice applies to the source code of CPCEC and its binaries.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Contact information: <mailto:cngsoft@gmail.com> */

// The goal of this emulator isn't to provide a precise emulation of
// the ZX Spectrum family (although it sure is desirable) but to show
// that the modular design of the Amstrad CPC emulator CPCEC is good
// on its own and it allows easy reusing of its composing parts.

// This file focuses on the Spectrum-specific features: configuration,
// ULA logic+video, Z80 timings and support, and snapshot handling.

#include <stdio.h> // printf()...
#include <stdlib.h> // strtol()...
#include <io.h> // chsize(),strcpy()...

// ZX Spectrum metrics and constants defined as general types ------- //

#define MAIN_FRAMESKIP_FULL 25
#define MAIN_FRAMESKIP_MASK 31
#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (56<<4)
#define VIDEO_LENGTH_Y (39<<4)
#define VIDEO_OFFSET_X (0<<4)
#define VIDEO_OFFSET_Y (5<<4)
#define VIDEO_PIXELS_X (40<<4)
#define VIDEO_PIXELS_Y (30<<4)
#define AUDIO_PLAYBACK 44100 // 22050, 24000, 44100, 48000
#define AUDIO_LENGTH_Z (AUDIO_PLAYBACK/VIDEO_PLAYBACK) // division must be exact!
#define AUDIO_N_FRAMES 8

#define DEBUG_LENGTH_X 64
#define DEBUG_LENGTH_Y 32
#define DEBUG_LENGTH_Z 12
#define session_debug_show z80_debug_show
#define session_debug_user z80_debug_user

// The ZX Spectrum 48K keyboard; later models add combinations such as BREAK=CAPS SHIFT+SPACE
// +---------------------------------------------------------------------+
// | 1 18 | 2 19 | 3 1A | 4 1B | 5 1C | 6 24 | 7 23 | 8 22 | 9 21 | 0 20 |
// +---------------------------------------------------------------------+
// | Q 10 | W 11 | E 12 | R 13 | T 14 | Y 2C | U 2B | I 2A | O 29 | P 28 |
// +---------------------------------------------------------------------+
// | A 08 | S 09 | D 0A | F 0B | G 0C | H 34 | J 33 | K 32 | L 31 | r 30 | r = RETURN
// +---------------------------------------------------------------------+ c = CAPS SHIFT
// | c 00 | Z 01 | X 02 | C 03 | V 04 | B 3C | N 3B | M 3A | y 39 | s 38 | s = SPACE
// +---------------------------------------------------------------------+ y = SYMBOL SHIFT

/*const*/ unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1, fire2...
	{ 0,0,0,0,0 };

#include "cpcec-os.h" // OS-specific code!
#include "cpcec-rt.h" // OS-independent code!

const unsigned char kbd_map_xlt[]=
{
	// control keys
	KBCODE_F1	,0x81,	KBCODE_F2	,0x82,	KBCODE_F3	,0x83,	KBCODE_F4	,0x84,
	KBCODE_F5	,0x85,	KBCODE_F6	,0x86,	KBCODE_F7	,0x87,	KBCODE_F8	,0x88,
	KBCODE_F9	,0x89,	KBCODE_HOLD	,0x8F,	KBCODE_F11	,0x8B,	KBCODE_F12	,0x8C,
	KBCODE_X_ADD	,0x91,	KBCODE_X_SUB	,0x92,	KBCODE_X_MUL	,0x93,	KBCODE_X_DIV	,0x94,
	//KBCODE_PRIOR	,0x95,	KBCODE_NEXT	,0x96,	KBCODE_HOME	,0x97,	KBCODE_END	,0x98,
	// actual keys
	KBCODE_1	,0x18,	KBCODE_Q	,0x10,	KBCODE_A	,0x08,	KBCODE_L_SHIFT	,0x00,
	KBCODE_2	,0x19,	KBCODE_W	,0x11,	KBCODE_S	,0x09,	KBCODE_Z	,0x01,
	KBCODE_3	,0x1A,	KBCODE_E	,0x12,	KBCODE_D	,0x0A,	KBCODE_X	,0x02,
	KBCODE_4	,0x1B,	KBCODE_R	,0x13,	KBCODE_F	,0x0B,	KBCODE_C	,0x03,
	KBCODE_5	,0x1C,	KBCODE_T	,0x14,	KBCODE_G	,0x0C,	KBCODE_V	,0x04,
	KBCODE_6	,0x24,	KBCODE_Y	,0x2C,	KBCODE_H	,0x34,	KBCODE_B	,0x3C,
	KBCODE_7	,0x23,	KBCODE_U	,0x2B,	KBCODE_J	,0x33,	KBCODE_N	,0x3B,
	KBCODE_8	,0x22,	KBCODE_I	,0x2A,	KBCODE_K	,0x32,	KBCODE_M	,0x3A,
	KBCODE_9	,0x21,	KBCODE_O	,0x29,	KBCODE_L	,0x31,	KBCODE_L_CTRL	,0x39,
	KBCODE_0	,0x20,	KBCODE_P	,0x28,	KBCODE_ENTER	,0x30,	KBCODE_SPACE	,0x38,
	// key mirrors
	KBCODE_R_SHIFT	,0x00,	KBCODE_R_CTRL	,0x39,
	KBCODE_X_7	,0x23,	KBCODE_X_8	,0x22,	KBCODE_X_9	,0x21,
	KBCODE_X_4	,0x1B,	KBCODE_X_5	,0x1C,	KBCODE_X_6	,0x24,
	KBCODE_X_1	,0x18,	KBCODE_X_2	,0x19,	KBCODE_X_3	,0x1A,
	KBCODE_X_0	,0x20,	KBCODE_X_ENTER	,0x30,
	// built-in combinations
	KBCODE_ESCAPE	,0x78, // CAPS SHIFT FLAG (0x40) + SPACE (0x38)
	KBCODE_BKSPACE	,0x60, // CAPS SHIFT FLAG (0x40) + "0" (0x20)
	KBCODE_TAB	,0x58, // CAPS SHIFT FLAG (0x40) + "1" (0x18)
	KBCODE_CAP_LOCK	,0x59, // CAPS SHIFT FLAG (0x40) + "2" (0x19)
	KBCODE_UP	,0x63, // CAPS SHIFT FLAG (0x40) + "7" (0x23)
	KBCODE_DOWN	,0x64, // CAPS SHIFT FLAG (0x40) + "6" (0x24)
	KBCODE_LEFT	,0x5C, // CAPS SHIFT FLAG (0x40) + "5" (0x1C)
	KBCODE_RIGHT	,0x62, // CAPS SHIFT FLAG (0x40) + "8" (0x22)
	//KBCODE_INSERT	,0x61, // CAPS SHIFT FLAG (0x40) + "9" (0x21) GRAPH?
};

const VIDEO_DATATYPE video_table[][16]= // colour table, 0xRRGGBB style
{
	// monochrome - black and white // =(b+r*3+g*9+13/2)/13;
	{
		VIDEO1(0x000000),VIDEO1(0x1B1B1B),VIDEO1(0x373737),VIDEO1(0x525252),VIDEO1(0x6E6E6E),VIDEO1(0x898989),VIDEO1(0xA5A5A5),VIDEO1(0xC0C0C0),
		VIDEO1(0x000000),VIDEO1(0x242424),VIDEO1(0x494949),VIDEO1(0x6D6D6D),VIDEO1(0x929292),VIDEO1(0xB6B6B6),VIDEO1(0xDBDBDB),VIDEO1(0xFFFFFF)
	},
	// dark colour
	{
		VIDEO1(0x000000),VIDEO1(0x000080),VIDEO1(0x800000),VIDEO1(0x800080),VIDEO1(0x008000),VIDEO1(0x008080),VIDEO1(0x808000),VIDEO1(0x808080),
		VIDEO1(0x000000),VIDEO1(0x0000FF),VIDEO1(0xFF0000),VIDEO1(0xFF00FF),VIDEO1(0x00FF00),VIDEO1(0x00FFFF),VIDEO1(0xFFFF00),VIDEO1(0xFFFFFF)
	},
	// normal colour
	{
		VIDEO1(0x000000),VIDEO1(0x0000C0),VIDEO1(0xC00000),VIDEO1(0xC000C0),VIDEO1(0x00C000),VIDEO1(0x00C0C0),VIDEO1(0xC0C000),VIDEO1(0xC0C0C0),
		VIDEO1(0x000000),VIDEO1(0x0000FF),VIDEO1(0xFF0000),VIDEO1(0xFF00FF),VIDEO1(0x00FF00),VIDEO1(0x00FFFF),VIDEO1(0xFFFF00),VIDEO1(0xFFFFFF)
	},
	// bright colour
	{
		VIDEO1(0x000000),VIDEO1(0x0000E0),VIDEO1(0xE00000),VIDEO1(0xE000E0),VIDEO1(0x00E000),VIDEO1(0x00E0E0),VIDEO1(0xE0E000),VIDEO1(0xE0E0E0),
		VIDEO1(0x000000),VIDEO1(0x0000FF),VIDEO1(0xFF0000),VIDEO1(0xFF00FF),VIDEO1(0x00FF00),VIDEO1(0x00FFFF),VIDEO1(0xFFFF00),VIDEO1(0xFFFFFF)
	},
	// monochrome - green screen
	{
		VIDEO1(0x003C00),VIDEO1(0x084808),VIDEO1(0x176017),VIDEO1(0x1E6C1E),VIDEO1(0x449044),VIDEO1(0x4B9C4B),VIDEO1(0x5AB45A),VIDEO1(0x62C062),
		VIDEO1(0x003C00),VIDEO1(0x0A4B0A),VIDEO1(0x1E691E),VIDEO1(0x287828),VIDEO1(0x5AC35A),VIDEO1(0x64D264),VIDEO1(0x78F078),VIDEO1(0x82FF82)
	},
};

// sound table, 16 static levels + 1 dynamic level, 16-bit sample style
int audio_table[17]={0,85,121,171,241,341,483,683,965,1365,1931,2731,3862,5461,7723,10922,0};

// GLOBAL DEFINITIONS =============================================== //

int TICKS_PER_FRAME;// ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)/32);
int TICKS_PER_SECOND;// (TICKS_PER_FRAME*VIDEO_PLAYBACK);
// Everything in the ZX Spectrum is tuned to a 3.5 MHz clock,
// using simple binary divisors to adjust the devices' timings;
// the "3.5 MHz" isn't neither exact or the same on each machine:
// 50*312*224= 3494400 Hz on 48K, 50*311*228= 3545400 Hz on 128K.
// (and even the 50Hz screen framerate isn't the exact PAL value!)

// HARDWARE DEFINITIONS ============================================= //

BYTE mem_ram[9<<14],mem_rom[4<<14]; // memory: 9*16k RAM and 4*16k ROM
BYTE *mmu_ram[4],*mmu_rom[4]; // memory is divided in 16k pages
#define PEEK(x) mmu_rom[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!

BYTE type_id=3; // 0=48k, 1=128k, 2=PLUS2, 3=PLUS3
BYTE video_type=length(video_table)/2; // 0 = monochrome, 1=darkest colour, etc.
VIDEO_DATATYPE video_clut[17]; // precalculated colour palette, 16 attr + border

// 0x??FE,0x7FFD,0x1FFD: ULA 48K,128K,PLUS3 ------------------------- //

BYTE ula_v1,ula_v2,ula_v3; // 48k, 128k and PLUS3 respectively
BYTE disc_disabled=0,psg_disabled=0,ula_v1_issue=~64; // auxiliar ULA variables
BYTE *ula_screen,*ula_bitmap,*ula_attrib; // VRAM pointers

BYTE ula_clash[4][1<<16],*ula_clash_mreq[5],*ula_clash_iorq[5]; // the fifth entry stands for constant clashing
int ula_clash_z; // 16-bit cursor that follows the ULA clash map
int ula_clash_delta; // ULA adjustment for attribute-based effects
int ula_clash_disabled=0;
void ula_setup_clash(int i,int j,int l,int x0,int x1,int x2,int x3,int x4,int x5,int x6,int x7)
{
	for (int y=0;y<192;++y)
	{
		for (int x=0;x<16;++x)
		{
			ula_clash[i][j++]=x0;
			ula_clash[i][j++]=x1;
			ula_clash[i][j++]=x2;
			ula_clash[i][j++]=x3;
			ula_clash[i][j++]=x4;
			ula_clash[i][j++]=x5;
			ula_clash[i][j++]=x6;
			ula_clash[i][j++]=x7;
		}
		j+=l-128;
	}
}
void ula_setup(void)
{
	MEMZERO(ula_clash);
	ula_setup_clash(1,14335,224,6,5,4,3,2,1,0,0); // ULA v1: 48k
	ula_setup_clash(2,14361,228,6,5,4,3,2,1,0,0); // ULA v2: 128k,PLUS2
	ula_setup_clash(3,14365,228,1,0,7,6,5,4,3,2); // ULA v3: PLUS3
}

const int mmu_ram_mode[4][4]= // relative offsets of every bank for each PLUS3 RAM mode
{
	{ 0x00000-0x0000,0x04000-0x4000,0x08000-0x8000,0x0C000-0xC000 }, // RAM0: 0 1 2 3
	{ 0x10000-0x0000,0x14000-0x4000,0x18000-0x8000,0x1C000-0xC000 }, // RAM1: 4 5 6 7
	{ 0x10000-0x0000,0x14000-0x4000,0x18000-0x8000,0x0C000-0xC000 }, // RAM2: 4 5 6 3
	{ 0x10000-0x0000,0x1C000-0x4000,0x18000-0x8000,0x0C000-0xC000 }, // RAM3: 4 7 6 3
};
void mmu_update(void) // update the MMU tables with all the new offsets
{
	// the general idea is as follows: contention applies to the banks:
	// V1 (48k): area 4000-7FFF (equivalent to 128k bank 5)
	// V2 (128k,Plus2): banks 1,3,5,7
	// V3 (Plus3): banks 4,5,6,7
	// in other words:
	// banks 5 and 7 are always contended
	// banks 1 and 3 are contended on V2
	// banks 4 and 6 are contended on V3
	if (ula_clash_disabled)
	{
		ula_clash_mreq[0]=ula_clash_mreq[1]=ula_clash_mreq[2]=ula_clash_mreq[3]=ula_clash_mreq[4]=
		ula_clash_iorq[0]=ula_clash_iorq[1]=ula_clash_iorq[2]=ula_clash_iorq[3]=ula_clash_iorq[4]=ula_clash[0];
	}
	else
	{
		ula_clash_mreq[4]=ula_clash[type_id<2?type_id+1:type_id]; // turn TYPE_ID (0, 1+2, 3) into ULA V (1, 2, 3)
		ula_clash_mreq[0]=ula_clash_mreq[2]=(type_id==3&&(ula_v3&7)==1)?ula_clash_mreq[4]:ula_clash[0];
		ula_clash_mreq[1]=(type_id<3||(ula_v3&7)!=1)?ula_clash_mreq[4]:ula_clash[0];
		ula_clash_mreq[3]=((type_id==3&&(ula_v3&7)==3)||(type_id&&(ula_v2&1)))?ula_clash_mreq[4]:ula_clash[0];
		if (type_id==3)
			ula_clash_iorq[0]=ula_clash_iorq[1]=ula_clash_iorq[2]=ula_clash_iorq[3]=ula_clash_iorq[4]=ula_clash[0]; // IORQ doesn't clash at all on PLUS3 and PLUS2A!
		else
			ula_clash_iorq[0]=ula_clash_iorq[2]=ula_clash_iorq[3]=ula_clash[0], ula_clash_iorq[1]=ula_clash_iorq[4]=ula_clash_mreq[4]; // clash on 0x4000-0x7FFF only
	}
	if (ula_v3&1) // PLUS3 custom mode?
	{
		int i=(ula_v3>>1)&3;
		mmu_rom[0]=mmu_ram[0]=&mem_ram[mmu_ram_mode[i][0]];
		mmu_rom[1]=mmu_ram[1]=&mem_ram[mmu_ram_mode[i][1]];
		mmu_rom[2]=mmu_ram[2]=&mem_ram[mmu_ram_mode[i][2]];
		mmu_rom[3]=mmu_ram[3]=&mem_ram[mmu_ram_mode[i][3]];
	}
	else // normal 128K mode
	{
		mmu_rom[0]=&mem_rom[((ula_v2&16)<<10)+((ula_v3&4)<<13)-0x0000]; // i.e. ROM ID=((ula_v3&4)/2)+((ula_v2&16)/16)
		mmu_ram[0]=&mem_ram[(8<<14)-0x0000]; // dummy 16k for ROM writes
		mmu_rom[1]=mmu_ram[1]=&mem_ram[(5<<14)-0x4000];
		mmu_rom[2]=mmu_ram[2]=&mem_ram[(2<<14)-0x8000];
		mmu_rom[3]=mmu_ram[3]=&mem_ram[((ula_v2&7)<<14)-0xC000];
	}
	ula_screen=&mem_ram[(ula_v2&8)?0x1C000:0x14000];
}

INLINE void ula_v1_send(BYTE i) { video_clut[16]=video_table[video_type][(ula_v1=i)&7]; }
#define ula_v2_send(i) (ula_v2=i,mmu_update())
#define ula_v3_send(i) (ula_v3=i,mmu_update())

void video_clut_update(void) // precalculate palette following `video_type`
{
	for (int i=0;i<16;++i)
		video_clut[i]=video_table[video_type][i];
	ula_v1_send(ula_v1);
}

BYTE z80_irq;
int irq_delay=0,ula_limit_x=0,ula_limit_y=0; // IRQ counter, horizontal+vertical limits

void ula_reset(void) // reset the ULA
{
	ula_limit_x=type_id?57:56; // characters per scanline
	ula_limit_y=type_id?311:312; // scanlines per frame
	TICKS_PER_SECOND=((TICKS_PER_FRAME=4*ula_limit_x*ula_limit_y)*VIDEO_PLAYBACK);
	ula_v1=ula_v2=ula_v3=0;
	video_clut_update();
	mmu_update();
	ula_attrib=&(ula_bitmap=ula_screen)[0x1800];
	ula_clash_delta=type_id<3?!type_id?4:3:2;
	// lowest valid ULA deltas for V1, V2 and V3
	// Black Lamp (48K)	4	*	*
	// Black Lamp (128K)	*	2	2
	// Sly Spy (128K/DISC)	*	3	1
	// LED Storm (DISC)	*	0	0
}

// 0xBFFD,0xFFFD: PSG AY-3-8910 ------------------------------------- //

#define PSG_TICK_STEP 16 // 3.5 MHz /2 /16 = 109375 Hz
#define PSG_KHZ_CLOCK 1750
#define PSG_MAIN_EXTRABITS 0

#include "cpcec-ay.h"

// behind the ULA: TAPE --------------------------------------------- //

#define TAPE_MAIN_TZX_STEP (35<<1) // amount of T units per packet
int tape_enabled=0; // tape motor and delay

#include "cpcec-k7.h"

// 0x1FFD,0x2FFD,0x3FFD: FDC 765 ------------------------------------ //

#define DISC_PARMTR_UNIT (disc_parmtr[1]&1) // CPC hardware is limited to two drives;
#define DISC_PARMTR_UNITHEAD (disc_parmtr[1]&5) // however, it allows two-sided discs.
#define DISC_TIMER_INIT ( 4<<6) // rough approximation: cfr. CPCEC for details. "GAUNTLET 3" works fine.
#define DISC_TIMER_BYTE ( 2<<6) // rough approximation, too
#define DISC_WIRED_MODE 0 // Plus3 FDC is unwired, like the CPC; "HATE.DSK" proves it.
#define DISC_PER_FRAME (312<<6) // = 1 MHz / 50 Hz ; compare with TICKS_PER_FRAME

#define DISC_NEW_SIDES 1
#define DISC_NEW_TRACKS 40
#define DISC_NEW_SECTORS 9
char DISC_NEW_SECTOR_IDS[]={0xC1,0xC6,0xC2,0xC7,0xC3,0xC8,0xC4,0xC9,0xC5};
#define DISC_NEW_SECTOR_SIZE_FDC 2
#define DISC_NEW_SECTOR_GAPS 82
#define DISC_NEW_SECTOR_FILL 0xE5

#include "cpcec-d7.h"

// CPU-HARDWARE-VIDEO-AUDIO INTERFACE =============================== //

BYTE ula_temp; // Spectrum hardware before PLUS3 "forgets" cleaning the data bus
int ula_flash,ula_count_x=0,ula_count_y=0; // flash+horizontal+vertical counters
int ula_pos_x=0,ula_pos_y=0,ula_scr_x,ula_scr_y=0; // screen bitmap counters
int ula_snow_disabled=0,ula_snow_z,ula_snow_a;

BYTE ula_clash_bitmap[32],ula_clash_attrib[32];
int ula_clash_alpha=0;

INLINE void video_main(int t) // render video output for `t` clock ticks
{
	BYTE b,a=ula_temp;
	static int r=0;
	r+=t;
	while (r>=0)
	{
		r-=4;
		if (irq_delay&&(irq_delay-=4)<=0)
			z80_irq=irq_delay=0; // IRQs are lost after few microseconds
		if (!ula_clash_alpha++)
			MEMLOAD(ula_clash_bitmap,ula_bitmap),MEMLOAD(ula_clash_attrib,ula_attrib); // freeze the VRAM when "racing the beam" is over
		int q;
		if (q=(ula_pos_y>=0&&ula_pos_y<192&&ula_pos_x>=0&&ula_pos_x<32))
			b=ula_clash_bitmap[ula_pos_x],a=ula_clash_attrib[ula_pos_x];
		else
			a=0xFF; // border!
		if (!video_framecount&&(video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X))
		{
			#define VIDEO_NEXT *video_target++ // "VIDEO_NEXT = VIDEO_NEXT = ..." generates invalid code on VS13 and slower code on TCC
			if (q)
			{
				if ((ula_snow_z+=ula_snow_a)&256)
				{
					static BYTE z=0;
					b+=z+=16+1; ula_snow_z-=256-1;
				}
				{
					if ((a&128)&&(ula_flash&16))
						b=~b;
					VIDEO_DATATYPE p, v1=video_clut[(a&7)+((a&64)>>3)],v0=video_clut[(a&120)>>3];
					p=b&128?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=b&64?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=b&32?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=b&16?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=b&8?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=b&4?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=b&2?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=b&1?v1:v0; VIDEO_NEXT=p; VIDEO_NEXT=p;
				}
			}
			else // BORDER
			{
				VIDEO_DATATYPE p=video_clut[16];
				VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
				VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
			}
		}
		else
			video_target+=16;
		video_pos_x+=16;
		++ula_pos_x;
		if (++ula_count_x>=ula_limit_x)
		{
			if (!video_framecount)
				video_drawscanline();
			video_pos_y+=2;
			video_target+=VIDEO_LENGTH_X*2-video_pos_x;
			video_pos_x=0;
			ula_count_x=0,ula_pos_x=-4; // characters of BORDER before the leftmost byte
			++ula_count_y;
			if (++ula_pos_y>=0&&ula_pos_y<192)
			{
				ula_clash_alpha=ula_pos_x+ula_clash_delta;
				ula_bitmap=&ula_screen[((ula_pos_y>>6)<<11)+((ula_pos_y<<2)&224)+((ula_pos_y&7)<<8)];
				ula_attrib=&ula_screen[0x1800+((ula_pos_y>>3)<<5)];
			}
		}
		if (ula_pos_x==0&&ula_count_y>=ula_limit_y)
		{
			if (!video_framecount)
				video_endscanlines(video_table[video_type][0]);
			video_newscanlines(video_pos_x,(312-ula_limit_y)*2); // 128K screen is one line shorter, but begins one line later than 48K
			ula_attrib=&(ula_bitmap=ula_screen)[0x1800];
			++ula_flash;
			ula_count_y=0,ula_pos_y=248-ula_limit_y; // lines of BORDER before the top byte
			z80_irq=1; irq_delay=(type_id?33:32);//-(ula_clash_z&3); // Spectrum 48K "early" timing is different
			session_signal|=SESSION_SIGNAL_FRAME; // end of frame!
		}
	}
	ula_temp=a; // ditto! required for "Cobra" and "Arkanoid"!
}

void audio_main(int t) // render audio output for `t` clock ticks
{
	AUDIO_DATATYPE *z=audio_target;
	psg_main(t);
	if (z<audio_target)
	{
		// ULA MIXER: tape input (BIT 2) + beeper (BIT 4) + tape output (BIT 3; "Manic Miner" song, "Cobra's Arc" speech)
		AUDIO_DATATYPE k=((tape_status<<4)+((ula_v1&16)<<2)+((ula_v1&8)<<0))<<(AUDIO_BITDEPTH-8);
		static AUDIO_DATATYPE j=0;
		while (z<audio_target)
			*z++-=j=((k+j+(k>j))/2);
	}
}

// autorun runtime logic -------------------------------------------- //

char autorun_path[STRMAX]="",autorun_line[STRMAX];
int autorun_mode=0,autorun_t=0;
BYTE autorun_kbd[16]; // automatic keypresses
#define autorun_kbd_set(k) (autorun_kbd[k/8]|=1<<(k%8))
#define autorun_kbd_res(k) (autorun_kbd[k/8]&=~(1<<(k%8)))
#define autorun_kbd_bit (autorun_mode?autorun_kbd:kbd_bit)
INLINE void autorun_next(void)
{
	++autorun_t;
	switch (autorun_mode)
	{
		case 1: // autorun, 48k: type LOAD"" and hit RETURN; 128k+, hit RETURN
			if (autorun_t>3)
			{
				autorun_kbd_set(0x33); // PRESS "J"
				++autorun_mode;
				autorun_t=0;
			}
			break;
		case 2: if (autorun_t>3)
			{
				autorun_kbd_res(0x33); // RELEASE "J"
				++autorun_mode;
				autorun_t=0;
			}
			break;
		case 3:
		case 5: if (autorun_t>3)
			{
				autorun_kbd_set(0x39);
				autorun_kbd_set(0x28);
				++autorun_mode;
				autorun_t=0;
			}
			break;
		case 4:
		case 6: if (autorun_t>3)
			{
				autorun_kbd_res(0x39);
				autorun_kbd_res(0x28);
				++autorun_mode;
				autorun_t=0;
			}
			break;
		case 7:
		case 8: // shared autorun (1/2): press RETURN
			if (autorun_t>3)
			{
				autorun_kbd_set(0x30); // PRESS RETURN
				autorun_mode=9;
				autorun_t=0;
			}
			break;
		case 9: // shared autorun (2/2): release RETURN
			if (autorun_t>3)
			{
				autorun_kbd_res(0x30); // RELEASE RETURN
				autorun_mode=0;
				autorun_t=0;
			}
			break;
	}
}

// Z80-hardware procedures ------------------------------------------ //

Z80W z80_af,z80_bc,z80_de,z80_hl; // Accumulator+Flags, BC, DE, HL
Z80W z80_af2,z80_bc2,z80_de2,z80_hl2,z80_ix,z80_iy; // AF', BC', DE', HL', IX, IY
Z80W z80_pc,z80_sp,z80_iff,z80_ir; // Program Counter, Stack Pointer, Interrupt Flip-Flops, IR pair
BYTE z80_imd; // Interrupt Mode
BYTE z80_r7; // low 7 bits of R, required by several `IN X,(Y)` operations
int z80_turbo=0,z80_multi=1; // overclocking options

// the Spectrum hands the Z80 a mainly empty data bus value
#define z80_irq_bus 0xFF
// the Spectrum lacks special cases where RETI and RETN matter
#define z80_retn()
// the Spectrum doesn't obey the Z80 IRQ ACK signal
#define z80_irq_ack()

void z80_sync(int t) // the Z80 asks the hardware/video/audio to catch up
{
	static int r=0;
	r+=t;
	int tt=r/z80_multi; // calculate base value of `t`
	r-=(t=tt*z80_multi); // adjust `t` and keep remainder
	if (t)
	{
		if (type_id==3&&!disc_disabled)
			disc_main(t);
		if (tape_enabled)
			tape_main(t);
		if (tt)
		{
			video_main(tt);
			if (!audio_disabled)
				audio_main(tt);
		}
	}
}

void z80_send(WORD p,BYTE b) // the Z80 sends a byte to a hardware port
{
	if ((p&3)==2) // 0x??FE, ULA 48K
	{
		ula_v1_send(b);
		tape_output=(b>>3)&1; // tape record signal
	}
	else if ((p&15)==13) // 0x??FD, MULTIPLE DEVICES
	{
		if ((p&0x0A00)!=0x0A00) // catch broken 128K ports!
			p=3<<13; // AteBit's "Mipmap" and "Cube" need this!
			// 0x0F00 is excessive and hurts "Rescate Atlantida"!
		switch (p>>13)
		{
			case 0: // 0x1FFD: ULA PLUS3
				if (type_id==3) // PLUS3?
					if (!(ula_v2&32)) // 48K mode forbids further changes!
					{
						ula_v3_send(b);
						if (!disc_disabled)
							disc_motor_set(b&8); // BIT 3: MOTOR
					}
				break;
			case 1: // 0x3FFD: FDC DATA I/O
				if (type_id==3&&!disc_disabled) // PLUS3?
					disc_data_send(b);
			case 2: // 0x5FFD: ???
				break;
			case 3: // 0x7FFD: ULA 128K
				if (type_id&&!(ula_v2&32)) // 48K mode forbids further changes!
				{
					ula_v2_send(b);
					session_dirtymenu|=b&32; // show memory change on window
				}
			case 4: // 0x9FFD: ???
				break;
			case 5: // 0xBFFD: WRITE PSG REGISTER
				if (type_id||!psg_disabled) // !48K?
					psg_table_send(b);
			case 6: // 0xDFFD: ???
				break;
			case 7: // 0xFFFD: SELECT PSG REGISTER
				if (type_id||!psg_disabled) // !48K?
					psg_table_select(b);
				break;
		}
	}
}

// the emulator includes two methods to speed tapes up:
// * tape_skipload controls the physical method (disabling realtime, raising frameskip, etc. during tape operation)
// * tape_fastload controls the logical method (detecting tape loaders and feeding them data straight from the tape)

BYTE tape_fastload=1,tape_skipload=1,tape_fastskip=0,tape_feedskip=0;
BYTE z80_tape_fastindices[1<<16]; // full Z80 16-bit cache

const BYTE z80_tape_fastdumper[][24]=
{
	/*  0 */ {  -11,   4,0xDD,0x23,0x1B,0x08, +18,   7,0x7C,0xAD,0x67,0x7A,0xB3,0x20,0xCA }, // ZX SPECTRUM FIRMWARE, DINAMIC POLILOAD
	/*  1 */ {  -11,   4,0xDD,0x23,0x1B,0x08, +18,  11,0x3E,0x01,0xD3,0xFE,0x7C,0xAD,0x67,0x7A,0xB3,0x20,0xC3 }, // TOPO SOFT
	/*  2 */ {  -11,   4,0xDD,0x23,0x1B,0x08, +19,   7,0x7C,0xAD,0x67,0x7A,0xB3,0x20,0xCE }, // BLEEPLOAD
	/*  3 */ {  -11,   4,0xDD,0x23,0x1B,0x08, +18,   7,0x7C,0xAD,0x67,0x7A,0xB3,0x20,0xD1 }, // GREMLIN
};
int z80_tape_fastdump(WORD p)
{
	int i=z80_tape_fastindices[p^1]-192; // avoid clashing with z80_tape_fastfeed!
	if (i<0||i>length(z80_tape_fastdumper)) // already in the cache?
	{
		for (i=0;i<length(z80_tape_fastdumper);++i)
			if (fasttape_test(z80_tape_fastdumper[i],p))
				break;
		logprintf("FASTDUMP %04X:%02i\n",p,i<length(z80_tape_fastdumper)?i:-1);
	}
	return z80_tape_fastindices[p^1]=i+192,i;
}

const BYTE z80_tape_fastfeeder[][24]=
{
	/*  0 */ {   +0,   2,0xD0,0x3E,  +1,   4,0xB8,0xCB,0x15,0x06,  +1,   1,0xD2 }, // ZX SPECTRUM FIRMWARE
	/*  1 */ {   +0,   2,0xD0,0x3E,  +1,   4,0xB8,0xCB,0x15,0x3E,  +1,   1,0xD2 }, // ALKATRAZ
	/*  2 */ {   +0,   2,0xD0,0x3E,  +1,   1,0x3E,  +1,   4,0xB8,0xCB,0x15,0x06,  +1,   1,0xD2 }, // GREMLIN, SPEEDLOCK
	/*  3 */ {   +0,   3,0x30,0x11,0x3E,  +1,   4,0xB8,0xCB,0x15,0x06,  +1,   1,0xD2 }, // BLEEPLOAD
	/*  4 */ {   +0,   5,0xCA,0x3E,0x81,0x7B,0xFE,  +1,   5,0x3F,0xCB,0x15,0x30,0xDB }, // ABADIA
	/*  5 */ {   +0,   2,0xD0,0x3E,  +1,   4,0xB8,0xCB,0x15,0x06,  +1,   2,0x30,0xF3 }, // TIMING TESTS 48K
};
int z80_tape_fastfeed(WORD p)
{
	int i=z80_tape_fastindices[p]-128;
	if (i<0||i>length(z80_tape_fastfeeder)) // already in the cache?
	{
		for (i=0;i<length(z80_tape_fastfeeder);++i)
			if (fasttape_test(z80_tape_fastfeeder[i],p))
				break;
		logprintf("FASTFEED %04X:%02i\n",p,i<length(z80_tape_fastfeeder)?i:-1);
	}
	return z80_tape_fastindices[p]=i+128,i;
}
WORD z80_tape_spystack(void)
{
	BYTE i=PEEK(z80_sp.w+0);
	return i+(PEEK(z80_sp.w+1)<<8);
}

const BYTE z80_tape_fastloader[][24]= // format: offset relative to the PC of the trapped IN opcode, size, string of opcodes; final -128 stands for PC of the first byte
{
	/*  0 */ {   -6,   3,0x04,0xC8,0x3E,  +1,   9,0xDB,0xFE,0x1F,0xD0,0xA9,0xE6,0x20,0x28,0xF3 }, // ZX SPECTRUM FIRMWARE, POLILOAD
	/*  1 */ {   -2,  11,0xDB,0xFE,0x1F,0xE6,0x20,0xF6,0x02,0x4F,0xBF,0xC0,0xCD,  +7,   7,0x10,0xFE,0x2B,0x7C,0xB5,0x20,0xF9 }, // ZX SPECTRUM SETUP
	/*  2 */ {   -6,   3,0x04,0xC8,0x3E,  +1,   9,0xDB,0xFE,0x1F,0x00,0xA9,0xE6,0x20,0x28,0xF3 }, // DINAMIC
	/*  3 */ {   -6,   3,0x04,0xC8,0x3E,  +1,   8,0xDB,0xFE,0x1F,0xA9,0xE6,0x20,0x28,0xF4 }, // GREMLIN
	/*  4 */ {   -6,   3,0x04,0xC8,0x3E,  +1,   7,0xDB,0xFE,0xA9,0xE6,0x40,0x28,0xF5 }, // TINYTAPE/MINILOAD // tweak 20190730
	/*  5 */ {   -6,   3,0x04,0xC8,0x3E,  +1,   9,0xDB,0xFE,0xA9,0xE6,0x40,0xD8,0x00,0x28,0xF3 }, // MULTILOAD
	/*  6 */ {   -4,   8,0x1C,0xC8,0xDB,0xFE,0xE6,0x40,0xBA,0xCA,-128,  -8 }, // "ABADIA DEL CRIMEN"
	/*  7 */ {   -8,   4,0x04,0x20,0x03,0xC9,  +2,   9,0xDB,0xFE,0x1F,0xC8,0xA9,0xE6,0x20,0x28,0xF1 }, // ALKATRAZ 1/2
	/*  8 */ {   -8,   4,0x04,0x20,0x03,0xC3,  +2,   9,0xDB,0xFE,0x1F,0xC8,0xA9,0xE6,0x20,0x28,0xF1 }, // ALKATRAZ 2/2
	/*  9 */ {   -4,   9,0x04,0xC8,0xDB,0xFE,0xA9,0xE6,0x40,0x28,0xF7 }, // "TIMING TESTS 48K"
	/* 10 */ {   -6,   3,0x04,0xC8,0x3E,  +1,   9,0xDB,0xFE,0x1F,0xA9,0xD8,0xE6,0x20,0x28,0xF3 }, // "HYDROFOOL"
	/* 11 */ {   -6,   3,0x06,0xFF,0x3E,  +1,   7,0xDB,0xFE,0xE6,0x40,0xA9,0x28,0x09 }, // SPEEDLOCK 3 SETUP 2?
	/* 12 */ {   -2,   6,0xDB,0xFE,0x1F,0xE6,0x20,0x4F }, // SPEEDLOCK 3 SETUP 1?
};
INLINE void z80_tape_fastload(void)
{
	int i;
	if ((i=z80_tape_fastindices[z80_pc.w])>length(z80_tape_fastloader))
	{
		for (i=0;i<length(z80_tape_fastloader);++i)
			if (fasttape_test(z80_tape_fastloader[i],z80_pc.w))
				break;
		z80_tape_fastindices[z80_pc.w]=i;
		logprintf("FASTLOAD %04X:%02i\n",z80_pc.w,i<length(z80_tape_fastloader)?i:-1);
	}
	if (i<length(z80_tape_fastloader))
	{
		tape_fastskip=1;
		if (i!=5) // MULTILOAD expects IRQs!
			z80_irq=0;
		if (tape_enabled>=0&&tape_enabled<2)
			tape_enabled=2; // amount of frames the tape should keep moving
	}
	switch (i) // tape setup routines are always handled
	{
		case  1: // ZX SPECTRUM SETUP
			z80_pc.w+=23; // skip delay
			tape_enabled|=15; // abridge setup
			break;
		case 11: // SPEEDLOCK 3 SETUP 2?
		case 12: // SPEEDLOCK 3 SETUP 1?
			tape_enabled|=127; // play tape and listen to decryption noises
			break;
	}
	if (tape_fastload) // examine and speedup only when this flag is on
	{
		int j;
		switch (i) // is it a tape loader that we can optimize?
		{
			case  0: // ZX SPECTRUM FIRMWARE : (4+5+7+11+4+5+4+7+12+2)/4=15
			case 10: // "HYDROFOOL" : (4+5+7+11+4+4+5+7+12+2)/4=15
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==0)
				{
					if (((j=z80_tape_fastdump(z80_tape_spystack()))==0||j==1)&&(z80_af2.b.l&0x41)==0x41)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.b.l;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>5,59);
				}
				else
					z80_r7+=fasttape_add8(z80_bc.b.l>>5,59,&z80_bc.b.h,1)*9;
				break;
			case  2: // DINAMIC ("ARMY MOVES"), ANIMAGIC ("CYBERBIG"), BLEEPLOAD ("BLACK LAMP")... : (4+5+7+11+4+4+4+7+12+2)/4=15
			case  7: // ALKATRAZ 1/2 ("HATE") : (4+12+11+4+4+4+7+12+2)/4=15
			case  8: // ALKATRAZ 2/2 : (4+12+11+4+4+4+7+12+2)/4=15
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==0||i==1||i==3))
				{
					if (z80_tape_fastdump(z80_tape_spystack())==0&&(z80_af2.b.l&0x41)==0x41)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.b.l;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>5,58);
				}
				else
					z80_r7+=fasttape_add8(z80_bc.b.l>>5,58,&z80_bc.b.h,1)*9;
				break;
			case  3: // GREMLIN ("THING BOUNCES BACK"), PSS ("THE COVENANT") : (4+5+7+11+4+4+7+12+2)/4=14
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==0||i==2))
				{
					if (((j=z80_tape_fastdump(z80_tape_spystack()))==0||j==1||j==3)&&(z80_af2.b.l&0x41)==0x41)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.b.l;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>5,54);
				}
				else
					z80_r7+=fasttape_add8(z80_bc.b.l>>5,54,&z80_bc.b.h,1)*8;
				break;
			case  4: // TINYTAPE/MINILOAD ("JUSTIN") : (4+5+7+11+4+7+12+2)/4=13
				fasttape_add8(z80_bc.b.l>>6,50,&z80_bc.b.h,1);
				break;
			case  5: // MULTILOAD ("FINAL MATRIX") : (4+5+7+11+4+7+5+4+12+2)/4=15
				fasttape_add8(z80_bc.b.l>>6,59,&z80_bc.b.h,1);
				break;
			case  6: // "ABADIA DEL CRIMEN" : (4+5+11+7+4+10+2)/4=10
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==4)
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_de.b.l=j&1?-1:0,FASTTAPE_FEED_END(z80_de.b.h>>6,41);
				else
					fasttape_add8(z80_de.b.h>>6,41,&z80_de.b.l,1);
				break;
			case  9: // "TIMING TESTS 48K" : (4+5+11+4+7+12+2)/4=11
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==5)
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>6,43);
				else
					fasttape_add8(z80_bc.b.l>>6,43,&z80_bc.b.h,1);
				break;
		}
		//if (n) logprintf("[%02i:x%03i] ",i,n);
	}
}

BYTE z80_recv(WORD p) // the Z80 receives a byte from a hardware port
{
	BYTE b=0xFF;
	if ((p&63)==31) // KEMPSTON joystick
		b=autorun_kbd_bit[8];
	else if ((p&15)==14) // 0x??FE, ULA 48K
	{
		int i,j,k=autorun_kbd_bit[11]||autorun_kbd_bit[12]||autorun_kbd_bit[15];
		for (i=j=0;i<8;++i)
		{
			if (!(p&(256<<i)))
			{
				j|=autorun_kbd_bit[i]; // bits 0-4: keyboard rows
				if (!i)
					j|=k; // CAPS SHIFT: row 0, bit 0
				else
					j|=autorun_kbd_bit[8+i]; // other rows
			}
		}
		b=~j;
		if (tape&&!z80_iff.b.l) // does any tape loader enable interrupts at all???
			//if (tape_fastload) // call always, although not always for speedup
				z80_tape_fastload();
		if (tape&&tape_enabled)
		{
			if (!tape_status)
				b&=~64; // bit 6: tape input state
		}
		else
			b&=type_id?~64:ula_v1_issue; // 128k and later always show 0 when no tape is playing. Old versions of "Abu Simbel Profanation" require 1.
	}
	else if ((p&15)==13) // 0x??FD, MULTIPLE DEVICES
		switch (p>>12)
		{
			case 2: // 0x2FFD: FDC STATUS
				if (type_id==3&&!disc_disabled) // PLUS3?
					b=disc_data_info();
				break;
			case 3: // 0x3FFD: FDC DATA I/O
				if (type_id==3&&!disc_disabled) // PLUS3?
					b=disc_data_recv();
				break;
			case 15: // 0xFFFD: READ PSG REGISTER
				if (type_id||!psg_disabled) // !48K?
					b=psg_table_recv();
				break;
		}
	else if ((p&255)==255&&type_id<3) // NON-PLUS3: FLOATING BUS
		b=ula_temp; // not completely equivalent to z80_bus()
	//else logprintf("%04X! ",p);
	return b;
}

#ifdef DEBUG
#define z80_info(q) printf("\n%ULAs: 48k $%02X / 128k $%02X / PLUS3 $%02X\n",ula_v1,ula_v2,ula_v3) // prints a short hardware dump from the debugger
#else
int z80_debug_hard_tab(char *t)
{
	return sprintf(t,"    ");
}
int z80_debug_hard1(char *t,int q,BYTE i)
{
	return q?sprintf(t,":%02X",i):sprintf(t,":--");
}
void z80_debug_hard(int q,int x,int y)
{
	char s[16*20],*t;
	t=s+sprintf(s,"ULA:                ""    %04X:%02X %02X",(WORD)(ula_bitmap-mem_ram),(BYTE)ula_temp,ula_v1);
	t+=z80_debug_hard1(t,type_id,ula_v2);
	t+=z80_debug_hard1(t,type_id==3,ula_v3);
	t+=sprintf(t,"    %04i  T: %05i:%c",
		1000*(ula_clash_mreq[0]!=ula_clash[0])+100*(ula_clash_mreq[1]!=ula_clash[0])+10*(ula_clash_mreq[2]!=ula_clash[0])+(ula_clash_mreq[3]!=ula_clash[0]),
		ula_clash_z,48+ula_clash_mreq[4][(WORD)ula_clash_z]);
	int i;
	t+=sprintf(t,"PSG:                ""%02X: ",psg_index);
	for (i=0;i<8;++i)
		t+=sprintf(t,"%02X",psg_table[i]);
	t+=z80_debug_hard_tab(t);
	for (;i<16;++i)
		t+=sprintf(t,"%02X",psg_table[i]);
	t+=sprintf(t,"FDC:  %02X - %04X:%04X""    %c ",disc_parmtr[0],(WORD)disc_offset,(WORD)disc_length,48+disc_phase);
	for (i=0;i<7;++i)
		t+=sprintf(t,"%02X",disc_result[i]);
	char *r=t;
	for (t=s;t<r;++y)
	{
		debug_locate(x,y);
		MEMNCPY(debug_output,t,20);
		t+=20;
	}
}
#endif

// CPU: ZILOG Z80 MICROPROCESSOR ==================================== //

#define Z80_MREQ_PAGE(t,p) ( z80_t+=(z80_aux2=(t+ula_clash_mreq[p][(WORD)ula_clash_z])), ula_clash_z+=z80_aux2 )
#define Z80_IORQ_PAGE(t,p) ( z80_t+=(z80_aux2=(t+ula_clash_iorq[p][(WORD)ula_clash_z])), ula_clash_z+=z80_aux2 )
// input/output
#define Z80_SYNC_IO ( _t_-=z80_t, z80_sync(z80_t), z80_t=0 )
#define Z80_PRAE_RECV(w) do{ Z80_IORQ(1,w); if (w&1) Z80_IORQ_1X(3,w); else Z80_MREQ_PAGE(3,4); Z80_SYNC_IO; }while(0)
#define Z80_RECV z80_recv
#define Z80_POST_RECV(w)
#define Z80_PRAE_SEND(w) ( Z80_IORQ(1,w), Z80_SYNC_IO )
#define Z80_SEND z80_send
#define Z80_POST_SEND(w) do{ if (w&1) Z80_IORQ_1X(3,w); else Z80_MREQ_PAGE(3,4); }while(0)
// fine timings
#define Z80_AUXILIARY int z80_aux1,z80_aux2 // making it local performs better :-)
#define Z80_MREQ(t,w) Z80_MREQ_PAGE(t,z80_aux1=((w)>>14))
#define Z80_MREQ_1X(t,w) do{ if (type_id<3) { z80_aux1=(w)>>14; for (int z80_auxx=0;z80_auxx<t;++z80_auxx) Z80_MREQ_PAGE(1,z80_aux1); } else Z80_MREQ(t,w); }while(0)
#define Z80_MREQ_NEXT(t) Z80_MREQ_PAGE(t,z80_aux1) // when the very last z80_aux1 is the same as the current one
#define Z80_MREQ_1X_NEXT(t) do{ if (type_id<3) for (int z80_auxx=0;z80_auxx<t;++z80_auxx) Z80_MREQ_PAGE(1,z80_aux1); else Z80_MREQ_NEXT(t); }while(0)
#define Z80_IORQ(t,w) Z80_IORQ_PAGE(t,z80_aux1=((w)>>14))
#define Z80_IORQ_1X(t,w) do{ z80_aux1=(w)>>14; for (int z80_auxx=0;z80_auxx<t;++z80_auxx) Z80_IORQ_PAGE(1,z80_aux1); }while(0) // Z80_IORQ is always ZERO on type_id==3
#define Z80_IORQ_NEXT(t) Z80_IORQ_PAGE(t,z80_aux1) // when the very last z80_aux1 is the same as the current one
#define Z80_IORQ_1X_NEXT(t) do{ for (int z80_auxx=0;z80_auxx<t;++z80_auxx) Z80_IORQ_PAGE(1,z80_aux1); }while(0) // Z80_IORQ is always ZERO on type_id==3
#define Z80_PEEK(w) ( Z80_MREQ(3,w), mmu_rom[z80_aux1][w] )
#define Z80_PEEK1 Z80_PEEK
#define Z80_PEEK2 Z80_PEEK
#define Z80_PEEKX(w) ( Z80_MREQ(4,w), mmu_rom[z80_aux1][w] )
#define Z80_POKE(w,b) ( Z80_MREQ(3,w), mmu_ram[z80_aux1][w]=b ) // cfr. Z80_SYNC_IO
#define Z80_POKE0 Z80_POKE
#define Z80_POKE1 Z80_POKE
#define Z80_POKE2 Z80_POKE
#define Z80_WAIT(t) ( z80_t+=t, ula_clash_z+=t )
#define Z80_BEWARE int z80_t_=z80_t,ula_clash_z_=ula_clash_z
#define Z80_REWIND z80_t=z80_t_,ula_clash_z=ula_clash_z_
// coarse timings
#define Z80_STRIDE(o)
#define Z80_STRIDE_0
#define Z80_STRIDE_1
#define Z80_STRIDE_X(o)
#define Z80_STRIDE_IO(o)
#define Z80_STRIDE_HALT 4

#define Z80_DEBUG_LEN 16 // height of disassemblies, dumps and searches
#define Z80_DEBUG_MMU 0 // forbid ROM/RAM toggling, it's useless on Spectrum
#define Z80_DEBUG_EXT 0 // forbid EXTRA hardware debugging info pages
#define Z80_DEBUG_SCAN (ula_limit_x*4) // amount of ticks per scanline
#define z80_out0() 0 // hardware sets whether OUT (C) sends 0 or 255

#include "cpcec-z8.h"

// EMULATION CONTROL ================================================ //

char txt_error[]="Error!";
char txt_error_any_load[]="Cannot open file!";

// emulation setup and reset operations ----------------------------- //

void all_setup(void) // setup everything!
{
	ula_setup();
	tape_setup();
	disc_setup();
	psg_setup();
	z80_setup();
}
BYTE snap_done; // avoid accidents with ^F2, see below
void all_reset(void) // reset everything!
{
	MEMZERO(autorun_kbd);
	ula_reset();
	if (type_id<3)
		ula_v3_send(4); // disable PLUS3!
	if (!type_id)
		ula_v2_send(48); // enable 48K!
	tape_enabled=0;
	tape_reset();
	disc_reset();
	psg_reset();
	z80_reset();
	z80_debug_reset();
	snap_done=0; // avoid accidents!
	MEMFULL(z80_tape_fastindices);
}

// firmware ROM file handling operations ---------------------------- //

BYTE biostype_id=-1; // keeper of the latest loaded BIOS type
char bios_system[][13]={"SPECTRUM.ROM","SPEC128K.ROM","SPEC-P-2.ROM","SPEC-P-3.ROM"};
char bios_path[STRMAX]="";

int bios_load(char *s) // load ROM. `s` path; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb");
	if (!f)
		return 1;
	int i,j;
	fseek(f,0,SEEK_END); i=ftell(f);
	if (i>=0x4000)
		fseek(f,i-0x4000+0x0601,SEEK_SET),j=fgetmmmm(f);
	if ((i!=(1<<14)&&i!=(1<<15)&&i!=(1<<16))||j!=0xD3FE37C9) // 16/32/64k + Spectrum TAPE LOAD fingerprint
		return puff_fclose(f),1; // reject!
	fseek(f,0,SEEK_SET);
	fread1(mem_rom,sizeof(mem_rom),f);
	puff_fclose(f);
	if (i<(1<<15))
		memcpy(&mem_rom[1<<14],mem_rom,1<<14); // mirror 16k ROM up
	if (i<(1<<16))
		memcpy(&mem_rom[1<<15],mem_rom,1<<15); // mirror 32k ROM up
	biostype_id=type_id=i>(1<<14)?i>(1<<15)?3:mem_rom[0x5540]=='A'?2:1:0; // "Amstrad" (PLUS2) or "Sinclair" (128K)?
	//logprintf("Firmware %ik m%i\n",i>>10,type_id);
	if (bios_path!=s&&(char*)session_substr!=s)
		strcpy(bios_path,s);
	return 0;
}
int bios_path_load(char *s) // ditto, but from the base path
{
	return bios_load(strcat(strcpy(session_substr,session_path),s));
}
int bios_reload(void) // ditto, but from the current type_id
{
	return type_id>3?1:biostype_id==type_id?0:bios_load(strcat(strcpy(session_substr,session_path),bios_system[type_id]));
}

// snapshot file handling operations -------------------------------- //

char snap_path[STRMAX]="";
int snap_extended=1; // flexible behavior (i.e. 128k-format snapshots with 48k-dumps)
int snap_is_a_sna(char *s)
{
	return !globbing("*.z80",s,1);
}

#define SNAP_SAVE_Z80W(x,r) header[x]=r.b.l,header[x+1]=r.b.h
int snap_save(char *s) // save a snapshot. `s` path, NULL to resave; 0 OK, !0 ERROR
{
	if (!snap_is_a_sna(s))
		return 1; // cannot save Z80!
	FILE *f=fopen(s,"wb");
	if (!f)
		return 1;
	BYTE header[27]; int i=z80_sp.w;
	if (!snap_extended&&(ula_v2&32)) // strict and 48k!?
	{
		i=(WORD)(i-1); POKE(i)=z80_pc.b.h;
		i=(WORD)(i-1); POKE(i)=z80_pc.b.l;
	}
	header[0]=z80_ir.b.h;
	SNAP_SAVE_Z80W(1,z80_hl2);
	SNAP_SAVE_Z80W(3,z80_de2);
	SNAP_SAVE_Z80W(5,z80_bc2);
	SNAP_SAVE_Z80W(7,z80_af2);
	SNAP_SAVE_Z80W(9,z80_hl);
	SNAP_SAVE_Z80W(11,z80_de);
	SNAP_SAVE_Z80W(13,z80_bc);
	SNAP_SAVE_Z80W(15,z80_iy);
	SNAP_SAVE_Z80W(17,z80_ix);
	header[19]=z80_iff.b.l?4:0;
	header[20]=z80_ir.b.l;
	SNAP_SAVE_Z80W(21,z80_af);
	header[23]=i; header[24]=i>>8;
	header[25]=z80_imd&3;
	header[26]=ula_v1&7;
	fwrite1(header,27,f);
	fwrite1(&mem_ram[0x14000],1<<14,f); // bank 5
	fwrite1(&mem_ram[0x08000],1<<14,f); // bank 2
	fwrite1(&mem_ram[(ula_v2&7)<<14],1<<14,f); // active bank
	if (snap_extended||!(ula_v2&32)) // relaxed or 128k?
	{
		SNAP_SAVE_Z80W(0,z80_pc);
		header[2]=ula_v2;
		header[3]=type_id==3?ula_v3:4; // using this byte for Plus3 config is both questionable and unreliable; most emus write 0 here :-(
		fwrite1(header,4,f);
		if (!(ula_v2&32)) // 128k?
			for (i=0;i<8;++i)
				if (i!=5&&i!=2&&i!=(ula_v2&7))
					fwrite1(&mem_ram[i<<14],1<<14,f); // save all remaining banks
	}
	if (s!=snap_path)
		strcpy(snap_path,s);
	return snap_done=!fclose(f),0;
}

#define SNAP_LOAD_Z80W(x,r) r.b.l=header[x],r.b.h=header[x+1]
int snap_load_z80block(FILE *f,int length,int offset,int limits) // unpack a Z80-type compressed block; 0 OK, !0 ERROR
{
	int x,y;
	while (length-->0&&offset<limits)
	{
		if ((x=fgetc(f))==0xED)
		{
			--length;
			if ((y=fgetc(f))==0xED)
			{
				length-=2;
				x=fgetc(f);
				y=fgetc(f);
				while (x--)
					mem_ram[offset++]=y;
			}
			else
			{
				mem_ram[offset++]=x;
				mem_ram[offset++]=y;
			}
		}
		else
			mem_ram[offset++]=x;
	}
	return offset!=limits;
}
int snap_load(char *s) // load a snapshot. `s` path, NULL to reload; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb");
	if (!f)
		return 1;
	BYTE header[96]; int i,q;
	fseek(f,0,SEEK_END); i=ftell(f); // snapshots are defined by filesizes, there's no magic number!
	if ((q=snap_is_a_sna(s))&&i!=49179&&i!=49183&&i!=131103&&i!=147487) // ||(!memcmp("MV - SNA",header,8) fails on TAP files, for example
		return puff_fclose(f),1;
	fseek(f,0,SEEK_SET);
	ula_v3_send(4); // disable PLUS3!
	ula_v2_send(48); // enable 48K!
	if (!q) // Z80 format
	{
		logprintf("Z80 snapshot",i);
		fread1(header,30,f);
		z80_af.b.h=header[0];
		z80_af.b.l=header[1];
		SNAP_LOAD_Z80W(2,z80_bc);
		SNAP_LOAD_Z80W(4,z80_hl);
		SNAP_LOAD_Z80W(6,z80_pc);
		SNAP_LOAD_Z80W(8,z80_sp);
		z80_ir.b.h=header[10];
		z80_ir.b.l=header[11]&127;
		if (header[12]&1)
			z80_ir.b.l+=128;
		ula_v1_send((header[12]>>1)&7);
		SNAP_LOAD_Z80W(13,z80_de);
		SNAP_LOAD_Z80W(15,z80_bc2);
		SNAP_LOAD_Z80W(17,z80_de2);
		SNAP_LOAD_Z80W(19,z80_hl2);
		z80_af2.b.h=header[21];
		z80_af2.b.l=header[22];
		SNAP_LOAD_Z80W(23,z80_iy);
		SNAP_LOAD_Z80W(25,z80_ix);
		z80_iff.w=header[27]?0x0101:0;
		z80_imd=header[29]&3;
		if (header[12]!=255&&!z80_pc.w) // HEADER V2+
		{
			i=fgetii(f);
			logprintf(" (+%i)",i);
			if (i<=64)
				fread1(&header[32],i,f);
			else
				fread1(&header[32],64,f),fseek(f,i-64,SEEK_CUR); // ignore extra data
			SNAP_LOAD_Z80W(32,z80_pc);
			q=type_id;
			if (header[34]<3) // 48K system?
				psg_reset(),type_id=0;
			else
			{
				if (!type_id)
					type_id=1; // 48K? 128K!
				ula_v2_send(header[35]); // 128K/PLUS2 configuration
				psg_table_select(header[38]);
				MEMLOAD(psg_table,&header[39]);
				if (i>54&&header[34]>6)
					type_id=3,ula_v3_send(header[86]); // PLUS3 configuration
				else if (type_id==3)
					type_id=2; // reduce PLUS3 to PLUS2
			}
			if (q!=type_id)
				bios_reload(); // reload BIOS if required!
			while ((i=fgetii(f))&&(q=fgetc(f),!feof(f)))
			{
				logprintf(" %05i:%03i",i,q);
				if ((q-=3)<0||q>7) // ignore ROM copies
					q=8; // dummy 16K
				else if (!type_id) // 128K banks follow the normal order,
					switch(q) // but 48K banks are stored differently
					{
						case 5:
							q=5;
							break;
						case 1:
							q=2;
							break;
						case 2:
							q=0;
							break;
						default:
							q=8; // dummy 16K
							break;
					}
				if (i==0xFFFF) // uncompressed
					fread1(&mem_ram[q<<14],1<<14,f);
				else if (snap_load_z80block(f,i,q<<14,(q+1)<<14))
					logprintf(" ERROR!");
			}
		}
		else // HEADER V1
		{
			if (header[12]&32) // compressed V1
			{
				logprintf(" PACKED");
				if (snap_load_z80block(f,i,5<<14,8<<14))
					logprintf(" ERROR!");
				//memcpy(&mem_ram[5<<14],&mem_ram[5<<14],1<<14);
				memcpy(&mem_ram[2<<14],&mem_ram[6<<14],1<<14);
				memcpy(&mem_ram[0<<14],&mem_ram[7<<14],1<<14);
			}
			else // uncompressed 48K body
			{
				fread1(&mem_ram[0x14000],1<<14,f); // bank 5
				fread1(&mem_ram[0x08000],1<<14,f); // bank 2
				fread1(&mem_ram[0x00000],1<<14,f); // bank 0
			}
			if (type_id)
				type_id=0,bios_reload();
		}
		logprintf(" PC=$%04X\n",z80_pc.w);
	}
	else // SNA format
	{
		fread1(header,27,f);
		z80_ir.b.h=header[0];
		SNAP_LOAD_Z80W(1,z80_hl2);
		SNAP_LOAD_Z80W(3,z80_de2);
		SNAP_LOAD_Z80W(5,z80_bc2);
		SNAP_LOAD_Z80W(7,z80_af2);
		SNAP_LOAD_Z80W(9,z80_hl);
		SNAP_LOAD_Z80W(11,z80_de);
		SNAP_LOAD_Z80W(13,z80_bc);
		SNAP_LOAD_Z80W(15,z80_iy);
		SNAP_LOAD_Z80W(17,z80_ix);
		z80_iff.w=(header[19]&4)?0x0101:0;
		z80_ir.b.l=header[20];
		SNAP_LOAD_Z80W(21,z80_af);
		SNAP_LOAD_Z80W(23,z80_sp);
		z80_imd=header[25]&3;
		ula_v1_send(header[26]&7);
		fread1(&mem_ram[0x14000],1<<14,f); // bank 5
		fread1(&mem_ram[0x08000],1<<14,f); // bank 2
		fread1(&mem_ram[0x00000],1<<14,f); // bank 0
		if (fread1(header,4,f)==4) // new snapshot type, 128k-ready?
		{
			SNAP_LOAD_Z80W(0,z80_pc);
			ula_v2_send(header[2]); // set 128k mode
			if (!(header[2]&32))
				ula_v3_send(header[3]); // this is unreliable! most emulators don't handle Plus3!
			MEMNCPY(&mem_ram[(ula_v2&7)<<14],&mem_ram[0x00000],1<<14); // move bank 0 to active bank
			for (i=0;i<8;++i)
				if (i!=5&&i!=2&&i!=(ula_v2&7))
					if (fread1(&mem_ram[i<<14],1<<14,f)&&!type_id) // load all following banks, if any
						type_id=1,bios_reload(); // if 48K but extra ram, set 128K!
		}
		else // old snapshot type, 48k only: perform a RET
		{
			z80_pc.b.l=PEEK(z80_sp.w); /*POKE(z80_sp.w)=0;*/ ++z80_sp.w;
			z80_pc.b.h=PEEK(z80_sp.w); /*POKE(z80_sp.w)=0;*/ ++z80_sp.w;
			for (i=8;i<11;++i)
				psg_table_select(i),psg_table_send(0); // mute channels
		}
	}
	z80_irq=z80_iff0=z80_halted=0; // avoid nasty surprises!
	mmu_update(); // adjust RAM and ULA models
	psg_all_update();
	z80_debug_reset();
	if (snap_path!=s)
		strcpy(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

// "autorun" file and logic operations ------------------------------ //

int any_load(char *s,int q) // load a file regardless of format. `s` path, `q` autorun; 0 OK, !0 ERROR
{
	autorun_t=autorun_mode=0; // cancel any autoloading yet
	if (snap_load(s))
	{
		if (bios_load(s))
		{
			if (tape_open(s))
			{
				if (disc_open(s,0,0))
					return 1; // everything failed!
				if (q)
					type_id=3,disc_disabled=0,tape_close(); // open disc? force PLUS3, enable disc, close tapes!
			}
			else if (q)
				type_id=(type_id>2?2:type_id),disc_disabled|=2,disc_close(0),disc_close(1); // open tape? force PLUS2 if PLUS3, close disc!
			if (q) // autorun for tape and disc
			{
				all_reset(),bios_reload();
				autorun_mode=type_id?8:1; // only 48k needs typing
				autorun_t=((type_id==3&&!disc_disabled)?-144:(type_id?-72:-96)); // PLUS3 slowest, 128+PLUS2 fast, 48k slow
			}
		}
		else // load bios? reset!
			all_reset(),autorun_mode=0,disc_disabled&=~2;//,tape_close(),disc_close(0),disc_close(1);
	}
	if (autorun_path!=s&&q)
		strcpy(autorun_path,s);
	return 0;
}

// auxiliary user interface operations ------------------------------ //

BYTE onscreen_flag=1;

BYTE joy1_type=2;
BYTE joy1_types[][5]={
	{ 0x43,0x42,0x41,0x40,0x44 }, // Kempston
	{ 0x1B,0x1A,0x18,0x19,0x1C }, // Sinclair 1
	{ 0x21,0x22,0x24,0x23,0x20 }, // Sinclair 2, Interface II
	{ 0x23,0x24,0x1C,0x22,0x20 }, // Cursor, Protek, AGF
	{ 0x10,0x08,0x29,0x28,0x38 }, // QAOP+Space
};

char txt_error_snap_save[]="Cannot save snapshot!";
char snap_pattern[]="*.sna";

char session_menudata[]=
	"File\n"
	"0x8300 Open any file..\tF3\n"
	"0xC300 Load snapshot..\tShift+F3\n"
	"0x0300 Load last snapshot\tCtrl+F3\n"
	"0x8200 Save snapshot..\tF2\n"
	"0x0200 Save last snapshot\tCtrl+F2\n"
	"=\n"
	"0x8700 Insert disc into A:..\tF7\n"
	"0x8701 Create disc in A:..\n"
	"0x0700 Remove disc from A:\tCtrl+F7\n"
	"0x0701 Flip disc sides in A:\n"
	"0xC700 Insert disc into B:..\tShift+F7\n"
	"0xC701 Create disc in B:..\n"
	"0x4700 Remove disc from B:\tCtrl+Shift+F7\n"
	"0x4701 Flip disc sides in B:\n"
	"=\n"
	"0x8800 Insert tape..\tF8\n"
	"0xC800 Record tape..\tShift+F8\n"
	"0x0800 Remove tape\tCtrl+F8\n"
	"0x8801 Browse tape..\n"
	"0x4800 Play tape\tCtrl+Shift+F8\n"
	"=\n"
	"0x7F00 E_xit\n"
	"Edit\n"
	"0x8500 Select firmware..\tF5\n"
	"0x0500 Reset emulation\tCtrl+F5\n"
	"0x8F00 Pause\tPause\n"
	"0x8900 Debug\tF9\n"
	"=\n"
	"0x8600 Realtime\tF6\n"
	"0x0601 100% CPU speed\n"
	"0x0602 200% CPU speed\n"
	"0x0603 300% CPU speed\n"
	"0x0604 400% CPU speed\n"
	//"0x0600 Raise Z80 speed\tCtrl+F6\n"
	//"0x4600 Lower Z80 speed\tCtrl+Shift+F6\n"
	"=\n"
	"0x0900 Tape speed-up\tCtrl+F9\n"
	"0x4900 Tape analysis\tCtrl+Shift+F9\n"
	"0x0901 Tape auto-rewind\n"
	"0x0400 Virtual joystick\tCtrl+F4\n"
	"Settings\n"
	"0x8501 Kempston joystick\n"
	"0x8502 Sinclair 1 joystick\n"
	"0x8503 Sinclair 2 joystick\n"
	"0x8504 Cursor joystick\n"
	"0x8505 QAOP+Space joystick\n"
	"=\n"
	"0x8510 Disc controller\n"
	"0x8511 Memory contention\n"
	"0x8512 ULA video noise\n"
	"0x8513 Strict SNA files\n"
	"Video\n"
	"0x8901 Onscreen status\tShift+F9\n"
	"0x8904 Interpolation\n"
	"0x8903 X-Masking\n"
	"0x8902 Y-Masking\n"
	"=\n"
	"0x8B01 Monochrome\n"
	"0x8B02 Dark palette\n"
	"0x8B03 Normal palette\n"
	"0x8B04 Bright palette\n"
	"0x8B05 Green screen\n"
	//"0x8B00 Next palette\tF11\n"
	//"0xCB00 Prev. palette\tShift+F11\n"
	"=\n"
	//"0x0B00 Next scanline\tCtrl+F11\n"
	//"0x4B00 Prev. scanline\tCtrl+Shift+F11\n"
	"0x0B01 All scanlines\n"
	"0x0B02 Half scanlines\n"
	"0x0B03 Simple interlace\n"
	"0x0B04 Double interlace\n"
	"=\n"
	"0x9100 Raise frameskip\tNum.+\n"
	"0x9200 Lower frameskip\tNum.-\n"
	"0x9300 Full frameskip\tNum.*\n"
	"0x9400 No frameskip\tNum./\n"
	"=\n"
	"0x8C00 Save screenshot\tF12\n"
	"Audio\n"
	"0x8400 Sound\tF4\n"
	"0x8401 No interpolation\n"
	"0x8402 Light interpolation\n"
	"0x8403 Middle interpolation\n"
	"0x8404 Heavy interpolation\n"
	"=\n"
	"0x0C00 Record WAV file\tCtrl+F12\n"
	"0x4C00 Record YM file\tCtrl+Shift+F12\n"
	"Help\n"
	"0x8100 Help..\tF1\n"
	"0x0100 About..\tCtrl+F1\n"
	"";

void session_menuinfo(void)
{
	session_menucheck(0x8F00,session_signal&SESSION_SIGNAL_PAUSE);
	session_menucheck(0x8900,session_signal&SESSION_SIGNAL_DEBUG);
	session_menucheck(0x8400,!(audio_disabled&1));
	session_menuradio(0x8401+audio_filter,0x8401,0x8404);
	session_menucheck(0x8600,!(session_fast&1));
	session_menucheck(0x0C00,!!session_wavefile);
	session_menucheck(0x4C00,!!psg_logfile);
	session_menucheck(0x8700,!!disc[0]);
	session_menucheck(0xC700,!!disc[1]);
	session_menucheck(0x0701,disc_flip[0]);
	session_menucheck(0x4701,disc_flip[1]);
	session_menucheck(0x8800,tape_type>=0&&tape);
	session_menucheck(0xC800,tape_type<0&&tape);
	session_menucheck(0x4800,tape_enabled<0);
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	session_menucheck(0x0400,session_key2joy);
	session_menuradio(0x0601+z80_turbo,0x0601,0x0604);
	session_menuradio(0x8501+joy1_type,0x8501,0x8505);
	session_menucheck(0x8510,!(disc_disabled&1));
	session_menucheck(0x8511,!(ula_clash_disabled));
	session_menucheck(0x8512,!(ula_snow_disabled));
	session_menucheck(0x8513,!(snap_extended));
	session_menucheck(0x8901,onscreen_flag);
	session_menuradio(0x8B01+video_type,0x8B01,0x8B05);
	session_menuradio(0x0B01+video_scanline,0x0B01,0x0B04);
	session_menucheck(0x8902,video_filter&VIDEO_FILTER_Y_MASK);
	session_menucheck(0x8903,video_filter&VIDEO_FILTER_X_MASK);
	session_menucheck(0x8904,video_filter&VIDEO_FILTER_SMUDGE);
	MEMLOAD(kbd_joy,joy1_types[joy1_type]);
	z80_multi=1+z80_turbo; // setup overclocking
	sprintf(session_info,"%i:%s ULAv%c %gMHz"//" | disc %s | tape %s | %s"
		,(!type_id||(ula_v2&32))?48:128,type_id?(type_id!=1?(type_id!=2?(!disc_disabled?"Plus3":"Plus2A"):"Plus2"):"128K"):"48K",ula_clash_disabled?'0':type_id?type_id>2?'3':'2':'1',3.5*z80_multi);
}
int session_user(int k) // handle the user's commands; 0 OK, !0 ERROR
{
	char *s;
	switch (k)
	{
		case 0x8100: // F1: HELP..
			session_message(
				"F1\tHelp..\t\t"
				"^F1\tAbout..\t" // "\t"
				"\n"
				"F2\tSave snapshot..\t"
				"^F2\tSave last snapshot" // "\t"
				"\n"
				"F3\tLoad any file..\t"
				"^F3\tLoad last snapshot" // "\t"
				"\n"
				"F4\tToggle sound\t"
				"^F4\tToggle joystick" // "\t"
				"\n"
				"F5\tLoad firmware..\t"
				"^F5\tReset emulation" // "\t"
				"\n"
				"F6\tToggle realtime\t"
				"^F6\tToggle turbo Z80" // "\t"
				"\n"
				"F7\tInsert disc into A:..\t"
				"^F7\tEject disc from A:" // "\t"
				"\n"
				"\t(shift: ..into B:)\t"
				"\t(shift: ..from B:)" // "\t"
				"\n"
				"F8\tInsert tape..\t"
				"^F8\tRemove tape" // "\t"
				"\n"
				"\t(shift: record..)\t"
				"\t(shift: play/stop)\t" //"\t-\t" // "\t"
				"\n"
				#ifdef DEBUG
				"F9\tDebug\t\t"
				#else
				"F9\t-\t\t"
				#endif
				"^F9\tToggle fast tape" // "\t"
				"\n"
				"\t(shift: view status)\t"
				"\t(shift: ..fast load)" // "\t"
				"\n"
				"F11\tNext palette\t"
				"^F11\tNext scanlines" // "\t"
				"\n"
				"\t(shift: previous..)\t"
				"\t(shift: ..filter)" // "\t"
				"\n"
				"F12\tSave screenshot\t"
				"^F12\tRecord wavefile" // "\t"
				"\n"
				"\t-\t\t"
				"\t(shift: ..YM3 file)" // "\t"
				"\n"
				"\n"
				"Num.+\tRaise frameskip\t"
				"Num.*\tFull frameskip" // "\t"
				"\n"
				"Num.-\tLower frameskip\t"
				"Num./\tNo frameskip" // "\t"
				"\n"
				"Pause\tPause/continue\t"
				"*Return\tMaximize/restore" // "\t"
				"\n"
			,"Help");
			break;
		case 0x0100: // ^F1: ABOUT..
			session_aboutme(
				"ZX Spectrum emulator written by Cesar Nicolas-Gonzalez\n"
				"for the UNED 2019 Master's Degree in Computer Engineering.\n"
				"\nhome page, news and updates: http://cngsoft.no-ip.org/cpcec.htm"
				"\n\n" MY_LICENSE "\n\n" GPL_3_INFO
			,caption_version);
			break;
		case 0x8200: // F2: SAVE SNAPSHOT..
			if (s=puff_session_newfile(snap_path,snap_pattern,"Save snapshot"))
				if (snap_save(s))
					session_message(txt_error_snap_save,txt_error);
			break;
		case 0x0200: // ^F2: RESAVE SNAPSHOT
			if (*snap_path&&snap_done)
				if (snap_save(snap_path))
					session_message(txt_error_snap_save,txt_error);
			break;
		case 0x8300: // F3: LOAD ANY FILE..
			if (puff_session_getfile(session_shift?snap_path:autorun_path,session_shift?"*.sna;*.z80":"*.sna;*.z80;*.rom;*.dsk;*.tap;*.tzx;*.csw;*.wav",session_shift?"Load snapshot":"Load file"))
		case 0x8000: // DRAG AND DROP
			if (any_load(session_parmtr,!session_shift))
				session_message(txt_error_any_load,txt_error);
			break;
		case 0x0300: // ^F3: RELOAD SNAPSHOT
			if (*snap_path)
				if (snap_load(snap_path))
					session_message("Cannot load snapshot!",txt_error);
			break;
		case 0x8400: // F4: TOGGLE SOUND
			if (session_audio)
				audio_disabled^=1;
			break;
		case 0x8401:
		case 0x8402:
		case 0x8403:
		case 0x8404:
			audio_filter=(k&15)-1;
			break;
		case 0x0400: // ^F4: TOGGLE JOYSTICK
			session_key2joy=!session_key2joy;
			break;
		case 0x8501:
		case 0x8502:
		case 0x8503:
		case 0x8504:
		case 0x8505:
			joy1_type=(k&15)-1; // 0,1,2,3,4
			break;
		case 0x8510:
			disc_disabled^=1;
			break;
		case 0x8511:
			ula_clash_disabled=!ula_clash_disabled;
			mmu_update();
			break;
		case 0x8512:
			ula_snow_disabled=!ula_snow_disabled;
			break;
		case 0x8513:
			snap_extended=!snap_extended;
			break;
		case 0x8500: // F5: LOAD FIRMWARE..
			if (s=puff_session_getfile(bios_path,"*.rom","Load firmware"))
			{
				if (bios_load(s)) // error? warn and undo!
					session_message("Cannot load firmware!",txt_error),bios_reload(); // reload valid firmware, if required
				else
					autorun_mode=0,disc_disabled&=~2,all_reset(); // setup and reset
			}
			break;
		case 0x0500: // ^F5: RESET EMULATION
			autorun_mode=0,disc_disabled&=~2,all_reset();
			break;
		case 0x8600: // F6: TOGGLE REALTIME
			session_fast^=1;
			break;
		case 0x0601:
		case 0x0602:
		case 0x0603:
		case 0x0604:
			z80_turbo=(k&15)-1;
			break;
		case 0x0600: // ^F6: TOGGLE TURBO Z80
			z80_turbo=(z80_turbo+(session_shift?-1:1))&3;
			break;
		case 0x8701: // CREATE DISC..
			if (type_id==3&&!disc_disabled)
				if (s=puff_session_newfile(disc_path,"*.dsk",session_shift?"Create disc in B:":"Create disc in A:"))
				{
					if (disc_create(s))
						session_message("Cannot create disc!",txt_error);
					else
						disc_open(s,!!session_shift,1);
				}
			break;
		case 0x8700: // F7: INSERT DISC..
			if (type_id==3&&!disc_disabled)
				if (s=puff_session_getfilereadonly(disc_path,"*.dsk",session_shift?"Insert disc into B:":"Insert disc into A:",1))
					if (disc_open(s,!!session_shift,!session_filedialog_readonly))
						session_message("Cannot open disc!",txt_error);
			break;
		case 0x0700: // ^F7: EJECT DISC
			disc_close(!!session_shift);
			break;
		case 0x0701:
			disc_flip[!!session_shift]^=1;
			break;
		case 0x8800: // F8: INSERT OR RECORD TAPE..
			if (session_shift)
			{
				if (s=puff_session_newfile(tape_path,"*.csw","Record tape"))
					if (tape_create(s))
						session_message("Cannot create tape!",txt_error);
			}
			else if (s=puff_session_getfile(tape_path,"*.tap;*.tzx;*.csw;*.wav","Insert tape"))
				if (tape_open(s))
					session_message("Cannot open tape!",txt_error);
			break;
		case 0x8801: // BROWSE TAPE
			if (tape)
			{
				int i=tape_catalog(session_scratch,length(session_scratch));
				if (i>=0)
				{
					sprintf(session_parmtr,"Browse tape %s",tape_path);
					if (session_list(i,session_scratch,session_parmtr)>=0)
						tape_select(strtol(session_parmtr,NULL,10));
				}
			}
			break;
		case 0x0800: // ^F8: REMOVE TAPE
			if (session_shift)
			{
				if (tape_enabled<0)
					tape_enabled=1;
				else
					tape_enabled=-1;
			}
			else
				tape_close();
			break;
		case 0x8900: // F9: DEBUG
			if (!session_shift)
			{
				if (session_signal=SESSION_SIGNAL_DEBUG^(session_signal&~SESSION_SIGNAL_PAUSE))
					z80_debug_reset();
				break;
			}
		case 0x8901:
			onscreen_flag=!onscreen_flag;
			break;
		case 0x8902:
			video_filter^=VIDEO_FILTER_Y_MASK;
			break;
		case 0x8903:
			video_filter^=VIDEO_FILTER_X_MASK;
			break;
		case 0x8904:
			video_filter^=VIDEO_FILTER_SMUDGE;
			break;
		case 0x0900: // ^F9: TOGGLE FAST LOAD OR FAST TAPE
			if (session_shift)
				tape_fastload=!tape_fastload;
			else
				tape_skipload=!tape_skipload;
			break;
		case 0x0901:
			tape_rewind=!tape_rewind;
			break;
		case 0x8B01:
		case 0x8B02:
		case 0x8B03:
		case 0x8B04:
		case 0x8B05:
			video_type=(k&15)-1;
			video_clut_update();
			break;
		case 0x8B00: // F11: PALETTE
			video_type=(video_type+(session_shift?length(video_table)-1:1))%length(video_table);
			video_clut_update();
			break;
		case 0x0B01:
		case 0x0B02:
		case 0x0B03:
		case 0x0B04:
			video_scanline=(video_scanline&~3)+(k&15)-1;
			break;
		case 0x0B00: // ^F11: SCANLINES
			if (session_shift)
				video_filter=(video_filter+1)&7;
			else
				video_scanline=(video_scanline+1)&3;
			break;
		case 0x8C00: // F12: SAVE SCREENSHOT
			session_savebitmap();
			break;
		case 0x0C00: // ^F12: RECORD WAVEFILE OR YM3 FILE
			if (session_shift)
			{
				if (psg_closelog()) // toggles recording
					if (psg_nextlog=session_savenext("%s%08i.ym",psg_nextlog))
						psg_createlog(session_parmtr);
			}
			else if (session_closewave()) // toggles recording
				session_createwave();
			break;
		//case 0x8A00: // ^F10
		//case 0x0A00: // F10
		case 0x8F00: // ^PAUSE
		case 0x0F00: // PAUSE
			if (!(session_signal&SESSION_SIGNAL_DEBUG))
				session_please(),session_signal^=SESSION_SIGNAL_PAUSE;
			break;
		case 0x9100: // ^NUM.+
		case 0x1100: // NUM.+: INCREASE FRAMESKIP
			if ((video_framelimit&MAIN_FRAMESKIP_MASK)<MAIN_FRAMESKIP_FULL)
				++video_framelimit;
			break;
		case 0x9200: // ^NUM.-
		case 0x1200: // NUM.-: DECREASE FRAMESKIP
			if ((video_framelimit&MAIN_FRAMESKIP_MASK)>0)
				--video_framelimit;
			break;
		case 0x9300: // ^NUM.*
		case 0x1300: // NUM.*: MAXIMUM FRAMESKIP
			video_framelimit=(video_framelimit&~MAIN_FRAMESKIP_MASK)|MAIN_FRAMESKIP_FULL;
			break;
		case 0x9400: // ^NUM./
		case 0x1400: // NUM./: MINIMUM FRAMESKIP
			video_framelimit&=~MAIN_FRAMESKIP_MASK;
			break;
	}
	return session_menuinfo(),0;
}

void session_configreadmore(char *s)
{
	int i; if (!s||!*s||!session_parmtr[0]) ; // ignore if empty or internal!
	else if (!strcasecmp(session_parmtr,"type")) { if ((i=*s&15)<length(bios_system)) type_id=i; }
	else if (!strcasecmp(session_parmtr,"joy1")) { if ((i=*s&15)<length(joy1_types)) joy1_type=i; }
	else if (!strcasecmp(session_parmtr,"xsna")) snap_extended=*s&1;
	else if (!strcasecmp(session_parmtr,"file")) strcpy(autorun_path,s);
	else if (!strcasecmp(session_parmtr,"snap")) strcpy(snap_path,s);
	else if (!strcasecmp(session_parmtr,"tape")) strcpy(tape_path,s);
	else if (!strcasecmp(session_parmtr,"disc")) strcpy(disc_path,s);
	else if (!strcasecmp(session_parmtr,"card")) strcpy(bios_path,s);
	else if (!strcasecmp(session_parmtr,"info")) onscreen_flag=*s&1;
	else if (!strcasecmp(session_parmtr,"palette")) { if ((i=*s&15)<length(video_table)) video_type=i; }
	else if (!strcasecmp(session_parmtr,"autorewind")) tape_rewind=*s&1;
	else if (!strcasecmp(session_parmtr,"debug")) z80_debug_configread(strtol(s,NULL,10));
}
void session_configwritemore(FILE *f)
{
	fprintf(f,"type %i\njoy1 %i\nxsna %i\nfile %s\nsnap %s\ntape %s\ndisc %s\ncard %s\ninfo %i\npalette %i\nautorewind %i\ndebug %i\n"
		,type_id,joy1_type,snap_extended,autorun_path,snap_path,tape_path,disc_path,bios_path,onscreen_flag,video_type,tape_rewind,z80_debug_configwrite());
}

#ifdef DEBUG
void printferror(char *s) { printf("error: %s!\n",s); }
#else
void printferror(char *s) { sprintf(session_tmpstr,"error: %s!\n",s); session_message(session_tmpstr,txt_error); }
#endif

// START OF USER INTERFACE ========================================== //

int main(int argc,char *argv[])
{
	int i,j; FILE *f;
	session_detectpath(argv[0]);
	if (f=fopen(session_configfile(),"r"))
	{
		while (fgets(session_parmtr,STRMAX-1,f))
			session_configreadmore(session_configread(session_parmtr));
		fclose(f);
	}
	MEMZERO(mem_ram);
	all_setup();
	all_reset();
	video_pos_x=video_pos_y=audio_pos_z=0;
	i=0; while (++i<argc)
	{
		if (argv[i][0]=='-')
		{
			j=1; do
			{
				switch (argv[i][j++])
				{
					case 'c':
						video_scanline=(BYTE)(argv[i][j++]-'0');
						if (video_scanline<0||video_scanline>3)
							i=argc; // help!
						break;
					case 'C':
						video_type=0;
						break;
					case 'd':
						session_signal=SESSION_SIGNAL_DEBUG;
						break;
					case 'g':
						joy1_type=(BYTE)(argv[i][j++]-'0');
						if (joy1_type<0||joy1_type>=length(joy1_types))
							i=argc; // help!
						break;
					case 'j':
						session_key2joy=1;
						break;
					case 'J':
						session_stick=0;
						break;
					case 'I':
						ula_v1_issue=~0;
						break;
						;
						;
					case 'K':
						psg_disabled=1;
						break;
					case 'm':
						type_id=(BYTE)(argv[i][j++]-'0');
						if (type_id<0||type_id>3)
							i=argc; // help!
						break;
					case 'O':
						onscreen_flag=0;
						break;
					case 'r':
						video_framelimit=(BYTE)(argv[i][j++]-'0');
						if (video_framelimit<0||video_framelimit>9)
							i=argc; // help!
						break;
					case 'R':
						session_fast=1;
						break;
					case 'S':
						session_audio=0;
						break;
					case 'X':
						disc_disabled=1;
						break;
					case 'Y':
						tape_fastload=0;
						break;
					case 'Z':
						tape_skipload=0;
						break;
					case '+':
						session_intzoom=1;
						break;
					case '!':
						session_blit=1;
						break;
					default:
						i=argc; // help!
				}
			}
			while ((i<argc)&&(argv[i][j]));
		}
		else
			if (any_load(argv[i],1))
				i=argc; // help!
	}
	if (i>argc)
		return
			#ifdef DEBUG
			printf("usage: %s"
			#else
			session_message("Usage: " MY_CAPTION
			#endif
			" [option..] [file..]\n"
			"\t-cN\tscanline type (0..3)\n"
			"\t-C\tmonochrome\n"
			"\t-d\tdebug\n"
			"\t-gN\tset joystick type (0..4)\n"
			"\t-I\temulate Issue 2 ULA\n"
			"\t-j\tenable joystick keys\n"
			"\t-J\tdisable joystick\n"
			""
			""
			""
			""
			"\t-K\tdisable 48K AY chip\n"
			"\t-m0\tload 48K firmware\n"
			"\t-m1\tload 128K firmware\n"
			"\t-m2\tload +2 firmware\n"
			"\t-m3\tload +3 firmware\n"
			"\t-O\tdisable onscreen status\n"
			"\t-rN\tset frameskip (0..9)\n"
			"\t-R\tdisable realtime\n"
			"\t-S\tdisable sound\n"
			"\t-X\tdisable +3 disc drive\n"
			"\t-Y\tdisable tape analysis\n"
			"\t-Z\tdisable tape speed-up\n"
			#ifdef DEBUG
			,argv[0])
			#else
			,caption_version)
			#endif
			,1;
	if (bios_reload())
		return printferror("cannot load firmware"),1;
	if (session_create(session_menudata))
		return printferror("cannot create session"),1;
	session_kbdreset();
	session_kbdsetup(kbd_map_xlt,length(kbd_map_xlt)/2);
	video_target=&video_frame[video_pos_y*VIDEO_LENGTH_X+video_pos_y]; audio_target=audio_frame;
	audio_disabled=!session_audio;
	onscreen_inks(VIDEO1(0xAA0000),VIDEO1(0x55FF55));
	// it begins, "alea jacta est!"
	while (!session_listen())
	{
		#ifdef DEBUG
		while (!(session_signal&~SESSION_SIGNAL_DEBUG))
		#else
		while (!session_signal)
		#endif
			z80_main(
				z80_multi*( // clump Z80 instructions together to gain speed...
				tape_fastskip?ula_limit_x*4: // tape loading ignores most events and heavy clumping is feasible, although some sync is still needed
					irq_delay?irq_delay:(ula_pos_y<-1?(-ula_pos_y-1)*ula_limit_x*4:ula_pos_y<192?(ula_limit_x-ula_pos_x+ula_clash_delta)*4:
					ula_count_y<ula_limit_y-1?(ula_limit_y-ula_count_y-1)*ula_limit_x*4:4 // the safest way to handle the fastest interrupt countdown possible (ULA SYNC)
				)<<(session_fast&~1)) // ...without missing any IRQ and ULA deadlines!
			);
		if (session_signal&SESSION_SIGNAL_FRAME)
		{
			if (!video_framecount&&onscreen_flag)
			{
				if (type_id<3||disc_disabled)
					onscreen_text(+1, -3, "--\t--", 0);
				else
				{
					int q=(disc_phase&2)&&!(disc_parmtr[1]&1);
					onscreen_byte(+1,-3,disc_track[0],q);
					if (disc_motor|disc_action) // disc drive is busy?
						onscreen_char(+3,-3,!disc_action?'-':(disc_action>1?'W':'R'),1),disc_action=0;
					q=(disc_phase&2)&&(disc_parmtr[1]&1);
					onscreen_byte(+4,-3,disc_track[1],q);
				}
				int i,q=tape_enabled;
				if (tape_fastskip)
					onscreen_char(+6,-3,tape_feedskip?'*':'+',q);
				if (tape_filesize)
				{
					i=(long long)tape_filetell*1000/(tape_filesize+1);
					onscreen_byte(+7,-3,i/10,q);
					onscreen_char(+9,-3,'0'+(i%10),q);
				}
				else
					onscreen_text(+7, -3, tape_type < 0 ? "REC" : "---", q);
				if (session_stick|session_key2joy)
				{
					onscreen_bool(-5,-6,3,1,kbd_bit_tst(kbd_joy[0]));
					onscreen_bool(-5,-2,3,1,kbd_bit_tst(kbd_joy[1]));
					onscreen_bool(-6,-5,1,3,kbd_bit_tst(kbd_joy[2]));
					onscreen_bool(-2,-5,1,3,kbd_bit_tst(kbd_joy[3]));
					onscreen_bool(-4,-4,1,1,kbd_bit_tst(kbd_joy[4]));
					;
				}
			}
			// update session and continue
			if (autorun_mode)
				autorun_next();
			if (!audio_disabled)
				audio_main(TICKS_PER_FRAME); // fill sound buffer to the brim!
			psg_writelog();
			ula_snow_a=(type_id<2&&z80_ir.b.h>=0x40&&z80_ir.b.h<0x80&&!ula_snow_disabled)?27:0;
			ula_clash_z=(ula_clash_z&3)+(ula_count_y*ula_limit_x+ula_pos_x)*4;
			if (tape_type<0&&tape&&!z80_iff.b.l) // tape is recording?
				tape_enabled|=4;
			else if (tape_enabled>0) // tape is still busy?
				--tape_enabled;
			if (tape_closed)
				tape_closed=0,session_dirtymenu=1; // tag tape as closed
			tape_fastskip=tape_feedskip=audio_pos_z=0;
			if (tape&&tape_skipload&&tape_enabled)
				session_fast|=6,video_framelimit|=(MAIN_FRAMESKIP_MASK+1),video_interlaced|=2,audio_disabled|=2; // abuse binary logic to reduce activity
			else
				session_fast&=~6,video_framelimit&=~(MAIN_FRAMESKIP_MASK+1),video_interlaced&=~2,audio_disabled&=~2; // ditto, to restore normal activity
			session_update();
		}
	}
	// it's over, "acta est fabula"
	z80_debug_close();
	tape_close();
	disc_close(0); disc_close(1);
	psg_closelog();
	session_closewave();
	if (f=fopen(session_configfile(),"w"))
	{
		session_configwritemore(f),session_configwrite(f);
		fclose(f);
	}
	return puff_byebye(),session_byebye(),0;
}

BOOTSTRAP

// ============================================ END OF USER INTERFACE //
