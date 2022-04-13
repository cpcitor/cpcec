 //  ####    ####   ######  ######   ####  ------------------------- //
//  ##  ##  ##  ##  ##      ##      ##  ##  CSFEC, small Commodore 64 //
//  ##      ##      ##      ##      ##      (SixtyFour) emulator made //
//  ##       ####   ####    ####    ##      on top of CPCEC's modules //
//  ##          ##  ##      ##      ##      by Cesar Nicolas-Gonzalez //
//  ##  ##  ##  ##  ##      ##      ##  ##  since 2022-01-12 till now //
 //  ####    ####   ##      ######   ####  ------------------------- //

#define MY_CAPTION "CSFEC"
#define my_caption "csfec"
#define MY_VERSION "20220412"//"2555"
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

// In a similar spirit to ZXSEC, the goal is to provide a sufficient
// emulation of the Commodore 64-PAL home computer, trying to use as
// many parts of CPCEC as possible, a challenge because the hardware
// is completely different: the CPU chip is the MOS 6510, the video
// chip is the MOS 6569 (VIC-II), the audio chip is the MOS 6581 (SID)
// and timers and connections are handled by a pair of MOS 6526 (CIA).
// The C1541 disc drive runs its own MOS 6502 CPU with RAM and all!

// This file provides C64-specific features for configuration, VIC-II,
// PAL and CIA logic, 6510 timings and support, and file I/O.

#include <stdio.h> // printf()...
#include <stdlib.h> // strtol()...
#include <string.h> // strcpy()...

// Commodore 64 PAL metrics and constants defined as general types -- //

#define MAIN_FRAMESKIP_BITS 4
#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (63<<4) // HBLANK 9c, BORDER 7c, BITMAP 40c, BORDER 7c
#define VIDEO_LENGTH_Y (39<<4)
#define VIDEO_OFFSET_X (12<<4)
#define VIDEO_OFFSET_Y (23<<1) // the best balance for "Delta" (score panel on top) and "Megaphoenix" (on bottom)
#define VIDEO_PIXELS_X (48<<4)
#define VIDEO_PIXELS_Y (67<<3)
#define AUDIO_PLAYBACK 44100 // 22050, 24000, 44100, 48000
#define AUDIO_LENGTH_Z (AUDIO_PLAYBACK/VIDEO_PLAYBACK) // division must be exact!

#if defined(SDL2)||!defined(_WIN32)
unsigned short session_icon32xx16[32*32] = {
	0X0000,0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,0X0000,
	0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,
	0XF000,0XF000,0XF000,0XF800,0XF800,0XFF00,0XFFFF,0XFFFF,0XFF00,0XFFFF,0XFFFF,0XFFFF,0XFF00,0XFF00,0XFF00,0XFF00,	0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XF800,0XF800,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XFF00,0XFF00,0XFFFF,0XFFFF,0XFF00,0XFFFF,0XFFFF,0XFFFF,0XFF00,0XFF00,0XFF00,0XFF00,	0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XF800,0XF800,0XF000,0XF000,
	0XF000,0XF000,0XF800,0XF800,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,	0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XF800,0XF800,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF800,0XF800,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,	0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XFF00,0XF800,0XF800,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF840,0XF840,0XFF80,0XFFFF,0XFFFF,0XFF80,0XFFFF,0XFFFF,0XFFFF,0XFF80,0XFF80,0XFF80,0XFF80,	0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XF840,0XF840,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF840,0XF840,0XFF80,0XFF80,0XFFFF,0XFFFF,0XFF80,0XFFFF,0XFFFF,0XFFFF,0XFF80,0XFF80,0XFF80,0XFF80,	0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XF840,0XF840,0XF000,0XF000,
	0XF000,0XF000,0XF840,0XF840,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,	0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XF840,0XF840,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF840,0XF840,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,	0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XFF80,0XF840,0XF840,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF880,0XF880,0XFFF0,0XFFFF,0XFFFF,0XFFF0,0XFFFF,0XFFFF,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XF880,0XF880,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF880,0XF880,0XFFF0,0XFFF0,0XFFFF,0XFFFF,0XFFF0,0XFFFF,0XFFFF,0XFFFF,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XF880,0XF880,0XF000,0XF000,

	0XF000,0XF000,0XF880,0XF880,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XF880,0XF880,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF880,0XF880,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,	0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XFFF0,0XF880,0XF880,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF080,0XF080,0XF0F0,0XFFFF,0XFFFF,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF080,0XF080,0XF0F0,0XF0F0,0XFFFF,0XFFFF,0XF0F0,0XFFFF,0XFFFF,0XFFFF,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF000,
	0XF000,0XF000,0XF080,0XF080,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF080,0XF080,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,	0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF0F0,0XF080,0XF080,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF048,0XF048,0XF08F,0XFFFF,0XFFFF,0XF08F,0XFFFF,0XFFFF,0XFFFF,0XF08F,0XF08F,0XF08F,0XF08F,	0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF048,0XF048,0XF000,0XF000,0XF000,
	0XF000,0XF000,0XF048,0XF048,0XF08F,0XF08F,0XFFFF,0XFFFF,0XF08F,0XFFFF,0XFFFF,0XFFFF,0XF08F,0XF08F,0XF08F,0XF08F,	0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF048,0XF048,0XF000,0XF000,
	0XF000,0XF000,0XF048,0XF048,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,	0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF048,0XF048,0XF000,0XF000,
	0XF000,0XF000,0XF000,0XF048,0XF048,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,	0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF08F,0XF048,0XF048,0XF000,0XF000,0XF000,
	0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,
	0X0000,0X0000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,	0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0XF000,0X0000,0X0000,
	};
#endif

// The Commodore 64 keyboard ( http://unusedino.de/ec64/technical/aay/c64/keybmatr.htm )
// +-------------------------------------------------------------------------------+  +----+
// |    | !  | "  | #  | $  | %  | %  | '  | (  | )  |    |    |    |    |CLR |INST|  | F2 |
// | <- | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 0  | +  | -  | £  |HOME|DEL |  | F1 |
// +-------------------------------------------------------------------------------+  +----+
// |      |    |    |    |    |    |    |    |    |    |    |    |    |    |       |  | F4 |
// | CTRL | Q  | W  | E  | R  | T  | Y  | U  | I  | O  | P  | @  | *  | ^  |RESTORE|  | F3 |
// +-------------------------------------------------------------------------------+  +----+
// |RUN |SHFT|    |    |    |    |    |    |    |    |    | [  | ]  |    |         |  | F6 |
// |STOP|LOCK| A  | S  | D  | F  | G  | H  | J  | K  | L  | :  | ;  | =  | RETURN  |  | F5 |
// +-------------------------------------------------------------------------------+  +----+
// |    |      |    |    |    |    |    |    |    | <  | >  | ?  |       |U/D |L/R |  | F8 |
// | C= |LSHIFT| Z  | X  | C  | V  | B  | N  | M  | ,  | .  | /  |RSHIFT |CRSR|CRSR|  | F7 |
// +-------------------------------------------------------------------------------+  +----+
//                |                                         |
//                |                  SPACE                  |
//                +-----------------------------------------+
// Bear in mind that the keyboard codes are written in OCTAL rather than HEXADECIMAL!
// +-------------------------------------------------------------------------------+  +----+
// |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |  |    |
// | 71 | 70 | 73 | 10 | 13 | 20 | 23 | 30 | 33 | 40 | 43 | 50 | 53 | 60 | 63 | 00 |  | 04 |
// +-------------------------------------------------------------------------------+  +----+
// |      |    |    |    |    |    |    |    |    |    |    |    |    |    |       |  |    |
// | 72   | 76 | 11 | 16 | 21 | 26 | 31 | 36 | 41 | 46 | 51 | 56 | 61 | 66 |RESTORE|  | 05 |
// +-------------------------------------------------------------------------------+  +----+
// |    |SHFT|    |    |    |    |    |    |    |    |    |    |    |    |         |  |    |
// | 77 |LOCK| 12 | 15 | 22 | 25 | 32 | 35 | 42 | 45 | 52 | 55 | 62 | 65 |   01    |  | 06 |
// +-------------------------------------------------------------------------------+  +----+
// |    |      |    |    |    |    |    |    |    |    |    |    |       |    |    |  |    |
// | 75 | 17   | 14 | 27 | 24 | 37 | 34 | 47 | 44 | 57 | 54 | 67 | 64    | 07 | 02 |  | 03 |
// +-------------------------------------------------------------------------------+  +----+
//                |                                         |
//                |                   74                    |
//                +-----------------------------------------+

#define KBD_JOY_UNIQUE 5 // exclude repeated buttons
unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1-fire4
	{ 0110,0111,0112,0113,0114,0114,0114,0114 }; // constant, joystick bits are hard-wired
//#define MAUS_EMULATION // ignore!
//#define MAUS_LIGHTGUNS // ignore!
//#define INFLATE_RFC1950 // no file needs inflating RFC1950 data
//#define DEFLATE_RFC1950 // no file needs deflating RFC1950 data
//#define DEFLATE_LEVEL 6 // no file needs DEFLATE
#include "cpcec-os.h" // OS-specific code!
#include "cpcec-rt.h" // OS-independent code!

const unsigned char kbd_map_xlt[]=
{
	// control keys
	KBCODE_F1	,0x81,	KBCODE_F2	,0x82,	KBCODE_F3	,0x83,	KBCODE_F4	,0x84,
	KBCODE_F5	,0x85,	KBCODE_F6	,0x86,	KBCODE_F7	,0x87,	KBCODE_F8	,0x88,
	KBCODE_F9	,0x89,	KBCODE_HOLD	,0x8F,	KBCODE_F11	,0x8B,	KBCODE_F12	,0x8C,
	KBCODE_X_ADD	,0x91,	KBCODE_X_SUB	,0x92,	KBCODE_X_MUL	,0x93,	KBCODE_X_DIV	,0x94,
	#ifdef DEBUG
	KBCODE_PRIOR	,0x95,	KBCODE_NEXT	,0x96,	//KBCODE_HOME	,0x97,	KBCODE_END	,0x98,
	#endif
	// actual keys; again, notice the octal coding
	KBCODE_CHR4_5	,0071,	KBCODE_1	,0070,	KBCODE_2	,0073,	KBCODE_3	,0010, // "~" = ARROW LEFT
	KBCODE_4	,0013,	KBCODE_5	,0020,	KBCODE_6	,0023,	KBCODE_7	,0030,
	KBCODE_8	,0033,	KBCODE_9	,0040,	KBCODE_0	,0043,	KBCODE_CHR1_1	,0050,
	KBCODE_CHR1_2	,0053,	KBCODE_BKSPACE	,0000,	KBCODE_INSERT	,0060,	KBCODE_HOME	,0063, // INSERT = POUND; HOME = HOME (+SHIFT: CLR)
	KBCODE_TAB	,0072,	KBCODE_Q	,0076,	KBCODE_W	,0011,	KBCODE_E	,0016,
	KBCODE_R	,0021,	KBCODE_T	,0026,	KBCODE_Y	,0031,	KBCODE_U	,0036,
	KBCODE_I	,0041,	KBCODE_O	,0046,	KBCODE_P	,0051,	KBCODE_CHR2_1	,0056,
	KBCODE_CHR2_2	,0061,
	KBCODE_CAPSLOCK	,0175,	KBCODE_A	,0012,	KBCODE_S	,0015,	KBCODE_D	,0022, // CAPS LOCK = CONTROL+SHIFT
	KBCODE_F	,0025,	KBCODE_G	,0032,	KBCODE_H	,0035,	KBCODE_J	,0042,
	KBCODE_K	,0045,	KBCODE_L	,0052,	KBCODE_CHR3_1	,0055,	KBCODE_CHR3_2	,0062,
	KBCODE_CHR3_3	,0065,	KBCODE_ENTER	,0001,
	KBCODE_L_CTRL	,0075,	KBCODE_L_SHIFT	,0017,	KBCODE_Z	,0014,	KBCODE_X	,0027,
	KBCODE_C	,0024,	KBCODE_V	,0037,	KBCODE_B	,0034,	KBCODE_N	,0047,
	KBCODE_M	,0044,	KBCODE_CHR4_1	,0057,	KBCODE_CHR4_2	,0054,	KBCODE_CHR4_3	,0067,
	KBCODE_CHR4_4	,0071, // "<" = ARROW LEFT
	KBCODE_SPACE	,0074,	KBCODE_R_SHIFT	,0064,	KBCODE_R_CTRL	,0075,
	KBCODE_ESCAPE	,0077, // RUN/STOP
	KBCODE_DELETE	,0066, // ARROW UP
	KBCODE_RIGHT	,0002, // CURSOR RIGHT
	KBCODE_LEFT	,0102, // CURSOR RIGHT + LSHIFT = CURSOR LEFT
	KBCODE_DOWN	,0007, // CURSOR DOWN
	KBCODE_UP	,0107, // CURSOR DOWN + LSHIFT = CURSOR UP
	// function keys on the numeric pad
	KBCODE_X_1  	,0004, // F1
	KBCODE_X_2  	,0104, // F1 + LSHIFT = F2
	KBCODE_X_3	,0005, // F3
	KBCODE_X_4	,0105, // F3 + LSHIFT = F4
	KBCODE_X_5	,0006, // F5
	KBCODE_X_6	,0106, // F5 + LSHIFT = F6
	KBCODE_X_7	,0003, // F7
	KBCODE_X_8	,0103, // F7 + LSHIFT = F8
	KBCODE_END	,0X90, // RESTORE (the NMI button, not a real key!)
};

const VIDEO_UNIT video_table[][16]= // colour table, 0xRRGGBB style: the 16 original colours
{
	// monochrome - black and white
	{
		0X000000,0XFFFFFF,0X454545,0XB9B9B9,0X565656,0X919191,0X353535,0XE6E6E6,
		0X5B5B5B,0X3A3A3A,0X818181,0X4A4A4A,0X7B7B7B,0XE4E4E4,0X777777,0XB2B2B2,
	},
	// dark colour, pow(a,1.5)/16.0
	{
		0X000000,0XFFFFFF,0X5C171A,0X4FB9B1,0X6A1D74,0X328D2A,0X131279,0XE4EA4B,
		0X6A2D10,0X311A00,0XAC464B,0X282828,0X555555,0X89FF7D,0X4A47E1,0X949494,
	},
	// normal colour, cfr. colodore.com
	{
		0X000000,0XFFFFFF,0X813338,0X75CEC8,0X8E3C97,0X56AC4D,0X2E2C9B,0XEDF171,
		0X8E5029,0X553800,0XC46C71,0X4A4A4A,0X7B7B7B,0XA9FF9F,0X706DEB,0XB2B2B2,
	},
	// bright colour, pow(a*16.0,0.666667)
	{
		0X000000,0XFFFFFF,0XA2575D,0X98DDD9,0XAD61B4,0X7CC473,0X524FB7,0XF3F694,
		0XAD764B,0X7B5D00,0XD69094,0X707070,0X9D9D9D,0XC2FFBA,0X9491F2,0XC9C9C9,
	},
	// monochrome - green screen
	{
		0X003C00,0X82FF82,0X1A631A,0X69DA69,0X1F6A1F,0X58C058,0X165E16,0X7BF47B,
		0X297929,0X1D671D,0X378F37,0X267526,0X3F9A3F,0X82FF82,0X388F38,0X5BC45B,
	},
};

// GLOBAL DEFINITIONS =============================================== //

#define TICKS_PER_FRAME ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)/32)
#define TICKS_PER_SECOND (TICKS_PER_FRAME*VIDEO_PLAYBACK)
int multi_t=1; // overclocking factor

// HARDWARE DEFINITIONS ============================================= //

BYTE mem_ram[9<<16],mem_rom[5<<12],mem_i_o[1<<12]; // RAM (64K C64 + 512K REU), ROM (8K KERNAL, 8K BASIC, 4K CHARGEN) and I/O (1K VIC-II, 1K SID, 1K VRAM, 1K CIA+EXTRAS)
BYTE *mmu_rom[256],*mmu_ram[256]; // pointers to all the 256-byte pages
BYTE mmu_bit[256]; // flags of all the 256-byte pages: +1 filters READs and +2 filters WRITEs
#define PEEK(x) mmu_rom[(x)>>8][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>8][x] // WARNING, x cannot be `x=EXPR`!

#define VICII_TABLE (mem_i_o)
#define VICII_COLOR (&mem_i_o[0X800])
#define CIA_TABLE_0 (&mem_i_o[0XC00])
#define CIA_TABLE_1 (&mem_i_o[0XD00])
#define SID_TABLE_0 (&mem_i_o[0X400])
BYTE *SID_TABLE[3]={SID_TABLE_0,&mem_i_o[0XE00],&mem_i_o[0XF00]}; // default addresses: $D400, $DE00 and $DF00

BYTE disc_disabled=0; // disables the disc drive altogether as well as its extended logic
BYTE disc_filemode=1; // default read only + strict disc writes
BYTE reu_depth=0,reu_dirty=0; // REU memory extension
int reu_kbyte[]={0,64,128,256,512};

