 // ######  #    #   ####   ######   ####    ----------------------- //
//  """"#"  "#  #"  #""""   #"""""  #""""#  ZXSEC, barebones Sinclair //
//     #"    "##"   "####   #####   #    "  Spectrum emulator written //
//    #"      ##     """"#  #""""   #       on top of CPCEC's modules //
//   #"      #""#   #    #  #       #    #  by Cesar Nicolas-Gonzalez //
//  ######  #"  "#  "####"  ######  "####"  since 2019-02-24 till now //
 // """"""  "    "   """"   """"""   """"    ----------------------- //

#define MY_CAPTION "ZXSEC"
#define my_caption "zxsec"
#define MY_VERSION "20220108"//"2555"
#define MY_LICENSE "Copyright (C) 2019-2022 Cesar Nicolas-Gonzalez"

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
#include <string.h> // strcpy()...

// ZX Spectrum metrics and constants defined as general types ------- //

#define MAIN_FRAMESKIP_BITS 4
#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (56<<4) // HBLANK 12c, BORDER 6c, BITMAP 32c, BORDER 6c
#define VIDEO_LENGTH_Y (39<<4)
#define VIDEO_OFFSET_X (14<<4) // Pentagon: (12<<4)
#define VIDEO_OFFSET_Y ( 5<<4) // Pentagon: ( 3<<4)
#define VIDEO_PIXELS_X (40<<4) // Pentagon: (44<<4)
#define VIDEO_PIXELS_Y (30<<4) // Pentagon: (34<<4)
#define AUDIO_PLAYBACK 44100 // 22050, 24000, 44100, 48000
#define AUDIO_LENGTH_Z (AUDIO_PLAYBACK/VIDEO_PLAYBACK) // division must be exact!

#define DEBUG_LENGTH_X 64
#define DEBUG_LENGTH_Y 32
#define session_debug_show z80_debug_show
#define session_debug_user z80_debug_user

#if defined(SDL2)||!defined(_WIN32)
unsigned short session_icon32xx16[32*32] = {
	0X0000,0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,0X0000,
	0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,
	0XF000,0XF000,0XF000,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFF00,0XF800,0XF000,0XFF80,	0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFF0,0XFF80,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XFF00,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFF00,0XF800,0XF000,0XFF80,0XFFFF,	0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFF0,0XFF80,0XF000,0XF080,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XFF00,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFF00,0XF800,0XF000,0XFF80,0XFFFF,0XFFFF,	0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFF0,0XFF80,0XF000,0XF080,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF800,0XFF00,0XFFFF,0XFFFF,0XFFFF,0XFF00,0XF800,0XF000,0XFF80,0XFFFF,0XFFFF,0XFFFF,	0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFF0,0XFF80,0XF000,0XF080,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF800,0XF800,0XFF00,0XFF00,0XF800,0XF800,0XF000,0XFF80,0XFFFF,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF800,0XF800,0XFF00,0XF800,0XF800,0XF000,0XFF80,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF800,0XF800,0XF800,0XF800,0XF000,0XFF80,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF800,0XF800,0XF800,0XF000,0XFF80,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF800,0XF800,0XF000,0XFF80,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF800,0XF000,0XFF80,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XF000,0XFF80,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF000,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,	0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,	0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XF0F0,0XF000,0XF000,

	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,	0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XF0F0,0XF080,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,	0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XFFFF,0XF0F0,0XF080,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF000,0XF08F,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF08F,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF08F,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF08F,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFFF0,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF08F,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF08F,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF08F,0XFFFF,0XF0FF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XFF80,0XF000,0XF080,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF08F,0XFFFF,0XF0FF,0XF0FF,0XFFFF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XFF80,0XF000,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,	0XF080,0XF080,0XF080,0XF080,0XF000,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF0FF,0XFFFF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XFF80,0XF000,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,	0XF080,0XF080,0XF080,0XF000,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF0FF,0XFFFF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XFF80,0XF000,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,	0XF080,0XF080,0XF000,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF0FF,0XFFFF,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,0XF080,	0XF080,0XF000,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF000,0XF000,0XF000,
	0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,
	0X0000,0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,0X0000,
	};
#endif

// The Spectrum 48K keyboard; later models add pairs such as BREAK = CAPS SHIFT + SPACE
// +---------------------------------------------------------------------+
// | 1 18 | 2 19 | 3 1A | 4 1B | 5 1C | 6 24 | 7 23 | 8 22 | 9 21 | 0 20 |
// +---------------------------------------------------------------------+
// | Q 10 | W 11 | E 12 | R 13 | T 14 | Y 2C | U 2B | I 2A | O 29 | P 28 |
// +---------------------------------------------------------------------+
// | A 08 | S 09 | D 0A | F 0B | G 0C | H 34 | J 33 | K 32 | L 31 | * 30 | * = RETURN
// +---------------------------------------------------------------------+ + = CAPS SHIFT
// | + 00 | Z 01 | X 02 | C 03 | V 04 | B 3C | N 3B | M 3A | - 39 | / 38 | / = SPACE
// +---------------------------------------------------------------------+ - = SYMBOL SHIFT

#define KBD_JOY_UNIQUE 5 // exclude repeated buttons
unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1-4
	{ 0,0,0,0,0,0,0,0 }; // variable instead of constant, there are several joystick types
#define MAUS_EMULATION // emulation can examine the mouse
#define MAUS_LIGHTGUNS // lightguns are emulated with the mouse
#define INFLATE_RFC1950 // reading ZXS files requires inflating RFC1950 data
//#define DEFLATE_RFC1950 // writing ZXS files can need deflating RFC1950 data
//#define DEFLATE_LEVEL 6 // compression level 0..9 -- ZXS files never go >16k
#include "cpcec-os.h" // OS-specific code!
#include "cpcec-rt.h" // OS-independent code!
BYTE joy1_type=2;
BYTE joy1_types[][8]={ // virtual button is repeated for all joystick buttons
	{ 0X43,0X42,0X41,0X40,0X44,0X44,0X44,0X44 }, // Kempston
	{ 0X1B,0X1A,0X18,0X19,0X1C,0X1C,0X1C,0X1C }, // 4312+5: Sinclair 1
	{ 0X21,0X22,0X24,0X23,0X20,0X20,0X20,0X20 }, // 9867+0: Interface II, Sinclair 2
	{ 0X23,0X24,0X1C,0X22,0X20,0X20,0X20,0X20 }, // 7658+0: Cursor, Protek, AGF
	{ 0X10,0X08,0X29,0X28,0X38,0X38,0X38,0X38 }, // QAOP+Space
};

int litegun=0; // 0 = standard joystick, 1 = Gunstick (MHT)
const unsigned char kbd_map_xlt[]=
{
	// control keys
	KBCODE_F1	,0x81,	KBCODE_F2	,0x82,	KBCODE_F3	,0x83,	KBCODE_F4	,0x84,
	KBCODE_F5	,0x85,	KBCODE_F6	,0x86,	KBCODE_F7	,0x87,	KBCODE_F8	,0x88,
	KBCODE_F9	,0x89,	KBCODE_HOLD	,0x8F,	KBCODE_F11	,0x8B,	KBCODE_F12	,0x8C,
	KBCODE_X_ADD	,0x91,	KBCODE_X_SUB	,0x92,	KBCODE_X_MUL	,0x93,	KBCODE_X_DIV	,0x94,
	#ifdef DEBUG
	KBCODE_PRIOR	,0x95,	KBCODE_NEXT	,0x96,	KBCODE_HOME	,0x97,	KBCODE_END	,0x98,
	#endif
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
	KBCODE_ESCAPE	,0x78, // CAPS SHIFT (0x40) + SPACE (0x38)
	KBCODE_BKSPACE	,0x60, // CAPS SHIFT (0x40) + "0" (0x20)
	KBCODE_TAB	,0x58, // CAPS SHIFT (0x40) + "1" (0x18)
	KBCODE_CAPSLOCK	,0x59, // CAPS SHIFT (0x40) + "2" (0x19)
	KBCODE_UP	,0x63, // CAPS SHIFT (0x40) + "7" (0x23)
	KBCODE_DOWN	,0x64, // CAPS SHIFT (0x40) + "6" (0x24)
	KBCODE_LEFT	,0x5C, // CAPS SHIFT (0x40) + "5" (0x1C)
	KBCODE_RIGHT	,0x62, // CAPS SHIFT (0x40) + "8" (0x22)
	//KBCODE_X_ADD	,0xB2, // SYMBOL SHIFT (0x80) + "K" (0x)
	//KBCODE_X_SUB	,0xB3, // SYMBOL SHIFT (0x80) + "J" (0x)
	//KBCODE_X_MUL	,0xBC, // SYMBOL SHIFT (0x80) + "B" (0x)
	//KBCODE_X_DIV	,0x84, // SYMBOL SHIFT (0x80) + "V" (0x)
	//KBCODE_X_DOT	,0xBA, // SYMBOL SHIFT (0x80) + "M" (0x3A)
	//KBCODE_INSERT	,0x61, // CAPS SHIFT FLAG (0x40) + "9" (0x21) GRAPH?
};

const VIDEO_UNIT video_table[][36]= // colour table, 0xRRGGBB style, followed by the 8-8-4 components of ULAPLUS
{
	// monochrome - black and white // =(b+r*3+g*9+13/2)/13;
	{
		0X000000,0X1B1B1B,0X373737,0X525252,
		0X6E6E6E,0X898989,0XA5A5A5,0XC0C0C0,
		0X000000,0X242424,0X494949,0X6D6D6D,
		0X929292,0XB6B6B6,0XDBDBDB,0XFFFFFF,
		// ULAPLUS subcomponents: 8G,8R,4B
		0X000000,0X191919,0X323232,0X4B4B4B,
		0X656565,0X7E7E7E,0X989898,0XB0B0B0,
		0X000000,0X080808,0X111111,0X191919,
		0X222222,0X2A2A2A,0X323232,0X3B3B3B,
		0X000000,0X080808,0X0E0E0E,0X141414,
	},
	// dark colour
	{
		0X000000,0X000080,0X800000,0X800080,
		0X008000,0X008080,0X808000,0X808080,
		0X000000,0X0000FF,0XFF0000,0XFF00FF,
		0X00FF00,0X00FFFF,0XFFFF00,0XFFFFFF,
		// ULAPLUS subcomponents: 8G,8R,4B
		0X000000,0X000500,0X001500,0X002F00,
		0X005400,0X008200,0X00BC00,0X00FF00,
		0X000000,0X050000,0X150000,0X2F0000,
		0X540000,0X820000,0XBC0000,0XFF0000,
		0X000000,0X00002F,0X000082,0X0000FF,
	},
	// normal colour
	{
		0X000000,0X0000C0,0XC00000,0XC000C0,
		0X00C000,0X00C0C0,0XC0C000,0XC0C0C0,
		0X000000,0X0000FF,0XFF0000,0XFF00FF,
		0X00FF00,0X00FFFF,0XFFFF00,0XFFFFFF,
		// ULAPLUS subcomponents: 8G,8R,4B
		0X000000,0X002400,0X004900,0X006D00,
		0X009200,0X00B600,0X00DB00,0X00FF00,
		0X000000,0X240000,0X490000,0X6D0000,
		0X920000,0XB60000,0XDB0000,0XFF0000,
		0X000000,0X00006D,0X0000B6,0X0000FF,
	},
	// bright colour
	{
		0X000000,0X0000E0,0XE00000,0XE000E0,
		0X00E000,0X00E0E0,0XE0E000,0XE0E0E0,
		0X000000,0X0000FF,0XFF0000,0XFF00FF,
		0X00FF00,0X00FFFF,0XFFFF00,0XFFFFFF,
		// ULAPLUS subcomponents: 8G,8R,4B
		0X000000,0X004300,0X007D00,0X00AB00,
		0X00D000,0X00EA00,0X00FA00,0X00FF00,
		0X000000,0X430000,0X7D0000,0XAB0000,
		0XD00000,0XEA0000,0XFA0000,0XFF0000,
		0X000000,0X0000AB,0X0000EA,0X0000FF,
	},
	// monochrome - green screen
	{
		0X003C00,0X084808,0X176017,0X1E6C1E,
		0X449044,0X4B9C4B,0X5AB45A,0X62C062,
		0X003C00,0X0A4B0A,0X1E691E,0X287828,
		0X5AC35A,0X64D264,0X78F078,0X82FF82,
		// ULAPLUS subcomponents: 8G,8R,4B
		0X003C00,0X0D4F0D,0X1A631A,0X277627,
		0X338933,0X409C40,0X4DB04D,0X5AC35A,
		0X000000,0X040604,0X090D09,0X0D130D,
		0X111A11,0X152015,0X1A271A,0X1E2D1E,
		0X000000,0X040604,0X070B07,0X0A0F0A,
	},
};

// GLOBAL DEFINITIONS =============================================== //

int TICKS_PER_FRAME;// ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)/32);
int TICKS_PER_SECOND;// (TICKS_PER_FRAME*VIDEO_PLAYBACK);
// Everything in the ZX Spectrum is tuned to a 3.5 MHz clock,
// using simple binary divisors to adjust the devices' timings;
// the "3.5 MHz" isn't neither exact or the same on each machine:
// 50*312*224= 3494400 Hz on 48K, 50*311*228= 3545400 Hz on 128K.
// (and even the 50Hz screen framerate isn't the exact PAL value!)
DWORD main_t=0; // the global tick counter, used by the debugger
int multi_t=1; // overclocking factor

// HARDWARE DEFINITIONS ============================================= //

BYTE mem_ram[10<<14],mem_rom[4<<14]; // memory: 10*16k RAM and 4*16k ROM
BYTE *mmu_ram[4],*mmu_rom[4]; // memory is divided in 16k banks
#define mem_16k (&mem_ram[8<<14]) // dummy bank: write-only area for ROM writes
#define mem_32k (&mem_ram[9<<14]) // dummy bank: read-only area filled with 255
#define PEEK(x) mmu_rom[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!

BYTE type_id=1; // 0=48K, 1=128K, 2=PLUS2, 3=PLUS3
BYTE video_type=length(video_table)/2; // 0 = monochrome, 1=darkest colour, etc.
VIDEO_UNIT video_clut[65]; // precalculated colour palette, 16 attr + 48-colour ULAPLUS extra palette + border

// Z80 registers: the hardware and the debugger must be allowed to "spy" on them!

Z80W z80_af,z80_bc,z80_de,z80_hl; // Accumulator+Flags, BC, DE, HL
Z80W z80_af2,z80_bc2,z80_de2,z80_hl2,z80_ix,z80_iy; // AF', BC', DE', HL', IX, IY
Z80W z80_pc,z80_sp,z80_iff,z80_ir; // Program Counter, Stack Pointer, Interrupt Flip-Flops, IR pair
BYTE z80_imd,z80_r7; // Interrupt Mode // low 7 bits of R, required by several `IN X,(Y)` operations

// the Dandanator cartridge system can spy on the Z80 and trap its operations

#define Z80_DANDANATOR
BYTE *mem_dandanator=NULL; char dandanator_path[STRMAX]="";
WORD dandanator_trap,dandanator_temp; // Dandanator-Z80 watchdogs
BYTE dandanator_cfg[8]; // CONFIG + OPCODE + PARAM1 + PARAM2 + active + return + asleep + EEPROM
int dandanator_canwrite=0,dandanator_dirty,dandanator_base; // R/W status

// the BETA128 disc interface (handled more like a Flash card here) can spy on the Z80, too

#define TRDOS_AVGTRACKS 80 // typical discs are 640K long
#define TRDOS_MAXTRACKS 86 // some discs are 688K instead
FILE *trdos[4]={NULL,NULL,NULL,NULL}; char trdos_path[STRMAX]=""; // file handles and current path
int trdos_mapped=0,trdos_canwrite[4],trdos_modified[4]={0,0,0,0},trdos_sides[4],trdos_tracks[4],trdos_cursor[4];
BYTE trdos_rom[1<<14],*trdos_ram[4]={NULL,NULL,NULL,NULL},trdos_buffer[256]; // 16K TR-DOS ROM + 640K per drive
BYTE trdos_system,trdos_status,trdos_sector,trdos_track,trdos_command; // configuration ports
int trdos_length=0,trdos_offset,trdos_target; // operation counters
#define trdos_closeall() (trdos_close(0),trdos_close(1))
int trdos_load(char *s) // load BETA128 TR-DOS BIOS. `s` path; 0 OK, !0 ERROR
{
	*trdos_rom=0; // tag ROM as empty
	FILE *f=fopen(strcat(strcpy(session_substr,session_path),s),"rb"); if (!f) return 1; // fail!
	int i=fread1(trdos_rom,1<<14,f); i+=fread1(trdos_rom,1<<14,f);
	return fclose(f),i!=(1<<14); //||!equalsmmmm(&trdos_rom[0X3D03],0X00181400); // TR-DOS fingerprint
}

#define trdos_setup()
void trdos_reset(void) // reset BETA128 logic; beware, don't touch `trdos_mapped`!
{
	trdos_system=4; trdos_status=trdos_sector=trdos_track=trdos_command=trdos_length=0; MEMZERO(trdos_cursor);
}
void trdos_close(int d) // close BETA128 disc in drive `d`(0 = A:), if any
{
	if (trdos_modified[d]<0)
		fseek(trdos[d],0,SEEK_SET),fwrite1(trdos_ram[d],-trdos_modified[d],trdos[d]),fsetsize(trdos[d],-trdos_modified[d]);
	if (trdos_ram[d])
		free(trdos_ram[d]),trdos_ram[d]=NULL;
	if (trdos[d])
		puff_fclose(trdos[d]),trdos[d]=NULL;
	trdos_modified[d]=0;
}
int trdos_create(char *s) // create a blank BETA128 disc in path `s`; !0 ERROR
{
	FILE *f; if (!(f=fopen(s,"wb")))
		return 1; // cannot create disc!
	BYTE t[4096]; // a whole track
	MEMZERO(t);
	//t[0X008E1]=0; // first sector
	t[0X008E2]=1; // first track
	t[0X008E3]=0X16; // 80 tracks, 2 sides
	//t[0X008E4]=0; // no files
	t[0X008E5]=-16;
	t[0X008E6]=9;
	t[0X008E7]=0X10; // TR-DOS filesystem
	memset(&t[0X008EA],' ',9);
	strcpy(&t[0X008F5],"new disc");
	fwrite1(t,sizeof(t),f);
	MEMZERO(t);
	for (int i=1;i<TRDOS_AVGTRACKS*2;++i)
		fwrite1(t,sizeof(t),f);
	return fclose(f);
}
int trdos_open(char *s,int d,int canwrite) // insert a BETA128 disc from path `s` in drive `d`; !0 ERROR
{
	trdos_close(d); //if (!*trdos_rom) return 1; // no TR-DOS ROM!
	if (!(trdos_ram[d]=malloc(TRDOS_MAXTRACKS<<13)))
		return 1; // memory error!
	if (!(trdos[d]=puff_fopen(s,(trdos_canwrite[d]=canwrite)?"rb+":"rb")))
		return trdos_close(d),1; // file error!
	int i=fread1(trdos_ram[d],9,trdos[d]);
	if (!memcmp(trdos_ram[d],"SINCLAIR",8)&&trdos_ram[d][8]<'B') // valid SCL header?
	{
		int j,k=trdos_ram[d][8];
		memset(trdos_ram[d],0,4096);
		for (i=16,j=0;j<k;++j) // build file list (the first 4k)
		{
			fread1(&trdos_ram[d][16*j],14,trdos[d]);
			trdos_ram[d][16*j+14]=i&15;
			trdos_ram[d][16*j+15]=i>>4;
			i+=trdos_ram[d][16*j+13];
		}
		cprintf("SCL header: %d files and %d sectors.\n",k,i);
		trdos_ram[d][0X008E1]=i&15;
		trdos_ram[d][0X008E2]=i>>4;
		trdos_ram[d][0X008E3]=0X16; // 80 tracks, 2 sides
		trdos_ram[d][0X008E4]=k;
		trdos_ram[d][0X008E5]=j=(2560-16-i); // skip the first 4k
		trdos_ram[d][0X008E6]=j>>8;
		trdos_ram[d][0X008E7]=0X10; // TR-DOS filesystem
		memset(&trdos_ram[d][0X008EA],' ',9);
		strcpy(&trdos_ram[d][0X008F5],"VIRTUAL!"); //memset(&trdos_ram[d][0X008F5],' ',8);
		i=4096; trdos_canwrite[d]=0; // SCL files are read-only
	}
	// either TRD or SCL, read the remainder of the file
	i+=fread1(&trdos_ram[d][i],(TRDOS_MAXTRACKS<<13)-i,trdos[d]);
	i+=fread1(trdos_ram[d],(TRDOS_MAXTRACKS<<13),trdos[d]);
	if (i<8192||i>(TRDOS_MAXTRACKS<<13)||trdos_ram[d][0X008E7]!=0X10||(trdos_ram[d][0X31]>0&&trdos_ram[d][0X31]<3))
		return trdos_close(d),1; // not a valid TR-DOS disc!
	if (i<(TRDOS_MAXTRACKS<<13))
		memset(&trdos_ram[d][i],0,(TRDOS_MAXTRACKS<<13)-i); // sanitize unused space
	trdos_sides[d]=(trdos_ram[d][0X008E3]&8)?1:2;
	trdos_tracks[d]=(trdos_ram[d][0X008E3]&1)?40:80;
	cprintf("TRDOS format: %d tracks and %d sides.\n",trdos_tracks[d],trdos_sides[d]);
	if (trdos_path!=s) strcpy(trdos_path,s);
	return trdos_modified[d]=trdos_tracks[d]*trdos_sides[d]*16*256,0;
}

