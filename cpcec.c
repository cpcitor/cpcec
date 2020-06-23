 //  ####  ######    ####  #######   ####    ----------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####    ----------------------- //

#define MY_CAPTION "CPCEC"
#define MY_VERSION "20200622"//"0955"
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

// The emulation of the Amstrad CPC family is complex because the
// multiple devices are tightly knit together; it's no surprise that
// late models merged most (if not all) devices into a single custom
// ASIC chip to cut costs down without losing any relevant function.

// This file provides CPC-specific features for configuration, Gate
// Array and CRTC, Z80 timings and support, and snapshot handling.

#include <stdio.h> // printf()...
#include <stdlib.h> // strtol()...
#include <string.h> // strcpy()...

// Amstrad CPC metrics and constants defined as general types ------- //

#define MAIN_FRAMESKIP_FULL 25
#define MAIN_FRAMESKIP_MASK 31
#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (64<<4)
#define VIDEO_LENGTH_Y (39<<4)
#define VIDEO_OFFSET_X (15<<4)
#define VIDEO_OFFSET_Y (17<<2) // (4<<4) //
#define VIDEO_PIXELS_X (48<<4)
#define VIDEO_PIXELS_Y (67<<3) // (34<<4) //
#define VIDEO_HSYNC_LO (62<<4)
#define VIDEO_HSYNC_HI (66<<4)
#define VIDEO_VSYNC_LO (37<<4)
#define VIDEO_VSYNC_HI (44<<4)
#define AUDIO_PLAYBACK 44100 // 22050, 24000, 44100, 48000
#define AUDIO_LENGTH_Z (AUDIO_PLAYBACK/VIDEO_PLAYBACK) // division must be exact!
#define AUDIO_N_FRAMES 8

#define DEBUG_LENGTH_X 64
#define DEBUG_LENGTH_Y 32
#define DEBUG_LENGTH_Z 12
#define session_debug_show z80_debug_show
#define session_debug_user z80_debug_user

// The CPC 6128 and PLUS keyboard; older models had the cursors (00,02,08,01) and COPY (09) on the right edge.
// +-----------------------------------------------------------------------------------------+      +----+
// | 42 | 40 | 41 | 39 | 38 | 31 | 30 | 29 | 28 | 21 | 20 | 19 | 18 | 10 | 4F | 0A | 0B | 03 |      | 48 |
// +-----------------------------------------------------------------------------------------+ +----+----+----+
// | 44    | 43 | 3B | 3A | 32 | 33 | 2B | 2A | 23 | 22 | 1B | 1A | 11 |      | 14 | 0C | 04 | | 4A | // | 4B |
// +--------------------------------------------------------------------+ 12  +--------------+ +----+----+----+
// | 46     | 45 | 3C | 3D | 35 | 34 | 2C | 2D | 25 | 24 | 1D | 1C | 13 |     | 0D | 0E | 05 |      | 49 |
// +-----------------------------------------------------------------------------------------+      +----+
// | 15      | 47 | 3F | 3E | 37 | 36 | 2E | 26 | 27 | 1F | 1E | 16 | 15      | 0F | 00 | 07 | Amstrad joystick
// +-----------------------------------------------------------------------------------------+ +--------------+
// | 17      | 09    | 2F                                    | 06             | 08 | 02 | 01 | | 4C | 4D | 4E |
// +-----------------------------------------------------------------------------------------+ +--------------+

const unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1, fire2...
	{ 0x48,0x49,0x4A,0x4B,0x4C,0x4D };

#include "cpcec-os.h" // OS-specific code!
#include "cpcec-rt.h" // OS-independent code!

const unsigned char kbd_map_xlt[]=
{
	// control keys
	KBCODE_F1	,0x81,	KBCODE_F2	,0x82,	KBCODE_F3	,0x83,	KBCODE_F4	,0x84,
	KBCODE_F5	,0x85,	KBCODE_F6	,0x86,	KBCODE_F7	,0x87,	KBCODE_F8	,0x88,
	KBCODE_F9	,0x89,	KBCODE_HOLD	,0x8F,	KBCODE_F11	,0x8B,	KBCODE_F12	,0x8C,
	KBCODE_X_ADD	,0x91,	KBCODE_X_SUB	,0x92,	KBCODE_X_MUL	,0x93,	KBCODE_X_DIV	,0x94,
	KBCODE_PRIOR	,0x95,	KBCODE_NEXT	,0x96,	KBCODE_HOME	,0x97,	KBCODE_END	,0x98,
	// actual keys
	KBCODE_1	,0x40,	KBCODE_Q	,0x43,	KBCODE_A	,0x45,	KBCODE_Z	,0x47,
	KBCODE_2	,0x41,	KBCODE_W	,0x3B,	KBCODE_S	,0x3C,	KBCODE_X	,0x3F,
	KBCODE_3	,0x39,	KBCODE_E	,0x3A,	KBCODE_D	,0x3D,	KBCODE_C	,0x3E,
	KBCODE_4	,0x38,	KBCODE_R	,0x32,	KBCODE_F	,0x35,	KBCODE_V	,0x37,
	KBCODE_5	,0x31,	KBCODE_T	,0x33,	KBCODE_G	,0x34,	KBCODE_B	,0x36,
	KBCODE_6	,0x30,	KBCODE_Y	,0x2B,	KBCODE_H	,0x2C,	KBCODE_N	,0x2E,
	KBCODE_7	,0x29,	KBCODE_U	,0x2A,	KBCODE_J	,0x2D,	KBCODE_M	,0x26,
	KBCODE_8	,0x28,	KBCODE_I	,0x23,	KBCODE_K	,0x25,	KBCODE_CHR4_1	,0x27,
	KBCODE_9	,0x21,	KBCODE_O	,0x22,	KBCODE_L	,0x24,	KBCODE_CHR4_2	,0x1F,
	KBCODE_0	,0x20,	KBCODE_P	,0x1B,	KBCODE_CHR3_1	,0x1D,	KBCODE_CHR4_3	,0x1E,
	KBCODE_CHR1_1	,0x19,	KBCODE_CHR2_1	,0x1A,	KBCODE_CHR3_2	,0x1C,	KBCODE_CHR4_4	,0x16,
	KBCODE_CHR1_2	,0x18,	KBCODE_CHR2_2	,0x11,	KBCODE_CHR3_3	,0x13,
	KBCODE_TAB	,0x44,	KBCODE_CAP_LOCK	,0x46,	KBCODE_L_SHIFT	,0x15,	KBCODE_L_CTRL	,0x17,
	KBCODE_ESCAPE	,0x42,	KBCODE_BKSPACE	,0x4F,	KBCODE_ENTER	,0x12,	KBCODE_SPACE	,0x2F,
	KBCODE_X_7	,0x0A,	KBCODE_X_8	,0x0B,	KBCODE_X_9	,0x03,	KBCODE_DELETE	,0x10,
	KBCODE_X_4	,0x14,	KBCODE_X_5	,0x0C,	KBCODE_X_6	,0x04,	KBCODE_INSERT	,0x09,
	KBCODE_X_1	,0x0D,	KBCODE_X_2	,0x0E,	KBCODE_X_3	,0x05,
	KBCODE_X_0	,0x0F,	KBCODE_X_DOT	,0x07,	KBCODE_X_ENTER	,0x06,
	KBCODE_UP	,0x00,	KBCODE_DOWN	,0x02,	KBCODE_LEFT	,0x08,	KBCODE_RIGHT	,0x01,
	// key mirrors
	KBCODE_R_SHIFT	,0x15,	KBCODE_R_CTRL	,0x17,	KBCODE_CHR4_5	,0x16,
};

const int video_asic_table[32]= // same 0GRB format used in ASIC PLUS
{
	0x666,0x666,0xF06,0xFF6,0x006,0x0F6,0x606,0x6F6, // "6" ensures that PREHISTORIK 2 PLUS shows well the scores
	0x0F6,0xFF6,0xFF0,0xFFF,0x0F0,0x0FF,0x6F0,0x6FF,
	0x006,0xF06,0xF00,0xF0F,0x000,0x00F,0x600,0x60F,
	0x066,0xF66,0xF60,0xF6F,0x060,0x06F,0x660,0x66F,
};
const VIDEO_DATATYPE video_table[][80]= // colour table, 0xRRGGBB style: the 32 original colours, followed by 16 levels of G, 16 of R and 16 of B
{
	// monochrome - black and white
	{
		VIDEO1(0x808080),VIDEO1(0x808080),VIDEO1(0xBABABA),VIDEO1(0xF5F5F5),
		VIDEO1(0x0A0A0A),VIDEO1(0x454545),VIDEO1(0x626262),VIDEO1(0x9D9D9D),
		VIDEO1(0x454545),VIDEO1(0xF5F5F5),VIDEO1(0xEBEBEB),VIDEO1(0xFFFFFF),
		VIDEO1(0x3B3B3B),VIDEO1(0x4E4E4E),VIDEO1(0x939393),VIDEO1(0xA7A7A7),
		VIDEO1(0x0A0A0A),VIDEO1(0xBABABA),VIDEO1(0xB0B0B0),VIDEO1(0xC4C4C4),
		VIDEO1(0x000000),VIDEO1(0x141414),VIDEO1(0x595959),VIDEO1(0x6C6C6C),
		VIDEO1(0x272727),VIDEO1(0xD8D8D8),VIDEO1(0xCECECE),VIDEO1(0xE2E2E2),
		VIDEO1(0x1D1D1D),VIDEO1(0x313131),VIDEO1(0x767676),VIDEO1(0x8A8A8A),
		// OLD ^ / v NEW //
		VIDEO1(0x000000),VIDEO1(0x0C0C0C),VIDEO1(0x171717),VIDEO1(0x232323),
		VIDEO1(0x2F2F2F),VIDEO1(0x3B3B3B),VIDEO1(0x474747),VIDEO1(0x525252),
		VIDEO1(0x5E5E5E),VIDEO1(0x6A6A6A),VIDEO1(0x767676),VIDEO1(0x818181),
		VIDEO1(0x8D8D8D),VIDEO1(0x999999),VIDEO1(0xA5A5A5),VIDEO1(0xB0B0B0),
		VIDEO1(0x000000),VIDEO1(0x040404),VIDEO1(0x080808),VIDEO1(0x0C0C0C),
		VIDEO1(0x101010),VIDEO1(0x141414),VIDEO1(0x171717),VIDEO1(0x1B1B1B),
		VIDEO1(0x1F1F1F),VIDEO1(0x232323),VIDEO1(0x272727),VIDEO1(0x2B2B2B),
		VIDEO1(0x2F2F2F),VIDEO1(0x333333),VIDEO1(0x373737),VIDEO1(0x3B3B3B),
		VIDEO1(0x000000),VIDEO1(0x010101),VIDEO1(0x030303),VIDEO1(0x040404),
		VIDEO1(0x050505),VIDEO1(0x060606),VIDEO1(0x080808),VIDEO1(0x090909),
		VIDEO1(0x0A0A0A),VIDEO1(0x0C0C0C),VIDEO1(0x0D0D0D),VIDEO1(0x0E0E0E),
		VIDEO1(0x101010),VIDEO1(0x111111),VIDEO1(0x121212),VIDEO1(0x141414),
	},
	// dark colour
	{
		VIDEO1(0x555555),VIDEO1(0x555555),VIDEO1(0x00FF55),VIDEO1(0xFFFF55),
		VIDEO1(0x000055),VIDEO1(0xFF0055),VIDEO1(0x005555),VIDEO1(0xFF5555),
		VIDEO1(0xFF0055),VIDEO1(0xFFFF55),VIDEO1(0xFFFF00),VIDEO1(0xFFFFFF),
		VIDEO1(0xFF0000),VIDEO1(0xFF00FF),VIDEO1(0xFF5500),VIDEO1(0xFF55FF),
		VIDEO1(0x000055),VIDEO1(0x00FF55),VIDEO1(0x00FF00),VIDEO1(0x00FFFF),
		VIDEO1(0x000000),VIDEO1(0x0000FF),VIDEO1(0x005500),VIDEO1(0x0055FF),
		VIDEO1(0x550055),VIDEO1(0x55FF55),VIDEO1(0x55FF00),VIDEO1(0x55FFFF),
		VIDEO1(0x550000),VIDEO1(0x5500FF),VIDEO1(0x555500),VIDEO1(0x5555FF),
		// OLD ^ / v NEW //
		VIDEO1(0x000000),VIDEO1(0x000100),VIDEO1(0x000600),VIDEO1(0x000C00),
		VIDEO1(0x001600),VIDEO1(0x002200),VIDEO1(0x003100),VIDEO1(0x004100),
		VIDEO1(0x005400),VIDEO1(0x006900),VIDEO1(0x007F00),VIDEO1(0x009700),
		VIDEO1(0x00B000),VIDEO1(0x00CA00),VIDEO1(0x00E400),VIDEO1(0x00FF00),
		VIDEO1(0x000000),VIDEO1(0x010000),VIDEO1(0x060000),VIDEO1(0x0C0000),
		VIDEO1(0x160000),VIDEO1(0x220000),VIDEO1(0x310000),VIDEO1(0x410000),
		VIDEO1(0x540000),VIDEO1(0x690000),VIDEO1(0x7F0000),VIDEO1(0x970000),
		VIDEO1(0xB00000),VIDEO1(0xCA0000),VIDEO1(0xE40000),VIDEO1(0xFF0000),
		VIDEO1(0x000000),VIDEO1(0x000001),VIDEO1(0x000006),VIDEO1(0x00000C),
		VIDEO1(0x000016),VIDEO1(0x000022),VIDEO1(0x000031),VIDEO1(0x000041),
		VIDEO1(0x000054),VIDEO1(0x000069),VIDEO1(0x00007F),VIDEO1(0x000097),
		VIDEO1(0x0000B0),VIDEO1(0x0000CA),VIDEO1(0x0000E4),VIDEO1(0x0000FF),
	},
	// normal colour
	{
		VIDEO1(0x808080),VIDEO1(0x808080),VIDEO1(0x00FF80),VIDEO1(0xFFFF80),
		VIDEO1(0x000080),VIDEO1(0xFF0080),VIDEO1(0x008080),VIDEO1(0xFF8080),
		VIDEO1(0xFF0080),VIDEO1(0xFFFF80),VIDEO1(0xFFFF00),VIDEO1(0xFFFFFF),
		VIDEO1(0xFF0000),VIDEO1(0xFF00FF),VIDEO1(0xFF8000),VIDEO1(0xFF80FF),
		VIDEO1(0x000080),VIDEO1(0x00FF80),VIDEO1(0x00FF00),VIDEO1(0x00FFFF),
		VIDEO1(0x000000),VIDEO1(0x0000FF),VIDEO1(0x008000),VIDEO1(0x0080FF),
		VIDEO1(0x800080),VIDEO1(0x80FF80),VIDEO1(0x80FF00),VIDEO1(0x80FFFF),
		VIDEO1(0x800000),VIDEO1(0x8000FF),VIDEO1(0x808000),VIDEO1(0x8080FF),
		// OLD ^ / v NEW //
		VIDEO1(0x000000),VIDEO1(0x001100),VIDEO1(0x002200),VIDEO1(0x003300),
		VIDEO1(0x004400),VIDEO1(0x005500),VIDEO1(0x006600),VIDEO1(0x007700),
		VIDEO1(0x008800),VIDEO1(0x009900),VIDEO1(0x00AA00),VIDEO1(0x00BB00),
		VIDEO1(0x00CC00),VIDEO1(0x00DD00),VIDEO1(0x00EE00),VIDEO1(0x00FF00),
		VIDEO1(0x000000),VIDEO1(0x110000),VIDEO1(0x220000),VIDEO1(0x330000),
		VIDEO1(0x440000),VIDEO1(0x550000),VIDEO1(0x660000),VIDEO1(0x770000),
		VIDEO1(0x880000),VIDEO1(0x990000),VIDEO1(0xAA0000),VIDEO1(0xBB0000),
		VIDEO1(0xCC0000),VIDEO1(0xDD0000),VIDEO1(0xEE0000),VIDEO1(0xFF0000),
		VIDEO1(0x000000),VIDEO1(0x000011),VIDEO1(0x000022),VIDEO1(0x000033),
		VIDEO1(0x000044),VIDEO1(0x000055),VIDEO1(0x000066),VIDEO1(0x000077),
		VIDEO1(0x000088),VIDEO1(0x000099),VIDEO1(0x0000AA),VIDEO1(0x0000BB),
		VIDEO1(0x0000CC),VIDEO1(0x0000DD),VIDEO1(0x0000EE),VIDEO1(0x0000FF),
	},
	// bright colour
	{
		VIDEO1(0xAAAAAA),VIDEO1(0xAAAAAA),VIDEO1(0x00FFAA),VIDEO1(0xFFFFAA),
		VIDEO1(0x0000AA),VIDEO1(0xFF00AA),VIDEO1(0x00AAAA),VIDEO1(0xFFAAAA),
		VIDEO1(0xFF00AA),VIDEO1(0xFFFFAA),VIDEO1(0xFFFF00),VIDEO1(0xFFFFFF),
		VIDEO1(0xFF0000),VIDEO1(0xFF00FF),VIDEO1(0xFFAA00),VIDEO1(0xFFAAFF),
		VIDEO1(0x0000AA),VIDEO1(0x00FFAA),VIDEO1(0x00FF00),VIDEO1(0x00FFFF),
		VIDEO1(0x000000),VIDEO1(0x0000FF),VIDEO1(0x00AA00),VIDEO1(0x00AAFF),
		VIDEO1(0xAA00AA),VIDEO1(0xAAFFAA),VIDEO1(0xAAFF00),VIDEO1(0xAAFFFF),
		VIDEO1(0xAA0000),VIDEO1(0xAA00FF),VIDEO1(0xAAAA00),VIDEO1(0xAAAAFF),
		// OLD ^ / v NEW //
		VIDEO1(0x000000),VIDEO1(0x001B00),VIDEO1(0x003500),VIDEO1(0x004F00),
		VIDEO1(0x006800),VIDEO1(0x007F00),VIDEO1(0x009600),VIDEO1(0x00AB00),
		VIDEO1(0x00BE00),VIDEO1(0x00CE00),VIDEO1(0x00DD00),VIDEO1(0x00E900),
		VIDEO1(0x00F300),VIDEO1(0x00F900),VIDEO1(0x00FE00),VIDEO1(0x00FF00),
		VIDEO1(0x000000),VIDEO1(0x1B0000),VIDEO1(0x350000),VIDEO1(0x4F0000),
		VIDEO1(0x680000),VIDEO1(0x7F0000),VIDEO1(0x960000),VIDEO1(0xAB0000),
		VIDEO1(0xBE0000),VIDEO1(0xCE0000),VIDEO1(0xDD0000),VIDEO1(0xE90000),
		VIDEO1(0xF30000),VIDEO1(0xF90000),VIDEO1(0xFE0000),VIDEO1(0xFF0000),
		VIDEO1(0x000000),VIDEO1(0x00001B),VIDEO1(0x000035),VIDEO1(0x00004F),
		VIDEO1(0x000068),VIDEO1(0x00007F),VIDEO1(0x000096),VIDEO1(0x0000AB),
		VIDEO1(0x0000BE),VIDEO1(0x0000CE),VIDEO1(0x0000DD),VIDEO1(0x0000E9),
		VIDEO1(0x0000F3),VIDEO1(0x0000F9),VIDEO1(0x0000FE),VIDEO1(0x0000FF),
	},
	// monochrome - green screen
	{
		VIDEO1(0x45A445),VIDEO1(0x45A445),VIDEO1(0x5FCB5F),VIDEO1(0x7DF87D),
		VIDEO1(0x054405),VIDEO1(0x237123),VIDEO1(0x358C35),VIDEO1(0x53B953),
		VIDEO1(0x237123),VIDEO1(0x7DF87D),VIDEO1(0x78F078),VIDEO1(0x82FF82),
		VIDEO1(0x1E691E),VIDEO1(0x287828),VIDEO1(0x4EB14E),VIDEO1(0x58C058),
		VIDEO1(0x054405),VIDEO1(0x5FCB5F),VIDEO1(0x5AC35A),VIDEO1(0x64D264),
		VIDEO1(0x003C00),VIDEO1(0x0A4B0A),VIDEO1(0x308430),VIDEO1(0x3A933A),
		VIDEO1(0x155C15),VIDEO1(0x6FE36F),VIDEO1(0x6ADB6A),VIDEO1(0x74EA74),
		VIDEO1(0x105410),VIDEO1(0x1A631A),VIDEO1(0x409C40),VIDEO1(0x4AAB4A),
		// OLD ^ / v NEW //
		VIDEO1(0x003C00),VIDEO1(0x064506),VIDEO1(0x0c4E0c),VIDEO1(0x125712),
		VIDEO1(0x186018),VIDEO1(0x1e691e),VIDEO1(0x247224),VIDEO1(0x2a7B2a),
		VIDEO1(0x308430),VIDEO1(0x368D36),VIDEO1(0x3c963c),VIDEO1(0x429F42),
		VIDEO1(0x48A848),VIDEO1(0x4eB14e),VIDEO1(0x54BA54),VIDEO1(0x5aC35a),
		VIDEO1(0x000000),VIDEO1(0x020302),VIDEO1(0x040604),VIDEO1(0x060906),
		VIDEO1(0x080C08),VIDEO1(0x0a0F0a),VIDEO1(0x0c120c),VIDEO1(0x0e150e),
		VIDEO1(0x101810),VIDEO1(0x121B12),VIDEO1(0x141E14),VIDEO1(0x162116),
		VIDEO1(0x182418),VIDEO1(0x1a271a),VIDEO1(0x1c2A1c),VIDEO1(0x1e2D1e),
		VIDEO1(0x000000),VIDEO1(0x000100),VIDEO1(0x010201),VIDEO1(0x020302),
		VIDEO1(0x020402),VIDEO1(0x030503),VIDEO1(0x040604),VIDEO1(0x040704),
		VIDEO1(0x050805),VIDEO1(0x060906),VIDEO1(0x060A06),VIDEO1(0x070B07),
		VIDEO1(0x080C08),VIDEO1(0x080D08),VIDEO1(0x090E09),VIDEO1(0x0a0F0a),
	},
};