BYTE video_type=length(video_table)/2; // 0 = monochrome, 1=darkest colour, etc.
VIDEO_UNIT video_clut[32]; // precalculated 16-colour palette + $D020 border [16] + $D021.. background 0-3 [17..20] + $D025.. sprite multicolour 0-1 [21,22] + sprite #0-7 colour [23..30] + DEBUG

void video_clut_update(void) // precalculate palette following `video_type`
{
	for (int i=0;i<16;++i)
		video_clut[i]=video_table[video_type][i]; // VIC-II built-in palette
	for (int i=0;i<15;++i)
		video_clut[16+i]=video_table[video_type][VICII_TABLE[32+i]&15]; // VIC-II dynamic palette
	video_clut[31]=video_clut[1]; // DEBUG
}

int audio_dirty,audio_queue=0; // used to clump audio updates together to gain speed

// the MOS 6510 and its MMU ----------------------------------------- //

Z80W m6510_pc; BYTE m6510_a,m6510_x,m6510_y,m6510_s,m6510_p,m6510_i; // the MOS 6510 registers; we must be able to spy them!
BYTE m6510_irq; // +1 = VIC-II IRQ; +2 = CIA #1 IRQ; +128 = CIA #2 NMI
BYTE mmu_cfg[2]; // the first two bytes in the MOS 6510 address map; distinct to the first two bytes of RAM!
BYTE mmu_out,mmu_mmu; // the full and MMU-only values defined by the MOS 6510's first two bytes
BYTE mmu_inp=0X17; // the tape bit mask, whose bits can appear at address 0X0001: bit 4 is !TAPE, bits 0-2 are always 1
BYTE tape_enabled=0; int cia_port_13_n=0; // manual + automatic tape playback

void mmu_setup(void) // by default, everything is RAM
{
	for (int i=0;i<256;++i)
		mmu_bit[i]=0,mmu_rom[i]=mmu_ram[i]=mem_ram;
	mmu_bit[0]=3; // ZEROPAGE is always special!
}

void tape_autoplay(void)
{
	int cia=CIA_TABLE_0[13]&16,mmu=mmu_cfg[1]&32;
	if (cia&&!mmu) tape_enabled|=+1;
	else if (!cia&&mmu) tape_enabled&=~1;
}

void mmu_update(void) // set the address offsets, as well as the tables for m6510_recv(WORD) and m6510_send(WORD,BYTE)
{
	mmu_out=(~mmu_cfg[0]&mmu_out)+(mmu_cfg[0]&mmu_cfg[1]);
	int i=(~mmu_cfg[0]&0X17)+(mmu_cfg[0]&mmu_cfg[1])&7;
	if (mmu_mmu!=i) // this function is slow, avoid clobbering
	{
		BYTE *recva=mem_ram,*recvd=mem_ram,*sendd=mem_ram,*recve=mem_ram;
		switch (mmu_mmu=i) // notice the "incremental" design and the lack of `break` here!
		{
			case 3: recva=&mem_rom[0X2000-0XA000]; // BASIC
			case 2: recve=&mem_rom[0X0000-0XE000]; // KERNAL
			case 1: recvd=&mem_rom[0X4000-0XD000]; // CHARGEN
			case 0: break;
			case 7: recva=&mem_rom[0X2000-0XA000]; // BASIC
			case 6: recve=&mem_rom[0X0000-0XE000]; // KERNAL
			case 5: recvd=sendd=&mem_i_o[-0XD000]; // I/O
			case 4: break;
		}
		for (i=0;i<16;++i)
			mmu_rom[i+0XA0]=mmu_rom[i+0XB0]=recva,
			mmu_rom[i+0XD0]=recvd,mmu_ram[i+0XD0]=sendd,
			mmu_rom[i+0XE0]=mmu_rom[i+0XF0]=recve;
		i=mmu_mmu>4?3:0; // tag the page I/O only if possible
		memset(&mmu_bit[0XD0],i  ,0X08); // $D000-$D7FF: VIC-II + SID catches R/W
		memset(&mmu_bit[0XD8],i&2,0X04); // $D800-$DBFF: VIC-II COLOR catches W
		memset(&mmu_bit[0XDC],i  ,0X04); // $DC00-$DFFF: CIA #1+#2 AREA catches R/W
	}
	tape_autoplay();
}

void mmu_reset(void)
	{ mmu_cfg[0]=mmu_cfg[1]=mmu_out=mmu_mmu=0XFF; mmu_update(); }

// the VIC-II video chip ============================================ //

BYTE *vicii_memory,*vicii_attrib,*vicii_bitmap; // pointers to video data
short vicii_pos_y,vicii_chr_y,vicii_irq_y,vicii_impact[512+8],vicii_pixel,vicii_lines; // X and Y ranges are 0-62 and 0-311 respectively in PAL
char vicii_pos_x,vicii_chr_x,vicii_pos_z,vicii_mode; // mode: +1, +2 and +4 = 8 pixel modes, +8 = BACKGROUND, +16 = BORDER HORIZONTAL, +32 = BORDER VERTICAL
BYTE vicii_sprite_l[8],vicii_sprite_h[8],vicii_sprite_z[8]; // sprite buffers + zoom

void vicii_setmaps(void) // recalculate memory maps
{
	int i=~CIA_TABLE_1[0]&CIA_TABLE_1[2]&3;
	vicii_memory=&mem_ram[i<<14];
	if (VICII_TABLE[17]&32) // BITMAP MODES?
	{
		vicii_attrib=&vicii_memory[(VICII_TABLE[24]&240)<<6];
		vicii_bitmap=&vicii_memory[(VICII_TABLE[24]& 8)<<10];
	}
	else // CHARACTER MODES!
	{
		vicii_attrib=&vicii_memory[(VICII_TABLE[24]&240)<<6];
		if (!(i&1)&&((VICII_TABLE[24]&12)==4)) // CHAR ROM?
			vicii_bitmap=&mem_rom[0X4000+((VICII_TABLE[24]&2)<<10)];
		else // 64K AREA!
			vicii_bitmap=&vicii_memory[(VICII_TABLE[24]&14)<<10];
	}
}

void vicii_setmode(void) // recalculate bitmap mode
	{ vicii_mode=(vicii_mode&-8)+(((VICII_TABLE[17]&96)+(VICII_TABLE[22]&16))>>4); }

void vicii_reset(void)
{
	VICII_TABLE[25]=VICII_TABLE[26]=vicii_pos_x=vicii_chr_x=vicii_pixel=0; vicii_irq_y=-1; vicii_setmaps(); vicii_setmode();
	memset(vicii_sprite_l,63,sizeof(vicii_sprite_l)),MEMZERO(vicii_sprite_h),MEMZERO(vicii_sprite_z);
}

// ...

// the CIA #1+#2 gate arrays ======================================== //

WORD cia_count_a[2],cia_count_b[2]; // the CIA event counters
DWORD cia_hhmmssd[2]; // the CIA time-of-day registers
BYTE cia_port_13[2]; // the CIA interrupt status flags

void cia_reset(void)
{
	cia_hhmmssd[0]=cia_hhmmssd[1]=0X01000000; // = 1:00:00.0 AM
	memset(CIA_TABLE_0,cia_port_13[0]=0,16);
	memset(CIA_TABLE_1,cia_port_13[1]=0,16);
	CIA_TABLE_0[11]=CIA_TABLE_1[11]=1;
	CIA_TABLE_0[0]=CIA_TABLE_0[1]=cia_count_a[0]=cia_count_b[0]=
	CIA_TABLE_1[0]=CIA_TABLE_1[1]=cia_count_a[1]=cia_count_b[1]=-1;
}

// ...

// behind the CIA: tape I/O handling -------------------------------- //

char tape_path[STRMAX]="",tap_magic[]="C64-TAPE-RAW",t64_magic1[]="C64 tape image",t64_magic2[]="C64S tape file";
FILE *tape=NULL; int tape_filetell=0,tape_filesize=0;

int tape_fastload=1; // catch musical loaders

BYTE tape_buffer[8<<8]; int tape_length,tape_offset; // tape buffer/directory (must be >1k!)
int tape_seek(int i) // moves file cursor to `i`, cancels the cache if required
{
	if (i<0) i=0; else if (i>tape_filesize) i=tape_filesize; // sanitize
	int j=i-tape_filetell+tape_offset; if (j>=0&&j<tape_length) // within cache?
		return tape_offset=j,tape_filetell=i;
	return fseek(tape,i,SEEK_SET),tape_offset=tape_length=0,tape_filetell=i;
}
int tape_getc(void) // returns the next byte in the file, or -1 on EOF
{
	if (tape_offset>=tape_length)
		if ((tape_offset=0)>=(tape_length=fread1(tape_buffer,sizeof(tape_buffer),tape)))
			return -1; // EOF
	return ++tape_filetell,tape_buffer[tape_offset++];
}
int tape_getcc(void) // returns an Intel-style WORD; see tape_getc()
	{ int i; if ((i=tape_getc())>=0) return i|(tape_getc()<<8); return i; }
int tape_getccc(void) // returns an Intel-style 24-bit; see tape_getc()
	{ int i; if ((i=tape_getc())>=0) if ((i|=tape_getc()<<8)>=0) return i|(tape_getc()<<16); return i; }
int tape_getcccc(void) // returns an Intel-style DWORD; mind the sign in 32bits!
	{ int i; if ((i=tape_getc())>=0) if ((i|=tape_getc()<<8)>=0) if ((i|=tape_getc()<<16)>=0) return i|(tape_getc()<<24); return i; }

int tape_rewind=0,tape_skipload=1/*,tape_fastload=1,tape_skipping=0*/; // tape options
int tape_signal/*,tape_status=0,tape_output*/,tape_record,tape_type=0; // tape status
int tape_n=-1; // TAP-specific variable
int m6510_t64ok=-1; // T64-specific variable

int tape_close(void) // closes tape and cleans up
{
	if (tape)
	{
		if (tape_type<0) fseek(tape,16,SEEK_SET),fputiiii(tape_filesize-20,tape); // store the real tape length
		puff_fclose(tape),tape=NULL;
		mmu_inp|=16; // i.e. TAPE IS MISSING
	}
	tape_filesize=tape_filetell=0; m6510_t64ok=-1;
	return tape_n=-1,tape_type=/*tape_output=tape_status=*/0;
}

int tape_open(char *s) // inserts a tape from path `s`; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"rb")))
		return 1; // cannot open file!
	tape_length=fread1(tape_buffer,sizeof(tape_buffer),tape);
	if (tape_length>(tape_offset=16)&&!memcmp(tape_buffer,tap_magic,12))
	{
		tape_filesize=tape_getcccc()+tape_filetell;
		tape_n=0; tape_type=+1; mmu_inp&=~16; // i.e. TAPE IS READY
	}
	else if (tape_length>=64+tape_buffer[34]*32&&!memcmp(tape_buffer,t64_magic1,14)&&tape_buffer[33]==1&&tape_buffer[34]>=tape_buffer[36]&&tape_buffer[36])
	{
		tape_filetell=0; tape_filesize=tape_buffer[36]; // the buffer won't be used as such, so we can rewire it to our purposes
		tape_type=+2; m6510_t64ok=0X07;
	}
	else if (tape_length>=64&&!memcmp(tape_buffer,t64_magic2,14)&&tape_buffer[36]<2) // obsolete T64 archives made with faulty tools
	{
		fseek(tape,0,SEEK_END); int i=ftell(tape)+mgetii(&tape_buffer[66])-mgetiiii(&tape_buffer[72]); // the actual size...
		mputii(&tape_buffer[68],i); //  ...is the archive size plus the loading address minus the offset within the archive
		tape_filetell=0; tape_filesize=1; // there's just one file in these archives, we don't need to guess anything else
		tape_type=+2; m6510_t64ok=0X07;
	}
	else
		return tape_close(),1; // wrong file type!
	if (tape_path!=s) strcpy(tape_path,s);
	return /*tape_output=tape_status=*/0;
}

int tape_t64load(char *s) // loads a file from a T64 archive into the emulated C64; `s` is the filename, NULL if none
{
	if (s&&strcmp("*",s)) // a filename instead of NULL or a wildcard? search!
	{
		int i; for (i=0;i<tape_filesize;++i)
			if (tape_buffer[64+i*32]==1&&tape_buffer[65+i*32]&&!memcmp(&tape_buffer[80+i*32],s,16))
				break; // found!!
		if (i>=tape_filesize) return 1; // file not found!
		tape_filetell=i;
	}
	WORD l=mgetii(&tape_buffer[66+tape_filetell*32]),h=mgetii(&tape_buffer[68+tape_filetell*32]);
	if (l<2||h<2||h<=l) return 1; // wrong init+exit values!!
	fseek(tape,mgetiiii(&tape_buffer[72+tape_filetell*32]),SEEK_SET);
	//if (!mem_ram[0XB9]) h=h-l+0X0801,l=0X0801; // adjust init+exit values? INTENSIT.T64 is at odds with https://www.c64-wiki.com/wiki/LOAD
	//mputii(&mem_ram[0X002D],h); mputii(&mem_ram[0X002F],h); mputii(&mem_ram[0X0031],h);
	mputii(&mem_ram[0X00AC],h); mputii(&mem_ram[0X00AE],h); // end-of-file KERNAL pokes
	fread1(&mem_ram[l],(WORD)(h-l),tape); // load the file data into RAM!
	if (++tape_filetell>=tape_filesize) tape_filetell=0; // wrap back!
	return mem_ram[0X90]=0X00; // OK!
}
int t64_loadfile(void) // handles the KERNAL operation "$F533: Load File From Tape"; 0 OK, !0 ERROR
{
	mem_ram[0X90]=0X04; // TAPE ERROR! (in theory FILE NOT FOUND)
	int i=mem_ram[0XB7]; // Number of Characters in Filename: see "$FDF9: Set Filename"
	if (mem_ram[0X93]||i>16) return 1; // VERIFY (instead of LOAD) or invalid filename? quit!
	if (!i) return tape_t64load(NULL);// no filename? load anything!
	char t[17]; memcpy(t,&mem_ram[mgetii(&mem_ram[0XBB])],16);
	while (i<16) t[i++]=' '; t[i]=0; // pad the filename with spaces!
	return tape_t64load(t);
}

int tape_create(char *s) // creates a tape on path `s`; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"wb")))
		return 1; // cannot create file!
	fwrite1(tap_magic,16,tape); fputiiii(0,tape); // the real tape length will be stored later
	if (tape_path!=s) strcpy(tape_path,s);
	mmu_inp&=~16; // i.e. TAPE IS READY
	tape_filesize=tape_filetell=0;
	tape_type=-1; return tape_n=-1,/*tape_output=tape_status=*/0;
}

int tape_catalog(char *t,int x) // fills the buffer `t` of size `x` with the tape contents as an ASCIZ list; returns <0 ERROR, >=0 CURRENT BLOCK
{
	if (!tape) return -1; // no tape!
	char *u=t;
	switch (tape_type)
	{
		case +1: // TAP
			for (int n=0,m;n<25;++n) // we can afford ignoring `x` because this list is always short
			{
				m=20+(tape_filesize-20)*n/25;
				u+=1+sprintf(u,"%010d -- %02d%%",m,n*4);
				if (m<=tape_filetell) x=n;
			}
			return *u=0,x;
		case +2: // T64
			for (int n=0;n<tape_filesize;++n) // ditto
			{
				char s[17],m; for (m=0;m<16;++m)
				{
					int k=tape_buffer[80+n*32+m]&127;
					if (k<32) k='?'; s[m]=k;
				}
				while (m>0&&s[m-1]==' ') --m; // trim spaces!
				s[m]=0;
				u+=1+sprintf(u,"%010d -- PROGRAM '%s', %d bytes",n,s,mgetii(&tape_buffer[68+n*32])-mgetii(&tape_buffer[66+n*32]));
				if (n<=tape_filetell) x=n;
			}
			return *u=0,x;
	}
	return -1; // unknown type!
}
int tape_select(int i)
{
	if (!tape) return -1; // no tape!
	switch (tape_type)
	{
		case +1: // TAP
			return tape_seek(i),tape_n=0;
		case +2: // T64
			return tape_filetell=i,0;
	}
	return -1; // unknown type!
}

void tape_main(int t) // play the current tape back; `t` is nonzero!
{
	switch (tape_type)
	{
		case +1: // TAP file?
			tape_n-=t; int watchdog=127; // the watchdog catches corrupted tapes!!
			while (tape_n<=0)
			{
				if (((cia_port_13[0]|=16)&CIA_TABLE_0[13]&31)&&!(cia_port_13[0]&128))
					cia_port_13[0]|=128,m6510_irq|=2; // throw tape CIA #1 IRQ!
				/*tape_status=1-tape_status;*/ // just for the echo
				if (!--watchdog)
					{ tape_close(); tape_signal=-1; break; } // bad tape!!
				if ((t=tape_getc())<0) // end of file?
				{
					if (tape_rewind)
						{ tape_seek(20); } // rewind tape, but allow reading more
					else
						{ tape_close(); tape_signal=-1; break; } // end of tape!!
				}
				else if (!t) // prefix?
				{
					if ((t=tape_getccc())<0) // can it be zero at all!?!?!?
						{ tape_close(); tape_signal=-1; break; } // bad tape!!
					else
						tape_n+=t; // prefix: 24-bit 1 MHz
				}
				else
					tape_n+=t*8; // no prefix: 8-bit 125 kHz
			}
			break;
	}
}