// 0x??FE,0x7FFD,0x1FFD: ULA 48K,128K,PLUS3 ------------------------- //

BYTE ula_v1,ula_v2,ula_v3; // 48K, 128K and PLUS3 respectively
#define ULA_V1_ISSUE3 16 // the Issue-3 ULA port base value
#define ULA_V1_ISSUE2 24 // ditto, on Issue-2
BYTE disc_disabled=0,psg_disabled=0,ula_v1_issue=ULA_V1_ISSUE3,ula_v1_cache=0; // auxiliar ULA variables
BYTE *ula_screen; int ula_bitmap,ula_attrib; // VRAM pointers
BYTE ula_clash[4][1<<16],*ula_clash_mreq[5],*ula_clash_iorq[5]; // the fifth entry stands for constant clashing
int ula_clash_z; // the in-frame T counter, a 17-bit cursor that follows the ULA clash map
int ula_fix_chr,ula_fix_out; // ULA adjustment for attribute and border effects
int ula_sixteen=0,ula_pentagon=0; // 16K mode, Pentagon memory contention and timing rules
int ula_start_x,ula_limit_x,ula_start_y,ula_limit_y=312; // horizontal+vertical limits
void ula_setup_clash(int i,int j,int l,int x0,int x1,int x2,int x3,int x4,int x5,int x6,int x7)
{
	for (int y=0;y<192;++y,j+=l-128)
		for (int x=0;x<16;++x)
			ula_clash[i][j++]=x0,
			ula_clash[i][j++]=x1,
			ula_clash[i][j++]=x2,
			ula_clash[i][j++]=x3,
			ula_clash[i][j++]=x4,
			ula_clash[i][j++]=x5,
			ula_clash[i][j++]=x6,
			ula_clash[i][j++]=x7;
}
void ula_setup(void)
{
	memset(mem_32k,-1,1<<14); // used by 16K models
	MEMZERO(ula_clash); // non-contended areas lack clash
	ula_setup_clash(1,14335,224,6,5,4,3,2,1,0,0); // ULA v1: 48K
	ula_setup_clash(2,14361,228,6,5,4,3,2,1,0,0); // ULA v2: 128K,PLUS2
	ula_setup_clash(3,14365,228,1,0,7,6,5,4,3,2); // ULA v3: PLUS3
}
void ula_update(void) // update ULA settings according to model
{
	// the Pentagon timings were measured with the beamracing-heavy demos "ACROSS THE EDGE" and "KPACKU DELUXE"
	ula_fix_out=ula_pentagon?  7:type_id?  2:  0; // contention tests ULA48, ULA128, ULA128P3, FPGA48ALL and FPGA128
	ula_fix_chr=ula_pentagon? 42:type_id?  1:  0; // NIRVANA games DREAMWALKER, MULTIDUDE, SUNBUCKET, STORMFINCH...
	ula_start_x=ula_pentagon? 38:type_id? 39: 38; // HBLANK position
	ula_limit_x=ula_pentagon? 56:type_id? 57: 56; // characters per scanline
	ula_start_y=ula_pentagon?-17:type_id?  1:  0; // vertical offset
	ula_limit_y=ula_pentagon?303:312; // VBLANK position (not frame height)
	TICKS_PER_SECOND=(TICKS_PER_FRAME=(ula_limit_y-ula_start_y)*ula_limit_x*4)*VIDEO_PLAYBACK;
}

const int mmu_ram_mode[4][4]= // relative offsets of every bank for each PLUS3 RAM mode
{
	{ 0x00000-0x0000,0x04000-0x4000,0x08000-0x8000,0x0C000-0xC000 }, // V3=1: 0 1 2 3
	{ 0x10000-0x0000,0x14000-0x4000,0x18000-0x8000,0x1C000-0xC000 }, // V3=3: 4 5 6 7
	{ 0x10000-0x0000,0x14000-0x4000,0x18000-0x8000,0x0C000-0xC000 }, // V3=5: 4 5 6 3
	{ 0x10000-0x0000,0x1C000-0x4000,0x18000-0x8000,0x0C000-0xC000 }, // V3=7: 4 7 6 3
};
void mmu_update(void) // update the MMU tables with all the new offsets
{
	// Contention applies to the banks:
	// - V1 (48K): area 4000-7FFF (equivalent to 128K bank 5)
	// - V2 (128K,PLUS2): banks 1,3,5,7
	// - V3 (PLUS3): banks 4,5,6,7
	// In other words:
	// - banks 5 and 7 are always contended;
	// - banks 1 and 3 are contended on V2;
	// - banks 4 and 6 are contended on V3.
	if (ula_pentagon||type_id>3) // Inves, Pentagon and Scorpion boards are contention-free
	{
		ula_clash_mreq[0]=ula_clash_mreq[1]=ula_clash_mreq[2]=ula_clash_mreq[3]=ula_clash_mreq[4]=
		ula_clash_iorq[0]=ula_clash_iorq[1]=ula_clash_iorq[2]=ula_clash_iorq[3]=ula_clash_iorq[4]=ula_clash[0];
	}
	else
	{
		ula_clash_mreq[4]=ula_clash[type_id<2?type_id+1:type_id]; // turn TYPE_ID (0, 1+2, 3) into ULA V (1, 2, 3)
		ula_clash_mreq[0]=ula_clash_mreq[2]=(type_id==3&&(ula_v3&7)==1)?ula_clash_mreq[4]:ula_clash[0];
		ula_clash_mreq[1]=(type_id<3||(ula_v3&7)!=1)?ula_clash_mreq[4]:ula_clash[0];
		ula_clash_mreq[3]=type_id&&(type_id==3?ula_v3&1?(ula_v3&7)==3:ula_v2&4:ula_v2&1)?ula_clash_mreq[4]:ula_clash[0];
		if (type_id==3)
			ula_clash_iorq[0]=ula_clash_iorq[1]=ula_clash_iorq[2]=ula_clash_iorq[3]=ula_clash_iorq[4]=ula_clash[0]; // IORQ doesn't clash at all on PLUS3 and PLUS2A!
		else
			ula_clash_iorq[0]=ula_clash_iorq[2]=ula_clash_iorq[3]=ula_clash[0],ula_clash_iorq[1]=ula_clash_iorq[4]=ula_clash_mreq[4]; // clash on 0x4000-0x7FFF only
	}
	if (ula_v3&1) // PLUS3 custom mode?
	{
		int i=(ula_v3>>1)&3;
		mmu_rom[0]=mmu_ram[0]=&mem_ram[mmu_ram_mode[i][0]]; // 0000-3FFF
		mmu_rom[1]=mmu_ram[1]=&mem_ram[mmu_ram_mode[i][1]]; // 4000-7FFF
		mmu_rom[2]=mmu_ram[2]=&mem_ram[mmu_ram_mode[i][2]]; // 8000-BFFF
		mmu_rom[3]=mmu_ram[3]=&mem_ram[mmu_ram_mode[i][3]]; // C000-FFFF
	}
	else // normal 48K + 128K modes
	{
		mmu_rom[0]=&mem_rom[((ula_v2&16)<<10)+((ula_v3&4)<<13)-0x0000]; // i.e. ROM ID=((ula_v3&4)/2)+((ula_v2&16)/16)
		if (/*mmu_rom[0]==&mem_rom[0xC000-0x0000]&&*/(trdos_mapped&trdos_rom[0])) // 0XF3 in TRDOS.ROM
			mmu_rom[0]=&trdos_rom[-0x0000]; // TR-DOS appears in place of the 48K BIOS
		mmu_ram[0]=mem_16k-0x0000; // 0000-3FFF is a dummy 16K for ROM writes (special case: Dandanator)
		mmu_rom[1]=mmu_ram[1]=&mem_ram[(5<<14)-0x4000]; // 4000-7FFF is always bank 5
		if (ula_sixteen&&!type_id) // 16K mode
			mmu_rom[2]=mem_32k-0x8000,mmu_rom[3]=mem_32k-0xC000,
			mmu_ram[2]=mem_16k-0x8000,mmu_ram[3]=mem_16k-0xC000;
		else
			mmu_rom[2]=mmu_ram[2]=&mem_ram[(2<<14)-0x8000], // 8000-BFFF is always bank 2
			mmu_rom[3]=mmu_ram[3]=&mem_ram[((ula_v2&7)<<14)-0xC000]; // C000-FFFF is only limited to bank 0 on 48K
	}
	ula_screen=&mem_ram[(ula_v2&8)?0x1C000:0x14000]; // bit 3: VRAM is bank 5 (OFF) or 7 (ON)

#ifdef Z80_DANDANATOR // Dandanator is always the last part of the MMU update
	if (mem_dandanator) // emulate the Dandanator (and more exactly its Spectrum memory map) only when a card is loaded
		if (dandanator_cfg[4]<32)
			mmu_rom[0]=&mem_dandanator[(dandanator_cfg[4]<<14)-0x0000];
}
#define dandanator_clear() (dandanator_cfg[0]=dandanator_cfg[1]=dandanator_cfg[2]=dandanator_cfg[3]=dandanator_temp=0)
void dandanator_update(void) // parse and run Dandanator commands, if any
{
	if (dandanator_cfg[0]==46) // wake up!
	{
		if (dandanator_cfg[1]==dandanator_cfg[2]) // parameters must match
		{
			cprintf("DAN! %08X: 046,%03d\n",dandanator_cfg[1]);
			if (dandanator_cfg[1]==1) dandanator_cfg[6]|=4; // go to sleep
			else if (dandanator_cfg[1]==16) dandanator_cfg[6]&=~4; // wake up
			else if (dandanator_cfg[1]==31) dandanator_cfg[6]|=8; // sleep till reset
		}
	}
	else if (!dandanator_cfg[6]&&mem_dandanator) // ignore if asleep or empty
	{
		cprintf("DAN! %08X: %03d,%03d,%03d,%03d\n",z80_pc.w,dandanator_cfg[0],dandanator_cfg[1],dandanator_cfg[2],dandanator_cfg[3]);
		if (!dandanator_cfg[0]) // no command, debug only
			;
		else if (dandanator_cfg[0]<34) // immediate bank change
			dandanator_cfg[4]=dandanator_cfg[0]-1;
		else if (dandanator_cfg[0]==34) // vanish and sleep!
			dandanator_cfg[4]=32,dandanator_cfg[6]|=4;
		else if (dandanator_cfg[0]==36) // reset Z80!
			z80_iff.w=z80_pc.w=0;
		else if (dandanator_cfg[0]==40) // bank + flags
		{
			if (dandanator_cfg[2]&3) // reset or NMI? changes are immediate!
				dandanator_cfg[4]=dandanator_cfg[1]-1,z80_iff.w=0,z80_pc.w=/*(dandanator_cfg[2]&1)?*/0/*:0x66*/; // is the NMI ever used?
			else // changes aren't immediate, wait till next RET
				dandanator_cfg[5]=dandanator_cfg[1];
			dandanator_cfg[6]=dandanator_cfg[2]&12; // +4 wakes up with command 46, +8 sleeps till reset
		}
		else if (dandanator_cfg[0]==48) // EEPROM: uses the dummy 16K bank as a bridge
		{
			if (dandanator_cfg[1]==16)
				dandanator_base=-1; // get ready for bridge setup with a later LD (HL),A
			else if (dandanator_cfg[1]==32) // request EEPROM sector for writing
				if (dandanator_cfg[7]=dandanator_cfg[2]&127) // keep EEPROM sector 0 read-only
					;//if (dandanator_canwrite&&mem_dandanator)
						//memset(mem_16k,-1,4<<12),dandanator_dirty=1; // reset dummy bank
		}
	}
	mmu_update(); dandanator_clear(); // update MMU and reset command queue
}
void dandanator_eeprom(void) // modify the cartridge, if allowed
{
	if (dandanator_cfg[7]) // before doing RET, "SWORD OF IANNA" sends 4K to 0000-0FFF and "BOBBY CARROT" sends 4K to 3000-3FFF
	{
		cprintf("DAN! %08X: %02X,%04X: %02X%02X%02X%02X%02X%02X%02X%02X...\n",z80_pc.w,dandanator_cfg[7],dandanator_base
			,mem_16k[dandanator_base+0],mem_16k[dandanator_base+1],mem_16k[dandanator_base+2],mem_16k[dandanator_base+3]
			,mem_16k[dandanator_base+4],mem_16k[dandanator_base+5],mem_16k[dandanator_base+6],mem_16k[dandanator_base+7]
			);
		if (dandanator_canwrite&&mem_dandanator&&!(dandanator_base&0x0FFF))
		{
			memcpy(&mem_dandanator[dandanator_cfg[7]<<12],mem_16k+dandanator_base,1<<12); // dump dummy 16K bank onto the EEPROM sector
			dandanator_dirty=dandanator_base=1;
		}
		dandanator_cfg[7]=0;
	}
}
#define Z80_DNTR_0X10(w) do{ if (!--dandanator_trap) { if (dandanator_temp&1) ++dandanator_temp; \
	if (w<0x4000) dandanator_trap=1; else if (dandanator_temp>3*2||(dandanator_cfg[0]>0&&dandanator_cfg[0]<40)) dandanator_update(); } }while(0)
//#define Z80_DNTR_0X02(w,b) do{ if (mem_16k[0x1555]==0xAA) dandanator_base=w,mem_16k[0x1555]=0; }while(0) // no known Dandanator titles need this, but nothing keeps them from needing it!
#define Z80_DNTR_0X12(w,b) do{ if (mem_16k[0x1555]==0xAA) dandanator_base=w,mem_16k[0x1555]=0; }while(0)
#define Z80_DNTR_0X32(w,b) do{ if (w<4) { ++dandanator_cfg[(dandanator_temp|=1)/2]; \
	if (dandanator_temp<=3*2) dandanator_trap=1; else dandanator_update(); } }while(0)
#define Z80_DNTR_0X77(w,b) do{ if (mem_16k[0x1555]==0xAA) dandanator_base=w,mem_16k[0x1555]=0; \
	else if (w<4) ++dandanator_cfg[(dandanator_temp|=1)/2],dandanator_trap=1; }while(0)
#define Z80_DNTR_0XC9() do{ if (mem_16k[0x1555]==0xA0) dandanator_eeprom(),mem_16k[0x1555]=0; \
	else if (dandanator_cfg[5]) dandanator_cfg[4]=dandanator_cfg[5]-1,dandanator_cfg[5]=0,mmu_update(); }while(0)
#define Z80_DNTR_0XFB() (dandanator_clear()) // we trap EI because interrupt handling implies timeouts
void z80_dandanator_reset(void)
{
	MEMZERO(dandanator_cfg); dandanator_trap=dandanator_temp=0;
	dandanator_update();
#endif
}

#define ula_v1_send(i) (ula_v1=i,(ulaplus_table[64]&ulaplus_enabled)||(video_clut[64]=video_table[video_type][ula_v1&7]))
#define ula_v2_send(i) (ula_v2=i,mmu_update())
#define ula_v3_send(i) (ula_v3=i,mmu_update())

BYTE ulaplus_enabled=1,ulaplus_index,ulaplus_table[65]; // ULAPLUS 64-colour palette + configuration byte (default 0, disabled)

#define ulaplus_clut_calc(i) (video_table[video_type][16+(i>>5)]+video_table[video_type][24+((i>>2)&7)]+video_table[video_type][32+(i&3)])
VIDEO_UNIT ula_clut[2][256];
void ula_clut_flash(void) // swap the FLASH-enabled entries in the CLUT
{
	VIDEO_UNIT t; for (int i=128;i<256;++i)
		t=ula_clut[0][i],ula_clut[0][i]=ula_clut[1][i],ula_clut[1][i]=t;
}
void ula_clut_send(int i) // update a valid ULAPLUS entry in the precalc'd table
{
	int l=i&7,h=(i&48)<<2;
	if (i&8)
		h+=l*8,
		ula_clut[0][0+h]=ula_clut[0][1+h]=
		ula_clut[0][2+h]=ula_clut[0][3+h]=
		ula_clut[0][4+h]=ula_clut[0][5+h]=
		ula_clut[0][6+h]=ula_clut[0][7+h]=
			video_clut[i];
	else
		h+=l,
		ula_clut[1][ 0+h]=ula_clut[1][ 8+h]=
		ula_clut[1][16+h]=ula_clut[1][24+h]=
		ula_clut[1][32+h]=ula_clut[1][40+h]=
		ula_clut[1][48+h]=ula_clut[1][56+h]=
			video_clut[i];
}
void ula_clut_update(void) // build a lookup table of ALL precalc'd colours
{
	if (ulaplus_table[64]&ulaplus_enabled) // ULAPLUS?
		for (int h=0;h<64;h+=16)
			for (int l=0;l<8;++l)
			{
				ula_clut[0][0+h*4+l*8]=ula_clut[0][1+h*4+l*8]=
				ula_clut[0][2+h*4+l*8]=ula_clut[0][3+h*4+l*8]=
				ula_clut[0][4+h*4+l*8]=ula_clut[0][5+h*4+l*8]=
				ula_clut[0][6+h*4+l*8]=ula_clut[0][7+h*4+l*8]=
					video_clut[h+l+8];
				ula_clut[1][  0+h*4+l]=ula_clut[1][  8+h*4+l]=
				ula_clut[1][ 16+h*4+l]=ula_clut[1][ 24+h*4+l]=
				ula_clut[1][ 32+h*4+l]=ula_clut[1][ 40+h*4+l]=
				ula_clut[1][ 48+h*4+l]=ula_clut[1][ 56+h*4+l]=
					video_clut[h+l+0];
			}
	else // original ULA
	{
		for (int i=0;i<8;++i)
		{
			ula_clut[1][  0+i]=ula_clut[0][  0+i*8]=
			ula_clut[1][  8+i]=ula_clut[0][  1+i*8]=
			ula_clut[1][ 16+i]=ula_clut[0][  2+i*8]=
			ula_clut[1][ 24+i]=ula_clut[0][  3+i*8]=
			ula_clut[1][ 32+i]=ula_clut[0][  4+i*8]=
			ula_clut[1][ 40+i]=ula_clut[0][  5+i*8]=
			ula_clut[1][ 48+i]=ula_clut[0][  6+i*8]=
			ula_clut[1][ 56+i]=ula_clut[0][  7+i*8]=
			ula_clut[1][128+i]=ula_clut[0][128+i*8]=
			ula_clut[1][136+i]=ula_clut[0][129+i*8]=
			ula_clut[1][144+i]=ula_clut[0][130+i*8]=
			ula_clut[1][152+i]=ula_clut[0][131+i*8]=
			ula_clut[1][160+i]=ula_clut[0][132+i*8]=
			ula_clut[1][168+i]=ula_clut[0][133+i*8]=
			ula_clut[1][176+i]=ula_clut[0][134+i*8]=
			ula_clut[1][184+i]=ula_clut[0][135+i*8]=
				video_clut[  0+i];
			ula_clut[1][ 64+i]=ula_clut[0][ 64+i*8]=
			ula_clut[1][ 72+i]=ula_clut[0][ 65+i*8]=
			ula_clut[1][ 80+i]=ula_clut[0][ 66+i*8]=
			ula_clut[1][ 88+i]=ula_clut[0][ 67+i*8]=
			ula_clut[1][ 96+i]=ula_clut[0][ 68+i*8]=
			ula_clut[1][104+i]=ula_clut[0][ 69+i*8]=
			ula_clut[1][112+i]=ula_clut[0][ 70+i*8]=
			ula_clut[1][120+i]=ula_clut[0][ 71+i*8]=
			ula_clut[1][192+i]=ula_clut[0][192+i*8]=
			ula_clut[1][200+i]=ula_clut[0][193+i*8]=
			ula_clut[1][208+i]=ula_clut[0][194+i*8]=
			ula_clut[1][216+i]=ula_clut[0][195+i*8]=
			ula_clut[1][224+i]=ula_clut[0][196+i*8]=
			ula_clut[1][232+i]=ula_clut[0][197+i*8]=
			ula_clut[1][240+i]=ula_clut[0][198+i*8]=
			ula_clut[1][248+i]=ula_clut[0][199+i*8]=
				video_clut[  8+i];
		}
		if (video_pos_z&16)
			ula_clut_flash();
	}
}
void video_clut_update(void) // precalculate palette following `video_type`
{
	if (ulaplus_table[64]&ulaplus_enabled)
	{
		for (int i=0;i<64;++i)
			video_clut[i]=ulaplus_clut_calc(ulaplus_table[i]);
		video_clut[64]=video_clut[8]; // border is CLUT #8
	}
	else
	{
		for (int i=0;i<16;++i)
			video_clut[i]=video_table[video_type][i];
		ula_v1_send(ula_v1); // border is CLUT #64 (ULA V1)
	}
	ula_clut_update();
}
#define ulaplus_table_select(i) (ulaplus_index=(i<=64?i:ulaplus_index))
#define ulaplus_table_recv() (ulaplus_table[ulaplus_index])
void ulaplus_table_send(int i)
{
	if (ulaplus_table[ulaplus_index]!=i)
	{
		ulaplus_table[ulaplus_index]=i;
		//cprintf("%08X: ULAPLUS %02X=%02X:%06X\n",z80_pc.w,ulaplus_index,i,video_clut[ulaplus_index]);
		if (ulaplus_index>=64)
			video_clut_update(); // recalculate whole palette on mode change
		else if (ulaplus_table[64]&ulaplus_enabled)
		{
			video_clut[ulaplus_index]=ulaplus_clut_calc(i);
			if (ulaplus_index==8) video_clut[64]=video_clut[ulaplus_index]; // border is CLUT #8
			ula_clut_send(ulaplus_index); // modify just one entry
		}
	}
}