// sound table, 16 static levels + 1 dynamic level, 16-bit sample style
int audio_table[17]={0,85,121,171,241,341,483,683,965,1365,1931,2731,3862,5461,7723,10922,0};

// GLOBAL DEFINITIONS =============================================== //

#define TICKS_PER_FRAME ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)/32)
#define TICKS_PER_SECOND (TICKS_PER_FRAME*VIDEO_PLAYBACK)
// everything in the Amstrad CPC is tuned to a 4 MHz clock,
// using simple binary divisors to adjust the devices' timings,
// mainly defined by the simultaneous operation of the video output
// (32 horizontal thin pixels) and the Z80 behavior (4 cycles)
// (the clock is technically 16 MHz but we mean atomic steps here)

// HARDWARE DEFINITIONS ============================================= //

BYTE mem_ram[9<<16],mem_rom[33<<14]; // RAM (BASE 64K BANK + 8x 64K BANKS) and ROM (512K ASIC PLUS CARTRIDGE + 16K BDOS)
BYTE *mem_xtr=NULL; // external 257x 16K EXTENDED ROMS
#define plus_enabled (type_id>2) // the PLUS ASIC hardware MUST BE tied to the model!
#define bdos_rom (&mem_rom[32<<14])
BYTE *mmu_ram[4],*mmu_rom[4]; // memory is divided in 14 6k R+W areas
BYTE mmu_bit[4]={0,0,0,0}; // RAM bit masks: nonzero raises a write event
BYTE mmu_xtr[257]; // ROM bit masks: nonzero reads from EXTENDED rather than from DEFAULT/CARTRIDGE
#define PEEK(x) mmu_rom[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!

BYTE type_id=2; // 0=464, 1=664, 2=6128, 3=PLUS
BYTE disc_disabled=0; // disables the disc drive altogether as well as its extended ROM
BYTE video_type=length(video_table)/2; // 0 = monochrome, 1=darkest colour, etc.
VIDEO_DATATYPE video_clut[32]; // precalculated colour palette, 16 bitmap inks, 1 border, 15 sprite inks

// above the Gate Array and the CRTC: PLUS ASIC --------------------- //

BYTE plus_gate_lock[]={0000,0x00,0xFF,0x77,0xB3,0x51,0xA8,0xD4,0x62,0x39,0x9C,0x46,0x2B,0x15,0x8A}; // dummy first byte
BYTE plus_gate_counter; // step in the plus lock sequence, starting from 0 (waiting for 0x00) until its length
BYTE plus_gate_enabled; // locked/unlocked state: UNLOCKED if byte after SEQUENCE is $CD, LOCKED otherwise!
BYTE plus_gate_mcr; // RMR2 register, that modifies the behavior of the original MRER (gate_mcr)
WORD plus_dma_regs[3][4]; // loop counter,loop address,pause counter,pause scaler
BYTE plus_dma_count,plus_dma_delay; // DMA channel channel + timing
//BYTE plus_dirtysprite; // tag sprite as "dirty"

// the following block is a hacky way to implement the entire Plus configuration RAM page:
BYTE plus_bank[1<<14];
#define plus_sprite_bmp (plus_bank) // i.e. RAM address range 0x4000-0x4FFF
#define plus_sprite_xyz (&plus_bank[0x2000]) // i.e. range 0x6000-0x607F
#define plus_palette (&plus_bank[0x2400]) // i.e. range 0x6400-0x643F
#define plus_pri plus_bank[0x2800] // 0x6800: Programmable Raster Interrupt
#define plus_sssl plus_bank[0x2801] // 0x6801: Screen Split Scan Line
#define plus_ssss (&plus_bank[0x2802]) // 0x6802: Screen Split Secondary Start (CRTC order, High [0] and Low [1])
#define plus_sscr plus_bank[0x2804] // 0x6804: Soft Scroll Control Register
#define plus_ivr plus_bank[0x2805] // 0x6805: Interrupt Vector Register
#define plus_analog (&plus_bank[0x2808]) // 0x6808: analog joystick channels, effectively unused :-(
#define plus_dmas (&plus_bank[0x2C00]) // 0x6C00: DMA channels' pointers and prescalers (WORD address,BYTE scaler,BYTE dummy)
#define plus_dcsr plus_bank[0x2C0F] // 0x6C0F: DMA control/status register
//#define plus_icsr z80_irq // ICSR is more than one bit, but Z80_IRQ is true on nonzero, so they can overlap

#define plus_setup()

void plus_reset(void)
{
	MEMZERO(plus_bank);
	MEMZERO(plus_dma_regs);
	plus_dma_delay=plus_gate_mcr=plus_gate_enabled=plus_gate_counter=0; // default configuration values
	plus_analog[0]=plus_analog[1]=plus_analog[2]=plus_analog[3]=plus_analog[4]=plus_analog[6]=0x3F; // default analog values; WinAPE lets them stay ZERO
	plus_ivr=1; // docs: "Interrupt Vector (Bit 0 set to 1 on reset)"; WinAPE uses 0x51 as the default state
}

// 0xBC00-0xBF00: CRTC 6845 ----------------------------------------- //

const BYTE crtc_valid[18]={-1,-1,-1,-1,127,31,127,127,63,31,127,31,63,-1,63,-1,63,-1}; // bit masks
BYTE crtc_index,crtc_table[18]; // index and table
BYTE crtc_type=1; // 0 Hitachi, 1 UMC, 2 Motorola, 3 Amstrad+, 4 Amstrad-
BYTE crtc_lo_decoding,crtc_hi_decoding; // LO.BITS0+1 = BITDEPTH, HI.BIT2 = !VDISP, HI.BIT3 = !HDISP, HI.BIT4 = BORDER, HI.BIT5+6 = DELAYS
int crtc_status;

#define CRTC_STATUS_VSYNC 1
#define CRTC_STATUS_HSYNC 2
#define CRTC_STATUS_REG_8 128
#define CRTC_STATUS_V_T_A 256

// Winape `HDC` and `VDUR` are `video_pos_x` and `video_pos_y`
BYTE crtc_count_r0; // HORIZONTAL CHAR COUNT / Winape `HCC`
BYTE crtc_count_r4; // VERTICAL CHAR COUNT / Winape `VCC`
BYTE crtc_count_r9; // VERTICAL LINE COUNT / Winape `VLC`
BYTE crtc_count_r5; // V.T.A. LINE COUNT / Winape `VTAC`
BYTE crtc_count_r3x; // HSYNC CHAR COUNT / Winape `HSC`
BYTE crtc_count_r3y; // VSYNC LINE COUNT / Winape `VSC`
BYTE crtc_limit_r4,crtc_limit_r5,crtc_limit_r9; // "BUFFERED" LIMITS
BYTE crtc_limit_r3x,crtc_limit_r3y; // limits of `r3x` and `r3y`
int video_vsync_min,video_vsync_max,crtc_hold=0; // VHOLD modifiers
int crtc_limit_r2,crtc_prior_r2,crtc_giga,crtc_giga_count; // HSYNC Gigascreen modifiers

#define crtc_line_set() crtc_line=(crtc_count_r9&7)+(crtc_count_r4&63)*8 // Plus scanline counter
#define crtc_r4x_update() (crtc_limit_r4=crtc_table[4],crtc_limit_r5=crtc_table[5],crtc_limit_r9=crtc_table[9])
// CRTC0,CRTC3,CRTC4: R8.b5b4=11b will display all border and no paper; CRTC1: R6=1 is immediate
void crtc_r6x_update(void)
{
	crtc_hi_decoding=((crtc_type==1)?!crtc_table[6]:((crtc_table[8]&0x30)==0x30))?(crtc_hi_decoding|16):(crtc_hi_decoding&~16);
}
void crtc_r3x_update(void)
{
	// VSYNC duration depends on the CRTC type!
	if (crtc_type>=1&&crtc_type<=2) // CRTC1,CRTC2: 16 lines
		crtc_limit_r3y=0; // i.e. when it wraps from 15 to 0 after 16 lines
	else if (!(crtc_limit_r3y=(crtc_table[3]>>4))) // CRTC0,CRTC3,CRTC4: 0=16,1..15
		crtc_limit_r3y=0;
	// HSYNC duration depends on the CRTC type!
	crtc_limit_r3x=crtc_table[3]&15;
	if (plus_enabled&&!crtc_limit_r3x) // docs: CRTC0,CRTC1: 0..15; CRTC2,CRTC3,CRTC4: 0=16,1..15?
		crtc_limit_r3x=16; // but WINAPE disagrees: CRTC3 turns 0 into 16, no one else does!
	if (crtc_hold<0)
		video_vsync_min=VIDEO_VSYNC_LO-(VIDEO_LENGTH_Y-VIDEO_VSYNC_LO),video_vsync_max=VIDEO_VSYNC_HI;
	else if (crtc_hold>0)
		video_vsync_min=VIDEO_VSYNC_LO,video_vsync_max=VIDEO_VSYNC_HI+(VIDEO_LENGTH_Y-VIDEO_VSYNC_LO);
	else
		video_vsync_min=VIDEO_VSYNC_LO,video_vsync_max=VIDEO_VSYNC_HI;
}

INLINE void crtc_table_select(BYTE i) { crtc_index=(i&16)?16:(i&15); }
INLINE void crtc_table_send(BYTE i)
{
	i&=crtc_valid[crtc_index];
	if (!(crtc_index|crtc_type|i)) // CRTC0: REG0 CANNOT BE 0!
		i=1;
	int d;
	if (crtc_index==2&&(d=crtc_table[crtc_index]-i)&&d>=-2&&d<=2)
		++crtc_giga_count;
	crtc_table[crtc_index]=i;
	switch (crtc_index)
	{
		case 2:
			if (!crtc_giga)
				crtc_prior_r2=crtc_limit_r2=i;
			break;
		case 3:
			crtc_r3x_update();
			break;
		// CRTC0 and CRTC2 limits are "buffered"; CRTC1, CRTC3 and CRTC4 aren't.
		case 4:
			if (crtc_type&5)//||crtc_count_r4==i)
				crtc_limit_r4=i;
			break;
		case 5:
			if (crtc_type&5)//||crtc_count_r5==i)
				crtc_limit_r5=i;
			break;
		case 6:
		case 8:
			crtc_r6x_update();
			break;
		case 9:
			if ((crtc_type&5&&(crtc_table[7]>crtc_limit_r4+1))||crtc_count_r9==i||crtc_table[4]==crtc_table[7]+1||crtc_table[4]>crtc_table[6]) // "PHX" (cylinders) and "PINBALL DREAMS" (BG GAMES logo) reset R4!
			// kludges (?): "PINBALL DREAMS" (ingame) needs crtc_count_r9==i; "PINBALL DREAMS" (scroll) needs crtc_table[4]==crtc_table[7]+1; things such as !crtc_count_r3y break "OVERFLOW PREVIEW 3"!
			// "ZAP'T'BALLS ADVANCED EDITION" (menu) needs crtc_table[4]>crtc_table[6]; REG4>REG6 ensures that both this title and "5KB3: NAYAD" work rather than just the former (>=) or the later (==)!
				crtc_limit_r9=i;
			break;
	}
}
INLINE BYTE crtc_table_recv(void) { return (crtc_index>=12&&crtc_index<18)?crtc_table[crtc_index]:(crtc_type>2&&crtc_index<8?crtc_table[crtc_index+8]:0); }
INLINE BYTE crtc_table_info(void) { return crtc_count_r4>=crtc_table[6]?32:0; } // +64???

#define crtc_setup()

void crtc_reset(void)
{
	crtc_index=crtc_status=crtc_lo_decoding=0;
	crtc_hi_decoding=16;
	MEMZERO(crtc_table);
	crtc_r3x_update();
}

// 0x7F00, 0xDF00: Gate Array --------------------------------------- //

BYTE gate_index,gate_table[17]; // colour table: respectively, Palette Pointer Register and Palette Memory
BYTE gate_mcr; // bit depth + MMU configuration (1/3), also known as MRER (Mode and Rom Enable Register)
BYTE gate_ram; // MMU configuration (2/3), the Memory Mapping Register
BYTE gate_rom; // MMU configuration (3/3)
BYTE gate_ram_depth=1; // RAM configuration: 0 = 64k, 1 = 128k, 2 = 192k, 3 = 320k, 4 = 576k
int gate_ram_dirty; // actually used RAM space, in kb
int gate_ram_kbyte[]={64,128,192,320,576};// (x?(32<<x)+64:64)

BYTE video_clut_index=0; VIDEO_DATATYPE video_clut_value; // slow colour update buffer

const int mmu_ram_mode[8][4]= // relative offsets of every bank for each +128K RAM mode
{
	{ 0x00000-0x0000,0x04000-0x4000,0x08000-0x8000,0x0C000-0xC000 }, // RAM0: 0 1 2 3
	{ 0x00000-0x0000,0x04000-0x4000,0x08000-0x8000,0x1C000-0xC000 }, // RAM1: 0 1 2 7
	{ 0x10000-0x0000,0x14000-0x4000,0x18000-0x8000,0x1C000-0xC000 }, // RAM2: 4 5 6 7
	{ 0x00000-0x0000,0x0C000-0x4000,0x08000-0x8000,0x1C000-0xC000 }, // RAM3: 0 3 2 7
	{ 0x00000-0x0000,0x10000-0x4000,0x08000-0x8000,0x0C000-0xC000 }, // RAM4: 0 4 2 3
	{ 0x00000-0x0000,0x14000-0x4000,0x08000-0x8000,0x0C000-0xC000 }, // RAM5: 0 5 2 3
	{ 0x00000-0x0000,0x18000-0x4000,0x08000-0x8000,0x0C000-0xC000 }, // RAM6: 0 6 2 3
	{ 0x00000-0x0000,0x1C000-0x4000,0x08000-0x8000,0x0C000-0xC000 }, // RAM7: 0 7 2 3
};
void mmu_update(void) // update the MMU tables with all the new offsets
{
	// classic CPC RAM/ROM paging
	int i=gate_ram&(gate_ram_depth?(4<<gate_ram_depth)-1:0);
	int j=i?128+64*(i/8):64;
	if (gate_ram_dirty<j) // tag memory as dirty?
		session_dirtymenu=gate_ram_dirty=j;
	int k=(i/8)<<16; i&=7; // k = bank offset, i = paging mode

	// page 0x0000-0x3FFF
	if ((j=mmu_ram_mode[i][0])>=0x10000-0x0000) j+=k;
	mmu_ram[0]=&mem_ram[j];
	mmu_rom[0]=(gate_mcr&4)?mmu_ram[0]:mmu_xtr[0]?&mem_xtr[0x0000-0x0000]:&mem_rom[0x0000-0x0000];
	// page 0x4000-0x7FFF
	if ((j=mmu_ram_mode[i][1])>=0x10000-0x4000) j+=k;
	mmu_rom[1]=mmu_ram[1]=&mem_ram[j];
	// page 0x8000-0xBFFF
	if ((j=mmu_ram_mode[i][2])>=0x10000-0x8000) j+=k;
	mmu_rom[2]=mmu_ram[2]=&mem_ram[j];
	// page 0xC000-0xFFFF
	if ((j=mmu_ram_mode[i][3])>=0x10000-0xC000) j+=k;
	mmu_ram[3]=&mem_ram[j];
	mmu_rom[3]=(gate_mcr&8)?mmu_ram[3]:(gate_rom<length(mmu_xtr)-1&&mmu_xtr[gate_rom+1]?&mem_xtr[0x4000+(gate_rom<<14)-0xC000]:(gate_rom!=7||disc_disabled)?&mem_rom[0x4000-0xC000]:&bdos_rom[0x0000-0xC000]);

	if (plus_enabled)
	{
		if (!(gate_mcr&4))
		{
			mmu_rom[0]=mmu_xtr[0]?&mem_xtr[0x0000-0x0000]:&mem_rom[(plus_gate_mcr&7)<<14]; // show low ROM
			switch (plus_gate_mcr&24)
			{
				case 8:
					mmu_rom[1]=&(mmu_rom[0])[0x0000-0x4000]; // show low ROM on page 1
					mmu_rom[0]=mmu_ram[0];
					break;
				case 16:
					mmu_rom[2]=&(mmu_rom[0])[0x0000-0x8000]; // show low ROM on page 2
					mmu_rom[0]=mmu_ram[0];
					break;
			}
		}
		if (mmu_bit[1]=((plus_gate_mcr&24)==24))
			mmu_rom[1]=mmu_ram[1]=&plus_bank[0x0000-0x4000]; // show PLUS ASIC bank
		if (!(gate_mcr&8))
			mmu_rom[3]=
			gate_rom<length(mmu_xtr)-1&&mmu_xtr[gate_rom+1]?&mem_xtr[0x4000+(gate_rom<<14)-0xC000]:
			gate_rom<128?gate_rom==7?&mem_rom[0xC000-0xC000]:&mem_rom[0x4000-0xC000]:&mem_rom[((gate_rom&31)<<14)-0xC000]; // show high ROM
	}
	else
		mmu_bit[1]=0; // hide PLUS ASIC bank
}

INLINE void gate_table_select(BYTE i) { gate_index=(i&16)?16:(i&15); }
INLINE void gate_table_send(BYTE i)
{
	gate_table[video_clut_index=gate_index]=(i&=31);
	if (!plus_enabled)
	{
		video_clut_value=video_table[video_type][i];
	}
	else
	{
		int j=video_asic_table[i]; // set both colour and the PLUS ASIC palette
		video_clut_value=video_table[video_type][32+((j>>8)&15)]+video_table[video_type][48+((j>>4)&15)]+video_table[video_type][64+(j&15)];
		plus_palette[gate_index*2+0]=j;
		plus_palette[gate_index*2+1]=j>>8;
	}
	//if (video_pos_x&8) video_clut[video_clut_index]=video_clut_value; // fast update
}
void video_clut_update(void) // precalculate palette following `video_type`
{
	if (!plus_enabled)
		for (int i=0;i<17;++i)
			video_clut[i]=video_table[video_type][gate_table[i]];
	else
		for (int i=0;i<32;++i)
			video_clut[i]=video_table[video_type][32+(plus_palette[i*2+1]&15)]+video_table[video_type][48+(plus_palette[i*2+0]>>4)]+video_table[video_type][64+(plus_palette[i*2+0]&15)];
	video_clut_value=video_clut[video_clut_index=gate_index];
}

BYTE gate_mode0[2][256],gate_mode1[4][256]; // lookup table for byte->pixel conversion
void gate_setup(void) // setup the Gate Array
{
	for (int i=0;i<256;++i)
	{
		gate_mode0[0][i]=((i&128)?1:0)+((i&32)?4:0)+((i&8)?2:0)+((i&2)?8:0);
		gate_mode0[1][i]=((i&64)?1:0)+((i&16)?4:0)+((i&4)?2:0)+((i&1)?8:0);
		gate_mode1[0][i]=((i&128)?1:0)+((i&8)?2:0);
		gate_mode1[1][i]=((i&64)?1:0)+((i&4)?2:0);
		gate_mode1[2][i]=((i&32)?1:0)+((i&2)?2:0);
		gate_mode1[3][i]=((i&16)?1:0)+((i&1)?2:0);
	}
}

BYTE irq_delay; // 0 = INACTIVE, 1 = LINE 1, 2 = LINE 2
BYTE irq_timer; // Winape: R52. Rises from 0 to 52 (IRQ!)
BYTE z80_irq; // Winape: ICSR. B7 Raster (Gate Array / PRI), B6 DMA0, B5 DMA1, B4 DMA2 (top -- PRI DMA2 DMA1 DMA0 -- bottom)

void gate_reset(void) // reset the Gate Array
{
	gate_mcr=gate_ram=gate_rom=gate_index=irq_timer=irq_delay=0;
	gate_ram_dirty=64;
	MEMZERO(gate_table);
	video_clut_update();
	mmu_update();
}

// 0xF400-0xF700: PIO 8255 ------------------------------------------ //

BYTE pio_port_a,pio_port_b,pio_port_c,pio_control;

#define pio_setup()

void pio_reset(void)
{
	pio_port_a=pio_port_b=pio_port_c=pio_control=0;
}