// ...

// behind the CIA: disc I/O handling -------------------------------- //

char disc_path[STRMAX]="",*disc_ram[4]={NULL,NULL,NULL,NULL},disc_canwrite[4]={0,0,0,0};
FILE *disc[4]={NULL,NULL,NULL,NULL};

int disc_close(int d)
{
	if (disc_canwrite[d]<0) // can write AND must write?
		fseek(disc[d],0,SEEK_SET),fwrite1(disc_ram[d],683<<8,disc[d]);
	if (disc_ram[d])
		free(disc_ram[d]),disc_ram[d]=NULL;
	if (disc[d])
		puff_fclose(disc[d]),disc[d]=NULL;
	return 0;
}
void disc_closeall(void)
{
	disc_close(0);
	disc_close(1);
}

int disc_create(char *s) // create a blank D64 disc on path `s`; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"wb");
	if (!f) return 1; // cannot create file!
	char t[256],u[256]=
	{
		18,1,65,0,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,
		21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,
		31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,
		-1,31,17,-4,-1,7,19,-1,-1,7,19,-1,-1,7,19,-1,-1,7,19,-1,-1,7,19,-1,-1,
		7,19,-1,-1,7,18,-1,-1,3,18,-1,-1,3,18,-1,-1,3,18,-1,-1,3,18,-1,-1,3,
		18,-1,-1,3,17,-1,-1,1,17,-1,-1,1,17,-1,-1,1,17,-1,-1,1,17,-1,-1,1,
		85,78,84,73,84,76,69,68,-96,-96,-96,-96,-96,-96,-96,-96,-96,-96,48,32,
		-96,50,65,-96,-96,-96,-96,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // "UNTITLED"
	};
	MEMZERO(t); for (int i=0;i<357;++i) fwrite1(t,256,f); // first 357 sectors
	fwrite1(u,256,f); // catalogue in the middle of the disc
	t[1]=-1; fwrite1(t,256,f); t[1]=0; // end of catalogue
	for (int i=359;i<683;++i) fwrite1(t,256,f); // last 324 sectors
	return fclose(f),0;
}

int disc_open(char *s,int drive,int canwrite) // open a disc file from path `s`, into drive 8+`drive`; 0 OK, !0 ERROR
{
	disc_close(drive);
	if (!(disc[drive]=puff_fopen(s,canwrite?"rb+":"rb")))
		return 1; // cannot open file!
	if (!(disc_ram[drive]=malloc(684<<8))) // a little more room, see below
		return puff_fclose(disc[drive]),disc[drive]=NULL,1; // cannot load file!
	if (fread1(disc_ram[drive],684<<8,disc[drive])!=683<<8)
		return disc_close(drive),1; // improper file!
	disc_canwrite[drive]=canwrite; // 0 or 1; later, 1 may become -1 (modified disc)
	if (disc_path!=s) strcpy(disc_path,s); return 0;
}

// ...

// CPU-HARDWARE-VIDEO-AUDIO INTERFACE =============================== //

#define audio_channel 16383 // =32767 /2 channels (+A, -B, +C and -DIGI)

#define SID_TICK_STEP 16
#define SID_MAIN_EXTRABITS 0
#if AUDIO_CHANNELS > 1
//BYTE sid_muted[3]={0,0,0}; // optional muting of channels
const int sid_stereos[][3]={{0,0,0},{+256,0,-256},{+128,0,-128},{+64,0,-64}};
#endif
int sid_extras=0;
#include "cpcec-m8.h"

VIDEO_UNIT *video_impact;
BYTE vicii_nohits=0; // cheating!!
void video_sprite_1x1b(int x,BYTE i,BYTE q) // SINGLE HI-RES
{
	VIDEO_UNIT p=video_clut[23+i];
	BYTE *s=&vicii_memory[vicii_sprite_h[i]*64+vicii_sprite_l[i]]; WORD j=0; i=1<<i;
	if (!q) // invisible?
	{
		for (BYTE n=3;n;--n)
			for (BYTE b=*s++,m=128;m;++x,m>>=1)
				if (b&m)
					j|=vicii_impact[x  ]|=i;
	}
	else if (VICII_TABLE[27]&i) // background?
	{
		for (BYTE n=3;n;--n)
			for (BYTE b=*s++,m=128;m;++x,m>>=1)
				if (b&m)
				{
					j|=vicii_impact[x  ]|=i;
					if (vicii_impact[x  ]<256)
						video_impact[x*2  ]=video_impact[x*2+1]=p;
				}
	}
	else // foreground!
		for (BYTE n=3;n;--n)
			for (BYTE b=*s++,m=128;m;++x,m>>=1)
				if (b&m)
				{
					j|=vicii_impact[x  ]|=i;
					video_impact[x*2]=video_impact[x*2+1]=p;
				}
	if (vicii_nohits) return;
	if (j&256) VICII_TABLE[31]|=i,VICII_TABLE[25]|=2; if ((j&=255)&&i!=j) VICII_TABLE[30]|=j,VICII_TABLE[25]|=4;
}
void video_sprite_2x1b(int x,BYTE i,BYTE q) // DOUBLE HI-RES
{
	VIDEO_UNIT p=video_clut[23+i];
	BYTE *s=&vicii_memory[vicii_sprite_h[i]*64+vicii_sprite_l[i]]; WORD j=0; i=1<<i;
	if (!q) // invisible?
	{
		for (BYTE n=3;n;--n)
			for (BYTE b=*s++,m=128;m;x+=2,m>>=1)
				if (b&m)
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i;
	}
	else if (VICII_TABLE[27]&i) // background?
	{
		for (BYTE n=3;n;--n)
			for (BYTE b=*s++,m=128;m;x+=2,m>>=1)
				if (b&m)
				{
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i;
					if (vicii_impact[x  ]<256)
						video_impact[x*2  ]=video_impact[x*2+1]=p;
					if (vicii_impact[x+1]<256)
						video_impact[x*2+2]=video_impact[x*2+3]=p;
				}
	}
	else // foreground!
		for (BYTE n=3;n;--n)
			for (BYTE b=*s++,m=128;m;x+=2,m>>=1)
				if (b&m)
				{
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i;
					video_impact[x*2]=video_impact[x*2+1]=video_impact[x*2+2]=video_impact[x*2+3]=p;
				}
	if (vicii_nohits) return;
	if (j&256) VICII_TABLE[31]|=i,VICII_TABLE[25]|=2; if ((j&=255)&&i!=j) VICII_TABLE[30]|=j,VICII_TABLE[25]|=4;
}
void video_sprite_1x2b(int x,BYTE i,BYTE q) // SINGLE LO-RES
{
	VIDEO_UNIT p,pp[4]={0,video_clut[21],video_clut[23+i],video_clut[22]};
	BYTE *s=&vicii_memory[vicii_sprite_h[i]*64+vicii_sprite_l[i]]; WORD j=0; i=1<<i;
	if (!q) // invisible?
	{
		for (BYTE n=3;n;--n)
			for (BYTE m=4,b=*s++;m;b<<=2,x+=2,--m)
				if (b&192)
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i;
	}
	else if (VICII_TABLE[27]&i) // background?
	{
		for (BYTE n=3;n;--n)
			for (BYTE m=4,b=*s++,c;m;b<<=2,x+=2,--m)
				if (c=b>>6)
				{
					p=pp[c];
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i;
					if (vicii_impact[x  ]<256)
						video_impact[x*2  ]=video_impact[x*2+1]=p;
					if (vicii_impact[x+1]<256)
						video_impact[x*2+2]=video_impact[x*2+3]=p;
				}
	}
	else // foreground!
		for (BYTE n=3;n;--n)
			for (BYTE m=4,b=*s++,c;m;b<<=2,x+=2,--m)
				if (c=b>>6)
				{
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i;
					video_impact[x*2]=video_impact[x*2+1]=video_impact[x*2+2]=video_impact[x*2+3]=pp[c];
				}
	if (vicii_nohits) return;
	if (j&256) VICII_TABLE[31]|=i,VICII_TABLE[25]|=2; if ((j&=255)&&i!=j) VICII_TABLE[30]|=j,VICII_TABLE[25]|=4;
}
void video_sprite_2x2b(int x,BYTE i,BYTE q) // DOUBLE LO-RES
{
	VIDEO_UNIT p,pp[4]={0,video_clut[21],video_clut[23+i],video_clut[22]};
	BYTE *s=&vicii_memory[vicii_sprite_h[i]*64+vicii_sprite_l[i]]; WORD j=0; i=1<<i;
	if (!q) // hidden?
	{
		for (BYTE n=3;n;--n)
			for (BYTE m=4,b=*s++;m;b<<=2,x+=4,--m)
				if (b&192)
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i,j|=vicii_impact[x+2]|=i,j|=vicii_impact[x+3]|=i;
	}
	else if (VICII_TABLE[27]&i) // background?
	{
		for (BYTE n=3;n;--n)
			for (BYTE m=4,b=*s++,c;m;b<<=2,x+=4,--m)
				if (c=b>>6)
				{
					p=pp[c];
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i,j|=vicii_impact[x+2]|=i,j|=vicii_impact[x+3]|=i;
					if (vicii_impact[x  ]<256)
						video_impact[x*2  ]=video_impact[x*2+1]=p;
					if (vicii_impact[x+1]<256)
						video_impact[x*2+2]=video_impact[x*2+3]=p;
					if (vicii_impact[x+2]<256)
						video_impact[x*2+4]=video_impact[x*2+5]=p;
					if (vicii_impact[x+3]<256)
						video_impact[x*2+6]=video_impact[x*2+7]=p;
				}
	}
	else // foreground!
		for (BYTE n=3;n;--n)
			for (BYTE m=4,b=*s++,c;m;b<<=2,x+=4,--m)
				if (c=b>>6)
				{
					j|=vicii_impact[x  ]|=i,j|=vicii_impact[x+1]|=i,j|=vicii_impact[x+2]|=i,j|=vicii_impact[x+3]|=i;
					video_impact[x*2]=video_impact[x*2+1]=video_impact[x*2+2]=video_impact[x*2+3]=
						video_impact[x*2+4]=video_impact[x*2+5]=video_impact[x*2+6]=video_impact[x*2+7]=pp[c];
				}
	if (vicii_nohits) return;
	if (j>=256) VICII_TABLE[31]|=i,VICII_TABLE[25]|=2; if ((j&=255)&&i!=j) VICII_TABLE[30]|=j,VICII_TABLE[25]|=4;
}

#if 1 // no contention at all

#define m6510_vicii_reading() 1
#define m6510_vicii_writing() 1

#else // still incomplete :_(

BYTE vicii_badline,vicii_sprites;

// ...........[[[[[[########################################]]]]]] //
// 123456789012345678901234567890123456789012345678901234567890123 //
// 3344556677...CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC#...001122 //
// ........................:  63   01   02   03   04   05   06   07   08   09   10  11...................................................................................54  55   56   57   58   59   60   61   62
BYTE vicii_chr_reading[66]={ 0X1C,0X18,0X38,0X30,0X70,0X60,0XE0,0XC0,0XC0,0X80,0X80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0X01,0X01,0X03,0X03,0X07,0X06,0X0E,0X0C };
BYTE vicii_chr_allbits[66]={ 0X0C,0X14,0X14,0X28,0X28,0X50,0X50,0XA0,0XA0,0X00,0X00,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0X00,0X00,0X00,0X03,0X03,0X06,0X06,0X0C };
BYTE vicii_chr_writing[66]={ 0X04,0X08,0X08,0X10,0X10,0X20,0X20,0X40,0X40,0X80,0X80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0X00,0X00,0X00,0X01,0X01,0X02,0X02,0X04 };

#define VICII_CHR_BADLINE (!(vicii_mode&32)&&vicii_chr_y>=48&&vicii_chr_y<248&&!((vicii_chr_y^VICII_TABLE[17])&7))
#define VICII_CHR_SPRITES ((vicii_mode&32)?0:((vicii_sprite_l[0]<63?1:0)+(vicii_sprite_l[1]<63?2:0)+(vicii_sprite_l[2]<63?4:0)+(vicii_sprite_l[3]<63?8:0)\
	+(vicii_sprite_l[4]<63?16:0)+(vicii_sprite_l[5]<63?32:0)+(vicii_sprite_l[6]<63?64:0)+(vicii_sprite_l[7]<63?128:0)))
#define VICII_CHR_BETWEEN (!(~vicii_sprites&vicii_chr_allbits[vicii_chr_x]))

int m6510_vicii_reading(void)
{
	int t=1; for (;;++t)
	{
		if (++vicii_chr_x<54) // before the PAL-NTSC safe zone
		{
			if (vicii_chr_x<11) // sprites 3..7
			{
				if (!(vicii_sprites&vicii_chr_reading[vicii_chr_x])&&!VICII_CHR_BETWEEN) return t;
			}
			else // 3-char prefix + 40 characters
			{
				if (vicii_chr_x==11) { if (!(vicii_badline=VICII_CHR_BADLINE)) return t; } // recalc! quit?
				else if (!vicii_badline) return t; // quit!
				//if (vicii_chr_y<56) printf("\n"); printf("R%03d ",vicii_chr_y);
				t+=54-vicii_chr_x,vicii_chr_x=54; return t; // badline!
			}
		}
		else // inside the safe zone or sprites 0..2
		{
			if (vicii_chr_x==63-8) vicii_sprites=VICII_CHR_SPRITES; // recalc
			if (vicii_chr_x>=63) ++vicii_chr_y,vicii_chr_x=0; // rewind!
			if (!(vicii_sprites&vicii_chr_reading[vicii_chr_x])&&!VICII_CHR_BETWEEN) return t;
		}
	}
}
int m6510_vicii_writing(void)
{
	int t=1; for (;;++t)
	{
		if (++vicii_chr_x<54) // before the PAL-NTSC safe zone
		{
			if (vicii_chr_x<11) // sprites 3..7
			{
				if (!(vicii_sprites&vicii_chr_writing[vicii_chr_x])&&!VICII_CHR_BETWEEN) return t;
			}
			else // 3-char prefix + 40 characters
			{
				if (vicii_chr_x==11) return vicii_badline=VICII_CHR_BADLINE,t; // recalc, quit!
				if (vicii_chr_x<14||!vicii_badline) return t; // quit!
				//if (vicii_chr_y<56) printf("\n"); printf("W%03d ",vicii_chr_y);
				t+=54-vicii_chr_x,vicii_chr_x=54; return t; // badline!
			}
		}
		else // inside the safe zone or sprites 0..2
		{
			if (vicii_chr_x==63-8) return vicii_sprites=VICII_CHR_SPRITES,t; // recalc and quit!
			if (vicii_chr_x>=63) ++vicii_chr_y,vicii_chr_x=0; // rewind!
			if (!(vicii_sprites&vicii_chr_writing[vicii_chr_x])&&!VICII_CHR_BETWEEN) return t;
		}
	}
}

#endif