int z80_irq,z80_active=0; // IRQ length (0 = no IRQ) and internal HALT flag: <0 EXPECT NMI!, 0 IGNORE IRQS, >0 ACCEPT IRQS, >1 EXPECT IRQ!
//#define z80_nmi_throw (z80_active=-1,z80_irq=96) // NMI has priority over IRQs and erases them!

void ula_reset(void) // reset the ULA
{
	ula_v1=ula_v2=ula_v3=ulaplus_index=ulaplus_table[64]=0;
	video_clut_update();
	ula_update(),mmu_update();
	ula_bitmap=0; ula_attrib=0x1800;
}

// 0xBFFD,0xFFFD: PSG AY-3-8910 ------------------------------------- //

// sound table, 16 static levels + 1 dynamic level, 16-bit sample style
int audio_table[17]={0,85,121,171,241,341,483,683,965,1365,1931,2731,3862,5461,7723,10922,0};

#define PSG_TICK_STEP 16 // 3.5 MHz /2 /16 = 109375 Hz
#define PSG_KHZ_CLOCK 1750 // compare with the 2000 kHz YM3 standard
#define PSG_MAIN_EXTRABITS 0 // "QUATTROPIC" [http://randomflux.info/1bit/viewtopic.php?id=21] improves weakly with >0
#if AUDIO_CHANNELS > 1
const int psg_stereos[][3]={{0,0,0},{+256,0,-256},{+128,0,-128},{+64,0,-64}}; // the most common config is ABC, but AY-Melodik is ACB
#endif
#define PSG_PLAYCITY 1 // base clock in comparison to the main PSG
#define PSG_PLAYCITY_HALF // the TURBO SOUND chip is emulated as a single-chip PLAYCITY card
int playcity_disabled=1,playcity_active=0; // this chip is an extension (disabled by default)
int dac_disabled=0; // Covox $FB DAC, enabled by default on almost every Pentagon 128 machine

#include "cpcec-ay.h"

// behind the ULA and the PSG: PRINTERS ----------------------------- //

FILE *printer=NULL; int printer_p=0; BYTE printer_t[256+2]; // the buffer MUST be at least 258 bytes long!!
void printer_flush(void)
	{ fwrite1(printer_t,printer_p,printer),printer_p=0; }
void printer_close(void)
	{ printer_flush(),fclose(printer),printer=NULL; }

// each model has its own printer interface: the PLUS3 printer is a 8-bit data port;
int printer_8,printer_1; // the 128K printer is based on a 1-bit serial port;
void printer_line(void) // the 48K ZX Printer is completely graphical.
{
	if (printer_p) // we must skip the first byte and trim useless spaces
	{
		while (printer_p>1&&printer_t[printer_p-1]==' ') --printer_p;
		printer_t[printer_p]='\n'; // put LINE FEED after the last dot!
		fwrite1(&printer_t[1],printer_p,printer); printer_p=0;
	}
}

// behind the ULA: TAPE --------------------------------------------- //

int tape_enabled=0; // tape playback length, in frames
#define TAPE_MAIN_TZX_STEP 70 // amount of T units per packet, must be a divisor of 3500000!
#define TAPE_SPECTRUM_FORMATS // required for Spectrum!
//#define TAPE_KANSAS_CITY // possibly useless outside MSX, are there any non-MSX protections using this method?
#include "cpcec-k7.h"

// 0x1FFD,0x2FFD,0x3FFD: FDC 765 ------------------------------------ //

#define DISC_PARMTR_UNIT (disc_parmtr[1]&1) // CPC hardware is limited to two drives;
#define DISC_PARMTR_UNITHEAD (disc_parmtr[1]&5) // however, it allows two-sided discs.
#define DISC_TIMER_INIT ( 4<<6) // rough approximation: cfr. CPCEC for details. "GAUNTLET 3" works fine.
#define DISC_TIMER_BYTE ( 2<<6) // rough approximation, too
#define DISC_WIRED_MODE 0 // PLUS3 FDC is unwired, like the CPC; "HATE.DSK" proves it.
#define DISC_PER_FRAME (312<<6) // = 1 MHz / 50 Hz ; compare with TICKS_PER_FRAME
#define DISC_CURRENT_PC (z80_pc.w)

#define DISC_NEW_SIDES 1
#define DISC_NEW_TRACKS 40
#define DISC_NEW_SECTORS 9
BYTE DISC_NEW_SECTOR_IDS[]={0xC1,0xC6,0xC2,0xC7,0xC3,0xC8,0xC4,0xC9,0xC5};
#define DISC_NEW_SECTOR_SIZE_FDC 2
#define DISC_NEW_SECTOR_GAPS 82
#define DISC_NEW_SECTOR_FILL 0xE5

#include "cpcec-d7.h"

// CPU-HARDWARE-VIDEO-AUDIO INTERFACE =============================== //

int audio_dirty,audio_queue=0; // used to clump audio updates together to gain speed

#define Z80_DNTR_0X3A(w,b) do{ if (w>=0X4000&&w<=0X7FFF) ula_bus3=b; }while(0) // this isn't Dandanator logic, but it operates the same way
int ula_bus,ula_bus3; // floating bus: -1 if we're beyond the bitmap, latest ATTRIB otherwise; notice that PLUS3 uses a different contended bus
int ula_count_x=0,ula_count_y=0; // horizontal+vertical sync counters
int ula_shown_x,ula_shown_y=192; // horizontal+vertical bitmap/attrib counters
int ula_snow_disabled=1,ula_snow_z,ula_snow_a; // the ULA snow flags and parameters
int ula_clash_a=0; // the still unprocessed T units within a character: 0, 1, 2 or 3

INLINE void video_main(int t) // render video output for `t` clock ticks; t is always nonzero!
{
	int a=ula_bus,b; // `ula_bus` is required because the video loop may fail if the Z80 is overclocked
	for (ula_clash_a+=t;ula_clash_a>=4;ula_clash_a-=4)
	{
		z80_irq=z80_irq<4?0:z80_irq-4;
		if (ula_shown_x==ula_start_x) // HBLANK? (the Pentagon timings imply this test is done in advance)
		{
			if (!video_framecount&&video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y) video_drawscanline();
			video_pos_y+=2,video_target+=VIDEO_LENGTH_X*2-video_pos_x; video_pos_x=0; session_signal|=session_signal_scanlines; // scanline event!
		}
		if ((video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X))
		{
			if ((ula_shown_y>=0&&ula_shown_y<192)&&(ula_shown_x>=0&&ula_shown_x<32))
			{
				if (ula_shown_x&1)
					a=ula_screen[ula_attrib++],b=ula_screen[ula_bitmap++];
				else
					a=ula_screen[ula_attrib+ula_snow_z],b=ula_screen[ula_snow_z+ula_bitmap++],ula_bus3=ula_screen[++ula_attrib]; // the PLUS3 floating bus is special
			}
			else
				a=-1; // border! (no matter how badly we do, the bitmap is always inside the visible screen)
			if (!video_framecount)
			{
				static BYTE a0,b0;
				#define VIDEO_NEXT *video_target++ // "VIDEO_NEXT = VIDEO_NEXT = ..." generates invalid code on VS13 and slower code on TCC
				if (a<0) // BORDER
				{
					VIDEO_UNIT p=video_clut[64];
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					video_pos_x+=16;
				}
				else if (ula_shown_x&1) // BITMAP, 2nd character
				{
					VIDEO_UNIT p,v1=ula_clut[1][a0],v0=ula_clut[0][a0];
					VIDEO_NEXT=p=b0&128?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b0& 64?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b0& 32?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b0& 16?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b0&  8?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b0&  4?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b0&  2?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b0&  1?v1:v0; VIDEO_NEXT=p;
					v1=ula_clut[1][a],v0=ula_clut[0][a];
					VIDEO_NEXT=p=b&128?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b& 64?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b& 32?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b& 16?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b&  8?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b&  4?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b&  2?v1:v0; VIDEO_NEXT=p;
					VIDEO_NEXT=p=b&  1?v1:v0; VIDEO_NEXT=p;
					video_pos_x+=32;
				}
				else // BITMAP, 1st character
					a0=a,b0=b;
			}
		}
		else
			video_target+=16,video_pos_x+=16;
		if (++ula_shown_x==ula_limit_x) // end of bitmap?
		{
			ula_shown_x=0; ++ula_shown_y;
			if (ula_shown_y>=0&&ula_shown_y<192)
			{
				ula_bitmap=((ula_shown_y&192)<<5)+((ula_shown_y&56)<<2)+((ula_shown_y&7)<<8);
				ula_attrib=0x1800+((ula_shown_y&248)<<2);
				ula_snow_z=z80_ir.b.l&ula_snow_a;
			}
		}
		if (++ula_count_x>=ula_limit_x) // end of scanline?
		{
			ula_count_x=0;
			// for lack of a more precise timer, the scanline is used to refresh the ULA's unstable input bit 6:
			// - Issue 2 systems (the first batch of 48K) make the bit depend on ULA output bits 3 and 4.
			// - Issue 3 systems (later batches of 48K) make the bit depend on ULA output bit 4.
			// - Whatever the issue ID, 48K doesn't update the unstable bit at once; it takes a while.
			// - 128K and later systems always mask the bit out.
			ula_v1_cache=type_id?64:ula_v1&ula_v1_issue?0:64;
			// "Abu Simbel Profanation" (menu doesn't obey keys; in-game is stuck jumping to the right) and "Rasputin" (menu fails to play the music) rely on this on 48K.
			if (++ula_count_y>=ula_limit_y) // end of frame?
			{
				z80_irq=(trdos_mapped|ula_pentagon|type_id)?33:32; // TR-DOS adds delays ("MANIFESTO" demo); Azesmbog's "BORDER8" for Pentagon needs <44 T; "TIMING TESTS 128K" needs 33..40!
				if (!(video_pos_z&15)) // FLASH update?
					if (!(ulaplus_table[64]&ulaplus_enabled))
						ula_clut_flash(); // ULAPLUS lacks FLASH!
				ula_shown_x=ula_fix_chr; // renew frame vars
				ula_shown_y=(ula_count_y=ula_start_y)-64; // lines of BORDER above the bitmap
				// all calculations are ready: feed everything to the video engine
				if (!video_framecount) video_endscanlines(); // frame is complete!
				video_newscanlines(video_pos_x,(ula_start_y+(ula_fix_chr>ula_start_x))*2);
				session_signal|=SESSION_SIGNAL_FRAME+session_signal_frames; // frame event!
			}
		}
	}
	ula_bus=a; // -1 in border, 0..255 (ATTRIB) in bitmap; cfr. z80_recv_ula()
}

int dac_level=0;
#define dac_level_byte(x) dac_level=(x)<<7
#define dac_level_zero() (dac_level=(dac_level*3)>>2) // soften signal as time passes
void audio_main(int t) // render audio output for `t` clock ticks; t is always nonzero!
{
	psg_main(t,(tape_status<<12)+((ula_v1&16)<<10)+((ula_v1&8)<<8)+dac_level); // ULA MIXER: tape input (BIT 2) + beeper (BIT 4) + tape output (BIT 3; "Manic Miner" song, "Cobra's Arc" speech)
}

// autorun runtime logic -------------------------------------------- //

BYTE snap_done; // avoid accidents with ^F2, see all_reset()
char autorun_path[STRMAX]="",autorun_line[STRMAX];
int autorun_mode=0,autorun_t=0;
BYTE autorun_kbd[16]; // automatic keypresses
#define autorun_kbd_set(k) (autorun_kbd[k>>3]|=1<<(k&7))
#define autorun_kbd_res(k) (autorun_kbd[k>>3]&=~(1<<(k&7)))
#define autorun_kbd_bit(k) (autorun_mode?autorun_kbd[k]:(kbd_bit[k]|joy_bit[k]))
INLINE void autorun_next(void)
{
	switch (autorun_mode)
	{
		case 1: // 48K and menu-less 128K: type 'LOAD ""' and press RETURN
		case 2: // BETA128: type 'RANDOMIZE USR 15619: REM: RUN"filename"' and press RETURN
			if (trdos_ram[0])
			{
				BYTE *t=NULL,s[]={249,192,'1','5','6','1','9',58,234,58,247,34,'b','o','o','t',32,32,32,32,34,13,128};
				memcpy(&POKE(0X5CCC),s,sizeof(s)); POKE(0X5C61)=0XCC+sizeof(s);
				for (int i=0;i<0x0800;i+=16)
					if (trdos_ram[0][i+8]=='B') // found a BASIC file?
					{
						if (!memcmp(&trdos_ram[0][i],&s[12],8))
							{ t=NULL; break; } // found the autoboot file!
						if (trdos_ram[0][i]&&!t)
							t=&trdos_ram[0][i]; // remember this BASIC file, but keep looking for autoboot files
					}
				if (t) memcpy(&POKE(0X5CD8),t,8); // overwrite the filename
			}
			else
			{
				BYTE s[]={239,34,34,13,128};
				memcpy(&POKE(0X5CCC),s,sizeof(s)); POKE(0X5C61)=0XCC+sizeof(s);
			}
			// no `break`!
		case 3: // menu-ready 128K: hit RETURN
			autorun_kbd_set(0x30); // PRESS RETURN
			autorun_mode=4; autorun_t=3;
			break;
		case 4: // release RETURN
			autorun_kbd_res(0x30); // RELEASE RETURN
			autorun_mode=0; // end of AUTORUN
			break;
	}
}

// Z80-hardware procedures ------------------------------------------ //

//#define Z80_NMI_ACK (z80_irq=0) // allow throwing NMIs // NMI overrides IRQ
// the Spectrum hands the Z80 a mainly empty data bus value
#define z80_irq_bus 255
// the Spectrum doesn't obey the Z80 IRQ ACK signal
#define z80_irq_ack() 0

void z80_sync(int t) // the Z80 asks the hardware/video/audio to catch up
{
	static int r=0; main_t+=t;
	int tt=(r+=t)/multi_t; // calculate base value of `t`
	r-=(t=tt*multi_t); // adjust `t` and keep remainder
	if (t)
	{
		if (type_id==3/*&&!disc_disabled*/)
			disc_main(t);
		if (tape_enabled&&tape)
			audio_dirty=1,tape_main(t); // echo the tape signal thru sound!
		if (tt)
		{
			audio_queue+=tt;
			if (audio_dirty&&!audio_disabled)
				audio_main(audio_queue),audio_dirty=audio_queue=0;
			video_main(tt);
		}
	}
}

int tape_skipload=1,tape_fastload=1,tape_skipping=0;

void z80_send(WORD p,BYTE b) // the Z80 sends a byte to a hardware port
{
	if ((p&31)==31) // BETA128 interface
	{
		if (trdos_mapped)
		{
			if (p&128) // 0x??FF: SYSTEM
			{
				if (!trdos_length) // ignore if busy!
				{
					if (4&b&~trdos_system) trdos_reset(); // setting bit 2 resets the controller!
					trdos_system=b; // bits 0 and 1 select the drive, bit 4 selects the side
				}
			}
			else if (p&64)
			{
				if (p&32) // 0x??7F: DATA
				{
					if (trdos_length)
					{
						trdos_buffer[trdos_offset++]=b;
						int d=trdos_system&3;
						if (!--trdos_length&&trdos_ram[d]&&trdos_target>=0&&trdos_canwrite[d])
							MEMSAVE(&trdos_ram[d][trdos_target],trdos_buffer);
					}
					else
						*trdos_buffer=b; // the parameter of the SEEK command
				}
				else // 0x??5F: SECTOR
				{
					if (!trdos_length) // ignore if busy!
						trdos_sector=b;
				}
			}
			else
			{
				if (p&32) // 0x??3F: TRACK
				{
					if (!trdos_length) // ignore if busy!
						trdos_track=b>99?99:b; // is there a maximum? is it checked here? see also the SEEK command
				}
				else // 0x??1F: COMMAND
				{
					if (!trdos_length||(b>>4)==13) // ignore if busy, unless command is FORCED INTERRUPT!
					{
						cprintf("%08X: BETA128 %02X %02X:%02X:%02X:%02X",z80_pc.w,b,trdos_track,trdos_system,trdos_sector,*trdos_buffer);
						int i,d=trdos_system&3; // the disc side is chosen from outside the command itself, thru the system config
						int s=trdos_sides[d]>1&&!(trdos_system&16); // the disc drive is chosen from outside too
						trdos_target=-1; // tag operation as non-writing
						switch (b>>4) // TR-DOS is limited to a subset of the WD179X controller command set
						{
							case  0: // RESTORE
								trdos_cursor[d]=trdos_track=0;
								trdos_status=0X04; // TRACK 0
								break;
							case  1: // SEEK
								trdos_cursor[d]=trdos_track=*trdos_buffer>99?99:*trdos_buffer; // is there a maximum? is it checked here?
								trdos_status=trdos_track>trdos_tracks[d]?0X10:trdos_track?0:0X04; // SEEK ERROR, TRACK 0, OK
								break;
							case 12: // READ ADDRESS
								if (!trdos_ram[d])
									trdos_status=0X01; // DISC MISSING?
								else if (trdos_track>=trdos_tracks[d]||trdos_sector<1||trdos_sector>16)
									trdos_status=0X10; // RECORD NOT FOUND
								else
								{
									trdos_buffer[0]=trdos_track;
									trdos_buffer[1]=s;
									trdos_buffer[2]=trdos_sector=(trdos_sector&15)+1; // follow sectors
									trdos_buffer[4]=//~trdos_crc16>>8;
									trdos_buffer[5]=//~trdos_crc16;
									trdos_buffer[3]=//0X00; // 0X00 means 256 bytes
									trdos_status=//0X00; // OK
									trdos_offset=0; trdos_length=6;
								}
								break;
							case 13: // FORCED INTERRUPTION
								trdos_length=0;
								break;
							case  8: // READ SECTOR
								if (!trdos_ram[d])
									trdos_status=0X01; // DISC MISSING?
								else if (/*trdos_cursor[d]!=trdos_track||*/trdos_track>=trdos_tracks[d]||trdos_sector<1||trdos_sector>16)
									trdos_status=0X10; // RECORD NOT FOUND
								else
								{
									i=((trdos_track*trdos_sides[d]+s)*16+trdos_sector-1)<<8;
									MEMLOAD(trdos_buffer,&trdos_ram[d][i]);
									trdos_offset=0; trdos_length=256;
									cprintf(" %08X:%04X",i,trdos_length);
									trdos_status=0X00; // OK
								}
								break;
							case 10: // WRITE SECTOR
								if (!trdos_ram[d])
									trdos_status=0X01; // DISC MISSING?
								else if (!trdos_canwrite[d])
									trdos_status=0X40; // WRITE PROTECT
								else if (/*trdos_cursor[d]!=trdos_track||*/trdos_track>=trdos_tracks[d]||trdos_sector<1||trdos_sector>16)
									trdos_status=0X10; // RECORD NOT FOUND
								else
								{
									i=((trdos_track*trdos_sides[d]+s)*16+trdos_sector-1)<<8;
									trdos_target=i; if (trdos_modified[d]>0) trdos_modified[d]=-trdos_modified[d];
									trdos_offset=0; trdos_length=256;
									cprintf(" %08X:%04X",i,trdos_length);
									trdos_status=0X00; // OK
								}
								break;
							case 15: // WRITE TRACK
								if (!trdos_ram[d])
									trdos_status=0X01; // DISC MISSING?
								else if (!trdos_canwrite[d])
									trdos_status=0X40; // WRITE PROTECT
								else if (/*trdos_cursor[d]!=trdos_track||*/trdos_track>=trdos_tracks[d])
									trdos_status=0X10; // RECORD NOT FOUND
								else
									memset(&trdos_ram[d][(trdos_track*trdos_sides[d]+s)<<12],0,16*256);
								break;
							// TR-DOS 5.03 does not use the following operations AFAIK
							case  2: // STEP
							case  3: // STEP (bis)
								if ((b=trdos_command)&2) // redo STEP-OUT?
							case  6: // STEP-OUT
							case  7: // STEP-OUT (bis)
								{
									if (trdos_track>0) trdos_cursor[d]=--trdos_track;
									break;
								}
								else // redo STEP-IN
								// no `break` here!
							case  4: // STEP-IN
							case  5: // STEP-IN (bis)
								{
									if (trdos_track<99) trdos_cursor[d]=++trdos_track;
									break;
								}
							case  9: // READ SECTORS
							case 14: // READ TRACK
							case 11: // WRITE SECTORS
							default:
								if (!trdos_ram[d])
									trdos_status=0X01; // DISC MISSING?
								else if (!trdos_canwrite[d]&&(b>>4)==11)
									trdos_status=0X40; // WRITE PROTECT
								else //if (trdos_track>=trdos_tracks[d])
									trdos_status=0X10; // RECORD NOT FOUND
								cprintf(" *UNKNOWN!");
						}
						cprintf(" %02X\n",trdos_status); trdos_command=b;
					}
				}
			}
		}
		return; // the following conditions cannot be true if we got here
	}
	if (!(p&1)) // 0x??FE, ULA 48K
	{
		ula_v1_send(b),tape_output=(b>>3)&1; // tape record signal
		if (ula_bus*ula_pentagon<0&&!video_framecount&&video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y
			&&video_pos_x>VIDEO_OFFSET_X&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X+16) // special case: Pentagon border?
			switch (ula_clash_a&3) // editing the bitmap from here is a dirty kludge, but the performance hit is lower
			{
				case 0: video_target[-12]=video_target[-11]=video_target[-10]=video_target[- 9]=video_clut[64]; // no `break`!
				case 1: video_target[- 8]=video_target[- 7]=video_target[- 6]=video_target[- 5]=video_clut[64]; // no `break`!
				case 2: video_target[- 4]=video_target[- 3]=video_target[- 2]=video_target[- 1]=video_clut[64]; // no `break`!
			}
	}
	if (!(p&2)) // 0x??FD, MULTIPLE DEVICES
	{
		if (type_id&&!(ula_v2&32)) // 48K mode forbids further changes!
		{
			if ((type_id!=3)?!(p&0x8000):((p&0xC000)==0x4000)) // 0x7FFD: ULA 128K
			{
				ula_v2_send(b);
				session_dirty|=b&32; // show memory change on window
				if (z80_pc.w==(type_id==3?0x0119:0x00D1)) // warm reset?
					snap_done=0;
			}
			if (type_id==3) // PLUS3 only!
			{
				if (!(p&0XF000)) // 0x0FFD: PLUS3 PRINTER
				{
					if (printer)
						if (printer_t[printer_p]=b,++printer_p>=256)
							printer_flush();
				}
				else if ((p&0xF000)==0x1000) // 0x1FFD: ULA PLUS3
				{
					ula_v3_send(b);
					if (!disc_disabled)
						disc_motor_set(b&8); // BIT 3: MOTOR
				}
				else if ((p&0xF000)==0x3000) // 0x3FFD: FDC DATA I/O
					if (!disc_disabled) // PLUS3?
						disc_data_send(b);
			}
		}
		if (p&0x8000) // PSG 128K
			if (type_id||!psg_disabled||!playcity_disabled) // optional on 48K
			{
				if (p&0x4000) // 0xFFFD: SELECT PSG REGISTER
				{
					#ifdef PSG_PLAYCITY
					if (b>=0xFE) // TURBO SOUND flag
						playcity_active=playcity_disabled?0:(b&1);
					else if (playcity_active)
						playcity_select(0,b);
					else
					#endif
						psg_table_select(b);
				}
				else // 0xBFFD: WRITE PSG REGISTER
				{
					#ifdef PSG_PLAYCITY
					if (playcity_active)
						playcity_send(0,b);
					else
					#endif
					if (psg_index==14&&(b&4)&&printer&&type_id>0&&type_id<3) // SPECTRUM 128K PRINTER
					{
						if ((b&8)) printer_8|=printer_1;
						if ((printer_1<<=1)&512) // a character is 11 bits long: 0 + B0...B7 + 1 + 1
							if (printer_t[printer_p]=printer_8>>1,++printer_p>=256)
								printer_flush();
					}
					else
						psg_table_send(b);
				}
			}
	}
	if (!(p&4)) // 0x??FB: ZX PRINTER and other extensions
	{
		#ifdef PSG_PLAYCITY
		if (!dac_disabled)
			dac_level_byte(b); // Covox $FB DAC, unsigned 8-bit audio
		#endif
		if (!type_id&&p<0XBF00) // avoid conflicts between ZX PRINTER and ULAPLUS
		{
			if (printer) // stop motor (bit 2 set) and ink (bit 7)
				if ((b&4)||((printer_t[printer_p]=(b&128)?'#':' '),++printer_p>256))
					printer_line();
		}
		else if (ulaplus_enabled) // the ULAPLUS output
		{
			if (p==0XFF3B)
				ulaplus_table_send(b);
			else if (p==0XBF3B)
				ulaplus_table_select(b);
		}
	}
}