// behind the PIO: PSG AY-3-8910 ------------------------------------ //

#define PSG_TICK_STEP 8 // 1 MHz /2 /8 = 62500 Hz
#define PSG_KHZ_CLOCK 1000
#define PSG_MAIN_EXTRABITS 0
#if AUDIO_STEREO
int psg_stereo[3][2]; const int psg_stereos[3][3]={{0,0,0},{+256,0,-256},{+128,0,-128}}; // A left, B middle, C right
#endif

#include "cpcec-ay.h"

// behind the PIO: TAPE --------------------------------------------- //

#define TAPE_MAIN_TZX_STEP (35<<1) // amount of T units per packet // highest value before "MARMALADE" breaks down is 197, but remainder isn't 0
#define tape_enabled (pio_port_c&16)
int tape_delay=0; // tape motor delay
#include "cpcec-k7.h"

// 0xFA7E, 0xFB7E, 0xFB7F: FDC 765 ---------------------------------- //

#define DISC_PARMTR_UNIT (disc_parmtr[1]&1) // CPC hardware is limited to two drives;
#define DISC_PARMTR_UNITHEAD (disc_parmtr[1]&5) // however, it allows two-sided discs.
#define DISC_TIMER_INIT ( 4<<6) // rough approximation: "PhX" requires 1<<7 at least and 8<<7 at most, but "Prehistorik 1" needs more than 1<<7.
#define DISC_TIMER_BYTE ( 2<<6) // rough approximation, too: the HEXAGON-protected "SWIV" needs at least 1<<6 and becomes very slow at 1<<8.
#define DISC_WIRED_MODE 0 // the CPC lacks the End-Of-Operation wire
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

int crtc_32kb,crtc_zz_char,crtc_line,gate_char,plus_hardbase,gate_count_r3x;
BYTE crtc_zz_decoding,plus_soft,plus_fix_init,plus_fix_last,plus_fix_exit,crtc_half_dirt;
WORD crtc_char,crtc_past,crtc_bank; VIDEO_DATATYPE plus_fill;
BYTE plus_sprite_xyz_backup[16*8]; // temporary copy of sprite coordinates
#define gate_limit_r3x 6 // IMPERIAL MAHJONG (INGAME): 1,2,3: fail; 4,5: glitch; 6 good; SCROLL FACTORY (TITLE): 1,2,3,4: glitch; 5,6 good

INLINE void video_main_sprites(void) // PLUS ASIC: Hardware Sprites relative to current plus_hardbase, video_target and video_pos_x
{
	int plus_span=video_pos_x-(plus_hardbase>video_pos_x?0:plus_hardbase);
	VIDEO_DATATYPE *video_source=&video_target[16-plus_span];
	for (int i=15;i>=0;--i) // sprites follow a priority: #0 highest, #15 lowest
	{
		int zoomy,zoomx;
		if (/*plus_dirtysprite!=i&&*/(zoomy=plus_sprite_xyz_backup[i*8+4]&3)&&(zoomx=plus_sprite_xyz_backup[i*8+4]&12))
		{
			int basey=(crtc_line-(plus_sprite_xyz_backup[i*8+2]+((signed char)plus_sprite_xyz_backup[i*8+3])*256))>>--zoomy;
			if (basey>=0&&basey<16)
			{
				int basex=(plus_sprite_xyz_backup[i*8+0]+((signed char)plus_sprite_xyz_backup[i*8+1])*256)-plus_soft;
				if (basex<plus_span&&(basex+(16<<(zoomx=zoomx/4-1)))>0)
				{
					VIDEO_DATATYPE z,*tgt=video_source;
					BYTE *src=&plus_sprite_bmp[i*256+basey*16];
					if (basex<0) // is the sprite clipped to the left?
					{
						// handle partial pixels, if any!
						int x=basex;
						zoomy=16+(basex>>=zoomx),src-=basex;
						if (x&=(1<<zoomx)-1)
						{
							if (z=src[-1])
							{
								z=video_clut[16+z];
								do
									*tgt++=z;
								while (--x);
							}
							else
								tgt+=x;
						}
					}
					else
						zoomy=16,tgt+=basex;
					if (zoomy>0) // recycling 'zoomy' as the pixel counter :-)
						switch (zoomx)
						{
							case 0:
								do
									if (z=*src++) *tgt++=video_clut[16+z];
									else ++tgt;
								while (--zoomy);
								break;
							case 1:
								do
									if (z=*src++) z=video_clut[16+z],*tgt++=z,*tgt++=z;
									else tgt+=2;
								while (--zoomy);
								break;
							case 2:
								do
									if (z=*src++) z=video_clut[16+z],*tgt++=z,*tgt++=z,*tgt++=z,*tgt++=z;
									else tgt+=4;
								while (--zoomy);
								break;
						}
				}
			}
		}
	}
	if (plus_sscr&128)
		for (int z=plus_soft;z<16;++z)
			*video_source++=plus_fill;
	//plus_dirtysprite=-1;
	plus_hardbase=-1;
}