void video_main(int t)
{
	static BYTE buffer[64]=""; static short vicii_cursor=0,vicii_backup=0; do
	{
		BYTE attrib=buffer[vicii_pos_x],bitmap,colour=VICII_COLOR[vicii_cursor];
		if (vicii_lines<0||vicii_lines>=200||vicii_mode&-8)
			bitmap=0;//vicii_memory[0X3FFF];
		else
		{
			if (vicii_mode&4) // EXTENDED
				bitmap=vicii_bitmap[(attrib&63) *8+vicii_pos_z];
			else if (vicii_mode&2) // GRAPHICS
				bitmap=vicii_bitmap[vicii_cursor*8+vicii_pos_z];
			else // CHARACTER
				bitmap=vicii_bitmap[ attrib     *8+vicii_pos_z];
			if (!(vicii_mode&1)||(colour<8&&!(vicii_mode&2))) // MASK!
				vicii_impact[vicii_pixel  ]=(bitmap&128)<<1,
				vicii_impact[vicii_pixel+1]=(bitmap& 64)<<2,
				vicii_impact[vicii_pixel+2]=(bitmap& 32)<<3,
				vicii_impact[vicii_pixel+3]=(bitmap& 16)<<4,
				vicii_impact[vicii_pixel+4]=(bitmap&  8)<<5,
				vicii_impact[vicii_pixel+5]=(bitmap&  4)<<6,
				vicii_impact[vicii_pixel+6]=(bitmap&  2)<<7,
				vicii_impact[vicii_pixel+7]=(bitmap&  1)<<8;
			else
				vicii_impact[vicii_pixel  ]=vicii_impact[vicii_pixel+1]=(bitmap&128)<<1,
				vicii_impact[vicii_pixel+2]=vicii_impact[vicii_pixel+3]=(bitmap& 32)<<3,
				vicii_impact[vicii_pixel+4]=vicii_impact[vicii_pixel+5]=(bitmap&  8)<<5,
				vicii_impact[vicii_pixel+6]=vicii_impact[vicii_pixel+7]=(bitmap&  2)<<7;
		}
		if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y&&video_pos_x>=VIDEO_OFFSET_X-15&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X)
			switch (vicii_mode)
			{
				VIDEO_UNIT p;
				case  1: // LO-RES CHARACTER
					if (colour>=8)
					{
						VIDEO_UNIT p1x[4]={video_clut[17],video_clut[18],video_clut[19],video_clut[colour-8]};
						VIDEO_NEXT=p=p1x[ bitmap>>6   ]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=p1x[(bitmap>>4)&3]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=p1x[(bitmap>>2)&3]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=p1x[ bitmap    &3]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						break;
					}
					// no `break` here!
				case  0: // HI-RES CHARACTER
					{
						VIDEO_UNIT p01=video_clut[colour];
						VIDEO_NEXT=p=(bitmap&128)?p01:video_clut[17]; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 64)?p01:video_clut[17]; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 32)?p01:video_clut[17]; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 16)?p01:video_clut[17]; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  8)?p01:video_clut[17]; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  4)?p01:video_clut[17]; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  2)?p01:video_clut[17]; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  1)?p01:video_clut[17]; VIDEO_NEXT=p;
					}
					break;
				case  2: // HI-RES GRAPHICS
					{
						VIDEO_UNIT p20=video_clut[attrib&15],p21=video_clut[attrib>>4];
						VIDEO_NEXT=p=(bitmap&128)?p21:p20; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 64)?p21:p20; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 32)?p21:p20; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 16)?p21:p20; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  8)?p21:p20; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  4)?p21:p20; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  2)?p21:p20; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  1)?p21:p20; VIDEO_NEXT=p;
					}
					break;
				case  3: // LO-RES GRAPHICS
					{
						VIDEO_UNIT p3x[4]={video_clut[17],video_clut[attrib>>4],video_clut[attrib&15],video_clut[colour]};
						VIDEO_NEXT=p=p3x[ bitmap>>6   ]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=p3x[(bitmap>>4)&3]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=p3x[(bitmap>>2)&3]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=p3x[ bitmap    &3]; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					}
					break;
				case  4: // HI-RES EXTENDED
					{
						VIDEO_UNIT p40=video_clut[(attrib>>6)+17],p41=video_clut[colour];
						VIDEO_NEXT=p=(bitmap&128)?p41:p40; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 64)?p41:p40; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 32)?p41:p40; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap& 16)?p41:p40; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  8)?p41:p40; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  4)?p41:p40; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  2)?p41:p40; VIDEO_NEXT=p;
						VIDEO_NEXT=p=(bitmap&  1)?p41:p40; VIDEO_NEXT=p;
					}
					break;
				default: // BORDER!!
					p=video_clut[16];
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					break;
				//case  5: // INVALID! (1/3)
				//case  6: // INVALID! (2/3)
				//case  7: // INVALID! (3/3)
				case  8: case  9: case 10: case 11: // BACKGROUND (1/2)
				case 12: case 13: case 14: case 15: // BACKGROUND (2/2)
					p=(bitmap&128)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=(bitmap& 64)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=(bitmap& 32)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=(bitmap& 16)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=(bitmap&  8)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=(bitmap&  4)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=(bitmap&  2)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					p=(bitmap&  1)?video_clut[0]:video_clut[17]; VIDEO_NEXT=p; VIDEO_NEXT=p;
					break;
			}
		else
			video_target+=16;

		video_pos_x+=16; vicii_pixel+=8;

		vicii_cursor=(vicii_cursor+1)&0X3FF;

		static short vicii_l_n=0,vicii_r_n=0; static VIDEO_UNIT vicii_l_p,vicii_r_p; // border masks
		if (++vicii_pos_x>=63)
		{
			video_impact=video_target-video_pos_x;
			int q=frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;
			if (!(vicii_mode&32))
				for (BYTE i=8,j=128;i--;j>>=1) // draw the sprites?
					if (vicii_sprite_l[i]<63&&(VICII_TABLE[21]&j))
					{
						int x=VICII_TABLE[i*2]; if (VICII_TABLE[16]&j) if ((x+=256)>384) x-=63<<3; // not 64<<3!
						if ((x+=104)<448)
						{
							if (VICII_TABLE[28]&j)
								{ if (VICII_TABLE[29]&j) { if (x>80) { video_sprite_2x2b(x,i,q); } } else { if (x>104) video_sprite_1x2b(x,i,q); } }
							else
								{ if (VICII_TABLE[29]&j) { if (x>80) { video_sprite_2x1b(x,i,q); } } else { if (x>104) video_sprite_1x1b(x,i,q); } }
						}
					}
			if (q)
			{
				if (vicii_l_n>0) // draw the left border?
				{
					VIDEO_UNIT *t=video_impact+VIDEO_OFFSET_X;
					do *t++=vicii_l_p; while (--vicii_l_n);
				}
				if (vicii_r_n>0) // draw the right border?
				{
					VIDEO_UNIT *t=video_impact+VIDEO_OFFSET_X+VIDEO_PIXELS_X;
					do *--t=vicii_r_p; while (--vicii_r_n);
				}
				video_drawscanline();
			}
			else
				vicii_l_n=vicii_r_n=0; // reset the border masks!
			for (int i=8,j=128;i--;j>>=1) // reload or update sprites?
			{
				vicii_sprite_h[i]=vicii_attrib[1016+i];
				if (!((vicii_pos_y^VICII_TABLE[i*2+1])&255))
					vicii_sprite_l[i]=vicii_sprite_z[i]=0;
				else if (vicii_sprite_l[i]<63&&(!(VICII_TABLE[23]&j)||!(vicii_sprite_z[i]=!vicii_sprite_z[i])))
					vicii_sprite_l[i]+=3;
			}
			MEMZERO(vicii_impact);

			frame_pos_y+=2,video_pos_y+=2,video_target+=VIDEO_LENGTH_X*2-video_pos_x; video_pos_x=vicii_pixel=0; session_signal|=session_signal_scanlines; // scanline event!

			++vicii_lines; if (++vicii_pos_y>=312)
			{
				vicii_pos_y=vicii_backup=vicii_pos_z=0;
				vicii_mode|=64; // forbid character rendering until the first badline happens

				// handle decimals, seconds, minutes, hours and alarm in a single block;
				// bit 30 keeps the clock from updating (f.e. while setting the time)
				#define CIAS_24HOURS(x,y,z) do{ if (!(x&0X40000000)) { if ((++x&15)>=10) \
				if (!(~(x+= 256 - 10 )&0X00000A00)) if (!(~(x+=0X00000600)&0X00006000)) \
				if (!(~(x+=0X0000A000)&0X000A0000)) if (!(~(x+=0X00060000)&0X00600000)) \
				{ if (!(~(x+=0X00A00000)&0X13000000)) x-=0X92000000; \
				else if (!(~(x&0X0A000000))) x+=0X06000000; } \
				if (equalsiiii(y,x)) z; } }while(0)
				static char decimal=0; if (++decimal>=5)
				{
					decimal=0;
					CIAS_24HOURS(cia_hhmmssd[0],&CIA_TABLE_0[8],0);
					CIAS_24HOURS(cia_hhmmssd[1],&CIA_TABLE_1[8],0);
				}

				if (!video_framecount) video_endscanlines(); // frame is complete!
				video_newscanlines(video_pos_x,0); // vertical reset
				session_signal|=SESSION_SIGNAL_FRAME+session_signal_frames; // frame event!
			}
			else if (vicii_pos_y== 51) // PAL 25CHAR BEGIN
				{ vicii_lines=3-(VICII_TABLE[17]&7); if ((VICII_TABLE[17]&24)==24) { vicii_mode&=~32; } }
			else if (vicii_pos_y== 55) // PAL 24CHAR BEGIN
				{ if ((VICII_TABLE[17]&24)==16) { vicii_mode&=~32; } }
			else if (vicii_pos_y==247) // PAL 24CHAR END
				{ if (!(VICII_TABLE[17]&8)) { vicii_mode|= 32; } }
			else if (vicii_pos_y==251) // PAL 25CHAR END
				{ if (  VICII_TABLE[17]&8 ) { vicii_mode|= 32; } }

			vicii_pos_x=0; //vicii_setmaps(); vicii_setmode();
			if (vicii_pos_y==vicii_irq_y) VICII_TABLE[25]|=1;
			if (VICII_TABLE[25]&VICII_TABLE[26]&15)
				VICII_TABLE[25]|=128,m6510_irq|=1; // throw VIC-II IRQ!
		}
		else if (vicii_pos_x==16)
		{
			if (!(vicii_mode&32))
			{
				int i=VICII_TABLE[22]&7; vicii_pixel+=i; video_pos_x+=i*2; // "shift" the screen right to horizontal scroll
				if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
					while (i-->0)
						VIDEO_NEXT=video_clut[17],VIDEO_NEXT=video_clut[17]; // the left gap is NOT the border!!
				else
					video_target+=i*2;
			}
			{ if (  VICII_TABLE[22]&8 ) { if (  vicii_mode&16 ) { vicii_l_n=(VIDEO_PIXELS_X-640   )/2,vicii_l_p=video_clut[16]; } } }
			vicii_mode&=~16;
			vicii_cursor=vicii_backup;
			if (!(vicii_mode&32)&&vicii_pos_y>=48&&vicii_pos_y<248&&!((vicii_pos_y^VICII_TABLE[17])&7)) // do we need to fetch a "badline"?
			{
				vicii_mode&=~64;
				vicii_pos_z=0;
				for (int i=0;i<40;++i)
					buffer[16+i]=vicii_attrib[(vicii_backup+i)&0X3FF];
			}
		}
		else if (vicii_pos_x==17)
			{ if (!(VICII_TABLE[22]&8)) { if (! vicii_l_n     ) { vicii_l_n=(VIDEO_PIXELS_X-640+32)/2,vicii_l_p=video_clut[16]; } } }
		else if (vicii_pos_x==55)
			{ if (!(VICII_TABLE[22]&8)) { if (!(vicii_mode&16)) { vicii_mode|= 16; vicii_r_n=(VIDEO_PIXELS_X-640+32)/2,vicii_r_p=video_clut[16]; } } }
		else if (vicii_pos_x==56)
		{
			int i=vicii_pixel&7; vicii_pixel-=i,video_target-=i*2,video_pos_x-=i*2; // "shift" the screen back to normal
			if (vicii_pos_y>=48&&vicii_pos_y<256)
			{
				if (vicii_pos_z>=7)
					vicii_backup=vicii_cursor,vicii_pos_z=0;
				else
					++vicii_pos_z;
			}
			{ if (  VICII_TABLE[22]&8 ) { if (!(vicii_mode&16)) { vicii_mode|= 16; vicii_r_n=(VIDEO_PIXELS_X-640   )/2,vicii_r_p=video_clut[16]; } } }
		}
	}
	while (--t>0);
}

int /*tape_loud=1,*/tape_song=0;
void audio_main(int t)
{
	sid_main(t,0/*+(((tape_status^tape_output)&tape_loud)<<12)*/);
}

// autorun runtime logic -------------------------------------------- //

BYTE snap_done; // avoid accidents with ^F2, see all_reset()
char autorun_path[STRMAX]="",autorun_line[STRMAX];
int autorun_mode=0,autorun_t=0,autorun_i=0;

BYTE autorun_kbd[16]; // automatic keypresses
#define autorun_kbd_set(k) (autorun_kbd[k>>3]|=1<<(k&7))
#define autorun_kbd_res(k) (autorun_kbd[k>>3]&=~(1<<(k&7)))

int autorun_type(char *s,int n)
{
	autorun_t=9; if (!(m6510_pc.w>=0XE5CD&&m6510_pc.w<0XE5D6)) return -1; // try again later!
	if (n)
	{
		memcpy(&mem_ram[mem_ram[0XD1]+mem_ram[0XD2]*256+mem_ram[0XD3]],s,n); // copy string
		mem_ram[0XD3]+=n; autorun_kbd_set(0001); // move the cursor forward and press RETURN
	}
	return 0;
}

INLINE void autorun_next(void)
{
	switch (autorun_mode)
	{
		case 1: // TYPE "RUN", PRESS RETURN...
			if (autorun_type("\022\025\016",3)) break;
			// no `break`!
		case 2: // INJECT FILE...
			if (autorun_type(NULL,0)) break;
			if (autorun_i) memcpy(&mem_ram[0X0801],&mem_ram[1<<16],autorun_i);
			autorun_i+=0X0801; // end of prog, rather than prog size
			mputii(&mem_ram[0X00AC],autorun_i); mputii(&mem_ram[0X00AE],autorun_i); // poke end-of-file KERNAL pokes
			mputii(&mem_ram[0X002D],autorun_i); mputii(&mem_ram[0X002F],autorun_i); mputii(&mem_ram[0X0031],autorun_i); // end-of-file BASIC pokes
			autorun_i=0; autorun_mode=9;
			break;
		case 3: // TYPE "LOAD", PRESS RETURN...
			if (autorun_type("\014\017\001\004",4)) break;
			++autorun_mode;
			break;
		case 4: // RELEASE RETURN...
			autorun_t=1;
			autorun_kbd_res(0001);
			++autorun_mode;
			break;
		case 5: // TYPE "RUN", PRESS RETURN...
			if (m6510_pc.w>=0XE000&&autorun_type("\022\025\016",3)) break;
			autorun_t=autorun_mode=9;
			break;
		case 8: // TYPE "LOAD"*",8,1", PRESS RETURN...
			if (autorun_type("\014\017\001\004\042\052\042" ",8,1",11)) break;
			autorun_mode=9;
			break;
		case 9: // ...RELEASE RETURN!
			autorun_kbd_res(0001);
			autorun_mode=0;
			break;
	}
}

// CPU: MOS 6510 MICROPROCESSOR ===================================== //

void m6502_main(int); // required by the 6510; will be defined later on!

void m6510_sync(int t) // the MOS 6510 "drives" the whole system
{
	static int r=0; main_t+=t;
	int tt=(r+=t)/multi_t; // calculate base value of `t`
	r-=(t=tt*multi_t); // adjust `t` and keep remainder
	if (t>0)
	{
		if (tt>0)
		{
			audio_queue+=tt;
			if (audio_dirty&&!audio_disabled)
				audio_main(audio_queue),audio_dirty=audio_queue=0;
			if (tape&&tape_enabled>(mmu_out&32))
				/*audio_dirty|=tape_loud,*/tape_main(tt);
			/*
			else if (!disc_disabled) // else!? *!*
				m6502_main(tt);
			*/
			video_main(tt);
			t=0; if ((CIA_TABLE_0[14]&33)==1) // update the CIA #1 timer A
			{
				if (cia_count_a[0]>tt)
					cia_count_a[0]-=tt;
				else
				{
					t=1; if (((cia_port_13[0]|=1)&CIA_TABLE_0[13]&31)&&!(cia_port_13[0]&128))
						cia_port_13[0]|=128,m6510_irq|=2;
					if ((CIA_TABLE_0[14]&8)) // just once?
						/*cia_count_a[0]=-1,*/CIA_TABLE_0[14]&=~1;
					//else
						cia_count_a[0]+=1+CIA_TABLE_0[4]+CIA_TABLE_0[5]*256-tt;
				}
			}
			if ((CIA_TABLE_0[15]&33)==1) // update the CIA #1 timer B
			{
				if (!(CIA_TABLE_0[15]&64))
					t=tt;
				if (cia_count_b[0]>t)
					cia_count_b[0]-=t;
				else //if (t)
				{
					if (((cia_port_13[0]|=2)&CIA_TABLE_0[13]&31)&&!(cia_port_13[0]&128))
						cia_port_13[0]|=128,m6510_irq|=2;
					if ((CIA_TABLE_0[15]&8)) // just once?
						/*cia_count_b[0]=-1,*/CIA_TABLE_0[15]&=~1;
					//else
						cia_count_b[0]+=1+CIA_TABLE_0[6]+CIA_TABLE_0[7]*256-t;
				}
			}
			t=0; if ((CIA_TABLE_1[14]&33)==1) // update the CIA #2 timer A
			{
				if (cia_count_a[1]>tt)
					cia_count_a[1]-=tt;
				else
				{
					t=1; if (((cia_port_13[1]|=1)&CIA_TABLE_1[13]&31)&&!(cia_port_13[1]&128))
						cia_port_13[1]|=128,m6510_irq|=128;
					if ((CIA_TABLE_1[14]&8)) // just once?
						/*cia_count_a[1]=-1,*/CIA_TABLE_1[14]&=~1;
					//else
						cia_count_a[1]+=1+CIA_TABLE_1[4]+CIA_TABLE_1[5]*256-tt;
				}
			}
			if ((CIA_TABLE_1[15]&33)==1) // update the CIA #2 timer B
			{
				if (!(CIA_TABLE_1[15]&64))
					t=tt;
				if (cia_count_b[1]>t)
					cia_count_b[1]-=t;
				else //if (t)
				{
					if (((cia_port_13[1]|=2)&CIA_TABLE_1[13]&31)&&!(cia_port_13[1]&128))
						cia_port_13[1]|=128,m6510_irq|=128;
					if ((CIA_TABLE_1[15]&8)) // just once?
						/*cia_count_b[1]=-1,*/CIA_TABLE_1[15]&=~1;
					//else
						cia_count_b[1]+=1+CIA_TABLE_1[6]+CIA_TABLE_1[7]*256-t;
				}
			}
		}
	}
}

BYTE key2joy_flag=0; // alternate joystick ports
BYTE kbd_bit0[10]; // combined keyboard+joystick bits