// the emulator includes two methods to speed tapes up:
// * tape_skipload controls the physical method (disabling realtime, raising frameskip, etc. during tape operation)
// * tape_fastload controls the logical method (detecting tape loaders and feeding them data straight from the tape)

BYTE z80_tape_index[1<<16]; // full Z80 16-bit cache

BYTE z80_tape_fastload[][32] = { // codes that read pulses : <offset, length, data> x N) -------------------------------------------------------------- MAXIMUM WIDTH //
	/*  0 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   9,0XDB,0XFE,0X1F,0XD0,0XA9,0XE6,0X20,0X28,0XF3 }, // ZX SPECTRUM FIRMWARE
	/*  1 */ {  -2,  11,0XDB,0XFE,0X1F,0XE6,0X20,0XF6,0X02,0X4F,0XBF,0XC0,0XCD,  +7,   3,0X10,0XFE,0X2B,  +2,   2,0X20,0XF9 }, // ZX SPECTRUM FIRMWARE (SETUP)
	/*  2 */ {  -6,  13,0X04,0XC8,0X3E,0X7F,0XDB,0XFE,0X1F,0X00,0XA9,0XE6,0X20,0X28,0XF3 }, // TOPO
	/*  3 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   9,0XDB,0XFE,0XA9,0XE6,0X40,0XD8,0X00,0X28,0XF3 }, // MULTILOAD V1+V2
	/*  4 */ {  -4,   8,0X1C,0XC8,0XDB,0XFE,0XE6,0X40,0XBA,0XCA,-128, -10 }, // "LA ABADIA DEL CRIMEN"
	/*  5 */ {  -6,  13,0X04,0XC8,0X3E,0X7F,0XDB,0XFE,0X1F,0XA9,0XD8,0XE6,0X20,0X28,0XF3 }, // "HYDROFOOL" (1/2)
	/*  6 */ {  -6,  12,0X04,0XC8,0X3E,0X7F,0XDB,0XFE,0X1F,0XA9,0XE6,0X20,0X28,0XF4 }, // "HYDROFOOL" (2/2)
	/*  7 */ {  -8,   3,0X04,0X20,0X03,  +3,   9,0XDB,0XFE,0X1F,0XC8,0XA9,0XE6,0X20,0X28,0XF1 }, // ALKATRAZ
	/*  8 */ {  -6,  13,0X04,0XC8,0X3E,0X7F,0XDB,0XFE,0X1F,0XC8,0XA9,0XE6,0X20,0X28,0XF3 }, // "RAINBOW ISLANDS"
	/*  9 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   7,0XDB,0XFE,0XA9,0XE6,0X40,0X28,0XF5 }, // MINILOAD-ZXS
	/* 10 */ {  -6,   3,0X06,0XFF,0X3E,  +1,   7,0XDB,0XFE,0XE6,0X40,0XA9,0X28,0X09 }, // SPEEDLOCK V1 SETUP
	/* 11 */ {  -2,   6,0XDB,0XFE,0X1F,0XE6,0X20,0X4F }, // SPEEDLOCK V2 SETUP
	/* 12 */ {  -5,  11,0X4F,0X3E,0X7F,0XDB,0XFE,0XE6,0X40,0XA9,0X28,0X0E,0X2A }, // SPEEDLOCK V3 SETUP
	/* 13 */ {  -6,  13,0X04,0X20,0X01,0XC9,0XDB,0XFE,0X1F,0XC8,0XA9,0XE6,0X20,0X28,0XF3 }, // "CHIP'S CHALLENGE"
	/* 14 */ {  -2,   9,0XDB,0XFE,0X14,0XC8,0XE6,0X40,0XA9,0X28,0XF7 }, // "BOBBY CARROT"
	/* 15 */ {  -4,   9,0X04,0XC8,0XDB,0XFE,0XA9,0XE6,0X40,0X28,0XF7 }, // "TIMING TESTS (48K+128K)"
	/* 16 */ {  -3,   5,0X2C,0XDB,0XFE,0XA4,0XCA,-128,  -7 }, // GREMLIN (1/2)
	/* 17 */ {  -3,   5,0X2C,0XDB,0XFE,0XA4,0XC2,-128,  -7 }, // GREMLIN (2/2)
	/* 18 */ {  -0,   8,0XA9,0XE6,0X40,0X20,0X04,0X05,0X20,0XF4 }, // "BC'S QUEST FOR TIRES"
	/* 19 */ {  -8,   5,0X78,0XC6,0X02,0X47,0X38,  +3,   5,0XAD,0XE6,0X40,0X20,0XF3 }, // AMSTRAD CPC FIRMWARE
	/* 20 */ {  -6,  13,0X14,0XC8,0X3E,0XFF,0XDB,0XFE,0X1F,0XD0,0XAB,0XE6,0X20,0X28,0XF3 }, // "SIL4.TZX"
	/* 21 */ {  -4,   7,0X04,0XC8,0XDB,0XFE,0XA9,0XA2,0XCA,-128,  -9 }, // AST A.MOORE
	/* 22 */ {  -4,   8,0X24,0XC8,0XED,0X78,0XA8,0XE6,0X40,0XCA,-128, -10 }, // UNILODE
};
BYTE z80_tape_fastfeed[][32] = { // codes that build bytes
	/*  0 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -14 }, // ZX SPECTRUM FIRMWARE + TOPO
	/*  1 */ {  -0,   3,0X30,0X11,0X3E,   1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -15 }, // BLEEPLOAD
	/*  2 */ {  -5,   1,0X06,  +4,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X3E,  +1,   1,0XD2,-128, -16 }, // ALKATRAZ
	/*  3 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   2,0X30,0XF3 }, // "ABU SIMBEL PROFANATION"
	/*  4 */ { -26,   1,0X3E,  +1,   1,0XCD, +13,   1,0X3E, +12,   2,0X7B,0XFE,  +1,   5,0X3F,0XCB,0X15,0X30,0XDB }, // "LA ABADIA DEL CRIMEN"
	/*  5 */ { -13,   1,0X3E,  +4,   1,0XD0,  +2,   1,0X3E,  +4,   2,0XD0,0X3E,  +1,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -26 }, // SPEEDLOCK V1
	/*  6 */ {  -9,   1,0X3E,  +4,   2,0XD0,0XCD,  +2,   2,0XD0,0X3E,  +1,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -22 }, // SPEEDLOCK V2+V3
	/*  7 */ {  -5,   1,0X06,  +4,   1,0XD2,  +2,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X3E,  +1,   1,0XD2,-128, -18 }, // "CHIP'S CHALLENGE"
	/*  8 */ {  -0,   1,0X30,  +1,   5,0X90,0XCB,0X15,0X30,0XF1 }, // MINILOAD-ZXS 1/2
	/*  9 */ {  -0,   6,0XD0,0X90,0XCB,0X15,0X30,0XF2 }, // MINILOAD-ZXS 2/2
	/* 10 */ {  -0,   1,0X30,  +1,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   2,0X30,0XF2 }, // MULTILOAD V2
	/* 11 */ {  -0,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -13 }, // "ELEVATOR ACTION"
	/* 12 */ {  -0,   1,0XFE,  +1,   4,0X3F,0XCB,0X11,0XD2,-128, -11 }, // GREMLIN
	/* 13 */ {  -0,   2,0X84,0X21,  +2,   7,0XBE,0X3F,0XD9,0XCB,0X15,0XD9,0X21,  +2,   2,0X30,0XE9 }, // "BC'S QUEST FOR TIRES"
	/* 14 */ {  -0,   4,0XD2,0X00,0X00,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -16 }, // MIKRO-GEN ("AUTOMANIA")
	/* 15 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X1D,0X06,  +1,   1,0XD2,-128, -14 }, // SOFTLOCK
	/* 16 */ {  -0,   3,0X06,0X01,0X18 }, // UNILODE
};
BYTE z80_tape_fastdump[][32] = { // codes that fill blocks
	/*  0 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +19,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCA }, // ZX SPECTRUM FIRMWARE
	/*  1 */ { -29,   8,0X08,0X20,0X05,0XDD,0X75,0X00,0X18,0X0A, +10,   5,0XDD,0X23,0X1B,0X08,0X06, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD1 }, // GREMLIN OLD
	/*  2 */ { -31,  10,0X08,0X20,0X07,0X30,0XFE,0XDD,0X75,0X00,0X18,0X0A, +10,   5,0XDD,0X23,0X1B,0X08,0X06, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCE }, // BLEEPLOAD
	/*  3 */ { -31,  10,0X08,0X20,0X07,0X20,0X00,0XDD,0X75,0X00,0X18,0X0A, +10,   5,0XDD,0X23,0X1B,0X08,0X06, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCF }, // "RAINBOW ISLANDS"
	/*  4 */ {  -0,  12,0XDD,0X75,0X00,0XDD,0X2B,0XE1,0X2B,0X7C,0XB5,0XC8,0XE5,0XC3,-128, -17 }, // "LA ABADIA DEL CRIMEN"
	/*  5 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCB }, // "ABU SIMBEL PROFANATION"
	/*  6 */ { -28,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X06,  +1,   4,0X2E,0X01,0x00,0x3E, +29,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD0 }, // SPEEDLOCK V1
	/*  7 */ { -16,   4,0XDD,0X75,0X00,0XDD,  +1,   4,0X1B,0X2E,0X01,0X06, +13,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE3 }, // MINILOAD-ZXS
	/*  8 */ { -21,   3,0X08,0X28,0X04,  +4,   8,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X08,0X06, +16,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XDA }, // "CRAY-5"
};

WORD z80_tape_spystack(WORD d) { d+=z80_sp.w; return PEEK(d)+256*PEEK(d+1); }
int z80_tape_testfeed(WORD p)
{
	int i; if ((i=z80_tape_index[p])>length(z80_tape_fastfeed))
	{
		for (i=0;i<length(z80_tape_fastfeed)&&!fasttape_test(z80_tape_fastfeed[i],p);++i) ;
		z80_tape_index[p]=i; cprintf("FASTFEED: %04X=%02d\n",p,(i<length(z80_tape_fastfeed))?i:-1);
	}
	return i;
}
int z80_tape_testdump(WORD p)
{
	int i; if ((i=z80_tape_index[p-1])>length(z80_tape_fastdump)) // the offset avoid conflicts with TABLE2
	{
		for (i=0;i<length(z80_tape_fastdump)&&!fasttape_test(z80_tape_fastdump[i],p);++i) ;
		z80_tape_index[p-1]=i; cprintf("FASTDUMP: %04X=%02d\n",p,(i<length(z80_tape_fastdump))?i:-1);
	}
	return i;
}
int z80_tape_fastload_rev8b(int i)
	{ return ((i&128)?1:0)+((i&64)?2:0)+((i&32)?4:0)+((i&16)?8:0)+((i&8)?16:0)+((i&4)?32:0)+((i&2)?64:0)+((i&1)?128:0); }

void z80_tape_trap(void)
{
	int i,j,k; if ((i=z80_tape_index[z80_pc.w])>length(z80_tape_fastload))
	{
		for (i=0;i<length(z80_tape_fastload)&&!fasttape_test(z80_tape_fastload[i],z80_pc.w);++i) ;
		z80_tape_index[z80_pc.w]=i; cprintf("FASTLOAD: %04X=%02d\n",z80_pc.w,(i<length(z80_tape_fastload))?i:-1);
	}
	if (i>=length(z80_tape_fastload)) return; // only known methods can reach here!
	if (tape_enabled>=0&&tape_enabled<2) // automatic tape playback/stop?
		tape_enabled=2; // amount of frames the tape should keep moving
	if (!tape_skipping) tape_skipping=-1;
	switch (i) // always handle these special cases
	{
		case  1: // ZX SPECTRUM FIRMWARE (SETUP)
			/*
			z80_hl.w=262; z80_pc.w+=16; // cancel and skip delay
			z80_bc.b.l=tape_status?2:34; // fix border and flags
			*/ // this optimisation does more harm than good :-(
			tape_enabled|=64-1; // shorten setup (~1 second)
			// no `break`!
		case 10: // SPEEDLOCK V1 SETUP ("BOUNTY BOB STRIKES BACK")
		case 11: // SPEEDLOCK V2 SETUP ("ATHENA 48K / 128K")
		case 12: // SPEEDLOCK V3 SETUP ("THE ADDAMS FAMILY")
			tape_enabled|=8; // play tape and listen to decryption noises
			break;
	}
	if (tape_fastload) switch (i) // handle only when this flag is on
	{
		case  0: // ZX SPECTRUM FIRMWARE, "ABU SIMBEL PROFANATION", SOFTLOCK ("ELITE 48K", "RASPUTIN 48K")
		case  5: // "HYDROFOOL" (1/2)
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==3||j==11))
			{
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==0||k==5||k==8)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1&&((WORD)(z80_ix.w-z80_sp.w)>2))
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				k=fasttape_feed(z80_bc.b.l>>5,59),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else if (z80_hl.b.l==0x80&&FASTTAPE_CAN_FEED&&z80_tape_testfeed(z80_tape_spystack(0))==15) // SOFTLOCK
			{
				if (z80_af2.b.l&0x40) if (z80_tape_testdump(z80_tape_spystack(0))==0)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1&&((WORD)(z80_ix.w-z80_sp.w)>2))
						k=z80_tape_fastload_rev8b(fasttape_dump()), // RR L instead of RL L!
						z80_hl.b.h^=POKE(z80_ix.w)=k,++z80_ix.w,--z80_de.w;
				k=z80_tape_fastload_rev8b(fasttape_feed(z80_bc.b.l>>5,59)), // ditto!
				tape_skipping=z80_hl.b.l=1+(k<<1),z80_bc.b.h=(k&128)?-1:0;
			}
			else
			{
				fasttape_add8(z80_bc.b.l>>5,59,&z80_bc.b.h,1); // z80_r7+=...*9;
				if (z80_tape_spystack(0)<=0x0585)
					fasttape_gotonext(); // if the ROM is expecting the PILOT, throw BYTES and PAUSE away!
			}
			break;
		case  2: // TOPO ("DESPERADO", "MAD MIX GAME", "STARDUST"), BLEEPLOAD ("BLACK LAMP")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==1))
			{
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==0||k==1||k==2)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				k=fasttape_feed(z80_bc.b.l>>5,58),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>5,58,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case  3: // MULTILOAD (V1: "DEFLEKTOR", "THE FINAL MATRIX"; V2: "SUPER CARS", "ATOMIC ROBO KID")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==10)
				k=fasttape_feed(z80_bc.b.l>>6,59),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>6,59,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case  4: // "LA ABADIA DEL CRIMEN"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==4)
			{
				if (z80_tape_testdump(z80_tape_spystack(2))==4)
					while (FASTTAPE_CAN_DUMP()&&POKE(z80_sp.w+4)>1)
						POKE(z80_ix.w)=fasttape_dump(),--z80_ix.w,--POKE(z80_sp.w+4);
				k=fasttape_feed(z80_de.b.h>>6,41),tape_skipping=z80_hl.b.l=128+(k>>1),z80_de.b.l=-(k&1);
			}
			else
				fasttape_add8(z80_de.b.h>>6,41,&z80_de.b.l,1); // z80_r7+=...*6;
			break;
		case  6: // "HYDROFOOL" (2/2), GREMLIN OLD ("THING BOUNCES BACK"), SPEEDLOCK V1+V2+V3 ("BOUNTY BOB STRIKES BACK", "ATHENA", "THE ADDAMS FAMILY"), DINAMIC ("COBRA'S ARC"), MIKRO-GEN ("AUTOMANIA")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==3||j==5||j==6||j==14))
			{
				if ((((k=z80_tape_testdump(z80_tape_spystack(0)))==1||k==5)&&z80_af2.b.l&0x40)||k==6) // SPEEDLOCK V1 ignores AF2
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				k=fasttape_feed(z80_bc.b.l>>5,54),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>5,54,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case  7: // ALKATRAZ ("HATE")
		case  8: // "RAINBOW ISLANDS"
		case 13: // "CHIP'S CHALLENGE"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==2||j==7))
			{
				if (((k=z80_tape_testdump(z80_tape_spystack(0)))==0&&j)||k==3) // "&&j" avoids bugs in "Advanced Soccer Simulator"
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				k=fasttape_feed(z80_bc.b.l>>5,59),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>5,59,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case  9: // MINILOAD-ZXS
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==8||j==9))
			{
				if (z80_tape_testdump(k=z80_tape_spystack(0))==7) // MINILOAD 1/2 doesn't check the length
				{
					k-=12; k=PEEK(k)&8?-1:1; // 0X23: ++; 0X2B: --
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),z80_ix.w+=k,--z80_de.w;
				}
				k=fasttape_feed(z80_bc.b.l>>6,50),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>6,50,&z80_bc.b.h,1); // z80_r7+=...*7;
			break;
		case 14: // "BOBBY CARROT"
			fasttape_add8(z80_bc.b.l>>6,43,&z80_de.b.h,1); // z80_r7+=...*6;
			break;
		case 15: // "TIMING TESTS (48K+128K)"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==3)
				k=fasttape_feed(z80_bc.b.l>>6,43),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>6,43,&z80_bc.b.h,1); // z80_r7+=...*6;
			break;
		case 16: // GREMLIN (1/2) ("BASIL THE GREAT MOUSE DETECTIVE")
			fasttape_add8(0,29,&z80_hl.b.l,1); // z80_r7+=...*4;
			break;
		case 17: // GREMLIN (2/2)
			if (z80_bc.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==12)
				k=fasttape_feed(1,29),tape_skipping=z80_bc.b.l=128+(k>>1),z80_hl.b.l=-(k&1);
			else
				fasttape_add8(1,29,&z80_hl.b.l,1); // z80_r7+=...*4;
			break;
		case 18: // "BC'S QUEST FOR TIRES" (needs 1982 SPECTRUM.ROM!)
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==13)
				k=fasttape_feed(z80_bc.b.l>>6,52),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc.b.h=k&1?0:z80_bc.b.h;
			else
				fasttape_sub8(z80_bc.b.l>>6,52,&z80_bc.b.h,1); // z80_r7+=...*7;
			break;
		case 19: // AMSTRAD CPC FIRMWARE ("DRAGONS OF FLAME")
			z80_r7+=fasttape_add8(~(z80_hl.b.l>>6),57,&z80_bc.b.h,2)*9;
			break;
		case 20: // "SIL4.TZX" (possibly a bad idea because of EI)
			fasttape_add8(z80_de.b.l>>5,59,&z80_de.b.h,1); // z80_r7+=...*9;
			tape_enabled|=32; // this loader must "pull" the tape
			break;
		case 21: // AST A.MOORE ("A YANKEE IN IRAQ")
			fasttape_add8(z80_bc.b.l>>6,38,&z80_bc.b.h,1); // z80_r7+=...*6;
			break;
		case 22: // UNILODE ("TRIVIAL PURSUIT")
			if ((z80_af.b.l&1)&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==16) // we must force a RET in this loader :-(
				k=fasttape_feed(z80_bc.b.h>>6,42),tape_skipping=z80_bc.b.h=128+(k>>1),z80_hl.b.h=-(k&1),k=z80_tape_spystack(0)+2,z80_pc.w=z80_tape_spystack(0)+2,z80_sp.w+=2;
			else
				fasttape_add8(z80_bc.b.h>>6,42,&z80_hl.b.h,1); // z80_r7+=...*7;
			break;
	}
}