INLINE void video_main(int t) // render video output for `t` clock ticks; t is always nonzero!
{
	while (t--)
	{
		// actual pixel rendering

		if (!video_framecount&&(video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y))
		{
			if (plus_fix_exit)
			{
				video_pos_x-=plus_soft,video_target-=plus_soft,plus_fix_exit=0;
				if (plus_hardbase>=0) video_main_sprites();
			}
			int x=video_pos_x+plus_fix_init;
			if (x>VIDEO_OFFSET_X-16&&x<VIDEO_OFFSET_X+VIDEO_PIXELS_X)
			{
				video_pos_x+=16;
				#define VIDEO_NEXT *video_target++ // "VIDEO_NEXT = VIDEO_NEXT = ..." generates invalid code on VS13 and slower code on TCC
				switch ((BYTE)(crtc_zz_decoding+crtc_lo_decoding))
				{
					case 0: // MODE 0: 4:2px 4bit
					{
						BYTE b; VIDEO_DATATYPE p;
						p=video_clut[gate_mode0[0][b=mem_ram[crtc_zz_char+0]]]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode0[1][b]]; VIDEO_NEXT= p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						video_clut[video_clut_index]=video_clut_value; // slow update
						p=video_clut[gate_mode0[0][b=mem_ram[crtc_zz_char+1]]]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode0[1][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					} break;
					case 1: // MODE 1: 2:2px 2bit
					{
						BYTE b; VIDEO_DATATYPE p;
						p=video_clut[gate_mode1[0][b=mem_ram[crtc_zz_char+0]]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[1][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[2][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[3][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
						video_clut[video_clut_index]=video_clut_value; // slow update
						p=video_clut[gate_mode1[0][b=mem_ram[crtc_zz_char+1]]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[1][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[2][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[3][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					} break;
					case 2: // MODE 2: 1:2px 1bit
					{
						BYTE b;
						VIDEO_NEXT=video_clut[(b=mem_ram[crtc_zz_char+0])>>7];
						VIDEO_NEXT=video_clut[(b>>6)&1];
						VIDEO_NEXT=video_clut[(b>>5)&1];
						VIDEO_NEXT=video_clut[(b>>4)&1];
						VIDEO_NEXT=video_clut[(b>>3)&1];
						VIDEO_NEXT=video_clut[(b>>2)&1];
						VIDEO_NEXT=video_clut[(b>>1)&1];
						VIDEO_NEXT=video_clut[b&1];
						video_clut[video_clut_index]=video_clut_value; // slow update
						VIDEO_NEXT=video_clut[(b=mem_ram[crtc_zz_char+1])>>7];
						VIDEO_NEXT=video_clut[(b>>6)&1];
						VIDEO_NEXT=video_clut[(b>>5)&1];
						VIDEO_NEXT=video_clut[(b>>4)&1];
						VIDEO_NEXT=video_clut[(b>>3)&1];
						VIDEO_NEXT=video_clut[(b>>2)&1];
						VIDEO_NEXT=video_clut[(b>>1)&1];
						VIDEO_NEXT=video_clut[b&1];
					} break;
					case 3: // MODE 3: 4:2px 2bit
					{
						BYTE b; VIDEO_DATATYPE p;
						p=video_clut[gate_mode1[0][b=mem_ram[crtc_zz_char+0]]]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[1][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						video_clut[video_clut_index]=video_clut_value; // slow update
						p=video_clut[gate_mode1[0][b=mem_ram[crtc_zz_char+1]]]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						p=video_clut[gate_mode1[1][b]]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					} break;
					case 32 ... 255: // HSYNC + VSYNC
					{
						VIDEO_DATATYPE p=video_table[video_type][20]; // BLACK!
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						video_clut[video_clut_index]=video_clut_value; // slow update
					} break;
					default: // BORDER
					{
						VIDEO_DATATYPE p=video_clut[16];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						video_clut[video_clut_index]=video_clut_value; // slow update
						p=video_clut[16];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						video_pos_x+=plus_fix_init;
						while (plus_fix_init)
							VIDEO_NEXT=p,--plus_fix_init;
					} break;
				}
			}
			else
			{
				video_pos_x+=16+plus_fix_init-(plus_fix_last?plus_soft:0),video_target+=16+plus_fix_init-(plus_fix_last?plus_soft:0),plus_fix_init=plus_fix_last=0;
				video_clut[video_clut_index]=video_clut_value; // slow update
			}
			plus_fix_exit=plus_fix_last,plus_fix_last=0;
		}
		else
			video_pos_x+=16,video_target+=16,
			video_clut[video_clut_index]=video_clut_value; // slow update

		// Gate Array delays rendering

		crtc_zz_char=crtc_bank+crtc_char;
		crtc_zz_decoding=crtc_hi_decoding;

		// logical CRTC+ASIC behavior

		if (!--plus_dma_delay) // DMA CHANNELS
			do
				if (plus_dcsr&(1<<plus_dma_count))
				{
					if (plus_dma_regs[plus_dma_count][2])
					if (plus_dma_regs[plus_dma_count][3])
						--plus_dma_regs[plus_dma_count][3]; // handle multiplier
					else
					{
						plus_dma_regs[plus_dma_count][3]=plus_dmas[4*plus_dma_count+2]; // reload multiplier
						--plus_dma_regs[plus_dma_count][2]; // handle pause
					}
					if (!plus_dma_regs[plus_dma_count][2])
					{
						int i=plus_dmas[4*plus_dma_count+0]+(plus_dmas[4*plus_dma_count+1]<<8); // restore pointer
						int n=mem_ram[i++]; n+=mem_ram[i++]<<8; // fetch command
						int h=(n>>12)&7;
						if (!h) // 0RDD = LOAD R,DD
							psg_table_sendto(n>>8,n&255); // load register
						else // warning! functions can build up!
						{
							if (h&1) // 1NNN = PAUSE NNN (0=no pause)
								plus_dma_regs[plus_dma_count][2]=n&4095; // reload pause
							if (h&2) // 2NNN = REPEAT NNN (0=no repeat)
								plus_dma_regs[plus_dma_count][0]=n&4095,plus_dma_regs[plus_dma_count][1]=i;
							if (h&4) // 40NN = CONTROL BIT MASK: +01 = LOOP, +10 = INT, +20 = STOP
							{
								if (n&1)
									if (plus_dma_regs[plus_dma_count][0])
										--plus_dma_regs[plus_dma_count][0],i=plus_dma_regs[plus_dma_count][1];
								if (n&16)
									z80_irq|=(64>>plus_dma_count);
								if (n&32)
									plus_dcsr&=~(1<<plus_dma_count);
							}
						}
						plus_dmas[4*plus_dma_count+0]=i; plus_dmas[4*plus_dma_count+1]=i>>8; // store pointer
					}
					if (++plus_dma_count<3)
						plus_dma_delay=1; // non-DMA RASTER interruptions
					break;
				}
			while (++plus_dma_count<3);

		if (crtc_count_r0==20&&plus_hardbase>=0) // SYNERGY 4 loses the dancing top left sprites with +23
			MEMLOAD(plus_sprite_xyz_backup,plus_sprite_xyz); // compare this with ZXSEC's `ula_clash_attrib`
		if (crtc_count_r0==crtc_table[0]) // new scanline?
		{
			if (crtc_giga)
				crtc_limit_r2=(crtc_prior_r2+crtc_table[2]+1)/2; // BATMAN FOREVER GIGASCREEN FAILS WITH `+(crtc_prior_r2<crtc_table[2])`
			crtc_prior_r2=crtc_table[2]; // average REG2!
			if (crtc_status&CRTC_STATUS_VSYNC)
				if ((crtc_count_r3y=(crtc_count_r3y+1)&15)==crtc_limit_r3y)
					crtc_status&=~CRTC_STATUS_VSYNC,crtc_hi_decoding&=~128; // VSYNC OFF

			if (crtc_count_r9==crtc_limit_r9) // new line?
			{
				if (!(crtc_status&CRTC_STATUS_V_T_A))
					crtc_count_r5=0; // reset!
				if (crtc_count_r4==crtc_limit_r4) // new screen?
					crtc_status|=CRTC_STATUS_V_T_A; // V_T_A ON!
				if (crtc_hi_decoding&8)
					crtc_char=crtc_past;
				crtc_count_r4=(crtc_count_r4+1)&127;
				crtc_count_r9=0;
			}
			else
				crtc_count_r9=(crtc_count_r9+1)&31; // increase!

			if (crtc_count_r5==crtc_limit_r5&&crtc_status&CRTC_STATUS_V_T_A)
			{
				crtc_status&=~CRTC_STATUS_V_T_A; // V_T_A OFF
				crtc_hi_decoding&=~4; // VDISP ON!
				crtc_count_r4=crtc_count_r5=crtc_count_r9=0; // reset!
			}
			crtc_count_r5=(crtc_count_r5+1)&31; // counter increases anyway!?

			crtc_bank=(crtc_count_r9<<11)+(plus_sscr<<7)&0x3800; // next scanline

			if (!crtc_count_r4&&(crtc_type==1||!crtc_count_r9))
				crtc_32kb=((~crtc_table[12])&0x0C)?0:0x4000,crtc_past=((crtc_table[12]&0x30)<<10)+(((crtc_table[12]&0x03)*256+crtc_table[13])<<1);

			if (!crtc_count_r9)
			{
				if (crtc_count_r4==crtc_table[6])
					crtc_hi_decoding|=4; // VDISP OFF
				if (crtc_count_r4==crtc_table[7])
					crtc_count_r3y=0,crtc_status|=CRTC_STATUS_VSYNC,crtc_hi_decoding|=128; // VSYNC ON!
			}
			crtc_r4x_update();
			crtc_count_r0=0;
			crtc_char=crtc_past;
			crtc_half_dirt=!(crtc_type&5||crtc_hi_decoding&8)&&!video_framecount&&(video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
				&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X);
			crtc_hi_decoding&=~8; // HDISP ON!
			crtc_line_set();
			if (plus_pri==crtc_line&&plus_pri&&crtc_status&CRTC_STATUS_HSYNC)
				gate_count_r3x=2; // PLUS ASIC: PROGRAMMABLE RASTER INTERRUPT (late)
			if (plus_enabled)
				plus_hardbase=(crtc_table[1]&&!(crtc_hi_decoding&4)&&
					!video_framecount&&(video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y))
					?plus_fill=video_clut[16],video_pos_x+(plus_fix_init=plus_soft=(plus_sscr&15)):-1; // remember for later usage
		}
		else
		{
			crtc_count_r0=(crtc_count_r0+1)&127;
			if ((crtc_char+=2)&0x0800) // point to next VRAM character
				crtc_char=(crtc_char+crtc_32kb)&0xC7FF; // handle 16/32k wrap
		}

		if (crtc_hi_decoding&(64+32))
			crtc_hi_decoding-=32; // Gate Array slowdown
		if (crtc_count_r0==crtc_table[1])
		{
			if (((crtc_count_r9+((plus_sscr&0x70)>>4))&0x1F)==crtc_limit_r9)
			{
				if ((crtc_past^crtc_char)&0xC000)
					crtc_32kb=-crtc_32kb;
				crtc_past=crtc_char;
			}
			if (plus_enabled)
			{
				plus_fix_last=1;
				if (plus_sssl==(BYTE)crtc_line&&plus_sssl/*&&crtc_count_r9<8*/) // PLUS ASIC: Split Screen (notice the BYTE)
					crtc_32kb=0, // the SSSL doesn't support 32kb mode!
					crtc_char=crtc_past=((plus_ssss[0]&0x30)<<10)+(((plus_ssss[0]&0x3)*256+plus_ssss[1])<<1);
			}
			crtc_hi_decoding|=crtc_limit_r3x==1?32+8:crtc_limit_r3x==2?64+8:8; // 32,64 : Gate Array slowdown
		}
		if (crtc_count_r0==crtc_limit_r2)
		{
			if (crtc_table[0]<crtc_table[1]) // "CAMEMBERT MEETING 4": HIDE VERTICAL STRIPES!
				crtc_lo_decoding|=4;
			plus_dma_count=0,plus_dma_delay=plus_enabled?"\000\004\004\005\004\005\005\006"[plus_dcsr&7]-!!(video_pos_x&8):0;
			crtc_count_r3x=0,crtc_status|=/*video_pos_x<VIDEO_HSYNC_LO?0:*/CRTC_STATUS_HSYNC; // HSYNC ON!
			if (plus_pri==crtc_line&&plus_pri)
				gate_count_r3x=2; // PLUS ASIC: PROGRAMMABLE RASTER INTERRUPT (early)
		}
		if (crtc_count_r0==1&&crtc_half_dirt) // "ONESCREEN COLONIES" shows a little shadow on the bricks and the plasma on CRTC 0; CRTC 2 can do it too.
			video_target[-8]=video_target[-7]=video_target[-6]=video_target[-5]=
			video_target[-4]=video_target[-3]=video_target[-2]=video_target[-1]=video_clut[16];

		// physical SYNC behavior

		if (((crtc_status&CRTC_STATUS_HSYNC)&&video_pos_x>=VIDEO_HSYNC_LO)||video_pos_x>=VIDEO_HSYNC_HI) // H.REFRESH?
		{
			if (plus_hardbase>=0) video_main_sprites();
			plus_fix_last=0; // restore soft scroll even if it's misaligned (Prehistorik 2)
			if (!video_framecount)
				video_drawscanline();
			video_pos_y+=2,video_target+=VIDEO_LENGTH_X*2-video_pos_x;
			// see below the horrible trick to tell apart "PREHISTORIK 2" (6-r), "CAMEMBERT MEETING 4" (6-r) and "SCROLL FACTORY" (2-r)
			video_target+=video_pos_x=((crtc_limit_r3x>2&&crtc_limit_r3x<6)?6-crtc_limit_r3x:0)*8;
		}
		if (((crtc_status&CRTC_STATUS_VSYNC)&&(video_pos_y>=video_vsync_min))||video_pos_y>=video_vsync_max) // V.REFRESH?
		{
			crtc_status=(crtc_table[8]&1)?(crtc_status^CRTC_STATUS_REG_8):(crtc_status&~CRTC_STATUS_REG_8);
			if (!video_framecount)
				video_endscanlines(video_table[video_type][20]); // 'T' = BLACK
			video_newscanlines(video_pos_x,(crtc_status&CRTC_STATUS_REG_8)?2:0); // vertical reset
			session_signal|=SESSION_SIGNAL_FRAME; // end of frame!
		}

		if (crtc_status&CRTC_STATUS_HSYNC)
		{
			if (crtc_count_r3x==gate_limit_r3x)
			{
				crtc_lo_decoding=gate_mcr&3; // SET BIT DEPTH (1/2)
			}
			if (crtc_count_r3x==crtc_limit_r3x)
			{
				crtc_lo_decoding&=~4; // CFR. INFRA, "CAMEMBERT MEETING 4"
				if (crtc_count_r3x<gate_limit_r3x)
					crtc_lo_decoding=gate_mcr&3; // SET BIT DEPTH (2/2)
				crtc_status&=~CRTC_STATUS_HSYNC; // HSYNC OFF
				// when HSYNC ends at the same time VSYNC begins ("ONESCREEN COLONIES" but not "IMPERIAL MAHJONG" or "SCROLL FACTORY")
				// IRQs must be reset one scanline later than usual (i.e. reset happens after inc), but not on PLUS! ("BLACK SABBATH")
				if (crtc_status&CRTC_STATUS_VSYNC&&crtc_limit_r4&&crtc_count_r3y==!(plus_enabled||crtc_count_r0)) // does it depend on VSYNC and nothing else?
					irq_delay=1,irq_timer-=crtc_count_r3y; // '&&crtc_limit_r4' seems implied by CHAPELLE SIXTEEN, but it isn't enough :-(
				if (++irq_timer>=52||(irq_delay&&++irq_delay>2))
				{
					if (irq_timer&32&&!plus_pri) // irq_timer&32 is required by ONESCREEN COLONIES: part 1 must show "LEVEL-1" in red, not white!
						gate_count_r3x=plus_enabled&&!(video_pos_x&8)?2:1; // PLUS ASIC: DEFAULT TIMER INTERRUPT
					irq_delay=irq_timer=0;
				}
			}
			else
				crtc_count_r3x=(crtc_count_r3x+1)&31;
		}
		if (!--gate_count_r3x) if (!plus_pri||plus_pri==crtc_line) z80_irq|=128; // raster interrupts happen 1 NOP later on PLUS!
	}
}

void audio_main(int t) // render audio output for `t` clock ticks; t is always nonzero!
{
	AUDIO_DATATYPE *z=audio_target;
	psg_main(t);
	AUDIO_DATATYPE k=((tape_status^tape_output)<<4)<<(AUDIO_BITDEPTH-8); // tape signal
	while (z<audio_target)
		*z++-=k;
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
	if (autorun_t==-4) // PLUS menu (1/2)
	{
		autorun_kbd_set(0x0D); // PRESS F1
	}
	else if (autorun_t==-2) // PLUS menu (2/2)
	{
		autorun_kbd_res(0x0D); // RELEASE F1
	}
	else switch (autorun_mode)
	{
		case 1: // autorun tape (1/2): press autorun keys
			if (autorun_t>50)
			{
				autorun_kbd_set(0x17); // PRESS CONTROL
				autorun_kbd_set(0x06); // PRESS ENTER
				autorun_mode=3;
			}
			break;
		case 3: // autorun tape (2/2): release autorun keys
			if (autorun_t>52)
			{
				autorun_kbd_res(0x17); // RELEASE CONTROL
				autorun_kbd_res(0x06); // RELEASE ENTER
				autorun_mode=8;
			}
			break;
		case 2: // autorun disc: generate RUN"FILENAME string
			if (autorun_t>50)
			{
				strcpy(&PEEK(type_id?0xAC8A:0xACA4),autorun_line); // type hidden line
				autorun_mode=8;
			}
			break;
		case 8: // shared autorun (1/2): press RETURN
			if (autorun_t>58)
			{
				autorun_kbd_set(0x12); // PRESS RETURN
				autorun_mode=9;
			}
			break;
		case 9: // shared autorun (2/2): release RETURN
			if (autorun_t>60)
			{
				autorun_kbd_res(0x12); // RELEASE RETURN
				autorun_mode=0;
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

// the CPC hands the Z80 a data bus value that is NOT constant on the PLUS ASIC
#define z80_irq_bus (plus_enabled?(plus_ivr&-8)+(z80_irq&128?6:z80_irq&16?0:z80_irq&32?2:4):0xFF) // 6 = PRI, 0 = DMA2, 2 = DMA1, 4 = DMA0.
// the CPC lacks special cases where RETI and RETN matter
#define z80_retn()
// the CPC obeys the Z80 IRQ ACK signal unless the PLUS ASIC IVR bit 0 is off (?)
void z80_irq_ack(void)
{
	if (z80_irq&128) // OLD IRQ + PLUS PRI
		plus_dcsr|=128,z80_irq&=64+32+16,irq_timer&=~32;
	else
	{
		plus_dcsr&=~128;
		if (z80_irq&16) // DMA2?
		{
			plus_dcsr|=16;
			if (!(plus_ivr&1))
				z80_irq&=64+32;
		}
		else if (z80_irq&32) // DMA1?
		{
			plus_dcsr|=32;
			if (!(plus_ivr&1))
				z80_irq&=64;
		}
		else // DMA0!
		{
			plus_dcsr|=64;
			if (!(plus_ivr&1))
				z80_irq=0;
		}
	}
}

void z80_sync(int t) // the Z80 asks the hardware/video/audio to catch up
{
	static int r=0; r+=t;
	int tt=r/z80_multi; // calculate base value of `t`
	r-=(t=tt*z80_multi); // adjust `t` and keep remainder
	if (t)
	{
		if (!disc_disabled)
			disc_main(t);
		if (tape_enabled) // TAPE MOTOR ON?
		{
			if ((tape_delay-=t)<0)
				tape_delay=0;
			if (tape_delay<=TICKS_PER_SECOND/15) // reject accidental "clicks" (cfr. "RUN THE GAUNTLET")
				tape_main(t);
		}
		else // delay later readings
		{
			if ((tape_delay+=t)>TICKS_PER_SECOND/14) // MINILOAD (>3) and Opera Soft (<25) tapes hinge on this!
				tape_delay=TICKS_PER_SECOND/14;
		}
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
	// * 0x7F00 : "Hero Quest" sends GATE ARRAY bytes to 00C0 by mistake! (OUT 00C0,C0 for OUT 7FC0,C0)
	// * 0xDF00 : "The Final Matrix" crack corrupts BC' (0389,038D rather than 7F89,7F8D) before it tries doing OUT 7F00,89 and OUT 7F00,8D!
	// * 0xF600 : "Bombfusion" sends PIO bytes (start tape signal) to 0210 by mistake!
	// * (p&0x100)+0xBC00 : "Knight Rider" sends CRTC bytes to 0088 by mistake! "Camenbert Meeting 4" sends CRTC bytes to $0C00! "The Demo" sends CRTC bytes to 0x1D00 and 0x1C00!
	// * 0x00XX : "Overflow Preview 3" ($521D et al.) shuffles RAM and ROM by mistake!
	if (!(p&0x8000)) // 0x7F00, GATE ARRAY (1/2)
	{
		if (!(b&0x80))
		{
			if (p&0x4000) // quirk explained by http://quasar.cpcscene.net/doku.php?id=assem:gate_table : the CLUT is NOT modified if address bit 14 is zero
			{
				if (!(b&0x40))
					gate_table_select(b); // 0x00-0x3F: SELECT PALETTE ENTRY
				else
					gate_table_send(b); // 0x40-0x7F: WRITE PALETTE ENTRY
			}
		}
		else
		{
			if (!(b&0x40)) // cfr. nota supra
			{
				if (p&0x4000) // Quasar CPC forgot to document that the MCR is NOT modified if address bit 14 is zero!!
				{
					if ((b&0x20)&&plus_gate_enabled)
						plus_gate_mcr=b&31; // PLUS ASIC MULTICONFIGURATION REGISTER
					else
					{
						if (b&16)
							z80_irq&=~128,irq_timer=0;
						gate_mcr=b&15; // 0x80-0xBF: MULTICONFIGURATION REGISTER
					}
					if (!(gate_mcr&4)&&z80_pc.w<8)
						gate_ram_dirty=64; // catch warm reset
				}
			}
			else
				gate_ram=b&63; // 0xC0-0xFF: SET RAM BANK MODE
			mmu_update();
		}
	}
	if (!(p&0x4000)) // 0xBC00-0xBF00, CRTC 6845
	{
		if (!(p&0x0200)) // "Night Shift" sends bytes to BFFD by mistake!
		{
			if (!(p&0x0100))
			{
				if (plus_enabled) // PLUS ASIC UNLOCKING SEQUENCE
					{
						if (!plus_gate_counter)
							plus_gate_counter=!b; // first byte must be nonzero
						else if (b!=plus_gate_lock[plus_gate_counter]) // ignore repetitions (SWITCHBLADE)
						{
							if (++plus_gate_counter>=length(plus_gate_lock))
								plus_gate_counter=0,plus_gate_enabled=b==0xCD;
							else if (b!=plus_gate_lock[plus_gate_counter])
								plus_gate_counter=!b; // detect nonzero case (DICK TRACY)
						}
					}
				crtc_table_select(b); // 0xBC00: SELECT CRTC REGISTER
			}
			else
				crtc_table_send(b); // 0xBD00: WRITE CRTC REGISTER
		}
	}
	if (!(p&0x2000)) // 0xDF00, GATE ARRAY (2/2)
	{
		gate_rom=b; // SELECT ROM BANK
		mmu_update();
	}
	if (!(p&0x1000)) // 0xEF00, PRINTER
		; // *!*
	if (!(p&0x0800)) // 0xF400-0xF700, PIO 8255
	{
		if (!(p&0x0200))
		{
			if (!(p&0x0100)) // 0xF400, PIO PORT A
			{
				if (!(pio_control&0x10)) // reject READ PSG REGISTER
				{
					pio_port_a=b;
					if (pio_port_c&0x80)
					{
						if (pio_port_c&0x40) // SELECT PSG REGISTER
							psg_table_select(pio_port_a);
						else // WRITE PSG REGISTER
							psg_table_send(pio_port_a);
					}
				}
			}
			else // 0xF500, PIO PORT B
			{
				pio_port_b=b;
			}
		}
		else
		{
			if (!(p&0x0100)) // 0xF600, PIO PORT C
			{
				pio_port_c=b;
				if (pio_port_c&0x80)
				{
					if (pio_port_c&0x40) // SELECT PSG REGISTER
						psg_table_select(pio_port_a);
					else // WRITE PSG REGISTER
						psg_table_send(pio_port_a);
				}
			}
			else // 0xF700, PIO CONTROL
			{
				if (b&128)
				{
					pio_control=b;
					if (!plus_enabled) // CRTC3 has a PIO bug! CRTC0,CRTC1,CRTC2,CRTC4 have a good PIO
						pio_port_a=pio_port_b=pio_port_c=0; // reset all ports!
				}
				else if (b&1)
					pio_port_c|=(1<<((b>>1)&7)); // SET BIT
				else
					pio_port_c&=~(1<<((b>>1)&7)); // RESET BIT
			}
			tape_output=(pio_port_c&32)&&tape; // tape record signal
		}
	}
	if (!(p&0x0480)) // 0xFB7F, FDC 765
	{
		if (!disc_disabled)
		{
			if (p&0x100)
				disc_data_send(b); // 0xFB7F: DATA I/O
			else
				disc_motor_set(b&1); // 0xFA7E: MOTOR
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
	/*  0 */ {  + 3,  10,0XD0,0XDD,0X77,0X00,0XDD,0X23,0X15,0X1D,0X20,0XF3 }, // AMSTRAD CPC FIRMWARE
	/*  1 */ {  - 8,   4,0XDD,0X23,0X1B,0X08, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCB }, // DINAMIC, GREMLIN OLD
	/*  2 */ {  - 8,   4,0XDD,0X23,0X1B,0X08, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD2 }, // TOPO SOFT
	/*  3 */ {  -26,   6,0XDD,0X75,0X00,0XDD,0X23,0X1B, +35,   7,0X08,0XAD,0X08,0X7A,0XB3,0X20,0XD0 }, // SPEEDLOCK1
	/*  4 */ {  - 8,   4,0XDD,0X2B,0X1B,0X08, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCB }, // GRANDSLAM
	/*  5 */ {  +18,  10,0XDD,0X74,0X00,0XDD,0X2B,0X1B,0X7B,0XB2,0X20,0XDE }, // OPERA SOFT 1
	/*  6 */ {  +18,   4,0XDD,0X74,0X00,0X7C,  +6,   7,0XDD,0X2B,0X1B,0X7B,0XB2,0X20,0XD7 }, // OPERA SOFT 2
	/*  7 */ {  +14,  10,0X73,0X23,0X1E,0X01,0XD9,0X1B,0X7A,0XB3,0XD9,0X3E,  +1,   2,0X20,0XE5 }, // CODEMASTERS1,OCEAN LEVELS (DALEY THOMPSON'S OLYMPIC CHALLENGE)
	/*  8 */ {  -10,   6,0XDD,0X75,0X00,0XDD,0X23,0X1B, +17,   8,0X00,0X00,0X00,0X00,0X7A,0XB3,0X20,0XE1 }, // RICOCHET
	/*  9 */ {  - 8,   4,0XDD,0X23,0X1B,0X08, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCA }, // MASTERTRONIC, CODEMASTERS2
	/* 10 */ {  -14,  11,0xDD,0x75,0x00,0xDD,0x23,0x1B,0x7A,0xB3,0x37,0xC8,0x06, +17,    2,0x18,0xE2 }, // HI-TEC
	/* 11 */ {  +14,  11,0XDD,0X75,0X00,0XDD,0X23,0X2E,0X01,0X1B,0X7A,0XB3,0X3E,  +1,   4,0X00,0X00,0X20,0XE2 }, // SPEEDLOCK LEVELS
	/* 12 */ {  -10,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X06, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE1 }, // CODEMASTERS3
	/* 13 */ {  -10,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X06, +17,   6,0X7C,0XAD,0X67,0X7A,0X3C,0X20 }, // GREMLIN 128K
	/* 14 */ {  - 8,   4,0XDD,0X23,0X1B,0X08, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD1 }, // GREMLIN OLD LEVELS
	/* 15 */ {  -22,   4,0XDD,0X75,0X00,0X18, +10,   5,0XDD,0X23,0X1B,0X08,0X06 }, // MIKROGEN
	/* 16 */ {  -13,   8,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X2E,0x01, +14,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE3 }, // MINILOAD ++ (NORMAL)
	/* 17 */ {  -13,   8,0XDD,0X75,0X00,0XDD,0X2B,0X1B,0X2E,0x01, +14,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE3 }, // MINILOAD -- (NORMAL)
	/* 18 */ {  -13,   8,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X2E,0x01, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XDF }, // MINILOAD ++ (CUSTOM)
	/* 19 */ {  -13,   8,0XDD,0X75,0X00,0XDD,0X2B,0X1B,0X2E,0x01, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XDF }, // MINILOAD -- (CUSTOM)
	/* 20 */ {  -13,   8,0xDD,0X75,0X00,0XDD,0X23,0X1B,0X2E,0X01, +15,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE2 }, // TINYTAPE ++ (COUNTER)
	/* 21 */ {  -13,   8,0xDD,0X75,0X00,0XDD,0X2B,0X1B,0X2E,0X01, +15,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE2 }, // TINYTAPE -- (COUNTER)
	/* 22 */ {  +10,  10,0X7C,0XAD,0X67,0XDD,0X75,0X00,0XDD,0X23,0X18,0XE5 }, // TINYTAPE ++ (NO COUNTER)
	/* 23 */ {  +10,  10,0X7C,0XAD,0X67,0XDD,0X75,0X00,0XDD,0X2B,0X18,0XE5 }, // TINYTAPE -- (NO COUNTER)
};
int z80_tape_fastdump(WORD p)
{
	p-=3; // avoid clashes with z80_tape_fastfeed()
	int i=z80_tape_fastindices[p]-192; // avoid clashing with z80_tape_fastfeed!
	if (i<0||i>length(z80_tape_fastdumper)) // already in the cache?
	{
		for (i=0;i<length(z80_tape_fastdumper);++i)
			if (fasttape_test(z80_tape_fastdumper[i],p))
				break;
		logprintf("FASTDUMP %04X:%02i\n",p,i<length(z80_tape_fastdumper)?i:-1);
	}
	return z80_tape_fastindices[p]=i+192,i;
}

const BYTE z80_tape_fastfeeder[][24]=
{
	/*  0 */ {   +0,   8,0X30,0X0D,0X7C,0X91,0X9F,0XCB,0X12,0XCD,  +2,   3,0X1D,0X20,0XEA }, // AMSTRAD CPC FIRMWARE
	/*  1 */ {   +0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   2,0X30,0XF3 }, // DINAMIC, TOPO SOFT
	/*  2 */ {   +0,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -11 }, // GRANDSLAM
	/*  3 */ {   +0,   3,0XE1,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   1,0XD2,-128, -22 }, // SPEEDLOCK1
	/*  4 */ {   +0,   2,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   1,0XD2 }, // SPEEDLOCK2
	/*  5 */ {   +0,   3,0X30,0X1E,0X3E,  +1,   4,0XBA,0XCB,0X13,0X16,  +1,   2,0X30,0XF0 }, // OCEAN LEVELS (DALEY THOMPSON'S OLYMPIC CHALLENGE)
	/*  6 */ {   +0,   3,0X30,0X1D,0X3E,  +1,   4,0XBA,0XCB,0X13,0X16,  +1,   2,0X30,0XF0 }, // OCEAN LEVELS (OPERATION WOLF)
	/*  7 */ {   +0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2 }, // HI-TEC, RAINBOW ARTS, CODEMASTERS2, CODEMASTERS3, SUPERTRUX
	/*  8 */ {   +0,   3,0X30,0X26,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   2,0X30,0XF0 }, // SPEEDLOCK LEVELS 1
	/*  9 */ {   +0,   3,0X30,0X1F,0X3E,  +1,   4,0XBA,0XCB,0X13,0X16,  +1,   2,0X30,0XF0 }, // CODEMASTERS1
	/* 10 */ {   +0,   3,0X30,0X32,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   2,0X30,0XF2 }, // GREMLIN 128K
	/* 11 */ {   +0,   3,0X30,0X33,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   2,0X30,0XF2 }, // GREMLIN 128K LEVELS
	/* 12 */ {   +0,   1,0X30,  +1,   5,0X90,0XCB,0X15,0X30,0XF1 }, // TINYTAPE
	/* 13 */ {   +0,   6,0XD0,0X90,0XCB,0X15,0X30,0XF2 }, // MINILOAD
	/* 14 */ {   +0,   1,0XD2,  +2,   1,0X3E,  +1,   9,0XB8,0XCB,0X15,0X3E,0X00,0X00,0X3E,0X15,0XD2,-128, -19 }, // ALKATRAZ
	/* 15 */ {   +0,   2,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   2,0X30,0XF3 }, // RICOCHET
	/* 16 */ {   -5,   3,0x1E,0x01,0xCD,  +2,   1,0XFE,  +1,   4,0X3F,0XCB,0X13,0XD2,-128,  -9 }, // GREMLIN 1+2 ("BASIL", "MASK")
	/* 17 */ {   +0,   2,0X7D,0XFE,  +1,   7,0X3F,0XCB,0X14,0XAC,0XE6,0X1F,0XCD,  +2,   3,0XC1,0X10,0XEA }, // OPERA SOFT
	/* 18 */ {   +0,   1,0XD2,  +2,   1,0X3E,  +1,  +4,0XB8,0XCB,0X15,0X3E,  +1,  +1,0XD2,-128, -16 }, // HEXAGON
	/* 19 */ {   +0,  10,0X30,0X0A,0X7C,0X91,0X9F,0XCB,0X12,0X1D,0X20,0XED }, // CASSYS
	/* 20 */ {   -8,   3,0X16,0X01,0XCD,  +5,   1,0X3A,  +2,   5,0XBB,0XCB,0X12,0X30,0XF2 }, // BLEEPLOAD 2(1)
	/* 21 */ {   -8,   3,0X16,0X01,0XCD,  +5,   1,0X3E,  +1,   5,0XBB,0XCB,0X12,0X30,0XF3 }, // BLEEPLOAD 2(2)
	/* 22 */ {   +0,   2,0XD0,0X3E,  +1,   4,0XBA,0XCB,0X13,0X16,  +1,   2,0X30,0XF1 }, // OCEAN LEVELS (SLY SPY)
	/* 23 */ {   +0,   1,0XD2,  +2,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2 }, // MIKROGEN
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
WORD z80_tape_spystackadd(int n)
{
	BYTE i=PEEK(z80_sp.w+n+0);
	return i+(PEEK(z80_sp.w+n+1)<<8);
}

const BYTE z80_tape_fastloader[][24]= // format: chains of {offset relative to the PC of the trapped IN opcode, size, string of opcodes}; extra -128 stands for PC of the first byte
{
	/*  0 */ {   -8,  13,0X79,0XC6,0X02,0X4F,0X38,0X0E,0XED,0X78,0XAD,0XE6,0X80,0X20,0XF3 }, // AMSTRAD CPC FIRMWARE
	/*  1 */ {   -5,  12,0X04,0XC8,0XD9,0XED,0X78,0XD9,0X1F,0XA9,0XE6,0X40,0X28,0XF4 }, // DINAMIC
	/*  2 */ {  -15,  20,0X04,0XC8,0X3E,0XF4,0XDB,0X00,0XE6,0X04,0XEE,0X04,0XC0,0X3E,0XF5,0XDB,0X00,0XA9,0XE6,0X80,0X28,0XEC }, // TOPO
	/*  3 */ {   -6,  12,0X04,0XC8,0X3E,0XF5,0XDB,0X00,0XA9,0XE6,0X80,0XD8,0X28,0XF4 }, // MULTILOAD
	/*  4 */ {   -6,  11,0X24,0XC8,0X06,0XF5,0XED,0X78,0XA9,0XE6,0X80,0X28,0XF5 }, // SPEEDLOCK
	/*  5 */ {   -6,  12,0X04,0XC8,0X3E,0XF5,0XDB,0XFF,0X1F,0XC8,0XA9,0XE6,0X40,0X28 }, // ALKATRAZ, HEXAGON
	/*  6 */ {   -6,   8,0X14,0XC8,0X06,0XF5,0XED,0X78,0XA9,0XF2,-128,  -8 }, // CODEMASTERS1
	/*  7 */ {   -7,  14,0X04,0XC8,0X37,0X00,0XD9,0XED,0X78,0XD9,0X00,0XA9,0XE6,0X80,0X28,0XF2 }, // MASTERTRONIC,CODEMASTERS0
	/*  8 */ {   -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   5,0XA9,0XE6,0X80,0X28,0XF5 }, // TINYTAPE/MINILOAD
	/*  9 */ {   -3,   5,0X1C,0XED,0X78,0XA9,0XF2,-128,  -5 }, // BLEEPLOAD(1+2)
	/* 10 */ {   -6,  15,0X04,0XC8,0X3E,0XF5,0XDB,0X00,0X1F,0X00,0X00,0X00,0XA9,0XE6,0X40,0X28,0XF1 }, // CODEMASTERS2
	/* 11 */ {   -5,  11,0X04,0XC8,0XD9,0XED,0X78,0XD9,0XA9,0XE6,0X80,0X28,0XF5 }, // MIKROGEN
	/* 12 */ {   -3,   5,0X2C,0XED,0X78,0XA9,0XF2,-128,  -5 }, // BLEEPLOAD1-MUSICAL
	/* 13 */ {   -4,   8,0X24,0XC8,0XED,0X78,0XA9,0XE6,0X80,0XCA,-128,  -8 }, // UNILODE
	/* 14 */ {   -7,  10,0X04,0XC8,0XD9,0X06,0XF5,0XED,0X78,0XD9,0XA9,0XF2,-128, -10 }, // HI-TEC, RAINBOW ARTS
	/* 15 */ {   -6,  13,0X04,0XC8,0X3E,0XF5,0XDB,0XFF,0X1F,0X1F,0XA9,0XE6,0X20,0X28,0XF3 }, // ZYDROLOAD
	/* 16 */ {   -5,   9,0X01,0X00,0XF5,0XED,0X78,0XCB,0X7F,0X20,0XF7 }, // OPERA1 1/2
	/* 17 */ {   -5,  10,0X01,0X00,0XF5,0XED,0X78,0X2C,0XCB,0X7F,0X28,0XF6 }, // OPERA1 2/2
	/* 18 */ {   -2,   4,0XED,0X78,0XA9,0XF2,-128,  -4 }, // BLEEPLOAD1-GAPS
	/* 19 */ {   -2,   3,0XED,0X78,0XFA,-128,  -3 }, // OPERA2 1/2 + MARMELADE 1/2
	/* 20 */ {   -3,   4,0X2C,0XED,0X78,0XF2,-128,  -4 }, // OPERA2 2/2
	/* 21 */ {   -5,  13,0X04,0XC8,0XD9,0XED,0X78,0XD9,0X00,0X00,0XA9,0XE6,0X80,0X28,0XF3 }, // RAINBOW ARTS
	/* 22 */ {   -4,   9,0X14,0XC8,0XED,0X78,0XA9,0XE6,0X80,0X28,0XF7 }, // HALFLOAD
	/* 23 */ {   -3,   4,0X0C,0XED,0X78,0XFA,-128,  -4 }, // GREMLIN 1/2
	/* 24 */ {   -3,   4,0X0C,0XED,0X78,0XF2,-128,  -4 }, // GREMLIN 2/2
	/* 25 */ {   -2,   3,0XED,0X78,0XF2,-128,  -3 }, // GREMLIN SKIP + MARMELADE 2/2
	/* 26 */ {   -6,  13,0X04,0XC8,0X3E,0XF5,0XDB,0X00,0X07,0XA9,0XE6,0X01,0X00,0X28,0XF3 }, // GREMLIN 128K
	/* 27 */ {   -5,  13,0X04,0XC8,0XD9,0XED,0X78,0XD9,0X1F,0X00,0XA9,0XE6,0X40,0X28,0XF3 }, // GREMLIN OLD
	/* 28 */ {   -4,   9,0X3E,0XF5,0XDB,0X00,0X07,0X38,0X02,0X10,0XF7 }, // "PUFFY'S SAGA" 1/2
	/* 29 */ {   -4,   9,0X3E,0XF5,0XDB,0X00,0X07,0X30,0X02,0X10,0XF7 }, // "PUFFY'S SAGA" 2/2
	/* 30 */ {   -8,  13,0X79,0XC6,0X02,0X4F,0X38,0X17,0XED,0X78,0XAD,0XE6,0X80,0X20,0XF3 }, // "KORONIS RIFT"
	/* 31 */ {   -5,   7,0X0C,0X28,0X0B,0XED,0X78,0XAD,0XF2,-128,  -7 }, // "TWINWORLD"
	/* 32 */ {   -4,   8,0X14,0XC8,0XED,0X78,0XE6,0X80,0XA9,0XCA,-128,  -8 }, // "SPLIT PERSONALITIES"
	/* 33 */ {   -4,   9,0X06,0XF5,0XED,0X78,0XE6,0X80,0XA9,0X28,0XF9 }, // TITUS 1/X
	/* 34 */ {   -2,   5,0XED,0X78,0XE6,0X80,0X4F }, // TITUS 2/X (JUST TO DETECT IT)
	/* 35 */ {   -9,  14,0XED,0X5F,0XFD,0XBE,0X00,0X30,0X12,0XED,0X78,0XE6,0X80,0XA9,0X28,0XF2 }, // TITUS 3/X
	/* 36 */ {   -9,  14,0XED,0X5F,0XFD,0XBE,0X00,0X30,0X11,0XED,0X78,0XE6,0X80,0XA9,0X28,0XF2 }, // TITUS 4/X
	/* 37 */ {   -2,  15,0XED,0X78,0XE6,0X80,0XA9,0X20,0X08,0X00,0X00,0X3E,0X04,0X83,0X5F,0X18,0XF1 }, // TITUS 5/X
	/* 38 */ {   -7,  13,0X04,0XC8,0XD9,0X06,0XF5,0XED,0X78,0XD9,0XA9,0XE6,0X80,0X28,0XF3 }, // CODEMASTERS3
	/* 39 */ {   -6,   8,0X24,0XC8,0X06,0XF5,0XED,0X78,0XA9,0XF2,-128,  -8 }, // SPEEDLOCK LEVELS
	/* 40 */ {   -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   7,0X00,0XA9,0XD8,0XE6,0X80,0X28,0XF3 }, // SUPERTRUX 1/2
	/* 41 */ {   -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   6,0X00,0XA9,0XE6,0X80,0X28,0XF4 }, // SUPERTRUX 2/2
	/* 42 */ {   -7,  10,0X06,0XF5,0X0C,0X28,0X14,0XED,0X78,0X17,0X30,0XF8 }, // AUDIOGENIC 1/2
	/* 43 */ {   -5,   8,0X0C,0X28,0X0C,0XED,0X78,0X17,0X38,0XF8 }, // AUDIOGENIC 2/2
	/* 44 */ {   -6,  13,0X04,0XC8,0X3E,0XF5,0XDB,0XFF,0XB7,0XD8,0XA9,0XE6,0X80,0X28,0XF3 }, // BIRDIE
};
void z80_tape_fastload_ccitt(int mask8)
{
	WORD x=type_id?0xB1EB:0xB8D3; // CCITT 16-bit checksum location
	WORD crc16=mgetii(&PEEK(x));
	while (mask8&0xFF)
	{
		if ((mask8^crc16)&0x8000)
			crc16=(crc16<<1)^0x1021;
		else
			crc16<<=1;
		mask8<<=1;
	}
	mputii(&POKE(x),crc16);
}
INLINE void z80_tape_fastload(void)
{
	int i;
	if ((i=z80_tape_fastindices[z80_pc.w])<0||i>length(z80_tape_fastloader))
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
		if (i!=3) // MULTILOAD expects IRQs!
			z80_irq=irq_timer=0;
	}
	//if (tape_fastload) // examine always, although not always for speedup
	{
		int j;
		switch (i) // is it a tape loader that we can optimize?
		{
			case 0: // AMSTRAD CPC FIRMWARE, CASSYS ("WONDER BOY"), HEWSON ("EXOLON","NEBULUS") // ADD C,$02 // XOR L:AND $80:JR NZ,..
			case 30: // "KORONIS RIFT" // ADD C,$02 // XOR L:AND $80:JR NZ,..
				if (z80_de.b.l==0x08&&FASTTAPE_CAN_FEED()&&(((i=z80_tape_fastfeed(z80_tape_spystack()))==0&&!(gate_mcr&4))||(i==19)))
				{
					if (i==0)
					{
						// DE is kept in the stack :-( E!=0 means there are bytes to write on memory
						if (z80_tape_fastdump(z80_tape_spystackadd(4))==0)
							while (tape_bits>15&&POKE(z80_sp.w+2)>1)
							{
								j=fasttape_dump();
								POKE(z80_ix.w)=j;
								z80_tape_fastload_ccitt((j<<8)+1);
								++z80_ix.w;
								--POKE(z80_sp.w+2);
								--POKE(z80_sp.w+3);
							}
						j=fasttape_feed();
						z80_tape_fastload_ccitt((j<<8)+2);
					}
					else
						j=fasttape_feed();
					z80_de.b.h=j>>1,tape_feedskip=z80_de.b.l=1,z80_bc.b.l=j&1?-1:0,FASTTAPE_FEED_END(!(z80_hl.b.l>>7),16);
				}
				else
					z80_r7+=fasttape_add8(!(z80_hl.b.l>>7),16,&z80_bc.b.l,2)*10;
				break;
			case 1: // DINAMIC ("ABU SIMBEL PROFANATION"), GREMLIN ("BOUNDER", "FUTURE KNIGHT"), GRANDSLAM ("PACMANIA") // INC B' // XOR C':AND $40:JR Z,..
				if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==1||i==2||i==7))
				{
					if (((j=z80_tape_fastdump(z80_tape_spystack()))==1||j==4||j==9)&&(z80_af2.b.l&0x41)==0x41)
						while (tape_bits>15&&z80_de2.b.l>1)
							z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),(j==4?--z80_ix.w:++z80_ix.w),--z80_de2.w;
					j=fasttape_feed(),tape_feedskip=z80_hl2.b.l=128+(j>>1),z80_bc2.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc2.b.l>>6,16);
				}
				else
					z80_r7+=fasttape_add8(z80_bc2.b.l>>6,16,&z80_bc2.b.h,1)*10;
				break;
			case 2: // TOPO ("MAD MIX GAME", "METROPOLIS"), ERBE ("DALEY THOMPSON'S OLYMPIC CHALLENGE", "DRAGON NINJA") // INC B // XOR C:AND $80:JR Z,..
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==1)
				{
					if (z80_tape_fastdump(z80_tape_spystack())==2&&z80_af2.b.l&0x40)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>7,25);
				}
				else
					z80_r7+=fasttape_add8(z80_bc.b.l>>7,25,&z80_bc.b.h,1)*12;
				break;
			case 3: // MULTILOAD ("TECHNICIAN TED", "DEFLEKTOR") // INC B // XOR C:AND $80:JR Z,..
				z80_r7+=fasttape_add8(z80_bc.b.l>>7,16,&z80_bc.b.h,1)*8;
				break;
			case 4: // SPEEDLOCK ("DONKEY KONG", "ARKANOID"), RICOCHET ("ANARCHY", "RICK DANGEROUS 2") // INC H // XOR C:AND $80:JR Z,..
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==3||i==4||i==15))
				{
					if ((j=z80_tape_fastdump(z80_tape_spystack()))==3||j==8)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_af2.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
					j=fasttape_feed();
					if (i==3)
						POKE(z80_sp.w+2)=tape_feedskip=128+(j>>1),POKE(z80_sp.w+3)=j&1?-1:0;
					else
						tape_feedskip=z80_hl.b.l=128+(j>>1),z80_hl.b.h=j&1?-1:0;
					FASTTAPE_FEED_END(z80_bc.b.l>>7,15);
				}
				else
					z80_r7+=fasttape_add8(z80_bc.b.l>>7,15,&z80_hl.b.h,1)*8;
				break;
			case 5: // ALKATRAZ ("STREET FIGHTER", "E-MOTION") // INC B // XOR C:AND $40:JR Z,..
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==14||i==18))
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>6,17);
				else
					fasttape_add8(z80_bc.b.l>>6,17,&z80_bc.b.h,1);
				break;
			case 6: // CODEMASTERS1 ("TREASURE ISLAND DIZZY"), OCEAN LEVELS ("DALEY THOMPSON'S OLYMPIC CHALLENGE") // INC D // XOR C:JP NS,..
			case 22: // HALFLOAD ("JUSTIN") // INC D // XOR C:AND $80:JR Z,..
			case 32: // "SPLIT PERSONALITIES" // XOR C:JP Z,... // INC D:RET Z
				if (z80_de.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==5||i==6||i==9||i==22))
				{
					if (z80_tape_fastdump(z80_tape_spystack())==7)
						while (tape_bits>15&&z80_de2.b.l>1)
							POKE(z80_hl.w)=fasttape_dump(),++z80_hl.w,--z80_de2.w;
					j=fasttape_feed(),tape_feedskip=z80_de.b.l=128+(j>>1),z80_de.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>7,13);
				}
				else
					fasttape_add8(z80_bc.b.l>>7,13,&z80_de.b.h,1);
				break;
			case 7: // MASTERTRONIC ("DEFCOM", "RASTERSCAN") // INC B' // XOR C':AND $80:JR Z,..
				if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==7)
				{
					if (z80_tape_fastdump(z80_tape_spystack())==9&&(z80_af2.b.l&0x41)==0x41)
						while (tape_bits>15&&z80_de2.b.l>1)
							z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de2.w;
					j=fasttape_feed(),tape_feedskip=z80_hl2.b.l=128+(j>>1),z80_bc2.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc2.b.l>>7,18);
				}
				else
					fasttape_add8(z80_bc2.b.l>>7,18,&z80_bc2.b.h,1);
				break;
			case 8: // TINYTAPE/MINILOAD (CPCRETRODEV 2018 et al.) // INC B // XOR C:AND $80:JR Z,..
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==12||i==13))
				{
					if ((i=z80_tape_fastdump(z80_tape_spystack()))>=16&&i<=23)
						while (tape_bits>15&&(i>=20||z80_de.b.l>1))
							z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),(i&1)?--z80_ix.w:++z80_ix.w,(i<22)?--z80_de.w:0;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>7,14);
				}
				else
					fasttape_add8(z80_bc.b.l>>7,14,&z80_bc.b.h,1);
				break;
			case 9: // BLEEPLOAD(1+2) ("SENTINEL", "THRUST" V2) // INC E // XOR C:JP NS,..
				if (z80_de.b.h==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystackadd(2)))==20||i==21)) // skip PUSH BC
					j=fasttape_feed(),tape_feedskip=z80_de.b.h=128+(j>>1),z80_de.b.l=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>7,9);
				else
					fasttape_add8(z80_bc.b.l>>7,9,&z80_de.b.l,1);
				break;
			case 10: // CODEMASTERS2 ("KWIK SNAX", "BUBBLE DIZZY") // INC B // XOR C:AND $40:JR Z,..
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==7)
				{
					if (z80_tape_fastdump(z80_tape_spystack())==9&&(z80_af2.b.l&0x41)==0x41)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>6,18);
				}
				else
					fasttape_add8(z80_bc.b.l>>6,18,&z80_bc.b.h,1);
				break;
			case 11: // MIKROGEN ("FROST BYTE", "COP OUT") // INC B' // XOR C':AND $80:JR Z,..
			case 14: // HI-TEC ("INTERCHANGE", "JONNY QUEST") // INC B' // XOR C':JP NS,..
				if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&(i=z80_tape_fastfeed(z80_tape_spystack()))==7||i==23)
				{
					if ((i=z80_tape_fastdump(z80_tape_spystack()))==10||i==15)
						while (tape_bits>15&&z80_de2.b.l>1)
							POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de2.w;
					j=fasttape_feed(),tape_feedskip=z80_hl2.b.l=128+(j>>1),z80_bc2.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc2.b.l>>7,15);
				}
				else
					fasttape_add8(z80_bc2.b.l>>7,15,&z80_bc2.b.h,1);
				break;
			case 12: // BLEEPLOAD1-MUSICAL ("SPIKY HAROLD", "RASPUTIN") // INC L // XOR C:JP NS,..
				fasttape_add8(z80_bc.b.l>>7,9,&z80_hl.b.l,1);
				break;
				break;
			case 39: // SPEEDLOCK LEVELS ("RAINBOW ISLANDS")
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==8)
				{
					if (z80_tape_fastdump(z80_tape_spystack())==11)
						while (tape_bits>15&&z80_de.b.l>1)
							POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_hl.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>7,13);
				}
				else
			case 13: // UNILODE ("YES PRIME MINISTER", "TRIVIAL PURSUIT") // INC H // XOR C:AND $80:JP Z,..
					fasttape_add8(z80_bc.b.l>>7,13,&z80_hl.b.h,1);
				break;
			case 15: // ZYDROLOAD ("HOSTAGES", "NORTH & SOUTH") // INC B // XOR C:AND $20:JR Z,..
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==7)
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>5,16);
				else
					fasttape_add8(z80_bc.b.l>>5,16,&z80_bc.b.h,1);
				break;
			case 16: // OPERA1 1/2 ("LIVINGSTONE SUPONGO", "GOODY") // NO COUNTER! // BIT 7,A:JR NZ,..
				fasttape_skip(1,12);
				break;
			case 17: // OPERA1 2/2 // INC L // BIT 7,A:JR Z,..
				if (FASTTAPE_CAN_FEED()&&PEEK(z80_sp.w+3)==0x08&&z80_tape_fastfeed(z80_tape_spystack())==17)
				{
					if ((i=z80_tape_fastdump(z80_tape_spystack()))==5||i==6)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl2.w+=POKE(z80_ix.w)=fasttape_dump(),--z80_ix.w,--z80_de.w;
					j=fasttape_feed(),POKE(z80_sp.w+3)=1,z80_hl.b.h=tape_feedskip=(j>>1),z80_hl.b.l=j&1?-2:0,FASTTAPE_FEED_END(0,13); // -2 avoids the later INC L!
				}
				else
					fasttape_add8(0,13,&z80_hl.b.l,1);
				break;
			case 18: // BLEEPLOAD1-GAPS ("THRUST") // NO COUNTER! // XOR C:JP NS,..
				fasttape_skip(z80_bc.b.l>>7,8);
				break;
			case 19: // OPERA2 1/2 ("MYTHOS") // COUNTER IS R7 // JP S,...
				fasttape_add8(1,7,&z80_r7,3);
				break;
			case 20: // OPERA2 2/2 // INC L // JP NS,...
				if (FASTTAPE_CAN_FEED()&&PEEK(z80_sp.w+3)==0x08&&z80_tape_fastfeed(z80_tape_spystack())==17)
				{
					if (z80_tape_fastdump(z80_tape_spystack())==6)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl2.w+=POKE(z80_ix.w)=fasttape_dump(),--z80_ix.w,--z80_de.w;
					j=fasttape_feed(),POKE(z80_sp.w+3)=1,z80_hl.b.h=tape_feedskip=(j>>1),z80_hl.b.l=j&1?-1:0,FASTTAPE_FEED_END(0,8);
				}
				else
					fasttape_add8(0,8,&z80_hl.b.l,1);
				break;
			case 21: // RAINBOW ARTS ("ROCK 'N ROLL") // INC B' // XOR C':AND $80:JR Z,..
			case 38: // CODEMASTERS3 ("SUPER SEYMOUR SAVES THE PLANET") // INC B' // XOR C':AND $80:JR Z,..
				if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==7)
				{
					if (z80_tape_fastdump(z80_tape_spystack())==12)
						while (tape_bits>15&&z80_de2.b.l>1)
							z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de2.w;
					j=fasttape_feed(),tape_feedskip=z80_hl2.b.l=128+(j>>1),z80_bc2.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc2.b.l>>7,17);
				}
				else
					fasttape_add8(z80_bc2.b.l>>7,17,&z80_bc2.b.h,1);
				break;
			case 23: // GREMLIN 1/2 ("BASIL THE MOUSE DETECTIVE") // INC C // JP S,...
				fasttape_add8(1,8,&z80_bc.b.l,1);
				break;
			case 24: // GREMLIN 2/2 // INC C // JP NS,...
				if (z80_de.b.l==0x01&&FASTTAPE_CAN_FEEDX()&&z80_tape_fastfeed(z80_tape_spystack())==16)
					j=fasttape_feed(),tape_feedskip=z80_de.b.l=128+(j>>1),z80_bc.b.l=j&1?-1:0,FASTTAPE_FEED_END(0,8);
				else
					fasttape_add8(0,8,&z80_bc.b.l,1);
				break;
			case 25: // GREMLIN SKIP ("SAMURAI TRILOGY") // COUNTER IS R7 // JP NS,...
				fasttape_add8(0,7,&z80_r7,3);
				break;
			case 26: // GREMLIN 128K ("SUPER CARS") // INC B // XOR C:AND $01:JR Z,..
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==10||i==11))
				{
					if (z80_tape_fastdump(z80_tape_spystack())==13)
						while (tape_bits>15&&z80_de.b.l>1)
							z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l,16);
				}
				else
					fasttape_add8(z80_bc.b.l,16,&z80_bc.b.h,1);
				break;
			case 27: // GREMLIN OLD ("THING ON A SPRING") // INC B' // XOR C':AND $40:JR Z,..
				if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&((i=z80_tape_fastfeed(z80_tape_spystack()))==2||i==7))
				{
					if (((i=z80_tape_fastdump(z80_tape_spystack()))==1||i==14)&&(z80_af2.b.l&0x41)==0x41)
						while (tape_bits>15&&z80_de2.b.l>1)
							z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de2.w;
					j=fasttape_feed(),tape_feedskip=z80_hl2.b.l=128+(j>>1),z80_bc2.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc2.b.l>>6,17);
				}
				else
					fasttape_add8(z80_bc2.b.l>>6,17,&z80_bc2.b.h,1);
				break;
			case 28: // "PUFFY'S SAGA" 1/2 // DJNZ ... // RLCA:JR C,...
			case 29: // "PUFFY'S SAGA" 2/2 // DJNZ ... // RLCA:JR NC,...
				fasttape_sub8(i==28,12,&z80_bc.b.h,1);
				break;
			case 31: // "TWINWORLD" // INC C:JR Z,... // XOR L:JP NS,...
				fasttape_add8(z80_hl.b.l>>7,11,&z80_bc.b.l,1);
				break;
			case 33: // TITUS 1/X // COUNTER IS R7 // XOR C:JR Z,...
				fasttape_add8(z80_bc.b.l>>7,10,&z80_r7,5);
				break;
			case 35: // TITUS 3/X // COUNTER IS R7, OPPOSITE LIMIT IS (IY+0) // XOR C:JR Z,...
			case 36: // TITUS 4/X // COUNTER IS R7, OPPOSITE LIMIT IS (IY+0) // XOR C:JR Z,...
				z80_r7+=5*10; tape_main(5*19); // 5 is a value that pleases both "ONE" and "BLUES BROTHERS"
				break;
			case 37: // TITUS 5/X // ADD E,$04 // XOR C:JR Z,...
				fasttape_add8(z80_bc.b.l>>7,18,&z80_de.b.l,4);
				break;
			case 40: // SUPERTRUX // INC B // XOR C:AND $80:JR Z...
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==7)
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>7,17);
				else
					fasttape_add8(z80_bc.b.l>>7,17,&z80_bc.b.h,1);
				break;
			case 41: // SUPERTRUX // INC B // XOR C:AND $80:JR Z...
				if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_fastfeed(z80_tape_spystack())==7)
					j=fasttape_feed(),tape_feedskip=z80_hl.b.l=128+(j>>1),z80_bc.b.h=j&1?-1:0,FASTTAPE_FEED_END(z80_bc.b.l>>7,15);
				else
					fasttape_add8(z80_bc.b.l>>7,15,&z80_bc.b.h,1);
				break;
			case 42: // AUDIOGENIC 1/2 (LONE WOLF)
				fasttape_add8(0,11,&z80_bc.b.l,1);
				break;
			case 43: // AUDIOGENIC 2/2
				fasttape_add8(1,11,&z80_bc.b.l,1);
				break;
			case 44: // BIRDIE
				fasttape_add8(z80_bc.b.l>>7,17,&z80_bc.b.h,1);
				break;
		}
		//if (n) logprintf("[%02i:x%03i] ",i,n);
	}
}