// the C64 special I/O addresses are divided in several spaces:
// 0000-0001 : memory configuration
// 0002-00FF : a window to PAGEZERO
// D000-D3FF : video configuration; mask is 003F
// D400-D7FF : audio configuration; mask is 001F
// D800-DBFF : color configuration
// DC00-DCFF : CIA #1 configuration; mask is 000F
// DD00-DDFF : CIA #2 configuration; mask is 000F
// DE00-DFFF : hardware expansions
// remember: 0100-CFFF + E000-FFFF get filtered out in advance!

BYTE sid_detection=-1; // kludge for "BOX CHECK TEST" :-(

#define m6510_sid_extras0() (sid_multichip1=sid_multichip2=0)
BYTE sid_multichip1=0; // kludge for multi SID chips at $D420 and $D440 :-(
void m6510_sid_extras1(WORD w,BYTE b) { sid_multichip1=(w==24&&b==128)?4:0; }
BYTE sid_multichip2=0; // kludge for multi SID chips at $DE00 and $DF00 :-(
void m6510_sid_extras2(WORD w,BYTE b) { sid_multichip2=(w==24&&b==128)?6:0; }

// PORT $0001 input mask: 0XFF -0X17 (MMU + TAPE INP) -0X20 (TAPE MOTOR) =0XC8!
#define MMU_CFG_GET(w) (w?(mmu_cfg[0]&mmu_cfg[1])+(~mmu_cfg[0]&((mmu_out&0XC8)+mmu_inp)):mmu_cfg[0])
#define MMU_CFG_SET(w,b) do{ mmu_cfg[w]=b; mmu_update(); }while(0)

BYTE m6510_recv(WORD w) // receive a byte from the I/O address `w`
{
	if (w<0XD800)
		if (w<0XD000)
			if (w<0X0002) // memory configuration, $0000-$0001
				return MMU_CFG_GET(w);
			else // a window to PAGEZERO, $0002-$00FF
				return mem_ram[w];
		else
			if (w<0XD400) // video configuration, $D000-$D03F
				switch (w&=63)
				{
					case 17: return (VICII_TABLE[w]&127)+((vicii_pos_y>>1)&128); // $D011: CONTROL REGISTER 1
					case 18: return vicii_pos_y; // $D012: RASTER COUNTER (BITS 0-7; BIT 8 IS IN $D011)
					case 22: return VICII_TABLE[22]|0XC0; // $D016: CONTROL REGISTER 2
					case 24: return VICII_TABLE[24]|0X01; // $D018: MEMORY CONTROL REGISTER
					case 25: return VICII_TABLE[25]|0X70; // $D019: INTERRUPT REQUEST REGISTER
					case 30: return w=VICII_TABLE[30],VICII_TABLE[30]=0,w; // $D01E: SPRITES-SPRITES COLLISIONS
					case 31: return w=VICII_TABLE[31],VICII_TABLE[31]=0,w; // $D01F: SPRITES-SCENERY COLLISIONS
					case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39: case 40:
					case 41: case 42: case 43: case 44: case 45: case 46: // $D020-$D02E: COLOURS
					case 26: // $D01A: INTERRUPT REQUEST MASK
						return VICII_TABLE[w]|0XF0;
					case 47: // ???
					case 48: // "PENTAGRAM": this is ALWAYS $FF on a C64, but not on a C128!
					case 49: // ???
					case 60: // "GUNFRIGHT": this is ALWAYS $FF on a C64, but not on a SUPER CPU!
					case 61: case 62: case 63: return 255; // unused, always 255!
					default: return VICII_TABLE[w];
				}
			else // audio configuration, $D400-$D43F
			{
				if (w>=0XD420&&sid_extras) // beyond the range $D400-$D41F and with SID extensions?
				{
					if (sid_extras==1||sid_extras==3)
					{
						if (w<0XD440)
							return SID_TABLE[1][w-0XD420];
						if (w<0XD460&&sid_extras==3)
							return SID_TABLE[2][w-0XD440];
					}
					return 0XFF; // nothing here!
				}
				// the base chip, without extensions, spans the whole $D400-$D7FF range
				switch (w&=31)
				{
					case 25: case 26: return 255; // GAME PADDLES
					case 27: return ++sid_detection; // "BOX CHECK TEST" examines this port; see below
					case 28: return w=sid_multichip1,sid_multichip1=0,w;
					case 29: case 30: case 31: return 0;
					default: return SID_TABLE_0[w];
				}
			}
	else
		if (w<0XDD00)
			if (w<0XDC00) // color configuration, $D800-$DBFF
				return VICII_COLOR[w-0XD800]|240;
			else // CIA #1 configuration, $DC00-$DC0F
				switch (w&=15)
				{
					BYTE b;
					case  0: // reverse keyboard bitmap
						w=CIA_TABLE_0[0]|~CIA_TABLE_0[2];
						b=CIA_TABLE_0[1]|~CIA_TABLE_0[3];
						if (key2joy_flag) b&=~kbd_bit0[9];
						for (int j=1;j<256;j<<=1)
							if (!(b&j)) w&=~(((kbd_bit0[0]&j)?1:0)+((kbd_bit0[1]&j)?2:0)+((kbd_bit0[2]&j)?4:0)+((kbd_bit0[3]&j)?8:0)
								+((kbd_bit0[4]&j)?16:0)+((kbd_bit0[5]&j)?32:0)+((kbd_bit0[6]&j)?64:0)+((kbd_bit0[7]&j)?128:0));
						if (!key2joy_flag) w&=~kbd_bit0[9];
						return w;
					case  1: // keyboard bitmap
						w=CIA_TABLE_0[1]|~CIA_TABLE_0[3];
						b=CIA_TABLE_0[0]|~CIA_TABLE_0[2];
						if (!key2joy_flag) b&=~kbd_bit0[9];
						for (int i=0;i<8;++i)
							if (!(b&(1<<i)))
								w&=~kbd_bit0[i];
						if (key2joy_flag) w&=~kbd_bit0[9];
						return w;
					case  4: return cia_count_a[0];
					case  5: return cia_count_a[0]>>8;
					case  6: return cia_count_b[0]; // "BUBBLE BOBBLE" expects 255 here because TIMER B is never used!
					case  7: return cia_count_b[0]>>8;
					case  8: return cia_hhmmssd[0];
					case  9: return cia_hhmmssd[0]>>8;
					case 10: return cia_hhmmssd[0]>>16;
					case 11: return cia_hhmmssd[0]>>24;
					case 13: // acknowledge CIA #1 IRQ!
						if (!(CIA_TABLE_0[13]&16)) ++cia_port_13_n; // autodetect non-IRQ tape loaders ("TWIN WORLD")
						w=cia_port_13[0]; cia_port_13[0]=0;
						return m6510_irq&=~2,w;
					case 14: return CIA_TABLE_0[14]&~16;
					case 15: return CIA_TABLE_0[15]&~16;
					default: return CIA_TABLE_0[w];
				}
		else
			if (w<0XDE00) // CIA #2 configuration, $DD00-$DD0F
				switch (w&=15)
				{
					case  0: return ((CIA_TABLE_1[0]|~CIA_TABLE_1[2])&0X3F)+128;
					case  1: return CIA_TABLE_1[1]|~CIA_TABLE_1[3];
					case  4: return cia_count_a[1];
					case  5: return cia_count_a[1]>>8;
					case  6: return cia_count_b[1];
					case  7: return cia_count_b[1]>>8;
					case  8: return cia_hhmmssd[1];
					case  9: return cia_hhmmssd[1]>>8;
					case 10: return cia_hhmmssd[1]>>16;
					case 11: return cia_hhmmssd[1]>>24;
					case 13: // acknowledge CIA #2 NMI!
						w=cia_port_13[1]; cia_port_13[1]=0;
						return m6510_irq&=127,w;
					case 14: return CIA_TABLE_1[14]&~16;
					case 15: return CIA_TABLE_1[15]&~16;
					default: return CIA_TABLE_1[w];
				}
			else // hardware expansions
			{
				if (sid_extras==2||sid_extras==4) // within the range $DE00-$DFFF and with SID extensions?
				{
					if (w>=0XDE00&&w<0XDE20)
						return (w==0XDE1C)?w=sid_multichip2,sid_multichip2=0,w:SID_TABLE[1][w-0XDE00];
					if (w>=0XDF00&&w<0XDF20&&sid_extras==4)
						return (w==0XDF1C)?w=sid_multichip2,sid_multichip2=0,w:SID_TABLE[2][w-0XDF00];
				}
				return 0XFF; // nothing else!
			}
}

void m6510_send(WORD w,BYTE b) // send the byte `b` to the I/O address `w`
{
	if (w<0XD800)
		if (w<0XD000)
			if (w<0X0002) // memory configuration, $0000-$0001
				MMU_CFG_SET(w,b);
			else // just a window to RAM, $0001-$CFFF
				mem_ram[w]=b;
		else
			if (w<0XD400) // video configuration, $D000-$D02E
				switch (w&=63)
				{
					case 17: // $D011: CONTROL REGISTER 1
					case 18: // $D012: RASTER POSITION
						VICII_TABLE[w]=b;
						if (w==17) vicii_setmode(),vicii_setmaps();
						w=(VICII_TABLE[17]&128)*2+VICII_TABLE[18];
						if (vicii_irq_y!=w)
							if (vicii_pos_y==(vicii_irq_y=w))
								if (VICII_TABLE[25]|=1,VICII_TABLE[26]&1)
									VICII_TABLE[25]|=128,m6510_irq|=1;
						break;
					case 22: // $D016: CONTROL REGISTER 2
						VICII_TABLE[22]=b&31; vicii_setmode();
						break;
					case 24: // $D018: MEMORY CONTROL REGISTER
						VICII_TABLE[24]=b&~1; vicii_setmaps();
						break;
					case 25: // $D019: INTERRUPT REQUEST REGISTER
						if ((VICII_TABLE[25]&=(15&~b))&VICII_TABLE[26]&15)
							VICII_TABLE[25]|=128,m6510_irq|=1; // throw pending VIC-II IRQ
						else
							VICII_TABLE[25]&=127,m6510_irq&=~1; // acknowledge VIC-II IRQ
						break;
					case 26: // $D01A: INTERRUPT REQUEST MASK
						if (VICII_TABLE[25]&(VICII_TABLE[26]=b&15))
							VICII_TABLE[25]|=128,m6510_irq|=1; // throw pending VIC-II IRQ
						else
							VICII_TABLE[25]&=127,m6510_irq&=~1; // acknowledge VIC-II IRQ
						break;
					case 19: case 20: break; // $D013 and $D014 are READ ONLY!?
					case 30: case 31: break; // $D01E and $D01F are READ ONLY!!
					case 32: case 33: case 34: case 35:
					case 36: case 37: case 38: case 39:
					case 40: case 41: case 42: case 43:
					case 44: case 45: case 46: // COLOR TABLE
						video_clut[16+w-32]=video_table[video_type][VICII_TABLE[w]=b&=15];
						break;
					default:
						VICII_TABLE[w]=b;
				}
			else // audio configuration, $D400-$D43F
			{
				if (w>=0XD420&&sid_extras) // beyond the range $D400-$D41F and with SID extensions?
				{
					if (sid_extras==1||sid_extras==3)
					{
						if (w<0XD440)
							SID_TABLE[1][w-=0XD420]=b,sid_reg_update(1,w),m6510_sid_extras1(w,b);
						else if (w<0XD460&&sid_extras==3)
							SID_TABLE[2][w-=0XD440]=b,sid_reg_update(2,w),m6510_sid_extras1(w,b);
					}
					return; // stop here!
				}
				// the base chip, without extensions, spans the whole $D400-$D7FF range
				if (w==0XD400+18&&b==0X20&&SID_TABLE_0[18]==0X7F)
					sid_detection=0; // "BOX CHECK TEST" modifies this port; see above
				SID_TABLE_0[w&=31]=b,sid_reg_update(0,w),m6510_sid_extras0(),tape_song|=w<21&&b;
			}
	else
		if (w<0XDD00)
			if (w<0XDC00) // color configuration, $D800-$DBFF
				VICII_COLOR[w-0XD800]=b&15;
			else // CIA #1 configuration, $DC00-$DC0F
				switch (w&=15)
				{
					case  5: // reload TIMER A
						CIA_TABLE_0[5]=b; if (!(CIA_TABLE_0[14]&1))
							cia_count_a[0]=CIA_TABLE_0[4]+b*256; // *!* set TIMER LOAD flag
						break;
					case  7: // reload TIMER B
						CIA_TABLE_0[7]=b; if (!(CIA_TABLE_0[15]&1))
							cia_count_b[0]=CIA_TABLE_0[6]+b*256; // *!* set TIMER LOAD flag
						break;
					case  8: // decimals
						if (CIA_TABLE_0[15]&128) CIA_TABLE_0[ 8]=b&15;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0XBFFFFF00)+ (b& 15);
						break;
					case  9: // seconds
						if (CIA_TABLE_0[15]&128) CIA_TABLE_0[11]|=64,CIA_TABLE_0[ 9]=b& 63;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0XBFFF00FF)+((b& 63)<< 8)+0X40000000;
						break;
					case 10: // minutes
						if (CIA_TABLE_0[15]&128) CIA_TABLE_0[11]|=64,CIA_TABLE_0[10]=b& 63;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0XBF00FFFF)+((b& 63)<<16)+0X40000000;
						break;
					case 11: // hours + AM/PM bit
						if (CIA_TABLE_0[15]&128) CIA_TABLE_0[11]=(b&143)+64;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0X00FFFFFF)+((b&143)<<24)+0X40000000;
						break;
					case 13: // CIA #1 int.mask
						if (b&128) CIA_TABLE_0[13]|=b&31;
							else CIA_TABLE_0[13]&=~(b&31);
						if ((cia_port_13[0]&CIA_TABLE_0[13]&31)&&!(cia_port_13[0]&128))
							cia_port_13[0]|=128,m6510_irq|=2; // throw pending CIA #1 IRQ!
						tape_autoplay();
						break;
					case 14: // CIA #1 TIMER A control
						if (b&16)
							cia_count_a[0]=CIA_TABLE_0[4]+CIA_TABLE_0[5]*256;
						if (b&64) // SERIAL PORT???
							cia_port_13[0]|=8;
						else
							cia_port_13[0]&=~8;
						CIA_TABLE_0[14]=b&~16;
						break;
					case 15: // CIA #1 TIMER B control
						if (b&16)
							cia_count_b[0]=CIA_TABLE_0[6]+CIA_TABLE_0[7]*256;
						CIA_TABLE_0[15]=b&~16;
						break;
					default:
						CIA_TABLE_0[w]=b;
				}
		else
			if (w<0XDE00) // CIA #2 configuration, $DD00-$DD0F
				switch (w&=15)
				{
					case  0:
					case  2:
						CIA_TABLE_1[w]=b; vicii_setmaps();
						break;
					case  5: // reload TIMER A
						CIA_TABLE_1[5]=b; if (!(CIA_TABLE_1[14]&1))
							cia_count_a[1]=CIA_TABLE_1[4]+b*256; // *!* set TIMER LOAD flag
						break;
					case  7: // reload TIMER B
						CIA_TABLE_1[7]=b; if (!(CIA_TABLE_1[15]&1))
							cia_count_b[1]=CIA_TABLE_1[6]+b*256; // *!* set TIMER LOAD flag
						break;
					case  8: // decimals
						if (CIA_TABLE_1[15]&128) CIA_TABLE_1[ 8]=b&15;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0XBFFFFF00)+ (b& 15);
						break;
					case  9: // seconds
						if (CIA_TABLE_1[15]&128) CIA_TABLE_1[11]|=64,CIA_TABLE_1[ 9]=b& 63;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0XBFFF00FF)+((b& 63)<< 8)+0X40000000;
						break;
					case 10: // minutes
						if (CIA_TABLE_1[15]&128) CIA_TABLE_1[11]|=64,CIA_TABLE_1[10]=b& 63;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0XBF00FFFF)+((b& 63)<<16)+0X40000000;
						break;
					case 11: // hours + AM/PM bit
						if (CIA_TABLE_1[15]&128) CIA_TABLE_1[11]=(b&143)+64;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0X00FFFFFF)+((b&143)<<24)+0X40000000;
						break;
					case 13: // CIA #2 int.mask
						if (b&128) CIA_TABLE_1[13]|=b&31;
							else CIA_TABLE_1[13]&=~(b&31);
						if ((cia_port_13[1]&CIA_TABLE_1[13]&31)&&!(cia_port_13[1]&128))
							cia_port_13[1]|=128,m6510_irq|=128; // throw pending CIA #2 NMI!
						break;
					case 14: // CIA #2 TIMER A control
						if (b&16)
							cia_count_a[1]=CIA_TABLE_1[4]+CIA_TABLE_1[5]*256;
						if (b&64) // SERIAL PORT???
							cia_port_13[1]|=8;
						else
							cia_port_13[1]&=~8;
						CIA_TABLE_1[14]=b&~16;
						break;
					case 15: // CIA #2 TIMER B control
						if (b&16)
							cia_count_b[1]=CIA_TABLE_1[6]+CIA_TABLE_1[7]*256;
						CIA_TABLE_1[15]=b&~16;
						break;
					default:
						CIA_TABLE_1[w]=b;
				}
			else // hardware expansions
			{
				if (sid_extras==2||sid_extras==4)
				{
					if (w>=0XDE00&&w<0XDE20)
						SID_TABLE[1][w-=0XDE00]=b,sid_reg_update(1,w),m6510_sid_extras2(w,b);
					else if (w>=0XDF00&&w<0XDF20&&sid_extras==4)
						SID_TABLE[2][w-=0XDF00]=b,sid_reg_update(2,w),m6510_sid_extras2(w,b);
					else
						m6510_sid_extras0();
				}
				else
					; // nothing else!
			}
}