int z80_recv_ula(void) // "Cobra" and "Arkanoid" were happy with `ula_bus` but the tests "FLOATSPY" and "HALT2INT" need more precision
{
	if (!(ula_shown_x&1)&&(ula_shown_y>=0&&ula_shown_y<192)&&(ula_shown_x>=0&&ula_shown_x<32))
		switch (ula_clash_a)
		{
			case 0: return ula_screen[ula_bitmap];
			case 1: return ula_screen[ula_attrib];
			case 2: return ula_screen[ula_bitmap+1];
			case 3: return ula_screen[ula_attrib+1];
		}
	return type_id==3?ula_bus3:255;
}
#define z80_recv_gunstick(button,sensor) (session_maus_z?button+((video_litegun&0x00C000)?sensor:0):0) // GUNSTICK detects bright pixels
BYTE z80_recv(WORD p) // the Z80 receives a byte from a hardware port
{
	if ((p&31)==31) // tell apart between KEMPSTON and BETA128 ports
	{
		if (trdos_mapped) // BETA128 interface
		{
			if (p&128) // 0x??FF: SYSTEM
				return trdos_length?0X40:0X80; // 0X40 = more bytes, 0X80 = end of operation
			if (p&64)
			{
				if (p&32) // 0x??7F: DATA
					return trdos_length?--trdos_length,trdos_buffer[trdos_offset++]:255;
				//else // 0x??5F: SECTOR
					return trdos_sector;
			}
			//else
			{
				if (p&32) // 0x??3F: TRACK
					return trdos_track;
				//else // 0x??1F: STATUS
					return trdos_command==0X08?trdos_command=0,0X02:trdos_length?0X02:trdos_status;
			}
		}
		if ((p&63)==31) // KEMPSTON port
			return litegun?z80_recv_gunstick(16,4):autorun_kbd_bit(8); // catch special case: lightgun or joystick ("TARGET PLUS", "MIKE GUNNER")
		if (ula_pentagon)
			return ula_bus; // http://worldofspectrum.net/rusfaq/ : "*port FF - port of current screen attributes. [...] on border [...] gives FF."
		if (type_id<3) // non-PLUS3 floating bus?
			return (p>=0X4000&&p<=0X7FFF)?(ula_shown_x&1)?ula_bus:255:z80_recv_ula(); // kludge: "A Yankee in Iraq" uses a contended floating bus address :-/
		return 255;
	}
	if ((p&7)==6) // 0x??FE, ULA 48K
	{
		#ifdef Z80_DANDANATOR
		dandanator_clear(); // Dandanator timeouts happen when polling the keyboard or the tape
		#endif
		int j=0,k=autorun_kbd_bit(11)||autorun_kbd_bit(12)||autorun_kbd_bit(15);
		for (int i=0;i<8;++i)
			if (!(p&(256<<i))) // the bit mask in the upper byte can merge keyboard rows together
			{
				j|=autorun_kbd_bit(i); // bits 0-4: keyboard rows
				j|=i?autorun_kbd_bit(8+i):k; // handle composite keys: CAPS SHIFT is row 0, bit 0
				if (litegun&&!autorun_mode)
				{
					if (i==4) j=z80_recv_gunstick(1,4); // GUNSTICK on SINCLAIR 1 ("SOLO")
					//else if (i==3) //j=z80_recv_gunstick(16,4); // GUNSTICK on SINCLAIR 2
				}
			}
		if (tape)//&&!z80_iff.b.l) // at least one tape loader enables interrupts: the musical demo "SIL4.TZX"
			z80_tape_trap();
		if (tape&&tape_enabled)
		{
			if (!tape_status)
				j|=64; // bit 6: tape input state
		}
		else
			j^=ula_v1_cache; // Issue 2/3 difference, see above
		return ~j;
	}
	if ((p&7)==5) // 0x??FD, MULTIPLE DEVICES
	{
		if (type_id==3&&!(ula_v2&32)) // PLUS3 devices; notice they're unusable from 48K mode!
		{
			if (!(p&0XF000)) // 0x0FFD: PLUS3 PRINTER (bit 0), that is also the PLUS3 floating bus (bits 1-7)!
				return (z80_recv_ula()&-2)+!printer; // the PLUS3 floating bus is always 255 on 48K mode;
				// it otherwise shows either the last ULA byte or the last contended byte handled by the Z80.
			if ((p&0xE000)==0x2000) // 0x2FFD: FDC STATUS ; 0x3FFD: FDC DATA I/O
				return disc_disabled?255:(p&0x1000)?disc_data_recv():disc_data_info();
		}
		if (!(~p&0xC000)) // 0xFFFD: READ PSG REGISTER
			if (type_id||!psg_disabled) // !48K?
			{
				#ifdef PSG_PLAYCITY
				if (playcity_active)
					return playcity_recv(0); // AYTEST relies on this!
				#endif
				if (psg_index==14&&printer&&type_id>0&&type_id<3)//&&z80_pc.w<0x2000) // debug
					return printer_1=1,printer_8=0; // SPECTRUM 128K gets PRINTER BUSY from R14 BIT6!
				//else
					return psg_table_recv();
			}
	}
	if ((p&7)==3) // 0x??FB: ZX PRINTER and other EXTENSIONS
	{
		if (p==0XFF3B&&ulaplus_enabled) // the ULAPLUS input
			return ulaplus_table_recv();
		if (!type_id&&printer)
			return 128+1; // BIT 6 = NO PRINTER, BIT 7 = READY
	}
	return 255;
}

int z80_debug_hard_tab(char *t) { return sprintf(t,"    "); }
int z80_debug_hard1(char *t,int q,BYTE i) { return q?sprintf(t,":%02X",i):sprintf(t,":--"); }
void z80_debug_flash(char *t) { *t+=-128,t[1]+=-128; }
void z80_debug_hard(int q,int x,int y)
{
	char s[16*20],*t;
	t=s+sprintf(s,"ULA:         %05d:%c" "    %04X:%02X %02X",ula_clash_z%100000,
		48+ula_clash_mreq[4][(WORD)ula_clash_z],(WORD)(ula_bitmap+0x4000),(BYTE)ula_bus,ula_v1);
	t+=z80_debug_hard1(t,type_id,ula_v2);
	t+=z80_debug_hard1(t,type_id==3,ula_v3);
	#define Z80_DEBUG_HARD_T_F(x) ((x)?'*':'-')
	#define Z80_DEBUG_HARD_MREQ(x) (Z80_DEBUG_HARD_T_F(ula_clash_mreq[x]!=ula_clash[0]))
	#define Z80_DEBUG_HARD_IORQ(x) (Z80_DEBUG_HARD_T_F(ula_clash_iorq[x]!=ula_clash[0]))
	t+=sprintf(t," MREQ:%c%c%c%c IORQ:%c%c%c%c",
		Z80_DEBUG_HARD_MREQ(0),Z80_DEBUG_HARD_MREQ(1),Z80_DEBUG_HARD_MREQ(2),Z80_DEBUG_HARD_MREQ(3),
		Z80_DEBUG_HARD_IORQ(0),Z80_DEBUG_HARD_IORQ(1),Z80_DEBUG_HARD_IORQ(2),Z80_DEBUG_HARD_IORQ(3));
	int i;
	t+=sprintf(t,"PSG:                " "%02X: ",psg_index);
	for (i=0;i<8;++i)
		t+=sprintf(t,"%02X",psg_table[i]);
	t+=z80_debug_hard_tab(t);
	for (;i<16;++i)
		t+=sprintf(t,"%02X",psg_table[i]);
	if (psg_index>=0&&psg_index<16)
		z80_debug_flash(t+4-20*2+(psg_index/8)*20+(psg_index%8)*2);
	#ifdef PSG_PLAYCITY
	t+=sprintf(t,"%02X: ",playcity_index[0]);
	for (i=0;i<8;++i)
		t+=sprintf(t,"%02X",playcity_table[0][i]);
	t+=z80_debug_hard_tab(t);
	for (;i<16;++i)
		t+=sprintf(t,"%02X",playcity_table[0][i]);
	if (playcity_index[0]>=0&&playcity_index[0]<16)
		z80_debug_flash(t+4-20*2+(playcity_index[0]/8)*20+(playcity_index[0]%8)*2);
	#endif
	t+=sprintf(t,"FDC:  %02X - %04X:%04X" "    %c ",disc_parmtr[0],(WORD)disc_offset,(WORD)disc_length,48+disc_phase);
	for (i=0;i<7;++i)
		t+=sprintf(t,"%02X",disc_result[i]);
	#ifdef Z80_DANDANATOR
	t+=sprintf(t,"DANDANATOR:         " "%c: %02X:%02X:%02X %02X:%02X:%02X",mem_dandanator?'0'+dandanator_temp:'-',dandanator_cfg[0],dandanator_cfg[1],dandanator_cfg[2],dandanator_cfg[4],dandanator_cfg[5],dandanator_cfg[6]);
	#endif
	char *r=t; t=s; do
	{
		debug_locate(x,y); ++y;
		MEMNCPY(debug_output,t,20); t+=20;
	}
	while (t<r);
}
#define ONSCREEN_GRAFX_RATIO 8
void onscreen_grafx_step(VIDEO_UNIT *t,BYTE b)
{
	t[0]=b&128?0:0xFFFFFF; // avoid "*t++=*t++=..." bug in GCC 4.6
	t[1]=b& 64?0:0xFFFFFF;
	t[2]=b& 32?0:0xFFFFFF;
	t[3]=b& 16?0:0xFFFFFF;
	t[4]=b&  8?0:0xFFFFFF;
	t[5]=b&  4?0:0xFFFFFF;
	t[6]=b&  2?0:0xFFFFFF;
	t[7]=b&  1?0:0xFFFFFF;
}
WORD onscreen_grafx(int q,VIDEO_UNIT *v,int ww,int mx,int my)
{
	WORD s=onscreen_grafx_addr; if (!(q&1))
	{
		int xx=0,lx=onscreen_grafx_size;
		if (lx*=8)
			do
				for (int y=0;y<my;++y)
					for (int x=0;x<lx;x+=8)
						onscreen_grafx_step(&v[xx+x+y*ww],PEEK(s)),++s;
			while ((xx+=lx)+lx<=mx);
		for (int y=0;v+=xx,y<my;v+=ww-mx,++y) // fill remainders
			for (int x=xx;x<mx;++x)
				*v++=0x808080;
	}
	else
	{
		int yy=0,ly=onscreen_grafx_size;
		if (ly)
			do
				for (int x=0;x<mx;x+=8)
					for (int y=0;y<ly;++y)
						onscreen_grafx_step(&v[x+(yy+y)*ww],PEEK(s)),++s;
			while ((yy+=ly)+ly<=my);
		v+=yy*ww;
		for (int y=yy;y<my;v+=ww-mx,++y) // fill remainders
			for (int x=0;x<mx;++x)
				*v++=0x808080;
	}
	return s;
}

// CPU: ZILOG Z80 MICROPROCESSOR ==================================== //

#define Z80_MREQ_PAGE(t,p) ( z80_t+=(z80_aux2=(t+ula_clash_mreq[p][(WORD)ula_clash_z])), ula_clash_z+=z80_aux2 )
#define Z80_IORQ_PAGE(t,p) ( z80_t+=(z80_aux2=(t+ula_clash_iorq[p][(WORD)ula_clash_z])), ula_clash_z+=z80_aux2 )
// input/output
#define Z80_SYNC() ( _t_-=z80_t, z80_sync(z80_t), z80_t=0 )
#define Z80_SYNC_IO(t) ( _t_-=z80_t-t, z80_sync(z80_t-t), z80_t=t ) // `t` sets a delay between updates
#define Z80_PRAE_RECV(w) do{ Z80_IORQ(1,w); Z80_SYNC_IO(ula_fix_out); }while(0)
#define Z80_RECV z80_recv
#define Z80_POST_RECV(w) do{ if ((w)&1) Z80_IORQ_1X_NEXT(3); else Z80_IORQ_PAGE(3,4); }while(0)
#define Z80_PRAE_SEND(w) do{ if ((w)&3) audio_dirty=1; Z80_IORQ(1,w); Z80_SYNC_IO(ula_fix_out); }while(0)
#define Z80_SEND z80_send
#define Z80_POST_SEND(w) do{ if ((w)&1) Z80_IORQ_1X_NEXT(3); else Z80_IORQ_PAGE(3,4); }while(0)
// fine timings
#define Z80_AUXILIARY int z80_aux1,z80_aux2 // they must stick between macros :-/
#define Z80_MREQ(t,w) Z80_MREQ_PAGE(t,z80_aux1=((w)>>14)) // these contended memory pauses are common to all official Sinclair/Amstrad ZX Spectrum machines
#define Z80_MREQ_1X(t,w) do{ if (t>1&&type_id<3) { z80_aux1=(w)>>14; int z80_auxx=t; do Z80_MREQ_PAGE(1,z80_aux1); while (--z80_auxx); } else Z80_MREQ(t,w); }while(0)
#define Z80_MREQ_NEXT(t) Z80_MREQ_PAGE(t,z80_aux1) // when the very last z80_aux1 is the same as the current one
#define Z80_MREQ_1X_NEXT(t) do{ if (t>1&&type_id<3) { int z80_auxx=t; do Z80_MREQ_PAGE(1,z80_aux1); while (--z80_auxx); } else Z80_MREQ_NEXT(t); }while(0)
#define Z80_IORQ(t,w) Z80_IORQ_PAGE(t,z80_aux1=((w)>>14)) // these pauses are different in Spectrum PLUS3 (and PLUS2A) because only MREQ actions are contended:
#define Z80_IORQ_1X(t,w) do{ if (t>1) { z80_aux1=(w)>>14; int z80_auxx=t; do Z80_IORQ_PAGE(1,z80_aux1); while (--z80_auxx); } else Z80_IORQ(t,w); }while(0) // Z80_IORQ is always ZERO on PLUS3!
#define Z80_IORQ_NEXT(t) Z80_IORQ_PAGE(t,z80_aux1) // when the very last z80_aux1 is the same as the current one
#define Z80_IORQ_1X_NEXT(t) do{ if (t>1) { int z80_auxx=t; do Z80_IORQ_PAGE(1,z80_aux1); while (--z80_auxx); } else Z80_IORQ_NEXT(t); }while(0) // Z80_IORQ is always ZERO on PLUS3
#define Z80_WAIT(t) ( z80_t+=t, ula_clash_z+=t )
#define Z80_WAIT_IR1X(t) do{ if (type_id<3) { if (t>1) { z80_aux1=z80_ir.w>>14; int z80_auxx=t; do Z80_MREQ_PAGE(1,z80_aux1); while (--z80_auxx); } else { Z80_MREQ(t,z80_ir.w); } } else Z80_WAIT(t); }while(0)
#define Z80_PEEK(w) ( Z80_MREQ(3,w), mmu_rom[z80_aux1][w] )
#define Z80_PEEK1 Z80_PEEK
#define Z80_PEEK2 Z80_PEEK
#define Z80_PEEKZ(w) ( Z80_MREQ(4,w), mmu_rom[z80_aux1][w] ) // slow PEEK
#define Z80_POKE(w,b) ( Z80_MREQ(3,w), mmu_ram[z80_aux1][w]=(b) ) // trappable single write
#define Z80_POKE0 Z80_POKE // untrappable single write, use with care
#define Z80_POKE1(w,b) ( Z80_MREQ(3,w), ((w)>=0X5800&&(w)<=0X5AFF&&(Z80_SYNC_IO(0))), mmu_ram[z80_aux1][w]=(b) ) // 1st twin write; NIRVANA games on PLUS3 require it
#define Z80_POKE2 Z80_POKE1 // 2nd twin write; NIRVANA games on 48K and 128K require it
#define Z80_POKE3 Z80_POKE1 // 1st twin write from PUSH rr; NIRVANA games always need it
#define Z80_POKE4 Z80_POKE0 // 2nd twin write from PUSH rr; no ATTRIB effects seem to need it
#define Z80_POKE5 Z80_POKE0 // 1st twin write from EX rr,(SP)
#define Z80_POKE6 Z80_POKE0 // 2nd twin write from EX rr,(SP)
#define Z80_BEWARE int z80_t_=z80_t,ula_clash_z_=ula_clash_z
#define Z80_REWIND z80_t=z80_t_,ula_clash_z=ula_clash_z_
// coarse timings
#define Z80_STRIDE(o)
#define Z80_STRIDE_0
#define Z80_STRIDE_1
#define Z80_STRIDE_ZZ(o)
#define Z80_STRIDE_IO(o)

#define Z80_SLEEP(t) Z80_WAIT(t)
#define Z80_HALT_STRIDE 0 // i.e. careful HALT, requires gradual emulation
#define Z80_XCF_BUG 1 // replicate the SCF/CCF quirk
#define Z80_DEBUG_MMU 0 // forbid ROM/RAM toggling, it's useless on Spectrum
#define Z80_DEBUG_EXT 0 // forbid EXTRA hardware debugging info pages
BYTE Z80_0XED71=0; // whether OUT (C) sends 0 (NMOS) or 255 (CMOS)
#define Z80_TRDOS_CATCH(r) if (trdos_mapped) { if (r.b.h>=0X40) z80_trdos_leave(); } else Z80_TRDOS_ENTER(r) // page TR-DOS in and out
#define Z80_TRDOS_ENTER(r) if (r.b.h==0X3D) z80_trdos_enter() // optimisation, TR-DOS uses a limited set of opcodes to leave
#define Z80_TRDOS_LEAVE(r) if (trdos_mapped&&r.b.h>=0X40) z80_trdos_leave() // used only when an IM2 INT takes over
void z80_trdos_enter(void) // allow entering TR-DOS only when it's available and enabled
	{ if (!(disc_disabled|trdos_mapped)&&type_id!=3&&ula_v2&16) trdos_mapped=1,mmu_update(); }
void z80_trdos_leave(void) { trdos_mapped=0,mmu_update(); } // always allow exiting TR-DOS!

#include "cpcec-z8.h"

// EMULATION CONTROL ================================================ //

char txt_error[]="Error!";
char txt_error_any_load[]="Cannot open file!";
char txt_error_bios[]="Cannot load firmware!";

// emulation setup and reset operations ----------------------------- //

void all_setup(void) // setup everything!
{
	ula_setup();
	tape_setup();
	disc_setup();
	trdos_setup();
	psg_setup();
	z80_setup();
}
void all_reset(void) // reset everything!
{
	MEMZERO(autorun_kbd);
	trdos_reset(); trdos_mapped=0;
	ula_reset();
	if (type_id<3)
		ula_v3_send(4); // disable PLUS3!
	if (!type_id)
		ula_v2_send(48); // enable 48K!
	tape_enabled=0;
	tape_reset();
	disc_reset();
	psg_reset();
	#ifdef PSG_PLAYCITY
	playcity_reset(),playcity_active=dac_level=0;
	#endif
	z80_reset();
	z80_debug_reset();
	MEMFULL(z80_tape_index); snap_done=0; // avoid accidents!
}

// firmware ROM file handling operations ---------------------------- //

BYTE old_type_id=99; // the last loaded BIOS type
char bios_system[][13]={"spectrum.rom","spec128k.rom","spec-p-2.rom","spec-p-3.rom"};
char bios_path[STRMAX]="";

int bios_wrong_dword(DWORD t) // catch fingerprints that belong to other file types; DWORD is MMMM-styled
{
	return //t==0x4D56202D|| // "MV - CPC" (floppy disc image) and "MV - SNA" (CPC Snapshot)
		//t==0x45585445|| // "EXTENDED" (advanced floppy disc image)
		t==0x5A585461|| // "ZXTape!" (advanced tape image)
		t==0x505A5854|| // "PZXT" ("perfect" tape image)
		//t==0x436F6D70|| // "Compressed Wave File" (advanced audio file)
		t==0x52494646|| // "RIFF" (WAVE audio file, CPC PLUS cartridge)
		t==0x01897FED|| // Amstrad CPC firmware (useless on Spectrum!)
		(t>=0x13000000&&t<=0x13000003); // Spectrum BASIC-BINARY header (simple tape image)
}

int bios_load(char *s) // load ROM. `s` path; 0 OK, !0 ERROR
{
	//if (globbing("*.mld",s,1)) return 1; // Dandanator [sub]cartridge!
	FILE *f=puff_fopen(s,"rb");
	if (!f) return 1;
	fseek(f,0,SEEK_END); int i=ftell(f),j=0;
	if (i>=0x4000)
		fseek(f,i-0x4000+0x0601,SEEK_SET),j=fgetmmmm(f);
	if ((i!=(1<<14)&&i!=(1<<15)&&i!=(1<<16))||j!=0xD3FE37C9) // 16/32/64k + Spectrum TAPE LOAD fingerprint
		return puff_fclose(f),1; // reject!
	fseek(f,0,SEEK_SET);
	fread1(mem_rom,sizeof(mem_rom),f);
	puff_fclose(f);
	#if 0 // fast tape hack!!
	if (equalsmmmm(&mem_rom[i-0x4000+0x0579],0x20F9CDE3))
		mem_rom[i-0x4000+0x0571+2]=1; // HACK: reduced initial tape reading delay!
	#endif
	if (i<(1<<15))
		memcpy(&mem_rom[1<<14],mem_rom,1<<14); // mirror 16k ROM up
	if (i<(1<<16))
		memcpy(&mem_rom[1<<15],mem_rom,1<<15); // mirror 32k ROM up
	old_type_id=type_id=i>(1<<14)?i>(1<<15)?3:mem_rom[0x5540]=='A'?2:1:0; // original Sinclair 128K or modified Amstrad PLUS2?
	//cprintf("Firmware %dk m%d\n",i>>10,type_id);
	if (bios_path!=s&&(char*)session_substr!=s) strcpy(bios_path,s);
	return 0;
}
int bios_path_load(char *s) // ditto, but from the base path
	{ return bios_load(strcat(strcpy(session_substr,session_path),s)); }