BYTE z80_recv(WORD p) // the Z80 receives a byte from a hardware port
{
	BYTE b=0xFF;
	if (!(p&0x4000)) // 0xBC00-0xBF00, CRTC 6845
	{
		if (p&0x0200)
		{
			if (!(p&0x100))
			{
				// 0xBE00: depends on the CRTC type!
				if (crtc_type>=3)
					b&=crtc_table_recv(); // CRTC3,CRTC4: READ CRTC REGISTER
				else if (crtc_type==1)
					b&=crtc_table_info(); // CRTC1: READ CRTC INFORMATION STATUS
				else if (crtc_type==0)
					b=0; // CRTC0: ALL RESET!
				//else b&=255; // CRTC2: ALL SET!
			}
			else
			{
				// 0xBF00: depends on the CRTC type too!
				if (crtc_type>0&&crtc_type<3)
					b=0; // CRTC1,CRTC2: ALL RESET!
				else
					b&=crtc_table_recv(); // CRTC0,CRTC3,CRTC4: READ CRTC REGISTER
			}
		}
	}
	if (!(p&0x0800)) // 0xF400-0xF700, PIO 8255
	{
		switch (p&0x0300)
		{
			case 0x0000: // 0xF400, PIO PORT A
				if (pio_control&0x10) // READ PSG REGISTER
					b&=psg_index==14?(~autorun_kbd_bit[pio_port_c&15]):psg_table_recv(); // index 14 is keyboard port!
				else if (!plus_enabled)
					b=0; // "TIRE AU FLAN" expects this!? Is it nonzero on PLUS!?
				break;
			case 0x0100: // 0xF500, PIO PORT B
				if (pio_control&2||plus_enabled) // PLUS ASIC CRTC3 has a PIO bug!
				{
					if (tape&&!z80_iff.b.l) // does any tape loader enable interrupts at all???
						if (tape_fastload) // call only when this flag is on
							z80_tape_fastload();
					// VSYNC (0x01; CRTC2 MISSES IT DURING HORIZONTAL OVERFLOW!) + AMSTRAD MODEL (0x1E) + PRINTER IS OFFLINE (0x40) + TAPE SIGNAL (0x80)
					b&=(crtc_status&CRTC_STATUS_VSYNC?
						/*video_pos_y<128&&*/(crtc_type!=2||crtc_table[2]+crtc_limit_r3x<crtc_table[0]||crtc_table[2]+crtc_limit_r3x>crtc_table[0]+1):
						(crtc_table[8]==3&&irq_timer<3&&video_pos_y>=VIDEO_LENGTH_Y*5/12&&video_pos_y<VIDEO_LENGTH_Y*7/12)) // interlaced CRTC VSYNC!
						+((tape_delay|tape_status)?0xDE:0x5E);
				}
				else
					b&=pio_port_b; // CRTC0,CRTC1,CRTC2,CRTC4 have a good PIO
				break;
			case 0x0200: // 0xF600, PIO PORT C
				b&=pio_port_c;
				break;
		}
	}
	if (!(p&0x0400)) // 0xFB00, FDC 765
	{
		if (!disc_disabled)
			switch (p&0x0381) // 0xF87E+n
			{
				case 0x0300: // 0xFB7E: STATUS
					b&=disc_data_info();
					break;
				case 0x0301: // 0xFB7F: DATA I/O
					b&=disc_data_recv();
					break;
			}
	}
	if (p==0xFEFE)
		b&=0xCE; // emulator ID styled after the old CPCE
	return b;
}