// the MOS 6510 memory operations PAGE (setup MMU), PEEK and POKE, plus ZEROPAGE and others
#define M65XX_LOCAL BYTE m6510_aux
#define M65XX_PEEKSYNC (m6510_sync(m65xx_t),_t_=m65xx_t=0)
#define M65XX_POKESYNC (audio_dirty=(m6510_aux>=0XD4&&m6510_aux<0XD8)?1:audio_dirty,M65XX_PEEKSYNC)
#define M65XX_PAGE(x) (m6510_aux=x)
#define M65XX_PEEK(x) (m65xx_t+=m6510_vicii_reading(),((mmu_bit[m6510_aux]&1)?M65XX_PEEKSYNC,m6510_recv(x):mmu_rom[m6510_aux][x]))
#define M65XX_POKE(x,o) do{ m65xx_t+=m6510_vicii_writing(); if (mmu_bit[m6510_aux]&2) M65XX_POKESYNC,m6510_send(x,o); else mem_ram[x]=o; }while(0) // simplify ROM-RAM fall back
#define M65XX_PEEKZERO(x) (m65xx_t+=m6510_vicii_reading(),x<0X0002?MMU_CFG_GET(x):mem_ram[x]) // ZEROPAGE is simpler, we only need to filter the first two bytes
#define M65XX_POKEZERO(x,o) do{ m65xx_t+=m6510_vicii_writing(); if (x<0X0002) MMU_CFG_SET(x,o); else mem_ram[x]=o; }while(0) // ditto
#define M65XX_PULL(x) (m65xx_t+=m6510_vicii_reading(),mem_ram[256+(x)]) // stack operations are simpler, they're always located on the same RAM area
#define M65XX_PUSH(x,o) (m65xx_t+=m6510_vicii_writing(),mem_ram[256+(x)]=o) // ditto

// the MOS 6510 "dumb" operations sometimes have an impact on the hardware of the C64
#define M65XX_DUMBPAGE(x) (m6510_aux=x)
#define M65XX_DUMBPEEK(x) (m65xx_t+=m6510_vicii_reading(),((mmu_bit[m6510_aux]&1)&&(M65XX_PEEKSYNC,m6510_recv(x))))
#define M65XX_DUMBPOKE(x,o) do{ m65xx_t+=m6510_vicii_writing(); if (mmu_bit[m6510_aux]&2) M65XX_POKESYNC,m6510_send(x,o); }while(0)
#define M65XX_DUMBPEEKZERO(x) (m65xx_t+=m6510_vicii_reading())
#define M65XX_DUMBPOKEZERO(x,o) (m65xx_t+=m6510_vicii_writing())
#define M65XX_DUMBPULL(x) (m65xx_t+=m6510_vicii_reading())

#define M65XX_START (mem_rom[0X1FFC]+mem_rom[0X1FFD]*256)
#define M65XX_RESET m6510_reset
#define M65XX_MAIN m6510_main
#define M65XX_SYNC m6510_sync
#define M65XX_IRQ m6510_irq
#define M65XX_NMI (m6510_irq&=127)
#define M65XX_ACK 0
#define M65XX_PC m6510_pc
#define M65XX_A m6510_a
#define M65XX_X m6510_x
#define M65XX_Y m6510_y
#define M65XX_S m6510_s
#define M65XX_P m6510_p
#define M65XX_I m6510_i

int m6510_ready=0,m6510_power=0X07;//,m6510_found=-1
#define M65XX_RDY m6510_ready
#define M65XX_TRAP_0X20 if (M65XX_PC.w==0XF53A&&mmu_mmu==m6510_t64ok) { t64_loadfile(); M65XX_X=mem_ram[0X00AC]; M65XX_Y=mem_ram[0X00AD]; M65XX_PC.w=0XF8D1; } // jump to the TEST BREAK routine
#define M65XX_TRAP_0XF0 if (M65XX_PC.w==0XFD87&&mmu_mmu==m6510_power) mem_ram[0XC2]=0X9F; // power-up boost
// `!m6510_ready` freezes FELLAS-EXT.PRG; compare with MICRO64 where "instable illegal opcode mode" must be 1!

#define DEBUG_HERE
#define DEBUG_INFOX 20 // panel width
DWORD debug_info_binary(BYTE b) // creates a fake DWORD with the binary display of `b`
	{ return ((b&128)<<21)+((b&64)<<18)+((b&32)<<15)+((b&16)<<12)+((b&8)<<9)+((b&4)<<6)+((b&2)<<3)+(b&1); }
void debug_info_sid(int x,int y)
{
	sprintf(DEBUG_INFOZ(y),"$%04X:",(WORD)((SID_TABLE[x]-VICII_TABLE)+0XD000));
	byte2hexa(DEBUG_INFOZ(y)+6,&SID_TABLE[x][21],7);
	for (int q=0;q<3;++q)
		sprintf(DEBUG_INFOZ(y+q+1)+4,"%c:",q+(sid_tone_stage[x][q]<2?'A':'a')),
		byte2hexa(DEBUG_INFOZ(y+q+1)+6,&SID_TABLE[x][q*7],7);
}
void debug_info(int q)
{
	sprintf(DEBUG_INFOZ( 0),"6510:");
	sprintf(DEBUG_INFOZ( 1)+4,"%07X:%08X",debug_info_binary(mmu_cfg[0]&127),debug_info_binary(mmu_cfg[1]));
	if (!(q&1)) // 1/2
	{
		sprintf(DEBUG_INFOZ( 2),"CIA #1: %02X  %04X%04X",cia_port_13[0],cia_count_a[0],cia_count_b[0]);
		if (m6510_irq&2) debug_hilight2(DEBUG_INFOZ(2)+4+4);
		byte2hexa(DEBUG_INFOZ( 3)+4,&CIA_TABLE_0[0],8);
		byte2hexa(DEBUG_INFOZ( 4)+4,&CIA_TABLE_0[8],8);
		sprintf(DEBUG_INFOZ( 5)+4,"%08X",cia_hhmmssd[0]);
		sprintf(DEBUG_INFOZ( 6),"CIA #2: %02X  %04X%04X",cia_port_13[1],cia_count_a[1],cia_count_b[1]);
		if (m6510_irq&128) debug_hilight2(DEBUG_INFOZ(6)+4+4);
		byte2hexa(DEBUG_INFOZ( 7)+4,&CIA_TABLE_1[0],8);
		byte2hexa(DEBUG_INFOZ( 8)+4,&CIA_TABLE_1[8],8);
		sprintf(DEBUG_INFOZ( 9)+4,"%08X",cia_hhmmssd[1]);
		if ((q='0'+(vicii_mode&31))>'9') q+=7;
		sprintf(DEBUG_INFOZ(10),"VIC-II: %02X %03X:%03X %c",VICII_TABLE[25]&VICII_TABLE[26]&0X8F,vicii_pos_y,VICII_TABLE[18]+(VICII_TABLE[17]&128)*2,q+(vicii_mode&32)*4);
		if (m6510_irq&1) debug_hilight2(DEBUG_INFOZ(10)+4+4);
		for (q=0;q<4;++q)
			byte2hexa(DEBUG_INFOZ(11+q)+4,&VICII_TABLE[q*8],8);
	}
	else // 2/2
	{
		sprintf(DEBUG_INFOZ(2),"SID x%d:",sid_main_end); debug_info_sid(0,3);
		if (sid_extras>0)
		{
			debug_info_sid(1,7);
			if (sid_extras>2)
				debug_info_sid(2,11);
		}
	}
}
int grafx_size(int i) { return i*8; }
WORD grafx_show(VIDEO_UNIT *t,int g,int n,BYTE m,WORD w,int o)
{
	while (n-->0)
	{
		BYTE b=mem_ram[w++];
		if (o&1)
		{
			VIDEO_UNIT p,pp[4]={0,0XFF0000,0X00FFFF,0XFFFFFF};
			*t++=p=pp[ b>>6   ]; *t++=p;
			*t++=p=pp[(b>>4)&3]; *t++=p;
			*t++=p=pp[(b>>2)&3]; *t++=p;
			*t++=p=pp[ b    &3]; *t++=p;
		}
		else
		{
			VIDEO_UNIT p0=0,p1=0XFFFFFF;
			*t++=b&128?p1:p0; *t++=b& 64?p1:p0;
			*t++=b& 32?p1:p0; *t++=b& 16?p1:p0;
			*t++=b&  8?p1:p0; *t++=b&  4?p1:p0;
			*t++=b&  2?p1:p0; *t++=b&  1?p1:p0;
		}
		t+=g-8;
	}
	return w;
}
void grafx_info(VIDEO_UNIT *t,int g,int o) // draw the palette and the sprites
{
	t-=16*6; for (int y=0;y<2*12;++y)
		for (int x=0;x<16*6;++x)
			t[y*g+x]=video_clut[(x/6)+(y/12)*16];
	for (int y=0,i=0;y<2;++y)
		for (int x=0;x<4;++x,++i)
		{
			VIDEO_UNIT *tt=&t[g*(2*12+y*21)+x*24];
			for (int yy=0,ii=vicii_attrib[1016+i]*64;yy<21;++yy,tt+=g)
				if (o&1)
					for (int xx=0;xx<3;++xx,++ii)
					{
						VIDEO_UNIT pp[4]={0,0XFF0000,0X00FFFF,0XFFFFFF};//;{video_clut[17],video_clut[21],video_clut[23+i],video_clut[22]};
						for (int zz=0;zz<8;zz+=2)
							tt[xx*8+zz]=tt[xx*8+zz+1]=pp[(vicii_memory[ii]>>(6-zz))&3];
					}
				else
					for (int xx=0;xx<3;++xx,++ii)
						for (int zz=0;zz<8;++zz)
							tt[xx*8+zz]=(vicii_memory[ii]<<zz)&128?0XFFFFFF:0;//video_clut[23+i]:video_clut[17];
		}
}
#include "cpcec-m6.h"
#undef DEBUG_HERE

// ... ... ... //
// ... ... ... //
// ... ... ... //

// D64: MOS 6502 MICROPROCESSOR ===================================== //

BYTE d64_mem[64<<10],d64_track[4]={0,0,0,0},d64_motor[4]={0,0,0,0}; // 48K RAM + 16K ROM
BYTE *d64_ram[4]={NULL,NULL,NULL,NULL};
FILE *d64[4]={NULL,NULL,NULL,NULL};
#define d64_rom &d64_mem[48<<10]

void m6502_sync(int t)
{
	; // nothing here yet!
}

Z80W m6502_pc; BYTE m6502_a,m6502_x,m6502_y,m6502_s,m6502_p,m6502_i,m6502_irq; // the MOS 6502 registers

BYTE m6502_recv(WORD w)
{
	return 0XFF; // nothing here yet!
}
void m6502_send(WORD w,BYTE b)
{
	; // nothing here yet!
}

#define M65XX_LOCAL
#define M65XX_PAGE(x) (0)
#define M65XX_PEEK(x) (++m65xx_t,(x>=0X2000&&x<0X1800)?m6502_recv(x):d64_mem[x]) // tell apart between 0000-17FF, 1800-1FFF, 2000-BFFF and C000-FFFF
#define M65XX_POKE(x,o) do{ ++m65xx_t; if (x<0X2000) { if (x<0X1800) d64_mem[x]=o; else m6502_send(x,o); } else if (x<0XC000) d64_mem[x]=0; }while(0)
#define M65XX_PEEKZERO(x) (++m65xx_t,d64_mem[x])
#define M65XX_POKEZERO(x,o) (++m65xx_t,(d64_mem[x]=o))
#define M65XX_PULL(x) (++m65xx_t,d64_mem[256+x])
#define M65XX_PUSH(x,o) (++m65xx_t,d64_mem[256+x]=o)

// the C1541's MOS 6502 "dumb" operations do nothing relevant, they only consume clock ticks
#define M65XX_DUMBPAGE(x) (0)
#define M65XX_DUMBPEEK(x) (++m65xx_t)
#define M65XX_DUMBPOKE(x,o) (++m65xx_t)
#define M65XX_DUMBPEEKZERO(x) (++m65xx_t)
#define M65XX_DUMBPOKEZERO(x,o) (++m65xx_t)
#define M65XX_DUMBPULL(x) (++m65xx_t)

#define M65XX_START (d64_mem[0XFFFC]+d64_mem[0XFFFD]*256)
#define M65XX_RESET m6502_reset
#define M65XX_MAIN m6502_main
#define M65XX_SYNC m6502_sync
#define M65XX_IRQ m6502_irq
//#define M65XX_NMI 0
#define M65XX_ACK 0
#define M65XX_PC m6502_pc
#define M65XX_A m6502_a
#define M65XX_X m6502_x
#define M65XX_Y m6502_y
#define M65XX_S m6502_s
#define M65XX_P m6502_p
#define M65XX_I m6502_i

#include "cpcec-m6.h"

// EMULATION CONTROL ================================================ //

char txt_error[]="Error!";
char txt_error_any_load[]="Cannot open file!";
char txt_error_bios[]="Cannot load firmware!";

// emulation setup and reset operations ----------------------------- //

void all_setup(void) // setup everything!
{
	//MEMZERO(mem_ram); // the C64 is a little special about this
	for (int i=0;i<sizeof(mem_ram);i+=64) memset(&mem_ram[i],(i&64)?255:0,64);
	mmu_setup(),m65xx_setup(); // 6510+6502!
	sid_setup();
}
void all_reset(void) // reset everything!
{
	MEMZERO(autorun_kbd);
	mmu_reset(),m6510_reset(),m6502_reset(),cia_reset();
	vicii_reset(),sid_all_reset();
	m6510_irq=m6502_irq=0; debug_reset();
	snap_done=0; //MEMFULL(z80_tape_index); // avoid accidents!
	memcmp(&mem_ram[0X8004],"\303\302\31580",5)||(mem_ram[0X8004]=0330); // disable "CBM80" trap!
}

// firmware ROM file handling operations ---------------------------- //

char bios_path[STRMAX]="";

int bios_load(char *s) // loads the CBM64 firmware; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1; // fail!
	int i=fread1(mem_rom,20<<10,f); i+=fread1(mem_rom,20<<10,f);
	if (fclose(f),i!=(20<<10)) return 1; // check filesize; are there any fingerprints?
	if (bios_path!=s&&(char*)session_substr!=s) strcpy(bios_path,s);
	return 0;
}

int bios_reload(void) // loads the default CBM64 firmware
	{ return bios_load(strcat(strcpy(session_substr,session_path),"c64en.rom")); }

int bdos_load(char *s) // loads the C1541 firmware; 0 OK, !0 ERROR
{
	FILE *f=fopen(strcat(strcpy(session_substr,session_path),s),"rb"); if (!f) return 1; // fail!
	int i=fread1(d64_rom,16<<10,f); i+=fread1(d64_rom,16<<10,f);
	return fclose(f),i-(16<<10); // check filesize; are there any fingerprints?
}

// snapshot file handling operations -------------------------------- //

char snap_pattern[]="*.s64"; BYTE snap_magic8[]="C64-SNAPSHOT v1\032";
char snap_path[STRMAX]="",snap_extended=1; // compress memory dumps

// extremely simple run-length encoding:
// - a single instance of byte X is stored as "X";
// - N=(2..256) repetitions of byte X become "X,X,N-2";
// - the end marker is "255-X,255-X,255".
int bin2rle(unsigned char *t,int o,unsigned char *s,int i) // encode `i` exact bytes at source `s` onto up to `o` bytes of run-length target `t`; >=0 OK (encoded bytes), <0 ERROR
{
	unsigned char *u=t,*r=&s[i],j=0; while (s<r)
	{
		if (o<3) return -1; *t++=j=*s,i=0; // ERROR: target is too short!
		while (++s<r&&j==*s&&i<255) ++i; if (i) *t++=j,*t++=i-1,o-=3; else --o;
	}
	return o<3?-1:(*t++=(j=~j),*t++=j,*t++=255,t-u); // ERROR: target is too short!
}
int rle2bin(unsigned char *t,int o,unsigned char *s,int i) // decode up to `i` bytes of run-length source `s` onto up to `o` bytes of target `t`; >=0 OK (decoded bytes), <0 ERROR
{
	unsigned char *u=t,j; short l; for (;;)
	{
		if (i<3) return -2; j=*s; // ERROR: source is too short!
		if (j!=*++s) --i,l=1; else if ((l=s[1])<255) i-=3,s+=2,l+=2; else break;
		if ((o-=l)<0) return -1; do *t++=j; while (--l); // ERROR: target is too short!
	}
	return t-u;
}