int bios_reload(void) // ditto, but from the current type_id
	{ return old_type_id==type_id?0:type_id>=length(bios_system)?1:bios_load(strcat(strcpy(session_substr,session_path),bios_system[type_id])); }

#ifdef Z80_DANDANATOR
int dandanator_load(char *s) // inserts Dandanator cartridge, performs several tests, removes cartridge if they fail
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1; // fail!
	BYTE t[0x1000]; fread1(t,sizeof(t),f); fseek(f,0X0601-0X4000,SEEK_END); int i=fgetmmmm(f);
	if (i!=0xD3FE37C9) fseek(f,0,SEEK_END),i=ftell(f); puff_fclose(f); // Spectrum TAPE LOAD fingerprint
	if (i<0X4000||i>(8<<16)||i&0x3FFF||bios_wrong_dword(mgetmmmm(t)))
		return 1; // ensure that the filesize is right and that the header doesn't belong to other filetypes!
	/*for (i=sizeof(t)-4;--i>=0;) if (t[i]==0xFD&&t[i+1]==0xFD&&t[i+2]==0xFD&&t[i+3]==0x71) break;
	if (i>=0)*/ for (i=8;--i>=0;) if (t[i]<0X20||t[i]>=0X80) break; // the weak ZX Dandanator heuristic relies on FAILING the strong CPC Dandanator test :-(
	if (i<0) return 1; // reject files whose first 8 characters are ZERO or ASCII text (f.e. TRDOS discs)
	dandanator_remove(); if (dandanator_insert(s)) return 1;
	if (dandanator_path!=s) strcpy(dandanator_path,s); return 0;
}
#endif

// snapshot file handling operations -------------------------------- //

char snap_pattern[]="*.sna;*.szx";
char snap_path[STRMAX]="";
int snap_extended=1; // flexible behavior (i.e. 128K-format snapshots with 48K-dumps)