// the PLUS ASIC features a special memory bank that behaves like a hardware address set rather than as a completely normal memory page.
void z80_trap(WORD p,BYTE b)
{
	switch (p>>8)
	{
		case 0x40: case 0x41: case 0x42: case 0x43:
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4A: case 0x4B:
		case 0x4C: case 0x4D: case 0x4E: case 0x4F:
			// sprite pixels
			plus_bank[p-0x4000]=b&15;
			//plus_dirtysprite=(p>>8)&15;
			break;
		case 0x60:
			// sprite coordinates
			if (p<0x6080)
				switch (p&7)
				{
					case 1: // POSX: -256..+767
						if ((b&=3)==3)
							b=-1;
						plus_bank[p-0x4000]=b;
						break;
					case 3: // POSY: -256..+255
						b=b&1?-1:0;
						// no `break`!
					case 0: // POSX low
					case 2: // POSY low
						plus_bank[p-0x4000]=b;
						break;
					case 4: case 5: case 6: case 7:
						plus_bank[p-0x4000]=b&15;
						break;
				}
			break;
		case 0x64:
			// colour palette
			if (p<0x6440)
			{
				if (p&1)
					b&=15;
				plus_bank[p-0x4000]=b;
				p&=64-2; // select ink
				video_clut_index=p>>1; // keep ASIC and Gate Array from clashing
				video_clut_value=video_table[video_type][32+plus_palette[p+1]]+video_table[video_type][48+(plus_palette[p]>>4)]+video_table[video_type][64+(plus_palette[p]&15)];
				if (!(video_pos_x&8)) video_clut[video_clut_index]=video_clut_value; // fast update
			}
			break;
		case 0x68:
			// scanline events
			if (p<0x6806)
			{
				if (p==0x6800) // plus_pri, PROGRAMMABLE RASTER INTERRUPT
				{
					if (b!=plus_pri)
					{
						if (crtc_line==b&&crtc_status&CRTC_STATUS_HSYNC) // "FIRE N FORGET 2" needs this!
							z80_irq|=128; // SET!
						else if (b) // "EERIE FOREST" needs this!
							z80_irq&=64+32+16;
					}
				}
				plus_bank[p-0x4000]=b;
			}
			break;
		case 0x6C:
			// DMA channel parameters
			if (p<0x6C10)
			{
				if (p==0x6C0F) // plus_dcsr, DMA CONTROL/STATUS REGISTER
				{
					z80_irq&=~(b&0x70);
					if (!(b&1))
						z80_irq&=~16;
					if (!(b&2))
						z80_irq&=~32;
					if (!(b&4))
						z80_irq&=~64;
					b=((plus_dcsr&128)+(b&7))|z80_irq;
				}
				plus_bank[p-0x4000]=b;
			}
			break;
	}
}