// uncompressed snapshots store 0 as the compressed size (Intel WORD at offset 16)
int snap_save(char *s) // saves snapshot file `s`; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"wb"); if (!f) return 1;
	BYTE header[512]; MEMZERO(header);
	strcpy(header,snap_magic8);
	int q=snap_extended?bin2rle(session_scratch,(1<<16)-1,mem_ram,1<<16):0;
	if (q>0) mputii(&header[0x10],q); else q=0;
	// CPU #1
	header[0x20]=m6510_pc.b.l;
	header[0x21]=m6510_pc.b.h;
	header[0x22]=m6510_p;
	header[0x23]=m6510_a;
	header[0x24]=m6510_x;
	header[0x25]=m6510_y;
	header[0x26]=m6510_s;
	header[0x27]=m6510_irq&=(128+2+1); // stick to NMI+CIA+VIC
	header[0x30]=mmu_cfg[0];
	header[0x31]=mmu_cfg[1];
	header[0x32]=mmu_out; // this value is normally identical to mmu_cfg[1], but not always, cfr. CPUPORT.PRG
	// CIA #1
	memcpy(&header[0x40],CIA_TABLE_0,0X10);
	mputii(&header[0x54],cia_count_a[0]);
	mputii(&header[0x56],cia_count_b[0]);
	mputiiii(&header[0x58],cia_hhmmssd[0]);
	header[0x5D]=cia_port_13[0];
	// CIA #2
	memcpy(&header[0x60],CIA_TABLE_1,0X10);
	mputii(&header[0x74],cia_count_a[1]);
	mputii(&header[0x76],cia_count_b[1]);
	mputiiii(&header[0x78],cia_hhmmssd[0]);
	header[0x7D]=cia_port_13[1];
	// SID #1
	memcpy(&header[0x80],SID_TABLE_0,0X20-4);
	mputii(&header[0x9C],sid_extras>0?0XD000+(SID_TABLE[1]-VICII_TABLE):0);
	mputii(&header[0x9E],sid_extras>2?0XD000+(SID_TABLE[2]-VICII_TABLE):0);
	// VIC-II
	memcpy(&header[0xB0],VICII_TABLE,0X30);
	fwrite1(header,256,f);
	// COLOR TABLE
	for (int i=0;i<512;++i) header[i]=(VICII_COLOR[i*2]<<4)+(VICII_COLOR[1+i*2]&15);
	fwrite1(header,512,f);
	// 64K RAM
	q?fwrite1(session_scratch,q,f):fwrite1(mem_ram,1<<16,f);
	// ... future blocks will go here ...
	if (snap_path!=s) strcpy(snap_path,s);
	return snap_done=!fclose(f),0;
}

int snap_load(char *s) // loads snapshot file `s`; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1;
	BYTE header[512];
	if ((fread1(header,256,f)!=256)||(memcmp(snap_magic8,header,16)))
		return puff_fclose(f),1;
	int q=mgetii(&header[0x10]); // compressed size, 0 if uncompressed
	// CPU #1
	m6510_pc.b.l=header[0x20];
	m6510_pc.b.h=header[0x21];
	m6510_p=header[0x22];
	m6510_a=header[0x23];
	m6510_x=header[0x24];
	m6510_y=header[0x25];
	m6510_s=header[0x26];
	m6510_irq=header[0x27]&(128+2+1);
	mmu_cfg[0]=header[0x30];
	mmu_cfg[1]=header[0x31];
	mmu_out=header[0x32];
	mmu_update();
	m6510_i=4; // avoid accidents!
	// CIA #1
	memcpy(CIA_TABLE_0,&header[0x40],0X10);
	cia_count_a[0]=mgetii(&header[0x54]);
	cia_count_b[0]=mgetii(&header[0x56]);
	cia_hhmmssd[0]=mgetiiii(&header[0x58]);
	cia_port_13[0]=header[0x5D];
	if ((CIA_TABLE_0[14]&1)>=cia_count_a[0]) cia_count_a[0]=0XFFFF; // bug?
	if ((CIA_TABLE_0[15]&1)>=cia_count_b[0]) cia_count_b[0]=0XFFFF; // bug?
	// CIA #2
	memcpy(CIA_TABLE_1,&header[0x60],0X10);
	cia_count_a[1]=mgetii(&header[0x74]);
	cia_count_b[1]=mgetii(&header[0x76]);
	cia_hhmmssd[1]=mgetiiii(&header[0x78]);
	cia_port_13[1]=header[0x7D];
	if ((CIA_TABLE_1[14]&1)>=cia_count_a[1]) cia_count_a[1]=0XFFFF; // bug?
	if ((CIA_TABLE_1[15]&1)>=cia_count_b[1]) cia_count_b[1]=0XFFFF; // bug?
	// SID #1
	memcpy(SID_TABLE_0,&header[0x80],0X1C);
	if (equalsii(&header[0x9C],0XD420))
		sid_extras=equalsii(&header[0x9E],0XD440)?3:1;
	else if (equalsii(&header[0x9C],0XDE00))
		sid_extras=equalsii(&header[0x9E],0XDF00)?4:2;
	else
		sid_extras=0;
	sid_all_update();
	// VIC-II
	memcpy(VICII_TABLE,&header[0xB0],0X30);
	vicii_irq_y=(VICII_TABLE[17]&128)*2+VICII_TABLE[18];
	vicii_setmaps(),vicii_setmode();
	debug_reset();
	video_clut_update();
	fread1(header,512,f);
	// COLOR TABLE
	for (int i=0;i<512;++i) VICII_COLOR[i*2]=header[i]>>4,VICII_COLOR[1+i*2]=header[i]&15;
	// 64K RAM
	q?rle2bin(mem_ram,1<<16,session_scratch,fread1(session_scratch,q,f)):fread1(mem_ram,1<<16,f);
	// ... future blocks will go here ...
	if (snap_path!=s) strcpy(snap_path,s);
	return snap_done=!fclose(f),0;
}

// "autorun" file and logic operations ------------------------------ //

char prog_path[STRMAX]="";

int prog_load(char *s) // "inject" a program file; actually a subset of the autorun handling
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1;
	if (fgetii(f)!=0X0801) return fclose(f),1; // wrong magic number!
	autorun_i=fread1(&mem_ram[0X10000],0XD000-0X0801,f); // *!* REU as temporary storage
	all_reset(),autorun_t=9,autorun_mode=2; // reset machine, but don't launch program yet
	if (prog_path!=s) strcpy (prog_path,s);
	return puff_fclose(f),!autorun_i;
}

int any_load(char *s,int q) // load a file regardless of format. `s` path, `q` autorun; 0 OK, !0 ERROR
{
	autorun_mode=0; // cancel any autoloading yet
	if (snap_load(s))
	{
		if (tape_open(s))
		{
			if (disc_open(s,0,0))
			{
				if (prog_load(s))
				{
					if (bios_load(s))
						return bios_reload(),1; // everything failed!
					else
						disc_disabled&=~2,all_reset(); // cleanup!
				}
				else
					{ disc_disabled|=2; if (q) autorun_mode=1; } // force autorun if required
			}
			else
				{ if (q) disc_disabled=0,all_reset(),autorun_t=9,autorun_mode=8; } // enable disc controller
		}
		else
			{ if (q) disc_disabled|=2,all_reset(),autorun_t=9,autorun_mode=3; } // temp. disable disc controller
	}
	if (autorun_path!=s&&q) strcpy(autorun_path,s);
	return 0;
}

// auxiliary user interface operations ------------------------------ //

char txt_error_snap_save[]="Cannot save snapshot!";
char file_pattern[]="*.s64;*.prg;*.rom;*.d64;*.t64;*.tap";