#define SNAP_SAVE_Z80W(x,r) (header[x]=r.b.l,header[x+1]=r.b.h)
#if 1 // experimental
void snap_save_zxs_page(int i,FILE *f)
{
	#ifdef DEFLATE_LEVEL
	int j=huff_zlib(session_scratch,sizeof(session_scratch),&mem_ram[i<<14],1<<14);
	if (j>0&&j<0X4000) // is the compression succesful? has it actually shrunk anything?
	{
		fputiiii(0X504D4152,f); fputiiii(j+3,f); // "RAMP"
		fputii(1,f); fputc(i,f); fwrite1(session_scratch,j,f);
	}
	else
	#endif
	{
		fputiiii(0X504D4152,f); fputiiii(0X4000+3,f); // "RAMP"
		fputii(0,f); fputc(i,f); fwrite1(&mem_ram[i<<14],1<<14,f);
	}
}
#endif
int snap_save(char *s) // save a snapshot. `s` path, NULL to resave; 0 OK, !0 ERROR
{
	FILE *f; if (globbing("*.sna",s,1))
	{
		if (!(f=puff_fopen(s,"wb"))) return 1;
		int i=z80_sp.w; BYTE header[27];
		Z80_PEEK_HALTED_PC;
		if (!(snap_extended|type_id)) // strict and 48K machine?
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
		fwrite1(&mem_ram[5<<14],1<<14,f); // bank 5
		fwrite1(&mem_ram[2<<14],1<<14,f); // bank 2
		fwrite1(&mem_ram[(ula_v2&7)<<14],1<<14,f); // active bank
		if (snap_extended|type_id) // relaxed or 128K machine?
		{
			SNAP_SAVE_Z80W(0,z80_pc);
			header[2]=ula_v2;
			#ifdef Z80_DANDANATOR
			if (mem_dandanator&&dandanator_cfg[4]<32) // the active Dandanator overrides PLUS3 special memory modes!!
				header[3]=dandanator_cfg[4]+32; // DANDANATOR: 64|DANDANATOR_ROM_PAGE
			else
			#endif
				// using this byte for extra config is unreliable; most emus write zero here :-(
				header[3]=type_id==3?(ula_v3&15)+16:trdos_mapped; // PLUS3: 16|[1FFD]
			fwrite1(header,4,f);
			if (!snap_extended||!(ula_v2&32)) // strict or 128K mode?
				for (i=0;i<8;++i)
					if (i!=5&&i!=2&&i!=(ula_v2&7))
						fwrite1(&mem_ram[i<<14],1<<14,f); // save all remaining banks
		}
		Z80_POKE_HALTED_PC;
	}
	#if 1 // experimental
	else if (globbing("*.szx",s,1))
	{
		if (!(f=puff_fopen(s,"wb"))) return 1;
		fputiiii(0X5453585A,f); // "ZXST"
		fputii(0X0101,f); fputii(!type_id?!ula_sixteen:type_id==3?5:ula_pentagon?7:1+type_id,f);
		fputiiii(0X52545243,f); fputiiii(36,f); // "CRTR"
		BYTE t[36]=MY_CAPTION " " MY_VERSION; fwrite1(t,36,f);
		fputiiii(0X5230385A,f); fputiiii(0X25,f); // "Z80R"
		fputii(z80_af.w,f);
		fputii(z80_bc.w,f);
		fputii(z80_de.w,f);
		fputii(z80_hl.w,f);
		fputii(z80_af2.w,f);
		fputii(z80_bc2.w,f);
		fputii(z80_de2.w,f);
		fputii(z80_hl2.w,f);
		fputii(z80_ix.w,f);
		fputii(z80_iy.w,f);
		fputii(z80_sp.w,f);
		Z80_PEEK_HALTED_PC;
		fputii(z80_pc.w,f);
		Z80_POKE_HALTED_PC;
		fputc(z80_ir.b.h,f);
		fputc(z80_ir.b.l,f);
		fputii((z80_iff.b.l&1)?257:0,f);
		fputc(z80_imd,f);
		fputiiii(ula_clash_z,f); // cycle count; unused?
		fputiiii(((BYTE)z80_irq)+(z80_wz<<16),f); // extra flags; unused?
		fputiiii(0X52435053,f); fputiiii(8,f); // "SPCR"
		fputc(ula_v1&7,f);
		fputc(type_id?ula_v2:0,f);
		fputc(type_id==3?ula_v3:0,f);
		fputc(ula_v1,f);
		fputiiii(0,f); // extra flags
		if (type_id&&(snap_extended||!(ula_v2&32))) // 128K?
			for (int i=0;i<8;++i)
				snap_save_zxs_page(i,f);
		else // 48K
		{
			snap_save_zxs_page(5,f);
			if (type_id||!ula_sixteen) // skip last 32K on a 16K machine
				snap_save_zxs_page(2,f),snap_save_zxs_page(0,f);
		}
		if (type_id||!psg_disabled) // AY chip?
		{
			fputiiii(0X00005941,f),fputiiii(1+1+16,f); // "AY\000\000"
			fputc(2,f); // ZXSTAYF_128AY
			fputc(psg_index,f);
			fwrite1(psg_table,16,f);
		}
		if (ulaplus_enabled&&ulaplus_table[64]) // ULAPLUS?
		{
			fputiiii(0X54544C50,f),fputiiii(1+1+64,f); // "PLTT"
			fputc(ulaplus_table[64],f);
			fputc(ulaplus_index,f);
			fwrite1(ulaplus_table,64,f);
		}
		if (!dac_disabled&&dac_level)
			fputiiii(0X58564F43,f),fputiiii(4,f),fputiiii(0,f); // "COVX"
	}
	#endif
	#if 0 // the SP format is useless outside 48K :-(
	else if (globbing("*.sp",s,1)) // "SP"
	{
		fputii(0X5053,f);
		fputii(!type_id&&ula_sixteen?0X4000:0XC000,f);
		fputii(0X4000,f);
		fputii(z80_bc .w,f);
		fputii(z80_de .w,f);
		fputii(z80_hl .w,f);
		fputii(z80_af .w,f);
		fputii(z80_ix .w,f);
		fputii(z80_iy .w,f);
		fputii(z80_bc2.w,f);
		fputii(z80_de2.w,f);
		fputii(z80_hl2.w,f);
		fputii(z80_af2.w,f);
		fputii(z80_ir .w,f);
		fputii(z80_sp .w,f);
		Z80_PEEK_HALTED_PC;
		fputii(z80_pc .w,f);
		Z80_POKE_HALTED_PC;
		fputii(ula_v1&7,f);
		fputii((z80_imd&2)+(z80_iff.b.l&1),f);
		fwrite1(&mem_ram[5<<14],1<<14,f); // first 16K
		if (type_id||!ula_sixteen)
			fwrite1(&mem_ram[2<<14],1<<14,f),
			fwrite1(&mem_ram[0<<14],1<<14,f); // last 32K
	}
	#endif
	else
		return 1; // unsupported snapshot type!
	if (s!=snap_path)
		strcpy(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

#define SNAP_LOAD_Z80W(x,r) (r.b.l=header[x],r.b.h=header[x+1])
int snap_load_z80_page(BYTE *t,int o,BYTE *s,int i) // unpack a Z80-type compressed block; 0 OK, !0 ERROR
{
	int x,y; while (i>0&&o>0)
		if (--i,(x=*s++)==0xED)
		{
			if (--i,(y=*s++)==0xED)
			{
				i-=2;x=*s++; // can this be zero!?
				for (y=*s++,o-=x;x;--x)
					*t++=y;
			}
			else
				o-=2,*t++=x,*t++=y;
		}
		else
			--o,*t++=x;
	return i|o;
}
int snap_load(char *s) // load a snapshot. `s` path, NULL to reload; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb");
	if (!f) return 1;
	BYTE header[96]; int i,q,prev_id=type_id;
	if (fread1(header,8,f)!=8||equalsmmmm(header,0x4D56202D)) // truncated file? disc image or CPC snapshot ("MV -"...)?
		return puff_fclose(f),1;
	if (equalsmmmm(header,0X5A585354)) // SZX format; it has its own ID, "ZXST"
	{
		cprintf("SZX snapshot: "); if (!snap_extended) ula_pentagon=0;
		ula_v3=4,ula_v2=48; // disable PLUS3 and 128K by default
		type_id=header[6]<2?0:header[6]<3?1:header[6]<4?2:header[6]<7?3:(ula_pentagon=1); // chMachineId
		ula_sixteen=!header[6]; // just one way to set the Spectrum 16K mode from this snapshot
		while ((i=fgetiiii(f))>0&&(q=fgetiiii(f))>=0)
		{
			cprintf("%08X:%04X ",i,q);
			if (i==0X5230385A) // "Z80R", the Z80 registers
			{
				fread1(header,29,f);
				SNAP_LOAD_Z80W( 0,z80_af);
				SNAP_LOAD_Z80W( 2,z80_bc);
				SNAP_LOAD_Z80W( 4,z80_de);
				SNAP_LOAD_Z80W( 6,z80_hl);
				SNAP_LOAD_Z80W( 8,z80_af2);
				SNAP_LOAD_Z80W(10,z80_bc2);
				SNAP_LOAD_Z80W(12,z80_de2);
				SNAP_LOAD_Z80W(14,z80_hl2);
				SNAP_LOAD_Z80W(16,z80_ix);
				SNAP_LOAD_Z80W(18,z80_iy);
				SNAP_LOAD_Z80W(20,z80_sp);
				SNAP_LOAD_Z80W(22,z80_pc);
				z80_ir.b.h=header[24]; // mind the byte order
				z80_ir.b.l=header[25]; // ditto
				SNAP_LOAD_Z80W(26,z80_iff);
				z80_imd=header[28]&3;
				// notice we aren't checking the T and flags :-/
				q-=29;
			}
			else if (i==0X52435053) // "SPCR", the ULA config
			{
				fread1(header,5,f);
				ula_v1_send(header[0]&7);
				if (type_id)
				{
					ula_v2=header[1]; // we call mmu_update() later, so no ula_v2_send() here
					if (type_id==3)
						ula_v3=header[2];
				}
				q-=5;
			}
			else if (i==0X504D4152&&q<0X8000) // "RAMP", the RAM contents
			{
				fread1(header,3,f);
				BYTE *t=&mem_ram[(header[2]&7)<<14]; // 128K max!
				if (header[0]&1)
				{
					fread1(mem_16k,q-3,f); // temp.buffer
					if (q=puff_zlib(t,0X4000,mem_16k,q-3))
					{
						cprintf("ERROR %08X/%08X:%08X/%08X! ",puff_srco,puff_srcl,puff_tgto,puff_tgtl);
						break;
					} //else q=0;
				}
				else
				{
					fread1(t,0X4000,f);
					q-=3+0X4000;
				}
			}
			else if (i==0X00005941) // "AY\000\000", the AY chip
			{
				fgetc(f);
				psg_table_select(fgetc(f));
				fread1(psg_table,16,f);
				q-=1+1+16;
			}
			else if (i==0X54544C50) // "PLTT", the ULAPLUS palette
			{
				ulaplus_table[64]=fgetc(f); // configuration
				ulaplus_index=fgetc(f); if (ulaplus_index>64) ulaplus_index=64; // palette index
				fread1(ulaplus_table,64,f); // palette
				ulaplus_enabled=1; video_clut_update();
				q-=1+1+64;
			}
			else if (i==0X58564F43) // "COVX", the Covox $FB DAC
				playcity_disabled=1,playcity_reset(),playcity_active=dac_disabled=0;
			#if 0 // useless?
			else if (i==0X00594F4A) // "JOY\000", the joysticks
			{
				fgetiiii(f);
				switch (fgetc(f))
				{
					case 0: joy1_type=0; break; // KEMPSTON
					case 2: joy1_type=3; break; // CURSOR
					case 3: joy1_type=1; break; // SINCLAIR1
					case 4: joy1_type=2; break; // SINCLAIR2
				}
				q-=5;
			}
			#endif
			fseek(f,q,SEEK_CUR); // skip remainders and go to next block
		}
		cprintf("EOF\n");
	}
	#if 0 // not too useful nowadays :-(
	else if (!strcmp("SP",header)&&header[5]==64) // SP format, it has its own ID
	{
		fread1(&header[8],38-8,f);
		SNAP_LOAD_Z80W( 6,z80_bc);
		SNAP_LOAD_Z80W( 8,z80_de);
		SNAP_LOAD_Z80W(10,z80_hl);
		SNAP_LOAD_Z80W(12,z80_af);
		SNAP_LOAD_Z80W(14,z80_ix);
		SNAP_LOAD_Z80W(16,z80_iy);
		SNAP_LOAD_Z80W(18,z80_bc2);
		SNAP_LOAD_Z80W(20,z80_de2);
		SNAP_LOAD_Z80W(22,z80_hl2);
		SNAP_LOAD_Z80W(24,z80_af2);
		SNAP_LOAD_Z80W(26,z80_ir);
		SNAP_LOAD_Z80W(28,z80_sp);
		SNAP_LOAD_Z80W(30,z80_pc);
		ula_v1_send(header[34]&7);
		z80_iff.w=header[36]&1?257:0;
		z80_imd=header[36]&2?2:1;
		fread1(&mem_ram[5<<14],1<<14,f);
		fread1(&mem_ram[2<<14],1<<14,f);
		fread1(&mem_ram[0<<14],1<<14,f);
		type_id=0,ula_sixteen=header[3]<128,ula_v3=4,ula_v2=48; // 16K or 48K, never 128K :-(
	}
	#endif
	else if (globbing("*.z80",s,1)) // Z80 format files are defined by extension -- no magic numbers at all!
	{
		cprintf("Z80 snapshot: "); if (!snap_extended) ula_pentagon=0;
		type_id=ula_sixteen=0,ula_v3=4,ula_v2=48; // set 48K and disable PLUS3 and 128K by default
		fread1(&header[8],30-8,f);
		z80_af.b.h=header[0];
		z80_af.b.l=header[1];
		SNAP_LOAD_Z80W(2,z80_bc);
		SNAP_LOAD_Z80W(4,z80_hl);
		SNAP_LOAD_Z80W(6,z80_pc);
		SNAP_LOAD_Z80W(8,z80_sp);
		z80_ir.b.h=header[10];
		z80_ir.b.l=header[11]&127;
		if (header[12]&1)
			z80_ir.b.l+=-128;
		ula_v1_send((header[12]>>1)&7);
		SNAP_LOAD_Z80W(13,z80_de);
		SNAP_LOAD_Z80W(15,z80_bc2);
		SNAP_LOAD_Z80W(17,z80_de2);
		SNAP_LOAD_Z80W(19,z80_hl2);
		z80_af2.b.h=header[21];
		z80_af2.b.l=header[22];
		SNAP_LOAD_Z80W(23,z80_iy);
		SNAP_LOAD_Z80W(25,z80_ix);
		z80_iff.w=header[27]&1?257:0;
		z80_imd=header[29]&3;
		if (header[12]!=255&&!z80_pc.w) // HEADER V2+
		{
			i=fgetii(f); cprintf("+%d ",i);
			if (i<=64)
				fread1(&header[32],i,f);
			else
				fread1(&header[32],64,f),fseek(f,i-64,SEEK_CUR); // ignore extra data
			SNAP_LOAD_Z80W(32,z80_pc);
			if (header[34]>=3) // 128K system?
			{
				ula_v2=header[35]&63; // 128K/PLUS2 configuration
				psg_table_select(header[38]);
				MEMLOAD(psg_table,&header[39]);
				if (i>=55&&header[34]==7)
				{
					type_id=3,ula_v3=header[86]&15; // PLUS3
					if (header[37]&128) disc_disabled=1; // PLUS2A
				}
				else if (i>=54&&header[34]==9)
					type_id=1,ula_pentagon=1; // Pentagon 128K
				else if (header[37]&128)
					type_id=2; // PLUS2
				else
					type_id=1; // 128K
			}
			else if (header[37]&128) // 48K system!
				ula_sixteen=1; // 16K
			while ((i=fgetii(f))&&(q=fgetc(f))>=0) // q<0 = feof
			{
				cprintf("%05d:%03d ",i,q);
				if ((q-=3)<0||q>7) // ignore ROM copies
					q=8; // dummy 16K
				else if (!type_id) // 128K banks follow the normal order,
					switch (q) // but 48K banks are stored differently
					{
						case 5:
							//q=5;
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
				else
				{
					fread1(&mem_ram[8<<14],i,f); if (q!=8) // don't unpack dummy 16K
						if (snap_load_z80_page(&mem_ram[q<<14],1<<14,&mem_ram[8<<14],i))
							cprintf("V2+ ERROR! ");
				}
			}
		}
		else // HEADER V1
		{
			if (header[12]&32) // compressed 48K body
			{
				i=fread1(mem_ram,5<<14,f); cprintf("%05d:48K ",i);
				// ignore errors, the V1 compression end marker is flawed
				snap_load_z80_page(&mem_ram[5<<14],3<<14,mem_ram,i-4);
				//memcpy(&mem_ram[5<<14],&mem_ram[5<<14],1<<14);
				memcpy(&mem_ram[2<<14],&mem_ram[6<<14],1<<14);
				memcpy(&mem_ram[0<<14],&mem_ram[7<<14],1<<14);
			}
			else // uncompressed 48K body
			{
				fread1(&mem_ram[5<<14],1<<14,f); // bank 5
				fread1(&mem_ram[2<<14],1<<14,f); // bank 2
				fread1(&mem_ram[0<<14],1<<14,f); // bank 0
			}
		}
		cprintf("PC=$%04X\n",z80_pc.w);
	}
	else if (globbing("*.sna",s,1)) // SNA files can be detected by extension, more reliable than filesize (49179,49183,131103,147487)
	{
		type_id=ula_sixteen=0,ula_v3=4,ula_v2=48; // set 48K and disable PLUS3 and 128K, as usual
		fread1(&header[8],27-8,f);
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
		z80_iff.w=(header[19]&4)?257:0;
		z80_ir.b.l=header[20];
		SNAP_LOAD_Z80W(21,z80_af);
		SNAP_LOAD_Z80W(23,z80_sp);
		z80_imd=header[25]&3;
		ula_v1_send(header[26]&7);
		fread1(&mem_ram[5<<14],1<<14,f); // bank 5
		fread1(&mem_ram[2<<14],1<<14,f); // bank 2
		fread1(&mem_ram[0<<14],1<<14,f); // bank 0
		#ifdef Z80_DANDANATOR
		dandanator_cfg[4]=32; dandanator_cfg[5]=dandanator_cfg[6]=dandanator_cfg[7]=0;
		#endif
		if (fread1(header,4,f)==4) // new snapshot type, 128K-ready?
		{
			SNAP_LOAD_Z80W(0,z80_pc);
			if (!(header[2]&32)) // 128K snapshot?
			{
				#ifdef Z80_DANDANATOR
				if (header[3]>=32) // Dandanator snapshot?
				{
					dandanator_cfg[4]=header[3]-32;
					if (!mem_dandanator&&*dandanator_path)
						dandanator_load(dandanator_path); // insert last known Dandanator file
				}
				else
				#endif
				if (header[3]>=16) // PLUS3 snapshot?
					type_id=prev_id=3,ula_v3=header[3]&15; // *force* upgrade to PLUS3!
				else // 128K/PLUS2 snapshot
					trdos_mapped=header[3]&1,type_id=prev_id=prev_id<3?prev_id:2; // downgrade PLUS3
			}
			ula_v2=header[2]&63; // set 128K mode
			MEMNCPY(&mem_ram[(ula_v2&7)<<14],&mem_ram[0x00000],1<<14); // move bank 0 to active bank
			if (!snap_extended||!(ula_v2&32)) // flexible mode won't read more datas on 48K mode
				for (i=0;i<8;++i)
					if (i!=5&&i!=2&&i!=(ula_v2&7))
						if (fread1(&mem_ram[i<<14],1<<14,f)) // load following banks, if any
							if (prev_id<1) type_id=prev_id=1; else type_id=prev_id; // extra ram, *force* 128K if 48K!
		}
		else // old snapshot type, 48K only: perform a RET
		{
			mmu_update(); // necessary before any PEEK
			z80_pc.b.l=PEEK(z80_sp.w); ++z80_sp.w;
			z80_pc.b.h=PEEK(z80_sp.w); ++z80_sp.w;
			//for (i=8;i<11;++i) psg_table_select(i),psg_table_send(0); // mute channels
		}
	}
	else // unknown type
		return puff_fclose(f),1;
	if (snap_extended&&type_id<prev_id) type_id=prev_id; // don't "downgrade" on flexible mode
	bios_reload(); // reload BIOS if required!
	z80_irq=z80_active=0; // avoid nasty surprises!
	ula_update(),mmu_update(); // adjust RAM and ULA models
	psg_all_update();
	z80_debug_reset();
	MEMFULL(z80_tape_index); // clean tape trap cache to avoid false positives
	if (snap_path!=s) strcpy(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

// "autorun" file and logic operations ------------------------------ //

int any_load(char *s,int q) // load a file regardless of format. `s` path, `q` autorun; 0 OK, !0 ERROR
{
	autorun_mode=0; // cancel any autoloading yet
	if (snap_load(s))
	{
		#ifdef Z80_DANDANATOR
		if (dandanator_load(s))
		{
		#endif
			if (bios_load(s))
			{
				if (tape_open(s))
				{
					if (disc_open(s,0,0))
					{
						if (trdos_open(s,0,0))
							return 1; // everything failed!
						if (q)
						{
							type_id=!type_id?1:type_id!=3?type_id:2; // avoid 48K and reject PLUS3
							disc_disabled=0,disc_closeall(),tape_close();
						}
					}
					else if (q)
						type_id=3,disc_disabled=0,tape_close(),trdos_closeall(); // open disc? force PLUS3, enable disc, close tapes and TR-DOS!
				}
				else if (q)
					type_id=(type_id==3?2:type_id),disc_disabled|=2,disc_closeall(),trdos_closeall(); // open tape? force PLUS2 if PLUS3, close disc!
				if (q) // autorun for tape and disc
				{
					dandanator_remove(),all_reset(),bios_reload();
					if (trdos_ram[0]) // TR-DOS lacks a precise boot procedure, it needs some hacking :-(
					{
						ula_v2_send(0X10); // force USR0 mode
						autorun_mode=2; autorun_t=96; // USR0 always needs typing
					}
					else
					{
						if (type_id&&mem_rom[0X8020]==0XEF) // detect machines without a main menu
							autorun_mode=3,autorun_t=type_id==3?149:75; // menu-based 128K machines; PLUS3 is very slow
						else
							autorun_mode=1,autorun_t=99; // 48K and menu-less 128K machines, f.e. OPENSE with 128K stub
					}
				}
			}
			else // load bios? reset!
				dandanator_remove(),all_reset(),disc_disabled&=~2;//,tape_close(),disc_closeall(),trdos_closeall();
		#ifdef Z80_DANDANATOR
		}
		else // dandanator? reset!
			all_reset(),disc_disabled&=~2;
		#endif
	}
	if (autorun_path!=s&&q) strcpy(autorun_path,s);
	return 0;
}

// auxiliary user interface operations ------------------------------ //

#define MAIN_FRAMESKIP_MASK ((1<<MAIN_FRAMESKIP_BITS)-1)
char txt_error_snap_save[]="Cannot save snapshot!";
char file_pattern[]="*.sna;*.szx;*.z80;*.rom;*.mld;*.dsk;*.trd;*.scl;*.tap;*.tzx;*.pzx;*.csw;*.wav";

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
	"0x0701 Flip disc sides in A:\n"
	"0x0700 Remove disc from A:\tCtrl+F7\n"
	"0xC700 Insert disc into B:..\tShift+F7\n"
	"0xC701 Create disc in B:..\n"
	"0x4701 Flip disc sides in B:\n"
	"0x4700 Remove disc from B:\tCtrl+Shift+F7\n"
	"=\n"
	"0x8800 Insert tape..\tF8\n"
	"0xC800 Record tape..\tShift+F8\n"
	"0x8801 Browse tape..\n"
	"0x0800 Remove tape\tCtrl+F8\n"
	"0x4800 Play tape\tCtrl+Shift+F8\n"
	"=\n"
	"0x0080 E_xit\n"
	"Machine\n"
	"0x8500 Select firmware..\tF5\n"
	"0x0500 Reset emulation\tCtrl+F5\n"
	"0x8F00 Pause\tPause\n"
	"0x8900 Debug\tF9\n"
	//"0x8910 NMI\n"
	"=\n"
	"0xC600 Pentagon timings\tShift+F6\n"
	"0x8513 64-colour ULAplus\n"
	"0x8514 Spectrum 16K mode\n"
	"0x8512 Issue-2 ULA line\n"
	"0x8511 ULA video noise\n"
	"0x8515 NMOS-style Z80\n"
	"=\n"
	//"0x0600 Raise Z80 speed\tCtrl+F6\n"
	//"0x4600 Lower Z80 speed\tCtrl+Shift+F6\n"
	"0x0601 1x CPU clock\n"
	"0x0602 2x CPU clock\n"
	"0x0603 3x CPU clock\n"
	"0x0604 4x CPU clock\n"
	"0x8517 AY-Melodik audio\n"
	#ifdef PSG_PLAYCITY
	"0x8518 Turbosound audio\n"
	"0x8519 Covox $FB audio\n"
	#endif
	#ifdef Z80_DANDANATOR
	"0xC500 Insert Dandanator..\tShift+F5\n"
	"0x4500 Remove Dandanator\tCtrl+Shift+F5\n"
	"0x0510 Writeable Dandanator\n"
	#endif
	"Settings\n"
	"0x8601 1x realtime speed\n"
	"0x8602 2x realtime speed\n"
	"0x8603 3x realtime speed\n"
	"0x8604 4x realtime speed\n"
	"0x8600 Run at full throttle\tF6\n"
	"=\n"
	"0x0400 Virtual joystick\tCtrl+F4\n"
	"0x8521 Kempston joystick\n"
	"0x8522 Sinclair 1 joystick\n"
	"0x8523 Sinclair 2 joystick\n"
	"0x8524 Cursor/AGF joystick\n"
	"0x8525 QAOP+Space joystick\n"
	"0x8526 Gunstick (MHT)\n"
	"=\n"
	"0x852F Printer output..\n"
	"0x851F Strict SNA files\n"
	"0x8510 Disc controller\n"
	"0x8590 Strict disc writes\n"
	"0x8591 Read-only disc by default\n"
	//"=\n"
	"0x0900 Tape speed-up\tCtrl+F9\n"
	"0x4900 Tape analysis\tCtrl+Shift+F9\n"
	//"0x4901 Tape analysis+cheating\n"
	"0x0901 Tape auto-rewind\n"
	"Video\n"
	"0x8A00 Full screen\tAlt+Return\n"
	"0x8A02 Video acceleration*\n"
	"0x8A01 Zoom to integer\n"
	"=\n"
	"0x8901 Onscreen status\tShift+F9\n"
	"0x8904 Pixel filtering\n"
	"0x8903 X-masking\n"
	"0x8902 Y-masking\n"
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
	"0x0B08 Blend scanlines\n"
	"=\n"
	"0x9100 Raise frameskip\tNum.+\n"
	"0x9200 Lower frameskip\tNum.-\n"
	"0x9300 Full frameskip\tNum.*\n"
	"0x9400 No frameskip\tNum./\n"
	"=\n"
	"0x8C00 Save BMP screenshot\tF12\n"
	"0xCC00 Record XRF film\tShift+F12\n"
	"0x8C01 High resolution\n"
	"0x8C02 High framerate\n"
	"Audio\n"
	"0x8400 Sound playback\tF4\n"
	"0x8A04 Audio acceleration*\n"
	#if AUDIO_CHANNELS > 1
	"0xC401 0% stereo\n"
	"0xC404 25% stereo\n"
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

void session_clean(void) // refresh options
{
	session_menucheck(0x8F00,session_signal&SESSION_SIGNAL_PAUSE);
	session_menucheck(0x8900,session_signal&SESSION_SIGNAL_DEBUG);
	session_menucheck(0x8400,!(audio_disabled&1));
	session_menuradio(0x8401+audio_filter,0x8401,0x8404);
	session_menuradio((session_fast&1)?0x8600:0x8601+session_rhythm,0x8600,0x8604);
	session_menucheck(0x8C01,!session_filmscale);
	session_menucheck(0x8C02,!session_filmtimer);
	session_menucheck(0xCC00,(size_t)session_filmfile);
	session_menucheck(0x0C00,(size_t)session_wavefile);
	session_menucheck(0x4C00,(size_t)psg_logfile);
	session_menucheck(0x8700,type_id!=3?(size_t)trdos_ram[0]:(size_t)disc[0]);
	session_menucheck(0xC700,type_id!=3?(size_t)trdos_ram[1]:(size_t)disc[1]);
	session_menucheck(0x0701,disc_flip[0]);
	session_menucheck(0x4701,disc_flip[1]);
	session_menucheck(0x8800,tape_type>=0&&tape);
	session_menucheck(0xC800,tape_type<0&&tape);
	session_menucheck(0x4800,tape_enabled<0);
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	//session_menucheck(0x4901,tape_fastfeed);
	session_menucheck(0x0400,session_key2joy);
	session_menuradio(0x0601+multi_t-1,0x0601,0x0604);
	session_menuradio(0x8521+joy1_type,0x8521,0x8525);
	session_menucheck(0x8526,litegun);
	session_menucheck(0x8590,!(disc_filemode&2));
	session_menucheck(0x8591,disc_filemode&1);
	session_menucheck(0x8510,!(disc_disabled&1));
	session_menucheck(0xC600,ula_pentagon);
	session_menucheck(0x8511,!(ula_snow_disabled));
	session_menucheck(0x8512,ula_v1_issue!=ULA_V1_ISSUE3);
	session_menucheck(0x8513,ulaplus_enabled);
	session_menucheck(0x8514,ula_sixteen);
	session_menucheck(0x8515,!Z80_0XED71);
	session_menucheck(0x8517,!psg_disabled);
	#ifdef Z80_DANDANATOR
	session_menucheck(0xC500,(size_t)mem_dandanator);
	session_menucheck(0x0510,dandanator_canwrite);
	#endif
	#ifdef PSG_PLAYCITY
	session_menucheck(0x8518,!playcity_disabled);
	session_menucheck(0x8519,!dac_disabled);
	#endif
	session_menucheck(0x851F,!(snap_extended));
	session_menucheck(0x852F,(size_t)printer);
	session_menucheck(0x8901,onscreen_flag);
	session_menucheck(0x8A00,session_fullscreen);
	session_menucheck(0x8A01,session_intzoom);
	session_menucheck(0x8A02,!session_softblit);
	session_menucheck(0x8A04,!session_softplay);
	session_menuradio(0x8B01+video_type,0x8B01,0x8B05);
	session_menuradio(0x0B01+video_scanline,0x0B01,0x0B04);
	session_menucheck(0x0B08,video_scanblend);
	session_menucheck(0x8902,video_filter&VIDEO_FILTER_MASK_Y);
	session_menucheck(0x8903,video_filter&VIDEO_FILTER_MASK_X);
	session_menucheck(0x8904,video_filter&VIDEO_FILTER_MASK_Z);
	MEMLOAD(kbd_joy,joy1_types[joy1_type]);
	#if AUDIO_CHANNELS > 1
	session_menuradio(0xC401+audio_mixmode,0xC401,0xC404);
	for (int i=0;i<3;++i)
	{
		int j=i?!psg_disabled?3-i:i:0; // i.e. swaps B and C if AY-Melodik is enabled
		psg_stereo[i][0]=256+psg_stereos[audio_mixmode][j],psg_stereo[i][1]=256-psg_stereos[audio_mixmode][j];
		#ifdef PSG_PLAYCITY
		playcity_stereo[0][i][0]=psg_stereo[i][0]<<1,playcity_stereo[0][i][1]=psg_stereo[i][1]<<1; // TURBO SOUND chip shares stereo with the PSG
		#endif
	}
	#endif
	video_resetscanline(); // video scanline cfg
	sprintf(session_info,"%d:%s %s %0.1fMHz"//" | disc %s | tape %s | %s"
		,(ula_v2&32)?ula_sixteen&&!type_id?16:48:128,type_id?(type_id!=1?(type_id!=2?(!disc_disabled?"Plus3":"Plus2A"):"Plus2"):"128K"):"48K",
		ula_pentagon?"Pentagon":type_id?type_id>2?"ULAv3":"ULAv2":"ULAv1",3.5*multi_t);
	video_lastscanline=video_table[video_type][0]; // BLACK in the CLUT
	video_halfscanline=VIDEO_FILTER_SCAN(video_table[video_type][15],video_lastscanline); // WHITE in the CLUT
	*debug_buffer=128; // force debug redraw! (somewhat overkill)
}
void session_user(int k) // handle the user's commands
{
	char *s; switch (k)
	{
		case 0x8100: // F1: HELP..
			/*if (session_shift)
			{
				if (playcity_disabled=!playcity_disabled)
					dac_disabled=0,playcity_reset(),playcity_active=0;
				else
					dac_disabled=1,dac_level=0;
			}
			else*/
			session_message(
				"F1\tHelp..\t" MESSAGEBOX_WIDETAB
				"^F1\tAbout..\t"
				"\n"
				"F2\tSave snapshot.." MESSAGEBOX_WIDETAB
				"^F2\tSave last snapshot"
				"\n"
				"F3\tLoad any file.." MESSAGEBOX_WIDETAB
				"^F3\tLoad last snapshot"
				"\n"
				"F4\tToggle sound" MESSAGEBOX_WIDETAB
				"^F4\tToggle joystick"
				//"\n"
				//"\t(shift: ..stereo)"
				"\n"
				"F5\tLoad firmware.." MESSAGEBOX_WIDETAB
				"^F5\tReset emulation"
				"\n"
				#ifdef Z80_DANDANATOR
				"\t(shift: load Dntr..)" "\t"
				"\t(shift: eject Dntr)"
				"\n"
				#endif
				"F6\tToggle realtime" MESSAGEBOX_WIDETAB
				"^F6\tNext CPU speed"
				"\n"
				"\t(shift: Pentagon)" "\t"
				"\t(shift: previous..)"
				"\n"
				"F7\tInsert disc into A:..\t"
				"^F7\tEject disc from A:"
				"\n"
				"\t(shift: ..into B:)\t"
				"\t(shift: ..from B:)"
				"\n"
				"F8\tInsert tape.." MESSAGEBOX_WIDETAB
				"^F8\tRemove tape"
				"\n"
				"\t(shift: record..)\t"
				"\t(shift: play/stop)\t"
				"\n"
				"F9\tDebug\t" MESSAGEBOX_WIDETAB
				"^F9\tToggle fast tape"
				"\n"
				"\t(shift: view status)\t"
				"\t(shift: ..fast load)"
				"\n"
				"F10\tMenu"
				"\n"
				"F11\tNext palette" MESSAGEBOX_WIDETAB
				"^F11\tNext scanlines"
				"\n"
				"\t(shift: previous..)\t"
				"\t(shift: ..filter)"
				"\n"
				"F12\tSave screenshot" MESSAGEBOX_WIDETAB
				"^F12\tRecord wavefile"
				"\n"
				"\t(shift: record film)\t"
				"\t(shift: ..YM file)"
				"\n"
				"\n"
				"Num.+\tRaise frameskip" MESSAGEBOX_WIDETAB
				"Num.*\tFull frameskip"
				"\n"
				"Num.-\tLower frameskip" MESSAGEBOX_WIDETAB
				"Num./\tNo frameskip"
				"\n"
				"Pause\tPause/continue" MESSAGEBOX_WIDETAB
				"*Return\tMaximize/restore" "\t"
				"\n"
			,"Help");
			break;
		case 0x0100: // ^F1: ABOUT..
			session_aboutme(
				"ZX Spectrum emulator written by Cesar Nicolas-Gonzalez\n"
				"for the UNED 2019 Master's Degree in Computer Engineering.\n"
				"\nhome page, news and updates: http://cngsoft.no-ip.org/cpcec.htm"
				"\n\n" MY_LICENSE "\n\n" GPL_3_INFO
			,session_caption);
			break;
		case 0x8200: // F2: SAVE SNAPSHOT..
			if (s=puff_session_newfile(snap_path,snap_pattern,"Save snapshot"))
			{
				if (snap_save(s))
					session_message(txt_error_snap_save,txt_error);
				else if (autorun_path!=s) strcpy(autorun_path,s);
			}
			break;
		case 0x0200: // ^F2: RESAVE SNAPSHOT
			if (snap_done&&*snap_path)
				if (snap_save(snap_path))
					session_message(txt_error_snap_save,txt_error);
			break;
		case 0x8300: // F3: LOAD ANY FILE.. // LOAD SNAPSHOT..
			if (puff_session_getfile(session_shift?snap_path:autorun_path,session_shift?snap_pattern:file_pattern,session_shift?"Load snapshot":"Load file"))
		case 0x8000: // DRAG AND DROP
			{
				if (multiglobbing(puff_pattern,session_parmtr,1))
					if (!puff_session_zipdialog(session_parmtr,file_pattern,"Load file"))
						break;
				if (any_load(session_parmtr,!session_shift))
					session_message(txt_error_any_load,txt_error);
				else strcpy(autorun_path,session_parmtr);
			}
			break;
		case 0x0300: // ^F3: RELOAD SNAPSHOT
			if (*snap_path)
				if (snap_load(snap_path))
					session_message("Cannot load snapshot!",txt_error);
			break;
		case 0x8400: // F4: TOGGLE SOUND
			if (session_audio)
			{
				/*
				#if AUDIO_CHANNELS > 1
				if (session_shift) // TOGGLE STEREO
					audio_mixmode=(audio_mixmode+1)&3;
				else
				#endif
				*/
					audio_disabled^=1;
			}
			break;
		case 0x8401:
		case 0x8402:
		case 0x8403:
		case 0x8404:
			#if AUDIO_CHANNELS > 1
			if (session_shift) // TOGGLE STEREO
				audio_mixmode=k-0x8401;
			else
			#endif
			audio_filter=k-0x8401;
			break;
		case 0x0400: // ^F4: TOGGLE JOYSTICK
			session_key2joy=!session_key2joy;
			break;
		case 0x8521: // KEMPSTON JOYSTICK
		case 0x8522: // SINCLAIR 1 STICK
		case 0x8523: // SINCLAIR 2 STICK
		case 0x8524: // CURSOR JOYSTICK
		case 0x8525: // QAOP+M JOYSTICK
			joy1_type=k-0x8521; // 0,1,2,3,4
			break;
		case 0x8526: // MHT GUNSTICK
			litegun=!litegun;
			break;
		case 0x8590: // STRICT DISC WRITES
			disc_filemode^=2;
			break;
		case 0x8591: // DEFAULT READ-ONLY
			disc_filemode^=1;
			break;
		case 0x8510: // DISC CONTROLLER
			disc_disabled^=1;
			break;
		case 0x8511:
			ula_snow_disabled=!ula_snow_disabled;
			break;
		case 0x8512:
			ula_v1_issue^=ULA_V1_ISSUE2^ULA_V1_ISSUE3;
			break;
		case 0x8513:
			if (ulaplus_enabled=!ulaplus_enabled)
				ulaplus_table[64]>>=1; // restore BIT0
			else
				ulaplus_table[64]<<=1; // store and reset BIT0
			video_clut_update();
			break;
		case 0x8514: // 16K MODE
			ula_sixteen=!ula_sixteen; ula_update(),mmu_update();
			break;
		case 0x8515: // NMOS Z80
			Z80_0XED71=Z80_0XED71?0:255;
			break;
		case 0x8517:
			if ((psg_disabled=!psg_disabled)&&!type_id) // mute on 48K!
				psg_table_sendto(8,0),psg_table_sendto(9,0),psg_table_sendto(10,0);
			break;
		#ifdef PSG_PLAYCITY
		case 0x8518: // PLAYCITY
			if (playcity_disabled=!playcity_disabled)
				playcity_reset(),playcity_active=0;
			else
				dac_disabled=1,dac_level=0; // forbid both PLAYCITY and COVOX at once
			break;
		case 0x8519: // COVOX
			if (dac_disabled=!dac_disabled)
				dac_level=0;
			else
				playcity_disabled=1,playcity_reset(),playcity_active=0; // forbid both COVOX and PLAYCITY at once
			break;
		#endif
		case 0x851F:
			snap_extended=!snap_extended;
			break;
		case 0x8500: // F5: LOAD FIRMWARE.. // INSERT DANDANATOR..
			#ifdef Z80_DANDANATOR
			if (session_shift)
			{
				if (s=puff_session_getfile(dandanator_path,"*.rom;*.mld","Insert Dandanator card"))
				{
					if (dandanator_load(s)) // error? warn!
						session_message(txt_error_any_load,txt_error);
					else
						autorun_mode=0,disc_disabled&=~2,all_reset(); // setup and reset
				}
			}
			else
			#endif
			if (s=puff_session_getfile(bios_path,"*.rom","Load firmware"))
			{
				if (bios_load(s)) // error? warn and undo!
					session_message(txt_error_bios,txt_error),bios_reload(); // reload valid firmware, if required
				else
					autorun_mode=0,disc_disabled&=~2,all_reset(); // setup and reset
			}
			break;
		case 0x0500: // ^F5: RESET EMULATION // REMOVE DANDANATOR
			#ifdef Z80_DANDANATOR
			if (session_shift)
			{
				if (mem_dandanator)
					dandanator_remove(),autorun_mode=0,disc_disabled&=~2,all_reset();
			}
			else
			#endif
			autorun_mode=0,disc_disabled&=~2,all_reset();
			break;
		#ifdef Z80_DANDANATOR
		case 0x0510:
			dandanator_canwrite=!dandanator_canwrite;
			break;
		#endif
		case 0x852F: // PRINTER
			if (printer)
				printer_close();
			else if (s=session_newfile(NULL,"*.txt","Printer output"))
				if (printer_p=0,!(printer=fopen(s,"wb")))
					session_message("Cannot record printer!",txt_error);
			break;
		case 0x8600: // F6: TOGGLE REALTIME
			if (!session_shift)
				session_fast^=1;
			else //case 0xC600: // PENTAGON TIMINGS
			{
				ula_pentagon=!ula_pentagon;
				ula_update(),mmu_update();
			}
			break;
		case 0x8601: // REALTIME x1
		case 0x8602: // REALTIME x2
		case 0x8603: // REALTIME x3
		case 0x8604: // REALTIME x4
			session_rhythm=k-0x8600-1; session_fast&=~1;
			break;
		case 0x0600: // ^F6: TOGGLE TURBO Z80
			multi_t=(((multi_t+(session_shift?-1:1))-1)&3)+1;
			break;
		case 0x0601: // CPU x1
		case 0x0602: // CPU x2
		case 0x0603: // CPU x3
		case 0x0604: // CPU x4
			multi_t=k-0x0600;
			break;
		case 0x8701: // CREATE DISC..
			if (!disc_disabled)
				if (s=puff_session_newfile(type_id!=3?trdos_path:disc_path,type_id!=3?"*.trd":"*.dsk",session_shift?"Create disc in B:":"Create disc in A:"))
				{
					if (type_id!=3?trdos_create(s):disc_create(s))
						session_message("Cannot create disc!",txt_error);
					else
						type_id!=3?trdos_open(s,session_shift,1):disc_open(s,session_shift,1);
				}
			break;
		case 0x8700: // F7: INSERT DISC..
			if (!disc_disabled)
				if (s=puff_session_getfilereadonly(type_id!=3?trdos_path:disc_path,type_id!=3?"*.scl;*.trd":"*.dsk",session_shift?"Insert disc into B:":"Insert disc into A:",disc_filemode&1))
					if (type_id!=3?trdos_open(s,session_shift,!session_filedialog_get_readonly()):disc_open(s,session_shift,!session_filedialog_get_readonly()))
						session_message("Cannot open disc!",txt_error);
			break;
		case 0x0700: // ^F7: EJECT DISC
			type_id!=3?trdos_close(session_shift):disc_close(session_shift);
			break;
		case 0x0701:
			disc_flip[session_shift]^=1;
			break;
		case 0x8800: // F8: INSERT OR RECORD TAPE..
			if (session_shift)
			{
				if (s=puff_session_newfile(tape_path,"*.csw","Record tape"))
					if (tape_create(s))
						session_message("Cannot create tape!",txt_error);
			}
			else if (s=puff_session_getfile(tape_path,"*.tap;*.tzx;*.pzx;*.csw;*.wav","Insert tape"))
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
					tape_enabled=1; // play just one more frame, if any
				else
					tape_enabled=-1; // play all the frames, forever
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
			video_filter^=VIDEO_FILTER_MASK_Y;
			break;
		case 0x8903:
			video_filter^=VIDEO_FILTER_MASK_X;
			break;
		case 0x8904:
			video_filter^=VIDEO_FILTER_MASK_Z;
			break;
		/*case 0x8910: // NMI
			z80_nmi_throw;
			break;*/
		case 0x0900: // ^F9: TOGGLE FAST LOAD OR FAST TAPE
			if (session_shift)
				tape_fastload=!tape_fastload;
			else
				tape_skipload=!tape_skipload;
			break;
		case 0x0901:
			//if (session_shift) tape_fastfeed=!tape_fastfeed; else
				tape_rewind=!tape_rewind;
			break;
		case 0x8A00: // FULL SCREEN
			session_togglefullscreen();
			break;
		case 0x8A01: // ZOOM TO INTEGER
			session_intzoom=!session_intzoom; session_clrscr();
			break;
		case 0x8A02: // VIDEO ACCELERATION / SOFTWARE RENDER (*needs restart)
			session_softblit=!session_softblit;
			break;
		case 0x8A04: // AUDIO ACCELERATION / LONG LOOP SPACE (*needs restart)
			session_softplay=!session_softplay;
			break;
		case 0x8B01: // MONOCHROME
		case 0x8B02: // DARK PALETTE
		case 0x8B03: // NORMAL PALETTE
		case 0x8B04: // BRIGHT PALETTE
		case 0x8B05: // GREEN SCREEN
			video_type=k-0x8B01;
			video_clut_update();
			break;
		case 0x8B00: // F11: PALETTE
			video_type=(video_type+(session_shift?length(video_table)-1:1))%length(video_table);
			video_clut_update();
			break;
		case 0x0B01: // ALL SCANLINES
		case 0x0B02: // HALF SCANLINES
		case 0x0B03: // SIMPLE INTERLACE
		case 0x0B04: // DOUBLE INTERLACE
			video_scanline=(video_scanline&-4)+k-0x0B01;
			break;
		case 0x0B08: // BLEND SCANLINES
			video_scanblend=!video_scanblend;
			break;
		case 0x0B00: // ^F11: SCANLINES
			if (session_shift)
				video_filter=(video_filter+1)&7;
			else if (!(video_scanline=(video_scanline+1)&3))
				video_scanblend=!video_scanblend;
			break;
		case 0x8C01:
			if (!session_filmfile)
				session_filmscale=!session_filmscale;
			break;
		case 0x8C02:
			if (!session_filmfile)
				session_filmtimer=!session_filmtimer;
			break;
		case 0x8C00: // F12: SAVE SCREENSHOT OR RECORD XRF FILM
			if (!session_shift)
				session_savebitmap();
			else if (session_closefilm())
					session_createfilm();
			break;
		case 0x0C00: // ^F12: RECORD WAVEFILE OR YM FILE
			if (session_shift)
			{
				if (psg_closelog()) // toggles recording
					if (psg_nextlog=session_savenext("%s%08u.ym",psg_nextlog))
						psg_createlog(session_parmtr);
			}
			else if (session_closewave()) // toggles recording
				session_createwave();
			break;
		case 0x8F00: // ^PAUSE
		case 0x0F00: // PAUSE
			if (!(session_signal&SESSION_SIGNAL_DEBUG))
				session_please(),session_signal^=SESSION_SIGNAL_PAUSE;
			break;
		case 0x9100: // ^NUM.+
		case 0x1100: // NUM.+: INCREASE FRAMESKIP
			if ((video_framelimit&MAIN_FRAMESKIP_MASK)<MAIN_FRAMESKIP_MASK)
				++video_framelimit;
			break;
		case 0x9200: // ^NUM.-
		case 0x1200: // NUM.-: DECREASE FRAMESKIP
			if ((video_framelimit&MAIN_FRAMESKIP_MASK)>0)
				--video_framelimit;
			break;
		case 0x9300: // ^NUM.*
		case 0x1300: // NUM.*: MAXIMUM FRAMESKIP
			video_framelimit|=MAIN_FRAMESKIP_MASK;
			break;
		case 0x9400: // ^NUM./
		case 0x1400: // NUM./: MINIMUM FRAMESKIP
			video_framelimit&=~MAIN_FRAMESKIP_MASK;
			break;
		#ifdef DEBUG
		case 0x9500: // PRIOR
			ula_fix_out=(ula_fix_out+1)&7;
			break;
		case 0x9600: // NEXT
			ula_fix_out=(ula_fix_out-1)&7;
			break;
		case 0x9700: // HOME
			++ula_fix_chr;
			break;
		case 0x9800: // END
			--ula_fix_chr;
			break;
		#endif
	}
}

void session_configreadmore(char *s)
{
	int i; if (!s||!*s||!session_parmtr[0]) ; // ignore if empty or internal!
	else if (!strcasecmp(session_parmtr,"type")) { if ((i=*s&15)<length(bios_system)) type_id=i; }
	else if (!strcasecmp(session_parmtr,"xsna")) snap_extended=*s&1,ula_pentagon=!!(*s&2);
	else if (!strcasecmp(session_parmtr,"joy1")) { if ((i=*s&15)<length(joy1_types)) joy1_type=i; }
	else if (!strcasecmp(session_parmtr,"fdcw")) disc_filemode=*s&3;
	#ifdef PSG_PLAYCITY
	else if (!strcasecmp(session_parmtr,"plct")) playcity_disabled=!(*s&1),dac_disabled=!(*s&2);
	#endif
	else if (!strcasecmp(session_parmtr,"misc")) psg_disabled=!(*s&1),ula_snow_disabled=!(*s&2);
	else if (!strcasecmp(session_parmtr,"file")) strcpy(autorun_path,s);
	else if (!strcasecmp(session_parmtr,"snap")) strcpy(snap_path,s);
	else if (!strcasecmp(session_parmtr,"tape")) strcpy(tape_path,s);
	else if (!strcasecmp(session_parmtr,"disc")) strcpy(disc_path,s),strcpy(trdos_path,s);
	else if (!strcasecmp(session_parmtr,"card")) strcpy(bios_path,s);
	#ifdef Z80_DANDANATOR
	else if (!strcasecmp(session_parmtr,"dntr")) strcpy(dandanator_path,s);
	#endif
	else if (!strcasecmp(session_parmtr,"palette")) { if ((i=*s&15)<length(video_table)) video_type=i; }
	else if (!strcasecmp(session_parmtr,"casette")) tape_rewind=*s&1,tape_skipload=!!(*s&2),tape_fastload=!!(*s&4);
	else if (!strcasecmp(session_parmtr,"debug")) z80_debug_configread(strtol(s,NULL,10));
}
void session_configwritemore(FILE *f)
{
	fprintf(f,"type %d\njoy1 %d\nxsna %d\nfdcw %d\nmisc %d\n"
		#ifdef PSG_PLAYCITY
		"plct %d\n"
		#endif
		"file %s\nsnap %s\ntape %s\ndisc %s\ncard %s\n"
		#ifdef Z80_DANDANATOR
		"dntr %s\n"
		#endif
		"palette %d\ncasette %d\ndebug %d\n",
		type_id,joy1_type,(snap_extended?1:0)+(ula_pentagon?2:0),disc_filemode,(psg_disabled?0:1)+(ula_snow_disabled?0:2),
		#ifdef PSG_PLAYCITY
		(playcity_disabled?0:1)+(dac_disabled?0:2),
		#endif
		autorun_path,snap_path,tape_path,type_id==3?disc_path:trdos_path,bios_path, // PLUS3 and TRDOS discs are different
		#ifdef Z80_DANDANATOR
		dandanator_path,
		#endif
		video_type,(tape_rewind?1:0)+(tape_skipload?2:0)+(tape_fastload?4:0),z80_debug_configwrite());
}

#if defined(DEBUG) || defined(SDL_MAIN_HANDLED)
void printferror(char *s) { printf("error: %s\n",s); }
#define printfusage(s) printf(MY_CAPTION " " MY_VERSION " " MY_LICENSE "\n" s)
#else
void printferror(char *s) { sprintf(session_tmpstr,"Error: %s\n",s); session_message(session_tmpstr,txt_error); }
#define printfusage(s) session_message(s,session_caption)
#endif

// START OF USER INTERFACE ========================================== //

int main(int argc,char *argv[])
{
	int i,j; FILE *f;
	session_detectpath(argv[0]);
	if ((f=session_configfile(1)))
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
						if (video_scanline<0||video_scanline>7)
							i=argc; // help!
						else
							video_scanblend=video_scanline&4,video_scanline&=3;
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
						joy1_type=(BYTE)(argv[i][j++]-'0');
						if (joy1_type<0||joy1_type>=length(joy1_types))
							i=argc; // help!
						break;
					case 'I':
						ula_v1_issue=ULA_V1_ISSUE2;
						break;
					case 'j':
						session_key2joy=1;
						break;
					case 'J':
						session_stick=0;
						break;
					case 'K':
						ula_sixteen=1;
						break;
					case 'm':
						type_id=(BYTE)(argv[i][j++]-'0');
						if (type_id<0||type_id>3)
							i=argc; // help!
						break;
					case 'o':
						onscreen_flag=1;
						break;
					case 'O':
						onscreen_flag=0;
						break;
					case 'p':
						ula_pentagon=1;
						break;
					case 'P':
						ula_pentagon=0;
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
					case 't':
						audio_mixmode=1;
						break;
					case 'T':
						audio_mixmode=0;
						break;
					case 'W':
						session_fullscreen=1;
						break;
					case 'X':
						disc_disabled=1;
						break;
					case 'y':
						tape_fastload=1;
						break;
					case 'Y':
						tape_fastload=0;
						break;
					case 'z':
						tape_skipload=1;
						break;
					case 'Z':
						tape_skipload=0;
						break;
					case '+':
						session_intzoom=1;
						break;
					case '$':
						session_hidemenu=1;
						break;
					case '!':
						session_softblit=session_softplay=1;
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
			printfusage("usage: " my_caption
			" [option..] [file..]\n"
			"\t-cN\tscanline type (0..7)\n"
			"\t-CN\tcolour palette (0..4)\n"
			"\t-d\tdebug\n"
			"\t-g0\tset Kempston joystick\n"
			"\t-g1\tset Sinclair 1 joystick\n"
			"\t-g2\tset Sinclair 2 joystick\n"
			"\t-g3\tset Cursor/AGF joystick\n"
			"\t-g4\tset QAOP+Space joystick\n"
			"\t-I\temulate Issue-2 ULA line\n"
			"\t-j\tenable joystick keys\n"
			"\t-J\tdisable joystick\n"
			"\t-K\tenable Spectrum 16K mode\n"
			"\t-m0\tload 48K firmware\n"
			"\t-m1\tload 128K firmware\n"
			"\t-m2\tload +2 firmware\n"
			"\t-m3\tload +3 firmware\n"
			"\t-o\tenable onscreen status\n"
			"\t-O\tdisable onscreen status\n"
			"\t-p\tenable Pentagon timings\n"
			"\t-P\tdisable Pentagon timings\n"
			"\t-rN\tset frameskip (0..9)\n"
			"\t-R\tdisable realtime\n"
			"\t-S\tdisable sound\n"
			"\t-t\tenable stereo\n"
			"\t-T\tdisable stereo\n"
			"\t-W\tfullscreen mode\n"
			"\t-X\tdisable disc drives\n"
			"\t-y\tenable tape analysis\n"
			"\t-Y\tdisable tape analysis\n"
			"\t-z\tenable tape speed-up\n"
			"\t-Z\tdisable tape speed-up\n"
			"\t-!\tforce software render\n"
			"\t-$\talternative user interface\n"
			),1;
	if (trdos_load("trdos.rom"),bios_reload())
		return printferror(txt_error_bios),1;
	char *s=session_create(session_menudata); if (s)
		return sprintf(session_scratch,"Cannot create session: %s!",s),printferror(session_scratch),1;
	session_kbdreset();
	session_kbdsetup(kbd_map_xlt,length(kbd_map_xlt)/2);
	video_target=&video_frame[video_pos_y*VIDEO_LENGTH_X+video_pos_y]; audio_target=audio_frame;
	audio_disabled=!session_audio;
	video_clut_update(); onscreen_inks(0xAA0000,0x55FF55);
	if (session_fullscreen) session_togglefullscreen();
	// it begins, "alea jacta est!"
	while (!session_listen())
	{
		while (!session_signal)
			z80_main(( // clump Z80 instructions together to gain speed...
				((session_fast&-2)|tape_skipping)?ula_limit_x<<2: // tape loading allows simple timings, but some sync is still needed
				z80_irq?z80_irq:(ula_limit_x-ula_count_x-1)<<2 // follow VRAM writes ("COBRA") and IRQ events ("TIMING TESTS")
			)*multi_t); // ...without missing any IRQ and ULA deadlines!
		if (session_signal&SESSION_SIGNAL_FRAME) // end of frame?
		{
			if (!video_framecount&&onscreen_flag)
			{
				if (disc_disabled)
					onscreen_text(+1,-3,"--\t--",0);
				else
				{
					if (type_id!=3)
					{
						int q=trdos_length;
						if (q) onscreen_char(+3,-3,trdos_target>=0?'W':'R');
						onscreen_byte(+1,-3,trdos_cursor[0],q&&((trdos_system&3)==0));
						onscreen_byte(+4,-3,trdos_cursor[1],q&&((trdos_system&3)==1));
					}
					else
					{
						int q=disc_phase&2; // disc drive is busy?
						if (q) onscreen_char(+3,-3,disc_phase&1?128+'R':128+'W');
						onscreen_byte(+1,-3,disc_track[0],q&&((disc_parmtr[1]&3)==0));
						onscreen_byte(+4,-3,disc_track[1],q&&((disc_parmtr[1]&3)==1));
					}
				}
				int q=tape_enabled?128:0;
				if (tape_skipping)
					onscreen_char(+6,-3,(tape_skipping>0?'*':'+')+q);
				if (!tape||tape_type<0)
					onscreen_text(+7,-3,tape?"REC":"---",q);
				else
				{
					int i=(long long)tape_filetell*1000/(tape_filesize+1);
					onscreen_char(+7,-3,'0'+i/100+q);
					onscreen_byte(+8,-3,i%100,q);
				}
				if (session_stick|session_key2joy)
				{
					onscreen_bool(-5,-6,3,1,kbd_bit_tst(kbd_joy[0]));
					onscreen_bool(-5,-2,3,1,kbd_bit_tst(kbd_joy[1]));
					onscreen_bool(-6,-5,1,3,kbd_bit_tst(kbd_joy[2]));
					onscreen_bool(-2,-5,1,3,kbd_bit_tst(kbd_joy[3]));
					onscreen_bool(-4,-4,1,1,kbd_bit_tst(kbd_joy[4]));
				}
				#ifdef DEBUG
				onscreen_byte(+1,+1,ula_fix_out,0);
				onscreen_byte(+4,+1,ula_fix_chr,0);
				if (session_audio) // Win32 audio cycle / SDL2 audio queue
				{
					if ((j=audio_session)>AUDIO_N_FRAMES) j=AUDIO_N_FRAMES;
					onscreen_bool(+11,-2,j,1,1); onscreen_bool(j+11,-2,AUDIO_N_FRAMES-j,1,0);
				}
				#endif
			}
			// update session and continue
			if (!--autorun_t)
				autorun_next();
			if (!audio_disabled)
			{
				if (audio_pos_z<AUDIO_LENGTH_Z) audio_main(TICKS_PER_FRAME); // fill sound buffer to the brim!
				#ifdef PSG_PLAYCITY
				dac_level_zero();
				if (!playcity_disabled)
					playcity_main(audio_frame,AUDIO_LENGTH_Z);
				#endif
			}
			psg_writelog();
			// ULA snow is tied to memory contention (cfr. "Egghead 4"): no snow on Pentagon!
			ula_snow_a=(!ula_snow_disabled&&(ula_clash_mreq[z80_ir.b.h>>6]!=ula_clash[0]))?31:0;
			ula_clash_z=((ula_count_y-ula_start_y)*ula_limit_x+ula_count_x)*4+ula_clash_a; // consistency across frames
			if (tape_type<0&&tape/*&&!z80_iff.b.l*/) // tape is recording? play always!
				tape_enabled|=4;
			else if (tape_enabled>0)
				--tape_enabled; // tape is still busy?
			if (tape_signal)
			{
				if ((tape_signal<2||(ula_v2&32))) tape_enabled=0; // stop tape if required
				tape_signal=0,session_dirty=1; // update config
			}
			tape_skipping=audio_queue=0; // reset tape and audio flags
			if (tape_filetell<tape_filesize&&tape_skipload&&tape_enabled)
				video_framelimit|=(MAIN_FRAMESKIP_MASK+1),session_fast|=2,video_interlaced|=2,audio_disabled|=2; // abuse binary logic to reduce activity
			else
				video_framelimit&=~(MAIN_FRAMESKIP_MASK+1),session_fast&=~2,video_interlaced&=~2,audio_disabled&=~2; // ditto, to restore normal activity
			session_update();
			//if (!audio_disabled) audio_main(1+ula_clash_z); // preload audio buffer
		}
	}
	// it's over, "acta est fabula"
	z80_close();
	tape_close();
	disc_closeall(); trdos_closeall(); if (!*trdos_path) strcpy(trdos_path,disc_path); else if (!*disc_path) strcpy(disc_path,trdos_path); // avoid accidental losses
	psg_closelog(); if (printer) printer_close();
	if ((f=session_configfile(0)))
		session_configwritemore(f),session_configwrite(f),fclose(f);
	return session_byebye(),0;
}

BOOTSTRAP

// ============================================ END OF USER INTERFACE //