#ifdef CONSOLE_DEBUGGER
void z80_info(void) // prints a short hardware dump from the debugger
{
	char c,s[STRMAX],*t;
	t=s+sprintf(s,"\nGATE %02X:",gate_index);
	for (c=0;c<17;++c)
		t+=sprintf(t," %02X",gate_table[c]);
	t+=sprintf(t,"\nCRTC %02X:",crtc_index);
	for (c=0;c<16;++c)
		t+=sprintf(t," %02X",crtc_table[c]);
	t+=sprintf(t,"\nPSG. %02X:",psg_index);
	for (c=0;c<14;++c)
		t+=sprintf(t," %02X",psg_table[c]);
	printf("%s\nMCR $%02X / RAM $%02X / ROM $%02X\n",s,gate_mcr,gate_ram,gate_rom);
}
#else
int z80_debug_hard_tab(char *t)
{
	return sprintf(t,"    ");
}
void z80_debug_hard(int q,int x,int y)
{
	int i; char s[16*20],*t;
	if (q&1&&plus_enabled)
	{
		t=s+sprintf(s,"PLUS: M:%02X D:%02X I:%02X",plus_gate_mcr,plus_dcsr,z80_irq);
		t+=sprintf(t,"    SSSL:%02X SCAN:%03X",plus_sssl,crtc_line);
		t+=sprintf(t,"    SSSS:%04X PRI:%02X",(WORD)mgetii(plus_ssss),plus_pri);
		t+=sprintf(t,"    SSCR:%02X IVR:%02X %c",plus_sscr,plus_ivr,plus_gate_enabled?'+':'@'+plus_gate_counter);
		//t+=sprintf(t,"DMAS:               ");
		for (i=0;i<3;++i)
			t+=sprintf(t," DMA%c %04X:%03X.%02X/%02X",
				48+i,
				mgetii(&plus_dmas[i*4+0]),
				plus_dma_regs[i][2],
				plus_dma_regs[i][3],
				plus_dmas[i*4+2]
			);
		for (i=0;i<8;++i)
			t+=sprintf(t," %03X,%03X:%1X %03X,%03X:%1X"
			,(mgetii(&plus_sprite_xyz[i*16+ 0]))&0xFFF
			,(mgetii(&plus_sprite_xyz[i*16+ 2]))&0xFFF
			,plus_sprite_xyz[i*16+ 4]&15
			,(mgetii(&plus_sprite_xyz[i*16+ 8]))&0xFFF
			,(mgetii(&plus_sprite_xyz[i*16+10]))&0xFFF
			,plus_sprite_xyz[i*16+12]&15
			);
	}
	else
	{
		t=s+sprintf(s,"GATE:               ""%02X: ",gate_index);
		for (i=0;i<8;++i)
			t+=sprintf(t,"%02X",gate_table[i]);
		t+=z80_debug_hard_tab(t);
		for (;i<16;++i)
			t+=sprintf(t,"%02X",gate_table[i]);
		t+=sprintf(t,"    %02X %cBPP %02X:%02X:%02X",gate_table[16],(crtc_lo_decoding&3)?(crtc_lo_decoding&1)?'2':'1':'4',0x80+gate_mcr,0xC0+gate_ram,gate_rom);
		t+=sprintf(t,"CRTC:               ""%02X: ",crtc_index);
		for (i=0;i<8;++i)
			t+=sprintf(t,"%02X",crtc_table[i]);
		t+=z80_debug_hard_tab(t);
		for (;i<16;++i)
			t+=sprintf(t,"%02X",crtc_table[i]);
		t+=sprintf(t,
		"    %02X:%02X:%02X:%02X %04X" // VCC, R52, HDC, HCC, VMA
		"    %02X%c%02X%c%02X%c%02X %04X" // VLC, VSC, VTAC, HSC, VDUR
		,crtc_count_r4,irq_timer,((video_pos_x-VIDEO_OFFSET_X)/16)&0xFF,crtc_count_r0,crtc_zz_char
		,crtc_count_r9,(crtc_status&CRTC_STATUS_VSYNC)?'*':'-',crtc_count_r3y,(crtc_status&CRTC_STATUS_V_T_A)?'*':'-'
		,crtc_count_r5,(crtc_status&CRTC_STATUS_HSYNC)?'*':'-',crtc_count_r3x,(WORD)((video_pos_y-VIDEO_OFFSET_Y)/2+3)
		);
		t+=sprintf(t,"PSG:                ""%02X: ",psg_index);
		for (i=0;i<8;++i)
			t+=sprintf(t,"%02X",psg_table[i]);
		t+=z80_debug_hard_tab(t);
		for (;i<16;++i)
			t+=sprintf(t,"%02X",psg_table[i]);
		t+=sprintf(t,"    PIO: %02X:%02X:%02X:%02X",pio_port_a,pio_port_b,pio_port_c,pio_control);
		t+=sprintf(t,"FDC:  %02X - %04X:%04X""    %c ",disc_parmtr[0],(WORD)disc_offset,(WORD)disc_length,48+disc_phase);
		for (i=0;i<7;++i)
			t+=sprintf(t,"%02X",disc_result[i]);
	}
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

const BYTE z80_delays[0x700]= // precalc'd coarse timings
{
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x000 -
	1,3,2,2,1,1,2,1,1,3,2,2,1,1,2,1, // base, 1/2
	3,3,2,2,1,1,2,1,3,3,2,2,1,1,2,1, // 0x10-0x1F
	2,3,5,2,1,1,2,1,2,3,5,2,1,1,2,1, // 0x20-0x2F
	2,3,4,2,3,3,3,1,2,3,4,2,1,1,2,1, // 0x30-0x3F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x40-0x4F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x50-0x5F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x60-0x6F
	2,2,2,2,2,2,1,2,1,1,1,1,1,1,2,1, // 0x70-0x7F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x80-0x8F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x90-0x9F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0xA0-0xAF
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0xB0-0xBF
	2,3,3,3,3,4,2,4,2,3,3,1,3,5,2,4, // 0xC0-0xCF
	2,3,3,2,3,4,2,4,2,1,3,3,3,1,2,4, // 0xD0-0xDF
	2,3,3,6,3,4,2,4,2,1,3,1,3,1,2,4, // 0xE0-0xEF
	2,3,3,1,3,4,2,4,2,2,3,1,3,1,2,4, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x100 -
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // base, 2/2
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x10-0x1F
	1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0, // 0x20-0x2F
	1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0, // 0x30-0x3F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x40-0x4F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x50-0x5F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x60-0x6F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x70-0x7F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x80-0x8F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x90-0x9F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xA0-0xAF
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xB0-0xBF
	2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0, // 0xC0-0xCF
	2,0,0,1,2,0,0,0,2,0,0,0,2,0,0,0, // 0xD0-0xDF
	2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0, // 0xE0-0xEF
	2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x200 -
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // EDxx, 1/2
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x10-0x1F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x20-0x2F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x30-0x3F
	3,2,3,5,1,3,1,2,3,2,3,5,1,3,1,2, // 0x40-0x4F
	3,2,3,5,1,3,1,2,3,2,3,5,1,3,1,2, // 0x50-0x5F
	3,2,3,5,1,3,1,4,3,2,3,5,1,3,1,4, // 0x60-0x6F
	3,2,3,5,1,3,1,1,3,2,3,5,1,3,1,1, // 0x70-0x7F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x80-0x8F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x90-0x9F
	4,3,4,3,1,1,1,1,4,3,4,3,1,1,1,1, // 0xA0-0xAF
	4,3,4,3,1,1,1,1,4,3,4,3,1,1,1,1, // 0xB0-0xBF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xC0-0xCF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xD0-0xDF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xE0-0xEF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x300 -
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // EDxx, 2/2
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x10-0x1F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x20-0x2F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x30-0x3F
	0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0, // 0x40-0x4F
	0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0, // 0x50-0x5F
	0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0, // 0x60-0x6F
	0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0, // 0x70-0x7F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x80-0x8F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x90-0x9F
	0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0, // 0xA0-0xAF
	1,2,1,1,0,0,0,0,1,2,1,1,0,0,0,0, // 0xB0-0xBF
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xC0-0xCF
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xD0-0xDF
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xE0-0xEF
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x400 -
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // CB prefix
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0x10-0x1F
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0x20-0x2F
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0x30-0x3F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x40-0x4F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x50-0x5F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x60-0x6F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x70-0x7F
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0x80-0x8F
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0x90-0x9F
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0xA0-0xAF
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0xB0-0xBF
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0xC0-0xCF
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0xD0-0xDF
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0xE0-0xEF
	1,1,1,1,1,1,3,1,1,1,1,1,1,1,3,1, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x500 -
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // XY+CB set
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0x10-0x1F
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0x20-0x2F
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0x30-0x3F
	4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, // 0x40-0x4F
	4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, // 0x50-0x5F
	4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, // 0x60-0x6F
	4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4, // 0x70-0x7F
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0x80-0x8F
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0x90-0x9F
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0xA0-0xAF
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0xB0-0xBF
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0xC0-0xCF
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0xD0-0xDF
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0xE0-0xEF
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x600 -
	0,0,0,2,0,0,0,0,0,3,0,2,0,0,0,0, // XY prefix
	0,0,0,2,0,0,0,0,0,3,0,2,0,0,0,0, // 0x10-0x1F
	0,3,5,2,1,1,2,0,0,3,5,2,1,1,2,0, // 0x20-0x2F
	0,0,0,2,5,5,5,0,0,3,0,2,0,0,0,0, // 0x30-0x3F
	0,0,0,0,1,1,4,0,0,0,0,0,1,1,4,0, // 0x40-0x4F
	0,0,0,0,1,1,4,0,0,0,0,0,1,1,4,0, // 0x50-0x5F
	1,1,1,1,1,1,4,1,1,1,1,1,1,1,4,1, // 0x60-0x6F
	4,4,4,4,4,4,0,4,0,0,0,0,1,1,4,0, // 0x70-0x7F
	0,0,0,0,1,1,4,0,0,0,0,0,1,1,4,0, // 0x80-0x8F
	0,0,0,0,1,1,4,0,0,0,0,0,1,1,4,0, // 0x90-0x9F
	0,0,0,0,1,1,4,0,0,0,0,0,1,1,4,0, // 0xA0-0xAF
	0,0,0,0,1,1,4,0,0,0,0,0,1,1,4,0, // 0xB0-0xBF
	2,0,0,0,0,0,0,0,2,0,0,1,0,0,0,0, // 0xC0-0xCF
	2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0, // 0xD0-0xDF
	2,3,0,6,0,4,0,0,2,1,0,0,0,0,0,0, // 0xE0-0xEF
	2,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0, // 0xF0-0xFF
};

int z80_ack_delay=0; // making it local adds overhead :-(
// input/output
#define Z80_SYNC_IO ( _t_-=z80_t, z80_sync(z80_t) )
#define Z80_PRAE_RECV(w) Z80_SYNC_IO
#define Z80_RECV z80_recv
#define Z80_POST_RECV(w)
#define Z80_PRAE_SEND(w) Z80_SYNC_IO
#define Z80_SEND z80_send
#define Z80_POST_SEND(w)
// fine timings
#define Z80_AUXILIARY
#define Z80_MREQ(t,w)
#define Z80_MREQ_1X(t,w)
#define Z80_MREQ_NEXT(t)
#define Z80_MREQ_1X_NEXT(t)
#define Z80_IORQ(t,w)
#define Z80_IORQ_1X(t,w)
#define Z80_IORQ_NEXT(t)
#define Z80_IORQ_1X_NEXT(t)
#define Z80_PEEK PEEK
#define Z80_PEEK1 Z80_PEEK
#define Z80_PEEK2 Z80_PEEK
#define Z80_PEEKX PEEK
#define Z80_POKE(x,a) do{ int z80_aux=x>>14; if (mmu_bit[z80_aux]) Z80_SYNC_IO,z80_trap(x,a),z80_t=0; else mmu_ram[z80_aux][x]=a; }while(0) // a single write
#define Z80_POKE0 Z80_POKE // non-unique twin write
#define Z80_POKE1(x,a) do{ --z80_t; int z80_aux=x>>14; if (mmu_bit[z80_aux]) Z80_SYNC_IO,z80_trap(x,a),z80_t=0; else mmu_ram[z80_aux][x]=a; }while(0) // 1st twin write
#define Z80_POKE2(x,a) do{ ++z80_t; int z80_aux=x>>14; if (mmu_bit[z80_aux]) Z80_SYNC_IO,z80_trap(x,a),z80_t=0; else mmu_ram[z80_aux][x]=a; }while(0) // 2nd twin write
#define Z80_WAIT(t)
#define Z80_BEWARE
#define Z80_REWIND
// coarse timings
#define Z80_STRIDE(o) z80_t+=z80_delays[o]
#define Z80_STRIDE_0 z80_ack_delay=1 // extra int. timing after "slow" opcode
#define Z80_STRIDE_1 z80_ack_delay=0 // extra int. timing after "fast" opcode
#define Z80_STRIDE_X(o) z80_t+=z80_delays[o]+z80_ack_delay
#define Z80_STRIDE_IO(o) z80_t=z80_delays[o]
#define Z80_STRIDE_HALT 1

#define Z80_XCF_BUG 1 // replicate the SCF/CCF quirk
#define Z80_DEBUG_LEN 16 // height of disassemblies, dumps and searches
#define Z80_DEBUG_MMU 1 // allow ROM/RAM toggling, it's useful on CPC!
#define Z80_DEBUG_EXT 1 // allow EXTRA hardware debugging info pages
#define z80_out0() 0 //  whether OUT (C) sends 0 (NMOS) or 255 (CMOS)

#include "cpcec-z8.h"

// EMULATION CONTROL ================================================ //

char txt_error[]="Error!";
char txt_error_any_load[]="Cannot open file!";
char txt_error_bios[]="Cannot load firmware!";

// emulation setup and reset operations ----------------------------- //

void all_setup(void) // setup everything!
{
	plus_setup();
	gate_setup();
	crtc_setup();
	tape_setup();
	disc_setup();
	pio_setup();
	psg_setup();
	z80_setup();
}
BYTE snap_done; // avoid accidents with ^F2, see below
void all_reset(void) // reset everything!
{
	MEMZERO(autorun_kbd);
	plus_reset();
	gate_reset();
	crtc_reset();
	tape_reset();
	disc_reset();
	pio_reset();
	psg_reset();
	z80_reset();
	z80_debug_reset();
	snap_done=0; // avoid accidents!
	z80_sp.w=0xC000; // implicit in "No Exit" PLUS!
	z80_imd=1; // implicit in "Pro Tennis Tour" PLUS!
	MEMFULL(z80_tape_fastindices);
}

// firmware/cartridge ROM file handling operations ------------------ //

BYTE biostype_id=-1; // keeper of the latest loaded BIOS type
char bios_system[][13]={"cpc464.rom","cpc664.rom","cpc6128.rom","cpcplus.rom"};
char bios_path[STRMAX]="";

int bios_load(char *s) // load a cartridge file or a firmware ROM file. 0 OK, !0 ERROR
{
	if (globbing("*.ini",s,1)&&(mem_xtr||(mem_xtr=malloc(257<<14)))) // profile?
	{
		char t[STRMAX],*tt,u[STRMAX],*uu;
		strcpy(u,s); if (uu=strrchr(u,PATHCHAR)) ++uu; else uu=u; // make ROM files relative to INI path
		MEMZERO(mmu_xtr);
		FILE *f,*ff;
		if (f=puff_fopen(s,"r"))
		{
			while (fgets(t,STRMAX,f))
			{
				tt=t; while (*tt>=' ') ++tt;
					*tt=0;
				if ((tt=strchr(t,'='))&&*++tt)
				{
					strcpy(uu,tt);
					if (globbing("LOWEST*",t,1))
					{
						if (ff=puff_fopen(u,"rb"))
							fread1(&mem_xtr[0],0x4000,ff),puff_fclose(ff),mmu_xtr[0]=1;
					}
					else if (globbing("HIGH*",t,1))
					{
						int i=strtol(&t[4],NULL,16);
						if (i>=0&&i<length(mmu_xtr)-1)
							if (ff=puff_fopen(u,"rb"))
								fread1(&mem_xtr[0x4000+(i<<14)],0x4000,ff),puff_fclose(ff),mmu_xtr[i+1]=1;
					}
				}
			}
			puff_fclose(f);
		}
	}
	else // binary file!
	{
		FILE *f=puff_fopen(s,"rb");
		if (!f)
			return 1;
		int i=0,j=0x63623030,k,l;
		if (fgetmmmm(f)==0x52494646&&fgetiiii(f)>0&&fgetmmmm(f)==0x414D5321) // cartridge file?
			while ((k=fgetmmmm(f))&&(l=fgetiiii(f))>=0&&!feof(f))
			{
				//logprintf(" [%08X:%08X]",k,l);
				if (k==j&&i+l<=(1<<19))
				{
					i+=fread1(&mem_rom[i],l,f);
					if (((++j)&15)==10)
						j+=256-10; // "cb09" is followed by "cb10" (decimal format)
				}
				else if (l)
					fseek(f,l,SEEK_CUR);
			}
		else
		{
			fseek(f,0,SEEK_END); i=ftell(f);
			if (i>0x4000)
				fseek(f,i-0x4000+0x0601,SEEK_SET),j=fgetmmmm(f);
			if (i<=0x4000||i&0x3FFF||i>(1<<19)||j==0xD3FE37C9) // 32k,48k,64k...512k + !Spectrum TAPE LOAD fingerprint
				return puff_fclose(f),1; // reject!
			fseek(f,0,SEEK_SET);
			fread1(mem_rom,sizeof(mem_rom),f);
		}
		puff_fclose(f);
		MEMZERO(mmu_xtr);
		if (i==(1<<15)&&mgetiiii(&mem_rom[0])==0xED7F8901&&mem_rom[0x4002]<3) // firmware fingerprint
		{
			type_id=mem_rom[0x4002]; // firmware revision
			if (crtc_type==3) crtc_type=0; // adjust hardware
			plus_reset(); // PLUS ASIC can be safely reset
			logprintf("ROM firmware v%i.\n",1+type_id);
		}
		else // Amstrad PLUS cartridge
		{
			type_id=crtc_type=3; // enforce hardware
			j=1<<14;
			while (j<i) j<<=1;
			if (j>i) memset(&mem_rom[i],-1,j-i); // pad memory
			while (j<(1<<19)) // mirror low ROM up
			{
				memcpy(&mem_rom[j],mem_rom,j);
				j<<=1;
			}
			logprintf("PLUS cartridge %iK.\n",i>>10);
		}
	}
	#if 0//1 // fast tape hack!!
	if (mgetiiii(&mem_rom[0x2BF9])==0x0B068201) //
		mem_rom[0x2BF9+2]/=2;//mputii(&mem_rom[0x2BF9+1],1); // 664+6128+PLUS HACK: reduced initial tape reading delay!
	else if (mgetiiii(&mem_rom[0x2A89])==0x0B068201)
		mem_rom[0x2A89+2]/=2;//mputii(&mem_rom[0x2A89+1],1); // ditto, for CPC464
	#endif
	if (!memcmp(&mem_rom[0xCE06],"\xCD\x1B\xBB\x30\xF0\xFE\x81\x28\x0C\xEE\x82",11))
		mem_rom[0xCE07]=0x09,mem_rom[0xCE0C]=0x31,mem_rom[0xCE10]=0x32; // PLUS HACK: boot menu accepts '1' and '2'!
	if (bios_path!=s&&(char*)session_substr!=s)
		strcpy(bios_path,s);
	return biostype_id=type_id,mmu_update(),0;
}
int bios_path_load(char *s) // ditto, but from the base path
{
	return bios_load(strcat(strcpy(session_substr,session_path),s));
}
int bios_reload(void) // ditto, but from the current type_id
{
	return type_id>3?1:biostype_id==type_id?0:bios_load(strcat(strcpy(session_substr,session_path),bios_system[type_id]));
}

int bdos_path_load(char *s) // load AMSDOS ROM. `s` path; 0 OK, !0 ERROR
{
	FILE *f=fopen(strcat(strcpy(session_substr,session_path),s),"rb");
	if (!f)
		return 1;
	int i=fread1(bdos_rom,1<<14,f);
	i+=fread1(bdos_rom,1<<14,f);
	fclose(f);
	return i!=(1<<14)||mgetiiii(&bdos_rom[0x3C])!=0xC3C666C3; // AMSDOS fingerprint
}

// snapshot file handling operations -------------------------------- //

char snap_path[STRMAX]="";
char snap_magic8[]="MV - SNA";

#define SNAP_SAVE_Z80W(x,r) header[x]=r.b.l,header[x+1]=r.b.h
int snap_save(char *s) // save a snapshot. `s` path, NULL to resave; 0 OK, !0 ERROR
{
	FILE *f=fopen(s,"wb");
	if (!f)
		return 1;
	BYTE header[256];
	MEMZERO(header);
	strcpy(header,snap_magic8);
	header[0x10]=3; // V3
	// V1 data
	int i;
	SNAP_SAVE_Z80W(0x11,z80_af);
	SNAP_SAVE_Z80W(0x13,z80_bc);
	SNAP_SAVE_Z80W(0x15,z80_de);
	SNAP_SAVE_Z80W(0x17,z80_hl);
	SNAP_SAVE_Z80W(0x19,z80_ir);
	SNAP_SAVE_Z80W(0x1B,z80_iff);
	SNAP_SAVE_Z80W(0x1D,z80_ix);
	SNAP_SAVE_Z80W(0x1F,z80_iy);
	SNAP_SAVE_Z80W(0x21,z80_sp);
	SNAP_SAVE_Z80W(0x23,z80_pc);
	header[0x25]=z80_imd&3;
	SNAP_SAVE_Z80W(0x26,z80_af2);
	SNAP_SAVE_Z80W(0x28,z80_bc2);
	SNAP_SAVE_Z80W(0x2A,z80_de2);
	SNAP_SAVE_Z80W(0x2C,z80_hl2);
	header[0x2E]=gate_index;
	for (i=0;i<17;++i)
		header[0x2F+i]=gate_table[i]&31;
	header[0x40]=(gate_mcr&15)|128;
	header[0x41]=gate_ram&63;
	header[0x42]=crtc_index;
	MEMSAVE(&header[0x43],crtc_table);
	header[0x55]=gate_rom;
	header[0x56]=pio_port_a;
	header[0x57]=pio_port_b;
	header[0x58]=pio_port_c;
	header[0x59]=pio_control|128;
	header[0x5A]=psg_index; // mistake in http://cpctech.cpc-live.com/docs/snapshot.html : INDEX can be invalid, it's the emulator's duty to behave accordingly
	MEMSAVE(&header[0x5B],psg_table);
	header[0x6B]=gate_ram_dirty; header[0x6C]=gate_ram_dirty>>8; // in V3, this field is zero if MEM0..MEM8 chunks are used. Avoid them to stay compatible.
	// V2 data
	header[0x6D]=type_id<3?type_id:gate_ram_depth?4:5;//bios_rom[0x4002]
	// V3 data
	header[0x9C]=!!disc_motor;
	MEMSAVE(&header[0x9D],disc_track);
	//header[0xA1]=printer_data;
	header[0xA2]=video_pos_y>>1; header[0xA3]=video_pos_y>>9; // quite unreliable, emulators often ignore it!
	header[0xA4]=crtc_type;
	header[0xA9]=crtc_count_r0;
	header[0xAB]=crtc_count_r4;
	header[0xAC]=crtc_count_r9;
	header[0xAD]=crtc_count_r5;
	header[0xAE]=crtc_count_r3x;
	header[0xAF]=crtc_count_r3y;
	header[0xB0]=crtc_status;
	header[0xB1]=crtc_status>>8;
	header[0xB2]=irq_delay;
	header[0xB3]=irq_timer;
	header[0xB4]=!!z80_irq;
	header[0xB5]=z80_irq; // compare plus_dcsr with ICSR
	header[0xB6]=!(header[0xB7]=plus_enabled);
	strcpy(&header[0xE0],caption_version);
	fwrite1(header,256,f);
	fwrite1(mem_ram,gate_ram_dirty<<10,f);

	if (plus_enabled) // PLUS ASIC?
	{
		fputmmmm(0x4350432B,f); // "CPC+"
		fputiiii(0x08F8,f);
		BYTE *s=plus_sprite_bmp,*t=session_scratch; // 0x4000-0x4FFF nibbles => 0x000-0x7FF byte
		for (i=0;i<16*16*8;++i)
			*t=*s++<<4,*t+++=*s++&15;
		MEMNCPY(&session_scratch[0x800],plus_sprite_xyz,128); // 0x6000-0x607F => 0x800-0x87F
		MEMNCPY(&session_scratch[0x880],plus_palette, 64); // 0x6400-0x643F => 0x880-0x8BF
		memset(&session_scratch[0x8C0],0,0x20);
		MEMNCPY(&session_scratch[0x8C0],&plus_bank[0x2800], 6); // 0x6800-0x6805 => 0x8C0-0x8C5
		session_scratch[0x8F5]=session_scratch[0x8C6]=plus_gate_mcr|160; // Winape uses 0x8C6!? ACE uses 0x8F5!?
		MEMNCPY(&session_scratch[0x8D0],&plus_bank[0x2C00],16); // 0x6C00-0x6C0F => 0x8D0-0x8DF
		for (i=0;i<3;++i)
		{
			mputii(&session_scratch[0x8E0+i*7+0],plus_dma_regs[i][0]);
			mputii(&session_scratch[0x8E0+i*7+2],plus_dma_regs[i][1]);
			mputii(&session_scratch[0x8E0+i*7+4],plus_dma_regs[i][2]);
			session_scratch[0x8E0+i*7+6]=plus_dma_regs[i][3];
		}
		session_scratch[0x8F6]=plus_gate_enabled;
		session_scratch[0x8F7]=plus_gate_counter;
		fwrite1(session_scratch,0x8F8,f);
	}

	if (snap_path!=s)
		strcpy(snap_path,s);
	return snap_done=!fclose(f),0;
}

void snap_load_plus(FILE *f,int i)
{
	BYTE *s=session_scratch,*t=plus_sprite_bmp;
	fread1(s,i,f);
	if (plus_enabled)
	{
		for (i=0;i<16*16*8;++i)
			*t++=*s>>4,*t++=15&*s++; // 0x000-0x7FF bytes => 0x4000-0x4FFF nibbles
		//MEMNCPY(plus_sprite_xyz,&session_scratch[0x800],128); // 0x800-0x87F => 0x6000-0x607F
		for (i=0;i<16*8;i+=8)
		{
			plus_sprite_xyz[i+0]=session_scratch[0x800+i+0];
			char b=session_scratch[0x800+i+1]&3; plus_sprite_xyz[i+1]=b==3?-1:b; // POSX ranges from -256 to 767
			plus_sprite_xyz[i+2]=session_scratch[0x800+i+2];
			plus_sprite_xyz[i+3]=session_scratch[0x800+i+3]&1?-1:0; // POSY ranges from -256 to 256
			plus_sprite_xyz[i+4]=session_scratch[0x800+i+4];
			plus_sprite_xyz[i+5]=session_scratch[0x800+i+5];
			plus_sprite_xyz[i+6]=session_scratch[0x800+i+6];
			plus_sprite_xyz[i+7]=session_scratch[0x800+i+7];
		}
		//MEMNCPY(plus_palette,&session_scratch[0x880],64); // 0x880-0x8BF => 0x6400-0x643F
		for (i=0;i<32*2;i+=2)
		{
			plus_palette[i+0]=session_scratch[0x880+i+0];
			plus_palette[i+1]=session_scratch[0x880+i+1]&15; // a nibble, not a byte
		}
		MEMNCPY(&plus_bank[0x2800],&session_scratch[0x8C0],6); // 0x8C0-0x8CF => 0x6800-0x680F
		MEMNCPY(&plus_bank[0x2C00],&session_scratch[0x8D0],16); // 0x8D0-0x8DF => 0x6C00-0x6C0F
		for (i=0;i<3;++i)
		{
			plus_dma_regs[i][0]=mgetii(&session_scratch[0x8E0+i*7+0]);
			plus_dma_regs[i][1]=mgetii(&session_scratch[0x8E0+i*7+2]);
			plus_dma_regs[i][2]=mgetii(&session_scratch[0x8E0+i*7+4]);
			plus_dma_regs[i][3]=session_scratch[0x8E0+i*7+6];
		}
		if  (!((plus_gate_mcr=session_scratch[0x8C6])>=0xA0&&plus_gate_mcr<0xC0)) // Winape uses 0x8C6!?
			plus_gate_mcr=session_scratch[0x8F5]; // ACE uses 0x8F5!?
		plus_gate_mcr&=31;
		plus_gate_enabled=!!session_scratch[0x8F6];
		plus_gate_counter=session_scratch[0x8F7];
	}
}
#define SNAP_LOAD_Z80W(x,r) r.b.l=header[x],r.b.h=header[x+1]
int snap_load(char *s) // load a snapshot. `s` path, NULL to reload; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb");
	if (!f)
		return 1;
	BYTE header[256];
	if ((fread1(header,256,f)!=256)||(memcmp(snap_magic8,header,8)))
		return puff_fclose(f),1;
	int dumpsize=mgetii(&header[0x6B]);
	if (dumpsize>576||dumpsize&15)
		return puff_fclose(f),1; // improper memory length!
	// V1 data
	int i;
	SNAP_LOAD_Z80W(0x11,z80_af);
	SNAP_LOAD_Z80W(0x13,z80_bc);
	SNAP_LOAD_Z80W(0x15,z80_de);
	SNAP_LOAD_Z80W(0x17,z80_hl);
	SNAP_LOAD_Z80W(0x19,z80_ir);
	SNAP_LOAD_Z80W(0x1B,z80_iff);
	z80_iff.w&=0x0101; // sanity!
	SNAP_LOAD_Z80W(0x1D,z80_ix);
	SNAP_LOAD_Z80W(0x1F,z80_iy);
	SNAP_LOAD_Z80W(0x21,z80_sp);
	SNAP_LOAD_Z80W(0x23,z80_pc);
	z80_imd=header[0x25]&3;
	SNAP_LOAD_Z80W(0x26,z80_af2);
	SNAP_LOAD_Z80W(0x28,z80_bc2);
	SNAP_LOAD_Z80W(0x2A,z80_de2);
	SNAP_LOAD_Z80W(0x2C,z80_hl2);
	z80_irq=z80_iff0=z80_halted=0; // avoid nasty surprises!
	for (i=0;i<17;++i)
		gate_table[i]=header[0x2F+i]&31;
	gate_table_select(header[0x2E]&31);
	gate_mcr=header[0x40]&15;
	gate_ram=header[0x41]&63;
	for (i=0;i<18;++i)
		crtc_table[i]=header[0x43+i]&crtc_valid[i];
	crtc_table_select(header[0x42]);
	gate_rom=header[0x55];
	pio_port_a=header[0x56];
	pio_port_b=header[0x57];
	pio_port_c=header[0x58];
	pio_control=header[0x59]|128;
	psg_table_select(header[0x5A]);
	MEMLOAD(psg_table,&header[0x5B]);
	if (header[0x10]>1) // V2?
	{
		if ((type_id=header[0x6D])>3) type_id=3; // merge all PLUS types together
		logprintf("SNAv2 ROM type v%i.\n",1+type_id);
		bios_reload();
	}
	if (header[0x10]>2) // V3?
	{
		disc_motor=header[0x9C];
		MEMLOAD(disc_track,&header[0x9D]);
		//printer_data=header[0xA1];
		if ((i=(mgetii(&header[0xA2])*2))>=0&&i<VIDEO_VSYNC_HI) // quite unreliable, ignore it if invalid!
		{
			video_target+=(i-video_pos_y)*VIDEO_LENGTH_X;
			video_pos_y=i;
		}
		crtc_type=header[0xA4]&7; // logprintf("SNAv3 CRTC type %i.\n",header[0xA4]); // unreliable too, just print it!
		crtc_count_r0=header[0xA9];
		crtc_count_r4=header[0xAB];
		crtc_count_r9=header[0xAC];
		crtc_count_r5=header[0xAD];
		crtc_count_r3x=header[0xAE];
		crtc_count_r3y=header[0xAF];
		crtc_status=mgetii(&header[0xB0]);
		irq_delay=crtc_status&CRTC_STATUS_VSYNC?header[0xB2]:0; // WINAPE sometimes stored unwanted values here
		irq_timer=header[0xB3];
		//z80_irq=!!header[0xB4];
		z80_irq=header[0xB5]&0xF0;
	}
	// handle memory dump (compressed or not) and PLUS ASIC from V3+, if required; careful, [6C][6B] isn't reliable!
	int j=dumpsize?fread1(mem_ram,dumpsize<<10,f):0,q=0;
	i=0x4D454D30+(j>>16); // "MEM0", "MEM1"...
	for (;;)
	{
		if ((i&15)>10)
			i+=7; // "MEM9" -> "MEMA"
		int k=fgetmmmm(f); // chunk
		int l=fgetiiii(f); // bytes
		if (feof(f))
			break;
		if (k==i)
		{
			++i;
			if (l<0x10000) // compressed, expand!
			{
				k=0;
				while (k<l&&j<sizeof(mem_ram))
				{
					int m,n;
					if ((++k,m=fgetc(f))!=0xE5||!(++k,n=fgetc(f)))
						mem_ram[j++]=m;
					else
					{
						if (j+n>sizeof(mem_ram))
							break; // overrun!
						++k,m=fgetc(f);
						while (n--)
							mem_ram[j++]=m;
					}
				}
				if (k!=l||(WORD)j) // damaged compression!
					return puff_fclose(f),1;
			}
			else if ((l==0x10000)&&(l+j<=sizeof(mem_ram))) // uncompressed!
				j+=fread1(&mem_ram[j],l,f);
			else
				return puff_fclose(f),1; // improperly sized snapshot!
		}
		else if (k==0x4350432B) // PLUS ASIC "CPC+"
			q=1,snap_load_plus(f,l);
		else
			fseek(f,l,SEEK_CUR); // skip unknown block
	}
	if (!q)
	{
		plus_reset(); // reset PLUS hardware if the snapshot is OLD style!
		for (i=0;i<17;++i) // avoid accidents in old snapshots recorded as PLUS but otherwise sticking to old hardware
		{
			int j=video_asic_table[gate_table[i]];
			plus_palette[i*2+0]=j; plus_palette[i*2+1]=j>>8;
		}
	}
	// we adjust the REAL RAM SIZE if the snapshot is bigger than the current setup!
	dumpsize=j>>10;
	logprintf("SNAv1 RAM size %iK.\n",dumpsize);
	if (dumpsize>(gate_ram_dirty=64))
	{
		q=0; // look for lowest RAM limit that fits
		while ((gate_ram_dirty=gate_ram_kbyte[++q])<dumpsize&&q<length(gate_ram_kbyte))
			;
		if (gate_ram_depth<q)
			gate_ram_depth=q; // expand RAM limit if required
	}
	video_clut_update(); // sync both Old and Plus palettes
	crtc_r3x_update();
	crtc_r4x_update();
	crtc_r6x_update();
	crtc_line_set();
	crtc_prior_r2=crtc_limit_r2=crtc_table[2]; // delay Gigascreen
	disc_track_update();
	psg_all_update();
	mmu_update();
	z80_debug_reset();
	autorun_mode=0,disc_disabled&=~2; // autorun is now irrelevant
	if (snap_path!=s)
		strcpy(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

// "autorun" file and logic operations ------------------------------ //

int any_load_catalog(int t,int r) // load track `t` and sectors from `r` to `r+3`; returns loaded length, if any
{
	if (disc_track_load(disc_parmtr[3]=0,disc_parmtr[2]=t))
		return 0; // track is missing, give up
	int s=r+4,l=disc_flip[0]&&(disc_index_table[0][0x31]!=1)?4:0;
	disc_parmtr[5]=2;
	for (t=0;r<s;++r)
		if (disc_parmtr[4]=r,!disc_sector_find(l)) // load sectors if available
			disc_sector_seek(l,disc_sector_last),t+=fread1(&disc_buffer[t],0x200,disc[0]);
	return t;
}

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
				{
					// disc autorun is more complex than tape because we must find out the right file
					int i,j,k;
					if ((j=any_load_catalog(0,0xC1))!=0x800) // VENDOR format instead of DATA?
						if ((j=any_load_catalog(2,0x41))!=0x800) // VENDOR format instead of DATA?
							j=any_load_catalog(1,0x01); // IBM format instead of VENDOR or DATA?
					BYTE bestfile[STRMAX];
					int bestscore=bestfile[0]=0;
					logprintf("AUTORUN: ");
					if (j==0x800) // we got a seemingly valid catalogue
						for (i=0;i<0x800;i+=32) // scan the catalogue
							// check whether the entry is valid at all: it belongs to USER 0, it's got a valid entry length, WORD is ZERO and SIZE is NONZERO and VALID
							if (!disc_buffer[i]&&!disc_buffer[i+12]&&!disc_buffer[i+14]&&disc_buffer[i+15]>0&&disc_buffer[i+15]<=128&&disc_buffer[i+16])
							{
								int h=disc_buffer[i+10]&128; // HIDDEN flag!
								for (k=1,j=1;j<12;++j)
									k&=(disc_buffer[i+j]&=127)>=32; // remove bit 7 (used to tag the file as READ ONLY, HIDDEN, etc)
								if (k) // all chars are valid; measure how good it is as a candidate
								{
										k=33-disc_buffer[i+15]/4; // the shortest files are often the most likely to be loaders, and BASIC the most likely, while BINARY goes next
										if (!h)
										k+=48; // visible files are better candidates
									if (!memcmp(&disc_buffer[i+1],"DISC    ",8)||!memcmp(&disc_buffer[i+1],"DISK    ",8))
										k+=128; // RUN"DISC and RUN"DISK are standard!!
									else if (disc_buffer[i+1]=='-')
										k+=48; // very popular shortcut in French prods!
									if (!memcmp(&disc_buffer[i+9],"   ",3))
										k+=32; // together with BAS, a typical launch file
									else if (!memcmp(&disc_buffer[i+9],"BAS",3))
										k+=32; // ditto
									else if (!memcmp(&disc_buffer[i+9],"BIN",3))
											k+=16;
									logprintf("`%s`:%i ",&disc_buffer[i+1],k);
									if (bestscore<k)
									{
										bestscore=k;
										MEMNCPY(bestfile,&disc_buffer[i+1],8);
										bestfile[8]='.';
										strcpy(&bestfile[9],&disc_buffer[i+9]);
									}
								}
							}
					if (bestscore) // load and run file
						sprintf(autorun_line,"RUN\"%s",bestfile);
					else // no known file, run boot sector
						strcpy(autorun_line,"|CPM");
					logprintf("%s\n",autorun_line);
					disc_disabled=0,tape_close(); // open disc? enable disc, close tapes!
				}
			}
			else if (q)
				disc_disabled|=2,disc_close(0),disc_close(1); // open tape? close discs!
			if (q) // autorun for tape and disc
			{
				biostype_id=biostype_id>2?-1:biostype_id,all_reset(),bios_reload(); // set PLUS BIOS if required
				autorun_mode=disc_disabled?1:2,autorun_t=(type_id<3?0:-50); // -50 to handle the PLUS menu
			}
		}
		else // load bios? reset!
			all_reset(),autorun_mode=0,disc_disabled&=~2;
	}
	if (autorun_path!=s&&q)
		strcpy(autorun_path,s);
	return 0;
}