char session_menudata[]=
	"File\n"
	"0x8300 Open any file..\tF3\n"
	"0xC300 Load snapshot..\tShift+F3\n"
	"0x0300 Load last snapshot\tCtrl+F3\n"
	"0x8200 Save snapshot..\tF2\n"
	"0x0200 Save last snapshot\tCtrl+F2\n"
	"=\n"
	"0XC500 Load programme..\tShift+F5\n"
	/*
	"=\n"
	"0x8700 Insert disc in 8:..\tF7\n"
	"0x8701 Create disc in 8:..\n"
	"0x0700 Remove disc from 8:\tCtrl+F7\n"
	"0xC700 Insert disc in 9:..\tShift+F7\n"
	"0xC701 Create disc in 9:..\n"
	"0x4700 Remove disc from 9:\tCtrl+Shift+F7\n"
	*/
	"=\n"
	"0x8800 Insert tape..\tF8\n"
	"0xC800 Record tape..\tShift+F8\n"
	"0x8801 Browse tape..\n"
	"0x0800 Remove tape\tCtrl+F8\n"
	"=\n"
	"0x0080 E_xit\n"
	"Machine\n"
	"0x8500 Select firmware..\tF5\n"
	"0x0500 Reset emulation\tCtrl+F5\n"
	"0x8F00 Pause\tPause\n"
	"0x8900 Debug\tF9\n"
	//"0x8910 NMI\n"
	"=\n"
	/*
	"0x8511 No REU\n"
	"0x8512 64k REU\n"
	"0x8513 128k REU\n"
	"0x8514 256k REU\n"
	"0x8515 512k REU\n"
	*/
	"0x8501 Power-up boost\n"
	"0x8502 RDY 6510 switch\n"
	"0x8503 Disable sprite impacts\n"
	"0x8504 Enable SID filter\n"
	"0x8505 SID 8580 type\n"
	"=\n"
	//"0x0600 Raise 6510 speed\tCtrl+F6\n"
	//"0x4600 Lower 6510 speed\tCtrl+Shift+F6\n"
	"0x0601 1x CPU clock\n"
	"0x0602 2x CPU clock\n"
	"0x0603 3x CPU clock\n"
	"0x0604 4x CPU clock\n"
	"0x8508 SID x1\n"
	"0x8509 SID x2 #:$D420\n"
	"0x850A SID x2 #:$DE00\n"
	"0x850B SID x3 $D420:#:$D440\n"
	"0x850C SID x3 $DE00:#:$DF00\n"
	//"0x850D SID channel A off\n"
	//"0x850E SID channel B off\n"
	//"0x850F SID channel C off\n"
	"Settings\n"
	"0x8601 1x realtime speed\n"
	"0x8602 2x realtime speed\n"
	"0x8603 3x realtime speed\n"
	"0x8604 4x realtime speed\n"
	"0x8600 Run at full throttle\tF6\n"
	"=\n"
	"0x0400 Virtual joystick\tCtrl+F4\n"
	"0x0401 Redefine virtual joystick\n"
	"0x4400 Flip joystick ports\tCtrl+Shift+F4\n"
	"=\n"
	"0x852F Printer output..\n"
	"0x851F Strict snapshots\n"
	/*
	"0x8510 Disc controller\n"
	*/
	"0x8590 Strict disc writes\n"
	"0x8591 Read-only disc by default\n"
	//"=\n"
	"0x0900 Tape speed-up\tCtrl+F9\n"
	"0x4900 Tape analysis\tCtrl+Shift+F9\n"
	"0x0901 Tape auto-rewind\n"
	"Video\n"
	"0x8A00 Full screen\tAlt+Return\n"
	"0x8A02 Video acceleration*\n"
	"0x8A01 Zoom to integer\n"
	"0x8901 Onscreen status\tShift+F9\n"
	"=\n"
	"0x8B01 Monochrome\n"
	"0x8B02 Dark palette\n"
	"0x8B03 Normal palette\n"
	"0x8B04 Bright palette\n"
	"0x8B05 Green screen\n"
	//"0x8B00 Next palette\tF11\n"
	//"0xCB00 Prev. palette\tShift+F11\n"
	"=\n"
	"0x8903 X-masking\n"
	"0x8902 Y-masking\n"
	"0x8904 Pixel filter\n"
	//"0x0B00 Next scanline\tCtrl+F11\n"
	//"0x4B00 Prev. scanline\tCtrl+Shift+F11\n"
	"0x0B01 All scanlines\n"
	"0x0B02 Half scanlines\n"
	"0x0B03 Simple interlace\n"
	"0x0B04 Double interlace\n"
	"0x0B05 Average scanlines\n"
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
	session_menucheck(0x8700,(size_t)disc[0]);
	session_menucheck(0xC700,(size_t)disc[1]);
	session_menucheck(0x8800,tape_type>=0&&tape);
	session_menucheck(0xC800,tape_type<0&&tape);
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	session_menucheck(0x0400,session_key2joy);
	session_menucheck(0x4400,key2joy_flag);
	session_menuradio(0x0601+multi_t-1,0x0601,0x0604);
	session_menucheck(0x8502,m6510_ready);
	session_menucheck(0x8503,vicii_nohits);
	session_menucheck(0x8504,sid_filters);
	session_menucheck(0x8505,sid_nouveau);
	session_menuradio(0x8508+sid_extras,0X8508,0X850C);
	//session_menucheck(0x850D,sid_muted[0]);
	//session_menucheck(0x850E,sid_muted[1]);
	//session_menucheck(0x850F,sid_muted[2]);
	session_menucheck(0x8501,m6510_power>=0);
	session_menucheck(0x8590,!(disc_filemode&2));
	session_menucheck(0x8591,disc_filemode&1);
	session_menucheck(0x8510,!(disc_disabled&1));
	session_menuradio(0x8511+reu_depth,0x8511,0x8515);
	session_menucheck(0x851F,!(snap_extended));
	session_menucheck(0x8901,onscreen_flag);
	session_menucheck(0x8A00,session_fullscreen);
	session_menucheck(0x8A01,session_intzoom);
	session_menucheck(0x8A02,!session_softblit);
	session_menucheck(0x8A04,!session_softplay);
	session_menuradio(0x8B01+video_type,0x8B01,0x8B05);
	session_menuradio(0x0B01+video_scanline,0x0B01,0x0B05);
	session_menucheck(0x0B08,video_scanblend);
	session_menucheck(0x8902,video_filter&VIDEO_FILTER_MASK_Y);
	session_menucheck(0x8903,video_filter&VIDEO_FILTER_MASK_X);
	session_menucheck(0x8904,video_filter&VIDEO_FILTER_MASK_Z);
	if (sid_extras&1)
		SID_TABLE[1]=&mem_i_o[0X420],SID_TABLE[2]=&mem_i_o[0X440];
	else
		SID_TABLE[1]=&mem_i_o[0XE00],SID_TABLE[2]=&mem_i_o[0XF00];
	sid_main_end=sid_extras<1?1:sid_extras<3?2:3;
	#if AUDIO_CHANNELS > 1
	session_menuradio(0xC401+audio_mixmode,0xC401,0xC404);
	switch (sid_extras)
	{
		case 1: case 2:
			for (int i=0;i<3;++i)
				sid_stereo[0][i][0]=(256+sid_stereos[audio_mixmode][0])/2,sid_stereo[0][i][1]=(256-sid_stereos[audio_mixmode][0])/2, // 1st chip is L
				sid_stereo[1][i][0]=(256+sid_stereos[audio_mixmode][2])/2,sid_stereo[1][i][1]=(256-sid_stereos[audio_mixmode][2])/2; // 2nd chip is R
			break;
		case 3: case 4:
			for (int i=0;i<3;++i)
				sid_stereo[0][i][0]=(256+sid_stereos[audio_mixmode][1])/3,sid_stereo[0][i][1]=(256-sid_stereos[audio_mixmode][1])/3, // 1st chip is C
				sid_stereo[1][i][0]=(256+sid_stereos[audio_mixmode][0])/3,sid_stereo[1][i][1]=(256-sid_stereos[audio_mixmode][0])/3, // 2nd chip is L
				sid_stereo[2][i][0]=(256+sid_stereos[audio_mixmode][2])/3,sid_stereo[2][i][1]=(256-sid_stereos[audio_mixmode][2])/3; // 3rd chip is R
			break;
		default:
			for (int i=0;i<3;++i) //if (sid_muted[i]) sid_stereo[0][i][0]=sid_stereo[0][i][1]=0; else
				sid_stereo[0][i][0]=256+sid_stereos[audio_mixmode][i],sid_stereo[0][i][1]=256-sid_stereos[audio_mixmode][i]; // channels A,B,C are L,C,R
	}
	#endif
	video_resetscanline(); // video scanline cfg
	sprintf(session_info,"%d:%dK %d x%d %0.1fMHz"//" | disc %s | tape %s | %s"
		,64,64+reu_kbyte[reu_depth],sid_nouveau?8580:6581,sid_main_end,1.0*multi_t);
	video_lastscanline=video_table[video_type][0]; // BLACK in the CLUT
	video_halfscanline=VIDEO_FILTER_SCAN(video_table[video_type][1],video_lastscanline); // WHITE in the CLUT
	debug_dirty=1; // force debug redraw! (somewhat overkill)
}
void session_user(int k) // handle the user's commands
{
	char *s; switch (k)
	{
		case 0x8100: // F1: HELP..
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
				"F6\tToggle realtime" MESSAGEBOX_WIDETAB
				"^F6\tNext CPU speed"
				"\n"
				"\t(shift: ..CRTC)" MESSAGEBOX_WIDETAB
				"\t(shift: previous..)"
				"\n"
				/*
				"F7\tInsert disc into 8:..\t"
				"^F7\tEject disc from 8:"
				"\n"
				"\t(shift: ..into 9:)\t"
				"\t(shift: ..from 9:)"
				"\n"
				*/
				"F8\tInsert tape.." MESSAGEBOX_WIDETAB
				"^F8\tRemove tape"
				"\n"
				"\t(shift: record..)"//"\t"
				//"\t(shift: play/stop)"
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
				"\t(shift: record film)"//"\t"
				//"\t-"
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
				"Commodore 64 emulator written by Cesar Nicolas-Gonzalez\n"
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
				audio_disabled^=1;
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
			if (session_shift)
				key2joy_flag=!key2joy_flag; // FLIP JOYSTICK BUTTONS
			else
				session_key2joy=!session_key2joy;
			break;
		case 0x0401: // REDEFINE VIRTUAL JOYSTICK
			s="Press a key or ESCAPE"; int t[5];
			if ((t[0]=session_scan("UP",s))>=0)
			if ((t[1]=session_scan("DOWN",s))>=0)
			if ((t[2]=session_scan("LEFT",s))>=0)
			if ((t[3]=session_scan("RIGHT",s))>=0)
			if ((t[4]=session_scan("FIRE",s))>=0)
				for (int i=0;i<length(t);++i) kbd_k2j[i]=t[i];
			break;
		case 0x8502: // RDY 6510 SWITCH
			m6510_ready=!m6510_ready;
			break;
		case 0x8503: // DISABLE SPRITE IMPACTS
			vicii_nohits=!vicii_nohits;
			VICII_TABLE[31]=VICII_TABLE[30]=0;
			break;
		case 0x8504: // SID FILTERS
			sid_filters=!sid_filters;
			sid_reg_update(0,23);
			sid_reg_update(1,23);
			sid_reg_update(2,23);
			break;
		case 0x8505: // SID 8580/6581
			sid_nouveau=!sid_nouveau;
			break;
		case 0x8501: // POWER-UP BOOST
			m6510_power=m6510_power>=0?-1:0X07;
			break;
		case 0x8508: // SID X1
		case 0x8509: // SID X2 (base:$D420)
		case 0x850A: // SID X2 (base:$DE00)
		case 0x850B: // SID X3 ($D420:base:$D440)
		case 0x850C: // SID X3 ($DE00:base:$DF00)
			if (sid_extras!=k-0X8508)
				{ sid_extras=k-0X8508; sid_all_update(); }
			break;
		//case 0x850D: // SID CHANNEL A OFF
		//case 0x850E: // SID CHANNEL B OFF
		//case 0x850F: // SID CHANNEL C OFF
			//sid_muted[k-0x850D]^=1;
			//break;
		case 0x8590: // STRICT DISC WRITES
			disc_filemode^=2;
			break;
		case 0x8591: // DEFAULT READ-ONLY
			disc_filemode^=1;
			break;
		case 0x8510: // DISC CONTROLLER
			disc_disabled^=1;
			break;
		case 0x8511: // NO REU
		case 0x8512: // 64K REU
		case 0x8513: // 128K REU
		case 0x8514: // 256K REU
		case 0x8515: // 512K REU
			reu_depth=k-0x8511; mmu_update();
			break;
		case 0x851F: // STRICT SNA DATA
			snap_extended=!snap_extended;
			break;
		case 0x8500: // F5: LOAD FIRMWARE.. // LOAD PROGRAMME..
			if (session_shift)
			{
				if (s=puff_session_getfile(prog_path,"*.prg","Load programme"))
					if (prog_load(s)) // error? warn and undo!
						session_message("Cannot load programme!",txt_error);
			}
			else if (s=puff_session_getfile(bios_path,"*.rom","Load firmware"))
			{
				if (bios_load(s)) // error? warn and undo!
					session_message(txt_error_bios,txt_error),bios_reload(); // reload valid firmware, if required
				else
					autorun_mode=0,disc_disabled&=~2,all_reset(); // setup and reset
			}
			break;
		case 0x0500: // ^F5: RESET EMULATION // REMOVE DANDANATOR
			autorun_mode=0,disc_disabled&=~2,all_reset();
			break;
		case 0x8600: // F6: TOGGLE REALTIME
			if (!session_shift)
				session_fast^=1;
			else
				;
			break;
		case 0x8601: // REALTIME x1
		case 0x8602: // REALTIME x2
		case 0x8603: // REALTIME x3
		case 0x8604: // REALTIME x4
			session_rhythm=k-0x8600-1; session_fast&=~1;
			break;
		case 0x0600: // ^F6: TOGGLE TURBO 6510
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
				if (s=puff_session_newfile(disc_path,"*.d64",session_shift?"Create disc in B:":"Create disc in A:"))
				{
					if (disc_create(s))
						session_message("Cannot create disc!",txt_error);
					else
						disc_open(s,session_shift,1);
				}
			break;
		/*
		case 0x8700: // F7: INSERT DISC..
			if (!disc_disabled)
				if (s=puff_session_getfilereadonly(disc_path,"*.d64",session_shift?"Insert disc into B:":"Insert disc into A:",disc_filemode&1))
					if (disc_open(s,session_shift,!session_filedialog_get_readonly()))
						session_message("Cannot open disc!",txt_error);
			break;
		case 0x0700: // ^F7: EJECT DISC
			disc_close(session_shift);
			break;
		*/
		case 0x8800: // F8: INSERT OR RECORD TAPE..
			if (session_shift)
			{
				if (s=puff_session_newfile(tape_path,"*.tap","Record tape"))
					if (tape_create(s))
						session_message("Cannot create tape!",txt_error);
			}
			else if (s=puff_session_getfile(tape_path,"*.t64;*.tap","Insert tape"))
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
			tape_close();
			break;
		case 0x8900: // F9: DEBUG
			if (!session_shift)
			{
				session_signal=SESSION_SIGNAL_DEBUG^(session_signal&~SESSION_SIGNAL_PAUSE);
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
		case 0x9000: // RESTORE key (NMI)
			m6510_irq|=128;
			break;
		case 0x0900: // ^F9: TOGGLE TAPE SONG OR FAST TAPE
			if (session_shift)
				tape_fastload=!tape_fastload;
			else
				tape_skipload=!tape_skipload;
			break;
		case 0x0901:
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
		case 0x0B05: // AVG. SCANLINES
			video_scanline=k-0x0B01;
			break;
		case 0x0B08: // BLEND SCANLINES
			video_scanblend=!video_scanblend;
			break;
		case 0x0B00: // ^F11: SCANLINES
			if (session_shift)
				video_filter=(video_filter+1)&7;
			else if ((video_scanline=video_scanline+1)>4)
				video_scanblend=!video_scanblend,video_scanline=0;
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
			;
			break;
		case 0x9600: // NEXT
			;
			break;
		case 0x9700: // HOME
			break;
		case 0x9800: // END
			break;
		#endif
	}
}

void session_configreadmore(char *s)
{
	int i; if (!s||!*s||!session_parmtr[0]) ; // ignore if empty or internal!
	else if (!strcasecmp(session_parmtr,"file")) strcpy(autorun_path,s);
	else if (!strcasecmp(session_parmtr,"snap")) strcpy(snap_path,s);
	else if (!strcasecmp(session_parmtr,"tape")) strcpy(tape_path,s);
	else if (!strcasecmp(session_parmtr,"disc")) strcpy(disc_path,s);
	else if (!strcasecmp(session_parmtr,"prog")) strcpy(prog_path,s);
	else if (!strcasecmp(session_parmtr,"card")) strcpy(bios_path,s);
	else if (!strcasecmp(session_parmtr,"vjoy")) { if (!hexa2byte(session_parmtr,s,5)) usbkey2native(kbd_k2j,session_parmtr,5); }
	else if (!strcasecmp(session_parmtr,"c15x")) disc_disabled=!(*s&1);
	else if (!strcasecmp(session_parmtr,"fdcw")) disc_filemode=*s&3;
	else if (!strcasecmp(session_parmtr,"bank")) { if ((i=*s&7)<length(reu_kbyte)) reu_depth=i; }
	else if (!strcasecmp(session_parmtr,"joys")) key2joy_flag=(*s&1);
	else if (!strcasecmp(session_parmtr,"sidx")) sid_nouveau=(*s&1),sid_extras=(*s/2)&7,sid_extras=sid_extras>4?0:sid_extras;
	else if (!strcasecmp(session_parmtr,"misc")) snap_extended=(*s&1);
	else if (!strcasecmp(session_parmtr,"palette")) { if ((i=*s&15)<length(video_table)) video_type=i; }
	else if (!strcasecmp(session_parmtr,"casette")) tape_rewind=*s&1,tape_skipload=!!(*s&2),tape_fastload=!!(*s&4);
	else if (!strcasecmp(session_parmtr,"debug")) debug_configread(strtol(s,NULL,10));
}
void session_configwritemore(FILE *f)
{
	native2usbkey(kbd_k2j,kbd_k2j,5); byte2hexa(session_parmtr,kbd_k2j,5); session_parmtr[10]=0;
	fprintf(f,
		"c15x %d\nfdcw %d\nbank %d\njoys %d\nsidx %d\nmisc %d\n"
		"file %s\nsnap %s\ntape %s\ndisc %s\nprog %s\ncard %s\n"
		"vjoy %s\npalette %d\ncasette %d\ndebug %d\n",
		!(disc_disabled&1),disc_filemode,reu_depth,!!key2joy_flag,(sid_nouveau?1:0)+sid_extras*2,!!snap_extended,
		autorun_path,snap_path,tape_path,disc_path,prog_path,bios_path,
		session_parmtr,video_type,(tape_rewind?1:0)+(tape_skipload?2:0)+(tape_fastload?4:0),debug_configwrite());
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
	all_setup();
	all_reset();
	video_pos_x=video_pos_y=frame_pos_y=audio_pos_z=0;
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
						/*n=(BYTE)(argv[i][j++]-'0');
						if (n<0||n>9)
							i=argc; // help!
						*/break;
					case 'j':
						session_key2joy=1;
						break;
					case 'J':
						session_stick=0;
						break;
					case 'k':
						reu_depth=(BYTE)(argv[i][j++]-'0');
						if (reu_depth<0||reu_depth>=length(reu_kbyte))
							i=argc; // help!
						break;
					case 'K':
						reu_depth=0;
						break;
					case 'm':
						/*n=(BYTE)(argv[i][j++]-'0');
						if (n<0||n>9)
							i=argc; // help!
						*/break;
					case 'o':
						onscreen_flag=1;
						break;
					case 'O':
						onscreen_flag=0;
						break;
					case 'p':
						//disabled=0;
						break;
					case 'P':
						//disabled=1;
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
			//"\t-gN\t-\n"
			"\t-j\tenable joystick keys\n"
			"\t-J\tdisable joystick\n"
			/*
			"\t-k0\tNo REU\n"
			"\t-k1\t64k REU\n"
			"\t-k2\t128k REU\n"
			"\t-k3\t256k REU\n"
			"\t-k4\t512k REU\n"
			"\t-K\t(-k0) No REU\n"
			//"\t-m0\t-\n"
			//"\t-m1\t-\n"
			//"\t-m2\t-\n"
			//"\t-m3\t-\n"
			*/
			"\t-o\tenable onscreen status\n"
			"\t-O\tdisable onscreen status\n"
			//"\t-p\-\n"
			//"\t-P\-\n"
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
	if (bios_reload()||bdos_load("c1541.rom"))
		return printferror(txt_error_bios),1;
	if (!m6510_pc.w) // if no files were loaded, the 6510 PC stays invalid...
		all_reset(); // ...so we get its expected value from the firmware!
	char *s=session_create(session_menudata); if (s)
		return sprintf(session_scratch,"Cannot create session: %s!",s),printferror(session_scratch),1;
	debug_setup(); session_kbdreset();
	session_kbdsetup(kbd_map_xlt,length(kbd_map_xlt)/2);
	video_target=&video_frame[video_pos_y*VIDEO_LENGTH_X+video_pos_y]; audio_target=audio_frame;
	audio_disabled=!session_audio;
	video_clut_update(); onscreen_inks(0xAA0000,0x55FF55);
	if (session_fullscreen) session_togglefullscreen();
	// it begins, "alea jacta est!"
	while (!session_listen())
	{
		if (autorun_mode)
			MEMLOAD(kbd_bit0,autorun_kbd);
		else
		{
			for (int i=0;i<length(kbd_bit0);++i)
				kbd_bit0[i]=kbd_bit[i]|joy_bit[i];
			if (kbd_bit[ 8]) kbd_bit0[0]|=kbd_bit[ 8],kbd_bit0[1]|=128; // LEFT SHIFT + right side KEY combos (1/2)
			if (kbd_bit[15]) kbd_bit0[7]|=kbd_bit[15],kbd_bit0[1]|=128; // LEFT SHIFT + left  side KEY combos (2/2)
			if (!(~kbd_bit0[9]&3)) kbd_bit0[9]-=3; if (!(~kbd_bit0[9]&12)) kbd_bit0[9]-=12; // catch illegal UP+DOWN and LEFT+RIGHT joystick bits
		}
		while (!session_signal)
		{
			int t=63-vicii_pos_x; if (tape&&tape_enabled&&t>tape_n) t=tape_n;
			if ((CIA_TABLE_0[14]&1)&&t>cia_count_a[0]) t=cia_count_a[0];
			if ((CIA_TABLE_0[15]&1)&&t>cia_count_b[0]) t=cia_count_b[0];
			if ((CIA_TABLE_1[14]&1)&&t>cia_count_a[1]) t=cia_count_a[1];
			if ((CIA_TABLE_1[15]&1)&&t>cia_count_b[1]) t=cia_count_b[1];
			m6510_main( // clump MOS 6510 instructions together to gain speed...
			t*multi_t); // ...without missing any deadlines!
		}
		if (session_signal&SESSION_SIGNAL_FRAME) // end of frame?
		{
			if (!video_framecount&&onscreen_flag)
			{
				if (disc_disabled)
					onscreen_text(+1,-3,"--\t--",0);
				else
				{
					//onscreen_char(+3,-3,'\t');
					onscreen_byte(+1,-3,d64_track[0],d64_motor[0]);
					onscreen_byte(+4,-3,d64_track[1],d64_motor[1]);
				}
				int q=tape_enabled?128:0;
				if (tape&&tape_skipload)
					onscreen_char(+6,-3,'+'+q);
				if (tape_filesize<=0||tape_type<0)
					onscreen_text(+7,-3,tape?"REC":"---",q);
				else
				{
					int i=(long long)tape_filetell*999/tape_filesize;
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
				sid_frame(); // soften the sample output, it must not stick forever
			}
			psg_writelog();
			if (m6510_pc.w>=0XE4E2&&m6510_pc.w<0XE4EB&&mmu_mmu==7) m6510_pc.w=0XE4EB; // tape "FOUND FILENAME" boost
			//#define M65XX_TRAP_0XD0 if (M65XX_PC.w==0XE4EA&&mmu_mmu==m6510_found) z=0; // tape "FOUND FILENAME" boost
			//m6510_found=(tape&&tape_fastload)?0X07:-1; // set tape "FOUND FILENAME" boost
			if (cia_port_13_n>500)
				{ /*if (~tape_enabled&2) */tape_enabled|=+2; }
			else if (cia_port_13_n<50)
				{ /*if (tape_enabled&2) */tape_enabled&=~2; }
			cia_port_13_n=0;
			static BYTE tape_loud_n=0; if (!tape_fastload)
				tape_loud_n=0/*,tape_loud=1*/;
			else if (tape_song||(sid_mixer[0]&&(SID_TABLE_0[4]|SID_TABLE_0[11]|SID_TABLE_0[18])))
				tape_loud_n|=16,tape_song=/*tape_loud=*/0; // expect song to play for several frames
			else if (tape_loud_n) // no sound, slowly return to normal
				/*if (!*/--tape_loud_n/*) tape_loud=1*/;
			if (tape_signal)
			{
				if (tape_signal<2) tape_enabled=0; // stop tape if required
				tape_signal=0,session_dirty=1; // update config
			}
			/*tape_skipping=*/audio_queue=0; // reset tape and audio flags
			if (tape_filetell<tape_filesize&&tape_skipload&&!session_filmfile&&tape_enabled&&!tape_loud_n)
				video_framelimit|=(MAIN_FRAMESKIP_MASK+1),session_fast|=2,video_interlaced|=2,audio_disabled|=2; // abuse binary logic to reduce activity
			else
				video_framelimit&=~(MAIN_FRAMESKIP_MASK+1),session_fast&=~2,video_interlaced&=~2,audio_disabled&=~2; // ditto, to restore normal activity
			session_update();
			vicii_chr_y=vicii_pos_y; vicii_chr_x=vicii_pos_x; //if (!audio_disabled) audio_main(1+(video_pos_x>>4)); // preload audio buffer
		}
	}
	// it's over, "acta est fabula"
	m65xx_close();
	tape_close();
	disc_closeall();
	psg_closelog();
	if ((f=session_configfile(0)))
		session_configwritemore(f),session_configwrite(f),fclose(f);
	return session_byebye(),0;
}

BOOTSTRAP

// ============================================ END OF USER INTERFACE //