// auxiliary user interface operations ------------------------------ //

BYTE onscreen_flag=1;

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
	//"0x4800 Play tape\tCtrl+Shift+F8\n"
	"=\n"
	"0x3F00 E_xit\n"
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
	"0x8501 CRTC type 0\n"
	"0x8502 CRTC type 1\n"
	"0x8503 CRTC type 2\n"
	"0x8504 CRTC type 3\n"
	"0x8505 CRTC type 4\n"
	//"=\n"
	"0x8509 Short V-Hold\n"
	"0x850A Middle V-Hold\n"
	"0x850B Long V-Hold\n"
	"=\n"
	"0x8511 64k RAM\n"
	"0x8512 128k RAM\n"
	"0x8513 192k RAM\n"
	"0x8514 320k RAM\n"
	"0x8515 576k RAM\n"
	"=\n"
	"0x8510 Disc controller\n"
	"Video\n"
	"0x8901 Onscreen status\tShift+F9\n"
	"0x8904 Filtering\n"
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
	"0x8400 Sound playback\tF4\n"
	#if AUDIO_STEREO
	//"0xC400 Enable stereo\tShift+F4\n"
	"0xC401 0% stereo\n"
	"0xC403 50% stereo\n"
	"0xC402 100% stereo\n"
	"=\n"
	#endif
	"0x8401 No filtering\n"
	"0x8402 Light filtering\n"
	"0x8403 Middle filtering\n"
	"0x8404 Heavy filtering\n"
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
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	session_menucheck(0x0400,session_key2joy);
	session_menuradio(0x0601+z80_turbo,0x0601,0x0604);
	session_menuradio(0x8501+crtc_type,0x8501,0x8505);
	session_menuradio(0x8509+(crtc_hold<0?0:!crtc_hold?1:2),0x8509,0x850B);
	session_menucheck(0x8510,!(disc_disabled&1));
	session_menuradio(0x8511+gate_ram_depth,0x8511,0x8515);
	session_menucheck(0x8901,onscreen_flag);
	session_menuradio(0x8B01+video_type,0x8B01,0x8B05);
	session_menuradio(0x0B01+video_scanline,0x0B01,0x0B04);
	session_menucheck(0x8902,video_filter&VIDEO_FILTER_Y_MASK);
	session_menucheck(0x8903,video_filter&VIDEO_FILTER_X_MASK);
	session_menucheck(0x8904,video_filter&VIDEO_FILTER_SMUDGE);
	#if AUDIO_STEREO
	//session_menucheck(0xC400,audio_channels);
	audio_channels=audio_channels<1?0:audio_channels>1?2:audio_channels;
	session_menuradio(0xC401+audio_channels,0xC401,0xC403);
	for (int i=0;i<length(psg_stereo);++i)
		psg_stereo[i][0]=256+psg_stereos[audio_channels][i],psg_stereo[i][1]=256-psg_stereos[audio_channels][i];
	#endif
	z80_multi=1+z80_turbo; // setup overclocking
	sprintf(session_info,"%i:%iK CRTC%c %gMHz"//" | disc %s | tape %s | %s"
		,gate_ram_dirty,gate_ram_kbyte[gate_ram_depth],48+crtc_type,4.0*z80_multi);
}
int session_user(int k) // handle the user's commands; 0 OK, !0 ERROR
{
	char *s;
	switch (k)
	{
		case 0x8100: // F1: HELP..
			session_message(
				"F1\tHelp..\t" MESSAGEBOX_WIDETAB
				"^F1\tAbout..\t" // "\t"
				"\n"
				"F2\tSave snapshot.." MESSAGEBOX_WIDETAB
				"^F2\tSave last snapshot" // "\t"
				"\n"
				"F3\tLoad any file.." MESSAGEBOX_WIDETAB
				"^F3\tLoad last snapshot" // "\t"
				"\n"
				"F4\tToggle sound" MESSAGEBOX_WIDETAB
				"^F4\tToggle joystick" // "\t"
				"\n"
				"F5\tLoad firmware.." MESSAGEBOX_WIDETAB
				"^F5\tReset emulation" // "\t"
				"\n"
				"F6\tToggle realtime" MESSAGEBOX_WIDETAB
				"^F6\tToggle turbo Z80" // "\t"
				"\n"
				"F7\tInsert disc into A:..\t"
				"^F7\tEject disc from A:" // "\t"
				"\n"
				"\t(shift: ..into B:)\t"
				"\t(shift: ..from B:)" // "\t"
				"\n"
				"F8\tInsert tape.." MESSAGEBOX_WIDETAB
				"^F8\tRemove tape" // "\t"
				"\n"
				"\t(shift: record..)\t"
				"\t-\t\t" //"\t(shift: play/stop)" // "\t"
				"\n"
				"F9\tDebug\t" MESSAGEBOX_WIDETAB
				"^F9\tToggle fast tape" // "\t"
				"\n"
				"\t(shift: view status)\t"
				"\t(shift: ..fast load)" // "\t"
				"\n"
				"F11\tNext palette" MESSAGEBOX_WIDETAB
				"^F11\tNext scanlines" // "\t"
				"\n"
				"\t(shift: previous..)\t"
				"\t(shift: ..filter)" // "\t"
				"\n"
				"F12\tSave screenshot" MESSAGEBOX_WIDETAB
				"^F12\tRecord wavefile" // "\t"
				"\n"
				"\t-\t" MESSAGEBOX_WIDETAB
				"\t(shift: ..YM file)" // "\t"
				"\n"
				"\n"
				"Num.+\tRaise frameskip" MESSAGEBOX_WIDETAB
				"Num.*\tFull frameskip" // "\t"
				"\n"
				"Num.-\tLower frameskip" MESSAGEBOX_WIDETAB
				"Num./\tNo frameskip" // "\t"
				"\n"
				"Pause\tPause/continue" MESSAGEBOX_WIDETAB
				"*Return\tMaximize/restore" "\t"
				"\n"
			,"Help");
			break;
		case 0x0100: // ^F1: ABOUT..
			session_aboutme(
				"Amstrad CPC emulator written by Cesar Nicolas-Gonzalez\n"
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
			if (puff_session_getfile(session_shift?snap_path:autorun_path,session_shift?snap_pattern:"*.sna;*.rom;*.crt;*.cpr;*.ini;*.dsk;*.cdt;*.tzx;*.csw;*.wav",session_shift?"Load snapshot":"Load file"))
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
			{
				#if AUDIO_STEREO
				if (session_shift) // TOGGLE STEREO
					audio_channels=!audio_channels;
				else
				#endif
					audio_disabled^=1;
			}
			break;
		case 0x8401:
		case 0x8402:
		case 0x8403:
		case 0x8404:
			#if AUDIO_STEREO
			if (session_shift) // TOGGLE STEREO
				audio_channels=(k&15)-1;
			else
			#endif
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
			crtc_type=(k&7)-1; crtc_r3x_update(); crtc_r6x_update(); // 0,1,2,3
			break;
		case 0x8509:
		case 0x850A:
		case 0x850B:
			crtc_hold=(k&7)-2; crtc_r3x_update(); crtc_r6x_update(); // -1,0,+1
			break;
		case 0x8510: // DISC CONTROLLER
			disc_disabled^=1;
			break;
		case 0x8511: // 64K RAM
			gate_ram_depth=0;
			break;
		case 0x8512: // 128K RAM
			gate_ram_depth=1;
			break;
		case 0x8513: // 192K RAM
			gate_ram_depth=2;
			break;
		case 0x8514: // 320K RAM
			gate_ram_depth=3;
			break;
		case 0x8515: // 576K RAM
			gate_ram_depth=4;
			break;
		case 0x8500: // F5: LOAD FIRMWARE..
			if (s=puff_session_getfile(bios_path,"*.rom;*.crt;*.cpr;*.ini","Load firmware"))
			{
				if (bios_load(s)) // error? warn and undo!
					session_message(txt_error_bios,txt_error),bios_reload(); // reload valid firmware, if required
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
			if (!disc_disabled)
				if (s=puff_session_newfile(disc_path,"*.dsk",session_shift?"Create disc in B:":"Create disc in A:"))
				{
					if (disc_create(s))
						session_message("Cannot create disc!",txt_error);
					else
						disc_open(s,!!session_shift,1);
				}
			break;
		case 0x8700: // F7: INSERT DISC..
			if (!disc_disabled)
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
			else if (s=puff_session_getfile(tape_path,"*.cdt;*.tzx;*.csw;*.wav","Insert tape"))
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
			;
			;
			;
			;
			;
			;
			;
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
		case 0x0C00: // ^F12: RECORD WAVEFILE OR YM FILE
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
	else if (!strcasecmp(session_parmtr,"crtc")) { if ((i=*s&15)<5) crtc_type=i; }
	else if (!strcasecmp(session_parmtr,"bank")) { if ((i=*s&15)<5) gate_ram_depth=i; }
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
	fprintf(f,"type %i\ncrtc %i\nbank %i\nfile %s\nsnap %s\ntape %s\ndisc %s\ncard %s\ninfo %i\npalette %i\nautorewind %i\ndebug %i\n"
		,type_id,crtc_type,gate_ram_depth,autorun_path,snap_path,tape_path,disc_path,bios_path,onscreen_flag,video_type,tape_rewind,z80_debug_configwrite());
}

#if defined(DEBUG) || defined(SDL_MAIN_HANDLED)
void printferror(char *s) { printf("Error: %s\n",s); }
#define printfusage(s) printf(MY_CAPTION " " MY_VERSION " " MY_LICENSE "\n" s)
#else
void printferror(char *s) { sprintf(session_tmpstr,"Error: %s\n",s); session_message(session_tmpstr,txt_error); }
#define printfusage(s) session_message(s,caption_version)
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
	BYTE want_fullscreen=0; i=0; while (++i<argc)
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
						video_type=(BYTE)(argv[i][j++]-'0');
						if (video_type<0||video_type>4)
							i=argc; // help!
						break;
					case 'd':
						session_signal=SESSION_SIGNAL_DEBUG;
						break;
					case 'g':
						crtc_type=(BYTE)(argv[i][j++]-'0');
						if (crtc_type<0||crtc_type>4)
							i=argc; // help!
						break;
					case 'j':
						session_key2joy=1;
						break;
					case 'J':
						session_stick=0;
						break;
					case 'k':
						gate_ram_depth=(BYTE)(argv[i][j++]-'0');
						if (gate_ram_depth<0||gate_ram_depth>4)
							i=argc; // help!
						break;
					case 'K':
						gate_ram_depth=0;
						break;
					case 'm':
						type_id=(BYTE)(argv[i][j++]-'0');
						if (type_id<0||type_id>length(bios_system))
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
					case 'T':
						audio_channels=0;
						break;
					case 'W':
						want_fullscreen=1;
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
			printfusage("usage: " MY_CAPTION
			" [option..] [file..]\n"
			"\t-cN\tscanline type (0..3)\n"
			"\t-CN\tcolour palette (0..4)\n"
			"\t-d\tdebug\n"
			"\t-gN\tset CRTC type (0..4)\n"
			"\t-j\tenable joystick keys\n"
			"\t-J\tdisable joystick\n"
			"\t-k0\t64k RAM\n"
			"\t-k1\t128k RAM\n"
			"\t-k2\t192k RAM\n"
			"\t-k3\t320k RAM\n"
			"\t-k4\t576k RAM\n"
			//"\t-K\tno extended RAM\n"
			"\t-m0\tload 464 firmware\n"
			"\t-m1\tload 664 firmware\n"
			"\t-m2\tload 6128 firmware\n"
			"\t-m3\tload PLUS firmware\n"
			"\t-O\tdisable onscreen status\n"
			"\t-rN\tset frameskip (0..9)\n"
			"\t-R\tdisable realtime\n"
			"\t-S\tdisable sound\n"
			"\t-T\tdisable stereo\n"
			"\t-W\tfullscreen mode\n"
			"\t-X\tdisable disc drives\n"
			"\t-Y\tdisable tape analysis\n"
			"\t-Z\tdisable tape speed-up\n"
			),1;
	if (bios_reload()||bdos_path_load("cpcados.rom"))
		return printferror(txt_error_bios),1;
	char *s="not enough memory"; if (s=session_create(session_menudata))
		return sprintf(session_scratch,"Cannot create session: %s!",s),printferror(session_scratch),1;
	session_kbdreset();
	session_kbdsetup(kbd_map_xlt,length(kbd_map_xlt)/2);
	video_target=&video_frame[video_pos_y*VIDEO_LENGTH_X+video_pos_y]; audio_target=audio_frame;
	audio_disabled=!session_audio;
	video_clut_update(); onscreen_inks(VIDEO1(0xAA0000),VIDEO1(0x55FF55));
	if (want_fullscreen) session_togglefullscreen();
	// it begins, "alea jacta est!"
	while (!session_listen())
	{
		#ifdef CONSOLE_DEBUGGER
		while (!(session_signal&~SESSION_SIGNAL_DEBUG))
		#else
		while (!session_signal)
		#endif
			z80_main(
				z80_multi*( // clump Z80 instructions together to gain speed...
				tape_fastskip?VIDEO_LENGTH_X/16:( // tape loading ignores most events and heavy clumping is feasible, although some sync is still needed
					plus_enabled?(video_pos_x<VIDEO_LENGTH_X/4)?1:(VIDEO_LENGTH_X-video_pos_x+16/2)/16 // PLUS IRQs happen in the leftmost quarter
					:irq_delay?3-irq_delay:(52-irq_timer) // the safest way to handle the fastest interrupt countdown possible (CRTC R0=0)
				)<<(session_fast&~1)) // ...without missing any IRQ and CRTC deadlines!
			);
		if (session_signal&SESSION_SIGNAL_FRAME) // end of frame?
		{
			if (!video_framecount&&onscreen_flag)
			{
				if (disc_disabled)
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
				int i,q=!tape_delay; //tape_enabled
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
					onscreen_bool(-4,-8,1,2,kbd_bit_tst(kbd_joy[0]));
					onscreen_bool(-4,-5,1,2,kbd_bit_tst(kbd_joy[1]));
					onscreen_bool(-6,-6,2,1,kbd_bit_tst(kbd_joy[2]));
					onscreen_bool(-3,-6,2,1,kbd_bit_tst(kbd_joy[3]));
					onscreen_bool(-6,-2,2,1,kbd_bit_tst(kbd_joy[4]));
					onscreen_bool(-3,-2,2,1,kbd_bit_tst(kbd_joy[5]));
				}
			}
			// update session and continue
			if (autorun_mode)
				autorun_next();
			if (!audio_disabled)
				audio_main(TICKS_PER_FRAME); // fill sound buffer to the brim!
			psg_writelog();
			crtc_giga=crtc_giga_count>150&&crtc_giga_count<300&&crtc_table[7]; crtc_giga_count=0; // autodetect Gigascreen effects
			;
			;
			;
			;
			;
			if (tape_closed)
				tape_closed=0,session_dirtymenu=1; // tag tape as closed
			tape_fastskip=tape_feedskip=audio_pos_z=0;
			if (tape&&tape_skipload&&!tape_delay) // &&tape_enabled
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
	if (mem_xtr) free(mem_xtr);
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
