 //  ####    ####   ######  ######   ####  ------------------------- //
//  ##  ##  ##  ##  ##      ##      ##  ##  CSFEC, small Commodore 64 //
//  ##      ##      ##      ##      ##      (SixtyFour) emulator made //
//  ##       ####   ####    ####    ##      on top of CPCEC's modules //
//  ##          ##  ##      ##      ##      by Cesar Nicolas-Gonzalez //
//  ##  ##  ##  ##  ##      ##      ##  ##  since 2022-01-12 till now //
 //  ####    ####   ##      ######   ####  ------------------------- //

#define MY_CAPTION "CSFEC"
#define my_caption "csfec"
#define MY_VERSION "20220531"//"2555"
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
	{ // monochrome - black 'n white
		0X000000,0XFFFFFF,0X505050,0XA0A0A0,0X606060,0X808080,0X404040,0XC0C0C0,
		0X606060,0X404040,0X808080,0X505050,0X787878,0XC0C0C0,0X787878,0XA0A0A0,
		},{
#if 0 // Pepto's Colodore v1 -- colodore.com
		//0X000000,0XFFFFFF,0X454545,0XB9B9B9,0X565656,0X919191,0X353535,0XE6E6E6,
		//0X5B5B5B,0X3A3A3A,0X818181,0X4A4A4A,0X7B7B7B,0XE4E4E4,0X777777,0XB2B2B2,
		//},{
		0X000000,0XFFFFFF,0X5C171A,0X4FB9B1,0X6A1D74,0X328D2A,0X131279,0XE4EA4B,
		0X6A2D10,0X311A00,0XAC464B,0X282828,0X555555,0X89FF7D,0X4A47E1,0X949494,
		},{
		0X000000,0XFFFFFF,0X813338,0X75CEC8,0X8E3C97,0X56AC4D,0X2E2C9B,0XEDF171,
		0X8E5029,0X553800,0XC46C71,0X4A4A4A,0X7B7B7B,0XA9FF9F,0X706DEB,0XB2B2B2,
		},{
		0X000000,0XFFFFFF,0XA2575D,0X98DDD9,0XAD61B4,0X7CC473,0X524FB7,0XF3F694,
		0XAD764B,0X7B5D00,0XD69094,0X707070,0X9D9D9D,0XC2FFBA,0X9491F2,0XC9C9C9,
		//},{
		//0X003C00,0X82FF82,0X237123,0X5EC95E,0X2C7E2C,0X4AAB4A,0X1B651B,0X75EC75,
		//0X2F822F,0X1E691E,0X429E42,0X267526,0X3F9A3F,0X74EA74,0X3D973D,0X5BC45B,
#else // Community-Colors v1.2a -- p1x3l.net
		//0X000000,0XFFFFFF,0X494949,0XBCBCBC,0X626262,0XA0A0A0,0X434343,0XDFDFDF,
		//0X6A6A6A,0X3F3F3F,0X8F8F8F,0X4D4D4D,0X848484,0XE0E0E0,0X818181,0XB6B6B6,
		//},{
		0X000000,0XFFFFFF,0X911110,0X3DC6B6,0X921F99,0X28AE28,0X191BAC,0XD7E42B,
		0X993409,0X421A01,0XE04E46,0X2A2A2A,0X5F5F5F,0X86F77C,0X4A56DA,0X999998,
		},{
		0X000000,0XFFFFFF,0XAF2A29,0X62D8CC,0XB03FB6,0X4AC64A,0X3739C4,0XE4ED4E,
		0XB6591C,0X683808,0XEA746C,0X4D4D4D,0X848484,0XA6FA9E,0X707CE6,0XB6B6B5,
		},{
		0X000000,0XFFFFFF,0XC74D4B,0X87E5DC,0XC765CC,0X70D870,0X5C5ED6,0XEDF374,
		0XCC7F3B,0X8C5D19,0XF19790,0X737373,0XA5A5A5,0XC0FCBA,0X949EEE,0XCCCCCB,
		//},{
		//0X003C00,0X82FF82,0X257425,0X60CC60,0X328732,0X51B651,0X226F22,0X72E672,
		//0X368D36,0X206C20,0X49A949,0X277727,0X43A143,0X72E772,0X429F42,0X5DC75D,
#endif
		},{
		0X003C00,0X82FF82,0X297929,0X52B652,0X318531,0X419E41,0X216D21,0X62CF62,
		0X318531,0X216D21,0X419E41,0X297929,0X3D983D,0X62CF62,0X3D983D,0X52B652,
	}, // monochrome - green screen
};

// GLOBAL DEFINITIONS =============================================== //

#define TICKS_PER_FRAME ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)/32)
#define TICKS_PER_SECOND (TICKS_PER_FRAME*VIDEO_PLAYBACK)
int multi_t=1; // overclocking factor

// HARDWARE DEFINITIONS ============================================= //

BYTE mem_ram[9<<16],mem_rom[5<<12],mem_i_o[1<<12]; // RAM (64K C64 + 512K REU), ROM (8K KERNAL, 8K BASIC, 4K CHARGEN) and I/O (1K VIC-II, 1K SID, 1K VRAM, 1K CIA+EXTRAS)
BYTE *mmu_rom[256],*mmu_ram[256]; // pointers to all the 256-byte pages
BYTE mmu_bit[256]; // flags of all the 256-byte pages: +1 PEEK, +2 POKE, +4 DUMBPEEK, +8 DUMBPOKE
#define PEEK(x) mmu_rom[(x)>>8][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>8][x] // WARNING, x cannot be `x=EXPR`!

#define VICII_TABLE (mem_i_o)
#define VICII_COLOR (&mem_i_o[0X800])
#define CIA_TABLE_0 (&mem_i_o[0XC00])
#define CIA_TABLE_1 (&mem_i_o[0XD00])
#define SID_TABLE_0 (&mem_i_o[0X400])
BYTE *SID_TABLE[3]={SID_TABLE_0,&mem_i_o[0XE00],&mem_i_o[0XF00]}; // default addresses: $D400 (always 1st), $DE00 (2nd) and $DF00 (3rd)

BYTE disc_disabled=0; // disables the disc drive altogether as well as its extended logic
BYTE disc_filemode=1; // default read only + strict disc writes
BYTE reu_depth=0,reu_dirty=0; // REU memory extension
int reu_kbyte[]={0,64,128,256,512}; // REU size in kb

BYTE video_type=length(video_table)/2; // 0 = monochrome, 1=darkest colour, etc.
VIDEO_UNIT video_clut[32]; // precalculated 16-colour palette + $D020 border [16] + $D021.. background 0-3 [17..20] + $D025.. sprite multicolour 0-1 [21,22] + sprite #0-7 colour [23..30] + DEBUG

void video_clut_update(void) // precalculate palette following `video_type`
{
	for (int i=0;i<16;++i)
		video_clut[i]=video_table[video_type][i]; // VIC-II built-in palette
	for (int i=0;i<15;++i)
		video_clut[16+i]=video_table[video_type][VICII_TABLE[32+i]&15]; // VIC-II dynamic palette
}

//char audio_dirty; int audio_queue=0; // used to clump audio updates together to gain speed

// the MOS 6510 and its MMU ----------------------------------------- //

Z80W m6510_pc; BYTE m6510_a,m6510_x,m6510_y,m6510_s,m6510_p; int m6510_i_t,m6510_n_t; // the MOS 6510 registers; we must be able to spy them!
BYTE mmu_cfg[2]; // the first two bytes in the MOS 6510 address map; distinct to the first two bytes of RAM!
BYTE mmu_out,mmu_mcr; // the full and MMU-only values defined by the MOS 6510's first two bytes
BYTE mmu_inp=0X17; // the tape bit mask, whose bits can appear at address 0X0001: bit 4 is !TAPE, bits 0-2 are always 1
BYTE mmu_fly; // the counter of the volatile contents of `mmu_out`, that fade away as time goes on
BYTE tape_enabled=0; int tape_polling=0; // hard/soft automatic tape playback
#define tape_disabled (tape_enabled<=(mmu_cfg[1]&32)) // the machine can disable the tape even when we enable it

int m6510_ready=0,m6510_power=0X07;

void mmu_setup(void) // by default, everything is RAM
{
	for (int i=0;i<256;++i) mmu_rom[i]=mmu_ram[i]=mem_ram;
	MEMZERO(mmu_bit); mmu_bit[0]=1+2; // ZEROPAGE is always special!
}

// PORT $0001 input mask: 0XFF -0X17 (MMU + TAPE INP) -0X20 (TAPE MOTOR) =0XC8!
#define MMU_CFG_GET(w) (w?(mmu_cfg[0]&mmu_cfg[1])+(~mmu_cfg[0]&((mmu_out&0XC8)|mmu_inp)):mmu_cfg[0])
BYTE mmu_cfg_get(WORD w) { return MMU_CFG_GET(w); }
#define MMU_CFG_SET(w,b) do{ mmu_cfg[w]=b; mmu_update(); }while(0)
#define mmu_cfg_set MMU_CFG_SET

void mmu_update(void) // set the address offsets, as well as the tables for m6510_recv(WORD) and m6510_send(WORD,BYTE)
{
	int i=((~mmu_cfg[0]&mmu_inp)+(mmu_cfg[0]&mmu_cfg[1]))&7;
	if ((mmu_cfg[1]&~mmu_out)&0X37) mmu_fly=4; // set limit
	mmu_out=(~mmu_cfg[0]&mmu_out)+(mmu_cfg[0]&mmu_cfg[1]);
	// KNUCKLEBUSTER.TAP expects something about bit 4 of mmu_out...
	if (mmu_mcr!=i) // this function is slow, avoid clobbering
	{
		BYTE *recva=mem_ram,*recvd=mem_ram,*sendd=mem_ram,*recve=mem_ram;
		switch (mmu_mcr=i) // notice the "incremental" design and the lack of `break` here!
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
		i=mmu_mcr>4?15:0; // tag the page I/O only if possible
		memset(&mmu_bit[0XD0],i  ,0X04); // $D000-$D7FF: VIC-II catches all operations
		memset(&mmu_bit[0XD4],i&3,0X04); // $D000-$D7FF: SID #1 catches non-dumb R/W
		memset(&mmu_bit[0XD8],i&2,0X04); // $D800-$DBFF: VIC-II COLOR catches non-dumb W
		memset(&mmu_bit[0XDC],i  ,0X04); // $DC00-$DFFF: CIA #1+#2 AREA catches all operations
	}
}

void mmu_reset(void)
	{ mmu_cfg[0]=0X2F; mmu_cfg[1]=mmu_mcr=0XFF; mmu_out=0; mmu_update(); }

// the VIC-II video chip ============================================ //

BYTE *vicii_memory,*vicii_attrib,*vicii_bitmap; // pointers to the video data blocks: the 16K page, the attribute map and the pixel table
BYTE vicii_mode; // the current video mode: 0..7 video mode proper, +8 LEFT/RIGHT BORDER, +16 = BACKGROUND COLOUR, +32 TOP/BOTTOM BORDER
int vicii_pos_y,vicii_len_y,vicii_irq_y; // coordinates and dimensions of the current frame (PAL-312 or NTSC-262)
int vicii_pos_x,vicii_len_x,vicii_irq_x; // coordinates and dimensions of the current scanline and its sprite fetch point
#define VICII_MINUS 8 // the amount of final ticks that are reserved for the sprite fetching
BYTE vicii_ready; // bit 4 of $D011, the DEN bit; it must stick in several moments of the frame
BYTE vicii_frame; // enabled when a new frame must begin (not exactly the same as vicii_pos_y>=vicii_len_y)
BYTE vicii_badline; // are we in a badline?
BYTE vicii_takeover; // is the VIC-II going to fetch the sprites, a badline...?
BYTE vicii_dmadelay; // advanced horizontal scroll caused by DMA delay
BYTE vicii_nosprites=0,vicii_noimpacts=0; // cheating!!

void vicii_setmode(void) // recalculate bitmap mode when VIC-II registers 17 or 22 are modified
	{ vicii_mode=(vicii_mode&~7)+(((VICII_TABLE[17]&96)+(VICII_TABLE[22]&16))>>4); }
void vicii_setmaps(void) // recalculate memory maps when VIC-II register 17 or 24 or CIA #2 registers 0 or 2 are modified
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
		else // BASE RAM!
			vicii_bitmap=&vicii_memory[(VICII_TABLE[24]&14)<<10];
	}
}

void vicii_reset(void)
{
	memset(VICII_TABLE,vicii_mode=vicii_pos_x=vicii_pos_y=vicii_irq_y=vicii_frame=0,64);
	vicii_setmaps(); vicii_setmode();
}

// the CIA #1+#2 gate arrays ======================================== //

BYTE key2joy_flag=0; // alternate joystick ports
BYTE kbd_bit0[10]; // combined keyboard+joystick bits

int cia_count_a[2],cia_count_b[2]; // the CIA event counters: the active value range is 0..65536
int cia_minor_a[2],cia_minor_b[2],cia_major_b[2]; // MINOR values matter on each clock tick, MAJOR values matter on each cia_count_a[] underflow
BYTE cia_state_a[2],cia_state_b[2]; // state bit masks: +8 LOAD, +4 WAIT, +2 MOVE, +1 STOP
int cia_event_a[2],cia_event_b[2]; // some events change the behavior of the CIA chips; -1 means no new event
int cia_xchange[2],cia_serials[2]; // serial bit transfer timeouts

#define cia_port_14(i,b) (cia_minor_a[i]=((b)&32)?0:-1)
#define cia_port_15(i,b) ((cia_minor_b[i]=((b)&96)?0:-1),(cia_major_b[i]=((b)&96)!=64?0:-1))

DWORD cia_hhmmssd[2]; // the CIA time-of-day registers
BYTE cia_port_13[2],cia_port_11[2]; // flags: CIA interrupt status + locked time-of-day status
char cia_old6526=1; // the original CIA 6526 had a bug that the later CIA 6526A fixed out: polling port 13 halted the countdown for an instant (?)
char cia_oldglue=1; // the original CIA 6526 - VIC-II 6569 bridge relied on discrete glue logic instead of a custom IC devised for the VIC-II 8565

void cia_reset(void)
{
	cia_hhmmssd[0]=cia_hhmmssd[1]=0X01000000; // = 1:00:00.0 AM
	memset(CIA_TABLE_0,cia_port_13[0]=0,16);
	memset(CIA_TABLE_1,cia_port_13[1]=0,16);
	CIA_TABLE_0[11]=CIA_TABLE_1[11]=1;
	cia_state_a[0]=cia_state_a[1]=cia_state_b[0]=cia_state_b[1]=+0+0+0+1;
	cia_minor_a[0]=cia_minor_a[1]=
	cia_minor_b[0]=cia_minor_b[1]=
	cia_major_b[0]=cia_major_b[1]=
	cia_port_11[0]=cia_port_11[1]=
	cia_xchange[0]=cia_xchange[1]=0;
	cia_serials[0]=cia_serials[1]=0;
	CIA_TABLE_0[4]=CIA_TABLE_0[5]=CIA_TABLE_0[6]=CIA_TABLE_0[7]=
	CIA_TABLE_1[4]=CIA_TABLE_1[5]=CIA_TABLE_1[6]=CIA_TABLE_1[7]=
	CIA_TABLE_0[0]=CIA_TABLE_0[1]=CIA_TABLE_1[0]=CIA_TABLE_1[1]=
	cia_count_a[0]=cia_count_a[1]=cia_count_b[0]=cia_count_b[1]=
	cia_event_a[0]=cia_event_a[1]=cia_event_b[0]=cia_event_b[1]=-1;
}

// behind the CIA: tape I/O handling -------------------------------- //

char tape_path[STRMAX]="",tap_magic[]="C64-TAPE-RAW",t64_magic1[]="C64 tape image",t64_magic2[]="C64S tape ";
FILE *tape=NULL; int tape_filetell=0,tape_filesize=0; // file handle, offset and length

BYTE tape_buffer[8<<8],tape_old_type; int tape_length,tape_offset; // tape buffer/directory (must be >1k!)
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

int tape_rewind=0,tape_skipload=1,tape_fastload=1/*,tape_skipping=0*/; // tape options
int tape_signal/*,tape_status=0,tape_output*/,tape_record,tape_type=0; // tape status
int tape_t; // duration of the current tape signal in microseconds
int m6510_t64ok=-1; // T64-specific variable: MMU configuration trap

int tape_close(void) // closes tape and cleans up
{
	if (tape)
	{
		if (tape_type<0) fseek(tape,16,SEEK_SET),fputiiii(tape_filesize-20,tape); // saving a tape? store the real tape length
		puff_fclose(tape),tape=NULL;
		mmu_inp|=16; // i.e. TAPE IS MISSING
	}
	tape_filesize=tape_filetell=0; m6510_t64ok=-1;
	return tape_type=tape_t=/*tape_output=tape_status=*/0;
}

int tape_open(char *s) // inserts a tape from path `s`; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"rb")))
		return 1; // cannot open file!
	tape_length=fread1(tape_buffer,sizeof(tape_buffer),tape);
	if (tape_length>(tape_offset=16)&&!memcmp(tape_buffer,tap_magic,12))
	{
		tape_old_type=!tape_buffer[12]; // in the old versions a zero isn't a prefix ("HAWKEYE") but a signal-less 256-tick duration
		tape_filesize=tape_getcccc()+tape_filetell;
		tape_type=+1; mmu_inp&=~16; // i.e. TAPE IS READY
	}
	else if (tape_length>=64+tape_buffer[34]*32&&!memcmp(tape_buffer,t64_magic1,14)&&tape_buffer[33]==1&&tape_buffer[34]>=tape_buffer[36]&&tape_buffer[36])
	{
		tape_filetell=0; tape_filesize=tape_buffer[36]; // the buffer won't be used as such, so we can rewire it to our purposes
		tape_type=+2; m6510_t64ok=0X07;
	}
	else if (tape_length>=64&&!memcmp(tape_buffer,t64_magic2,10)&&tape_buffer[36]<2) // obsolete T64 archives made with faulty tools
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

int tape_create(char *s) // creates a tape on path `s`; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"wb")))
		return 1; // cannot create file!
	fwrite1(tap_magic,16,tape); fputiiii(0,tape); // the real tape length will be stored later
	if (tape_path!=s) strcpy(tape_path,s);
	mmu_inp&=~16; // i.e. TAPE IS READY
	tape_filesize=tape_filetell=0;
	return tape_type=-1,tape_t=/*tape_output=tape_status=*/0;
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
			return tape_seek(i),tape_t=0,0;
		case +2: // T64
			return tape_filetell=i,0;
	}
	return -1; // unknown type!
}

int tape_feed(void) // fetch next tape sample length
{
	switch (tape_type)
	{
		case +1: // TAP
			for (int watchdog=99,i;watchdog>0;--watchdog) // catch bad tapes
			{
				if ((i=tape_getc())<0) // EOF?
				{
					if (!tape_rewind) break; // end of tape!
					tape_seek(20); // rewind and retry
				}
				else if (i>0) return i<<3;
				else if (tape_old_type) // zero in old tapes is a padding, not a prefix
				{
					do i+=256<<3; while (!(watchdog=tape_getc()));
					if (watchdog>0) return i+watchdog; break; // end of tape!
				}
				else if ((i=tape_getccc())<0) break; // end of tape!
				else if (i>0) return i;
				else return 1<<24; // can this ever happen?
			}
			tape_close(),tape_signal=-1; // watchdog crisis!
	}
	return 1<<16;
}

// T64 files aren't real tapes, but archives of virtual files; reading them relies on "trapping" the MOS 6510

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
	if (!i) return tape_t64load(NULL); // no filename? load anything!
	char t[17]; memcpy(t,&mem_ram[mgetii(&mem_ram[0XBB])],16);
	while (i<16) t[i++]=' '; t[i]=0; // pad the filename with spaces!
	return tape_t64load(t);
}

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
		18,1,65,-0,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,
		31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,
		-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,-1,-1,31,21,
		-1,-1,31,17,-4,-1,7,19,-1,-1,7,19,-1,-1,7,19,-1,-1,7,19,-1,-1,7,19,-1,
		-1,7,19,-1,-1,7,18,-1,-1,3,18,-1,-1,3,18,-1,-1,3,18,-1,-1,3,18,-1,-1,
		3,18,-1,-1,3,17,-1,-1,1,17,-1,-1,1,17,-1,-1,1,17,-1,-1,1,17,-1,-1,1,
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

// D64: MOS 6502 MICROPROCESSOR ===================================== //

BYTE d64_mem[64<<10],d64_track[4]={0,0,0,0},d64_motor[4]={0,0,0,0}; // 48K RAM + 16K ROM
BYTE *d64_ram[4]={NULL,NULL,NULL,NULL};
FILE *d64[4]={NULL,NULL,NULL,NULL};
#define d64_rom &d64_mem[48<<10]

void m6502_sync(int t) // runs the C1541 hardware for `t` microseconds
{
	; // nothing here yet!
}

Z80W m6502_pc; BYTE m6502_a,m6502_x,m6502_y,m6502_s,m6502_p; int m6502_i_t; // the MOS 6502 registers

BYTE m6502_recv(WORD w) // receive a byte from the D64 address `w`
{
	return 0XFF; // nothing here yet!
}
void m6502_send(WORD w,BYTE b) // send the byte `b` to the D64 address `w`
{
	; // nothing here yet!
}

// the MOS 6510 memory operations: PAGE, PEEK, POKE and others
#define M65XX_LOCAL
#define M65XX_PAGE(x) (0)
#define M65XX_PEEK(x) ((x>=0X2000&&x<0X1800)?m6502_recv(x):d64_mem[x]) // tell apart between 0000-17FF, 1800-1FFF, 2000-BFFF and C000-FFFF
#define M65XX_POKE(x,o) do{ if (x<0X2000) { if (x<0X1800) d64_mem[x]=o; else m6502_send(x,o); } else if (x<0XC000) d64_mem[x]=0; }while(0)
#define M65XX_PEEKZERO(x) (d64_mem[x])
#define M65XX_POKEZERO(x,o) (d64_mem[x]=o)
#define M65XX_PULL(x) (d64_mem[256+x])
#define M65XX_PUSH(x,o) (d64_mem[256+x]=o)
// the C1541's MOS 6502 "dumb" operations do nothing relevant, they only consume clock ticks
#define M65XX_DUMBPAGE(x) (0)
#define M65XX_DUMBPEEK(x) (0)
#define M65XX_DUMBPOKE(x,o) (0)
#define M65XX_DUMBPEEKZERO(x) (0)
#define M65XX_DUMBPOKEZERO(x,o) (0)
#define M65XX_DUMBPULL(x) (0)
// the delays between instructions are simple 1 T pauses
#define M65XX_XEE 0XEE
#define M65XX_XEF 0XEF
#define M65XX_RDY 0
#define M65XX_TICK (++m65xx_t)
#define M65XX_WAIT (++m65xx_t)

#define M65XX_START (mgetii(&d64_mem[0XFFFC]))
#define M65XX_RESET m6502_reset
#define M65XX_MAIN m6502_main
#define M65XX_SYNC m6502_sync
#define M65XX_I_T m6502_i_t
#define M65XX_IRQ_ACK 0
#define M65XX_PC m6502_pc
#define M65XX_A m6502_a
#define M65XX_X m6502_x
#define M65XX_Y m6502_y
#define M65XX_S m6502_s
#define M65XX_P m6502_p
#define M65XX_I m6502_i

#include "cpcec-m6.h"

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

int /*tape_loud=1,*/tape_song=0;
//void audio_main(int t) { sid_main(t/*,((tape_status^tape_output)&tape_loud)<<12*/); }
#define audio_main sid_main

WORD vicii_hits[512]; // 96+320+96 pixels (although we only show 32+320+32); +1..+128 means impact with sprite #0..#7, +256 means impact with a pixel in the scenery
WORD vicii_copy_hits; // the old value may stick for a short while (DL/Discrete Logic versus IC/Integrated Circuit)
BYTE vicii_copy_border[256],vicii_copy_border_l,vicii_copy_border_r; // backups of border colours and states
int vicii_cursor=0,vicii_backup=0; BYTE vicii_eighth=0; // 0..1023, 0..1023 and 0..7 respectively while drawing the bitmap
BYTE vicii_cache[40]; // the 40 ATTRIB bytes that are read once every badline
BYTE vicii_horizon=0,vicii_port_22=0; // bits 2-0 and 3 of register 22
DWORD vicii_sprite_k[8]; // the current 24 bits of each sprite's scanline
BYTE vicii_sprite_y[8]; // the low 6 bits of each sprite's counter
BYTE vicii_sprite_x; // the double-height flip-flop table
BYTE vicii_spritexy; // toggling the sprite height can delay sprite reloading
BYTE vicii_cow=0; // "fetchez la vache!"

void vicii_draw_sprite(VIDEO_UNIT *t,char i,DWORD k) // draw a sprite on screen; `t` is the video buffer (NULL if only the collision matters), `i` is the sprite ID and `k` is its current 24-bit data
{
	vicii_sprite_k[i]=0; // don't draw the same sprite twice!
	BYTE j=1<<i; int x=VICII_TABLE[0+i*2];
	if (VICII_TABLE[16]&j) if ((x+=256)>=384) x-=vicii_len_x*8; // PAL wraps at $01F8!
	if ((x-=24)>=320+8) return; // too far to the right? (cfr. the bouncing circle of letters of "4KRAWALL")
	WORD *h=&vicii_hits[96+x]; char a; // collision bit pointer and buffers
	if (t) // draw sprite and update collision bits
	{
		t+=x*2; if (UNLIKELY(VICII_TABLE[29]&j)) // double?
		{
			if (x<-48) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=48-4,t+=96-8;
				VIDEO_UNIT p,pp[4]={0,video_clut[21],video_clut[23+i],video_clut[22]};
				if (VICII_TABLE[27]&j) // back?
					do
						if (a=k&3)
						{
							p=pp[a];
							x|=h[0]|=j;
							x|=h[1]|=j;
							x|=h[2]|=j;
							x|=h[3]|=j;
							if (h[0]<256) t[0]=t[1]=p;
							if (h[1]<256) t[2]=t[3]=p;
							if (h[2]<256) t[4]=t[5]=p;
							if (h[3]<256) t[6]=t[7]=p;
						}
					while (h-=4,t-=8,k>>=2);
				else // fore!
					do
						if (a=k&3)
						{
							p=pp[a];
							x|=h[0]|=j;
							x|=h[1]|=j;
							x|=h[2]|=j;
							x|=h[3]|=j;
							t[0]=t[1]=
							t[2]=t[3]=
							t[4]=t[5]=
							t[6]=t[7]=p;
						}
					while (h-=4,t-=8,k>>=2);
			}
			else // HI-RES
			{
				h+=48-2,t+=96-4;
				VIDEO_UNIT p=video_clut[23+i];
				if (VICII_TABLE[27]&j) // back?
					do
						if (k&1)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
							if (h[0]<256) t[0]=t[1]=p;
							if (h[1]<256) t[2]=t[3]=p;
						}
					while (h-=2,t-=4,k>>=1);
				else // fore!
					do
						if (k&1)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
							t[0]=t[1]=
							t[2]=t[3]=p;
						}
					while (h-=2,t-=4,k>>=1);
			}
		}
		else // single!
		{
			if (x<-24) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=24-2,t+=48-4;
				VIDEO_UNIT p,pp[4]={0,video_clut[21],video_clut[23+i],video_clut[22]};
				if (VICII_TABLE[27]&j) // back?
					do
						if (a=k&3)
						{
							p=pp[a];
							x|=h[0]|=j;
							x|=h[1]|=j;
							if (h[0]<256) t[0]=t[1]=p;
							if (h[1]<256) t[2]=t[3]=p;
						}
					while (h-=2,t-=4,k>>=2);
				else // fore!
					do
						if (a=k&3)
						{
							p=pp[a];
							x|=h[0]|=j;
							x|=h[1]|=j;
							t[0]=t[1]=
							t[2]=t[3]=p;
						}
					while (h-=2,t-=4,k>>=2);
			}
			else // HI-RES
			{
				h+=24-1,t+=48-2;
				VIDEO_UNIT p=video_clut[23+i];
				if (VICII_TABLE[27]&j) // back?
					do
						if (k&1)
						{
							x|=h[0]|=j;
							if (h[0]<256) t[0]=t[1]=p;
						}
					while (h-=1,t-=2,k>>=1);
				else // fore!
					do
						if (k&1)
						{
							x|=h[0]|=j;
							t[0]=t[1]=p;
						}
					while (h-=1,t-=2,k>>=1);
			}
		}
	}
	else // don't draw, just plot the collision bits
	{
		if (UNLIKELY(VICII_TABLE[29]&j)) // double?
		{
			if (x<=-48) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=48-4;
				if (VICII_TABLE[27]&j) // back?
					do
						if (a=k&3)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
							x|=h[2]|=j;
							x|=h[3]|=j;
						}
					while (h-=4,k>>=2);
				else // fore!
					do
						if (a=k&3)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
							x|=h[2]|=j;
							x|=h[3]|=j;
						}
					while (h-=4,k>>=2);
			}
			else // HI-RES
			{
				h+=48-2;
				if (VICII_TABLE[27]&j) // back?
					do
						if (k&1)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
						}
					while (h-=2,k>>=1);
				else // fore!
					do
						if (k&1)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
						}
					while (h-=2,k>>=1);
			}
		}
		else // single!
		{
			if (x<=-24) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=24-2;
				if (VICII_TABLE[27]&j) // back?
					do
						if (a=k&3)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
						}
					while (h-=2,k>>=2);
				else // fore!
					do
						if (a=k&3)
						{
							x|=h[0]|=j;
							x|=h[1]|=j;
						}
					while (h-=2,k>>=2);
			}
			else // HI-RES
			{
				h+=24-1;
				if (VICII_TABLE[27]&j) // back?
					do
						if (k&1)
						{
							x|=h[0]|=j;
						}
					while (h-=1,k>>=1);
				else // fore!
					do
						if (k&1)
						{
							x|=h[0]|=j;
						}
					while (h-=1,k>>=1);
			}
		}
	}
	if (vicii_noimpacts) return; // no sprite hits!
	if (x&256) VICII_TABLE[31]|=j,VICII_TABLE[25]|=2; if ((x&=255)&&j!=x) VICII_TABLE[30]|=x,VICII_TABLE[25]|=4;
}

void vicii_draw_canvas(void) // render a single line of the canvas, relying on previously gathered datas
{
	if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
	{
		VIDEO_UNIT *t=video_target-video_pos_x+VIDEO_OFFSET_X,*u=t,p; WORD y=vicii_cursor;
		p=(vicii_mode&4)?video_clut[0]:video_clut[17]; // "UKIYO-SAMAR" uses mode 7 in the first picture (red background and black border)
		for (BYTE i=(VIDEO_PIXELS_X-640)/2+vicii_horizon*2;i--;) *t++=p;
		if ((!vicii_cow||vicii_nosprites)) // no sprites, no collisions, just the pixels!
		{
			switch (vicii_mode) // render all the background pixels in a single go!
			{
				case 0: // HI-RES CHARACTER
					for (BYTE x=0;x<40;++x)
					{
						BYTE b=vicii_bitmap[vicii_cache[x]*8+vicii_eighth];
						VIDEO_UNIT p0=video_clut[VICII_COLOR[y++&1023]];
						*t++=p=(b&128)?p0:video_clut[17]; *t++=p;
						*t++=p=(b& 64)?p0:video_clut[17]; *t++=p;
						*t++=p=(b& 32)?p0:video_clut[17]; *t++=p;
						*t++=p=(b& 16)?p0:video_clut[17]; *t++=p;
						*t++=p=(b&  8)?p0:video_clut[17]; *t++=p;
						*t++=p=(b&  4)?p0:video_clut[17]; *t++=p;
						*t++=p=(b&  2)?p0:video_clut[17]; *t++=p;
						*t++=p=(b&  1)?p0:video_clut[17]; *t++=p;
					}
					break;
				case 1: // LO-RES CHARACTER
					for (BYTE x=0;x<40;++x)
					{
						BYTE b=vicii_bitmap[vicii_cache[x]*8+vicii_eighth],c=VICII_COLOR[y++&1023];
						if (c>=8) // LO-RES?
						{
							VIDEO_UNIT q[4]={video_clut[17],video_clut[18],video_clut[19],video_clut[c-8]};
							*t++=p=q[ b>>6   ]; *t++=p; *t++=p; *t++=p;
							*t++=p=q[(b>>4)&3]; *t++=p; *t++=p; *t++=p;
							*t++=p=q[(b>>2)&3]; *t++=p; *t++=p; *t++=p;
							*t++=p=q[(b   )&3]; *t++=p; *t++=p; *t++=p;
						}
						else // HI-RES!
						{
							VIDEO_UNIT q1=video_clut[c];
							*t++=p=(b&128)?q1:video_clut[17]; *t++=p;
							*t++=p=(b& 64)?q1:video_clut[17]; *t++=p;
							*t++=p=(b& 32)?q1:video_clut[17]; *t++=p;
							*t++=p=(b& 16)?q1:video_clut[17]; *t++=p;
							*t++=p=(b&  8)?q1:video_clut[17]; *t++=p;
							*t++=p=(b&  4)?q1:video_clut[17]; *t++=p;
							*t++=p=(b&  2)?q1:video_clut[17]; *t++=p;
							*t++=p=(b&  1)?q1:video_clut[17]; *t++=p;
						}
					}
					break;
				case 2: // HI-RES GRAPHICS
					for (BYTE x=0;x<40;++x)
					{
						BYTE a=vicii_cache[x],b=vicii_bitmap[(y++&1023)*8+vicii_eighth];
						VIDEO_UNIT q0=video_clut[a&15],q1=video_clut[a>>4];
						*t++=p=(b&128)?q1:q0; *t++=p;
						*t++=p=(b& 64)?q1:q0; *t++=p;
						*t++=p=(b& 32)?q1:q0; *t++=p;
						*t++=p=(b& 16)?q1:q0; *t++=p;
						*t++=p=(b&  8)?q1:q0; *t++=p;
						*t++=p=(b&  4)?q1:q0; *t++=p;
						*t++=p=(b&  2)?q1:q0; *t++=p;
						*t++=p=(b&  1)?q1:q0; *t++=p;
					}
					break;
				case 3: // LO-RES GRAPHICS
					for (BYTE x=0;x<40;++x)
					{
						BYTE a=vicii_cache[x],b=vicii_bitmap[(y&=1023)*8+vicii_eighth];
						VIDEO_UNIT q[4]={video_clut[17],video_clut[a>>4],video_clut[a&15],video_clut[VICII_COLOR[y++]]};
						*t++=p=q[ b>>6   ]; *t++=p; *t++=p; *t++=p;
						*t++=p=q[(b>>4)&3]; *t++=p; *t++=p; *t++=p;
						*t++=p=q[(b>>2)&3]; *t++=p; *t++=p; *t++=p;
						*t++=p=q[(b   )&3]; *t++=p; *t++=p; *t++=p;
					}
					break;
				case 4: // HI-RES EXTENDED
					for (BYTE x=0;x<40;++x)
					{
						BYTE a=vicii_cache[x],b=vicii_bitmap[(a&63)*8+vicii_eighth];
						VIDEO_UNIT q0=video_clut[(a>>6)+17],q1=video_clut[VICII_COLOR[y++&1023]];
						*t++=p=(b&128)?q1:q0; *t++=p;
						*t++=p=(b& 64)?q1:q0; *t++=p;
						*t++=p=(b& 32)?q1:q0; *t++=p;
						*t++=p=(b& 16)?q1:q0; *t++=p;
						*t++=p=(b&  8)?q1:q0; *t++=p;
						*t++=p=(b&  4)?q1:q0; *t++=p;
						*t++=p=(b&  2)?q1:q0; *t++=p;
						*t++=p=(b&  1)?q1:q0; *t++=p;
					}
					break;
				case 16+0: // IDLE HI-RES CHARACTER
				case 16+1: // IDLE LO-RES CHARACTER
				case 16+4: // IDLE HI-RES EXTENDED
					for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x)
					{
						*t++=p=(b&128)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(b& 64)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(b& 32)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(b& 16)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(b&  8)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(b&  4)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(b&  2)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(b&  1)?video_clut[0]:video_clut[17]; *t++=p;
					}
					break;
				case 16+3: // IDLE LO-RES GRAPHICS
					for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x)
					{
						*t++=p=(b&192)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
						*t++=p=(b& 48)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
						*t++=p=(b& 12)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
						*t++=p=(b&  3)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
					}
					break;
				case 16+2: // IDLE HI-RES GRAPHICS
				case 5:
				case 6:
				case 7: // UNDEFINED MODES! (always black!?!?!?)
				case 16+5:
				case 16+6:
				case 16+7: // IDLE UNDEFINED MODES!
				default: // BLACK BACKGROUND!
					p=video_clut[0]; for (int x=0;x<320;++x) *t++=p,*t++=p;
			}
			p=video_clut[17]; for (BYTE i=(VIDEO_PIXELS_X-640)/2-vicii_horizon*2;i--;) *t++=p;
			MEMZERO(vicii_sprite_k); // nuke sprites!
		}
		else
		{
			memset( vicii_hits     ,0,sizeof(vicii_hits)/4);
			memset(&vicii_hits[384],0,sizeof(vicii_hits)/4);
			WORD *h=&vicii_hits[(512-320)/2+vicii_horizon];
			switch (vicii_mode) // render the background pixels and the collision bitmap in a single go!
			{
				case 0: // HI-RES CHARACTER
					for (BYTE x=0;x<40;++x,h+=8)
					{
						BYTE b=vicii_bitmap[vicii_cache[x]*8+vicii_eighth];
						VIDEO_UNIT p0=video_clut[VICII_COLOR[y++&1023]];
						*t++=p=(h[0]=(b&128)<<1)?p0:video_clut[17]; *t++=p;
						*t++=p=(h[1]=(b& 64)<<2)?p0:video_clut[17]; *t++=p;
						*t++=p=(h[2]=(b& 32)<<3)?p0:video_clut[17]; *t++=p;
						*t++=p=(h[3]=(b& 16)<<4)?p0:video_clut[17]; *t++=p;
						*t++=p=(h[4]=(b&  8)<<5)?p0:video_clut[17]; *t++=p;
						*t++=p=(h[5]=(b&  4)<<6)?p0:video_clut[17]; *t++=p;
						*t++=p=(h[6]=(b&  2)<<7)?p0:video_clut[17]; *t++=p;
						*t++=p=(h[7]=(b&  1)<<8)?p0:video_clut[17]; *t++=p;
					}
					break;
				case 1: // LO-RES CHARACTER
					for (BYTE x=0;x<40;++x,h+=8)
					{
						BYTE b=vicii_bitmap[vicii_cache[x]*8+vicii_eighth],c=VICII_COLOR[y++&1023];
						if (c>=8) // LO-RES?
						{
							VIDEO_UNIT q[4]={video_clut[17],video_clut[18],video_clut[19],video_clut[c-8]};
							h[0]=h[1]=(b&128)<<1; *t++=p=q[ b>>6   ]; *t++=p; *t++=p; *t++=p;
							h[2]=h[3]=(b& 32)<<3; *t++=p=q[(b>>4)&3]; *t++=p; *t++=p; *t++=p;
							h[4]=h[5]=(b&  8)<<5; *t++=p=q[(b>>2)&3]; *t++=p; *t++=p; *t++=p;
							h[6]=h[7]=(b&  2)<<7; *t++=p=q[(b   )&3]; *t++=p; *t++=p; *t++=p;
						}
						else // HI-RES!
						{
							VIDEO_UNIT q1=video_clut[c];
							*t++=p=(h[0]=(b&128)<<1)?q1:video_clut[17]; *t++=p;
							*t++=p=(h[1]=(b& 64)<<2)?q1:video_clut[17]; *t++=p;
							*t++=p=(h[2]=(b& 32)<<3)?q1:video_clut[17]; *t++=p;
							*t++=p=(h[3]=(b& 16)<<4)?q1:video_clut[17]; *t++=p;
							*t++=p=(h[4]=(b&  8)<<5)?q1:video_clut[17]; *t++=p;
							*t++=p=(h[5]=(b&  4)<<6)?q1:video_clut[17]; *t++=p;
							*t++=p=(h[6]=(b&  2)<<7)?q1:video_clut[17]; *t++=p;
							*t++=p=(h[7]=(b&  1)<<8)?q1:video_clut[17]; *t++=p;
						}
					}
					break;
				case 2: // HI-RES GRAPHICS
					for (BYTE x=0;x<40;++x,h+=8)
					{
						BYTE a=vicii_cache[x],b=vicii_bitmap[(y++&1023)*8+vicii_eighth];
						VIDEO_UNIT q0=video_clut[a&15],q1=video_clut[a>>4];
						*t++=p=(h[0]=(b&128)<<1)?q1:q0; *t++=p;
						*t++=p=(h[1]=(b& 64)<<2)?q1:q0; *t++=p;
						*t++=p=(h[2]=(b& 32)<<3)?q1:q0; *t++=p;
						*t++=p=(h[3]=(b& 16)<<4)?q1:q0; *t++=p;
						*t++=p=(h[4]=(b&  8)<<5)?q1:q0; *t++=p;
						*t++=p=(h[5]=(b&  4)<<6)?q1:q0; *t++=p;
						*t++=p=(h[6]=(b&  2)<<7)?q1:q0; *t++=p;
						*t++=p=(h[7]=(b&  1)<<8)?q1:q0; *t++=p;
					}
					break;
				case 3: // LO-RES GRAPHICS
					for (BYTE x=0;x<40;++x,h+=8)
					{
						BYTE a=vicii_cache[x],b=vicii_bitmap[(y&=1023)*8+vicii_eighth];
						VIDEO_UNIT q[4]={video_clut[17],video_clut[a>>4],video_clut[a&15],video_clut[VICII_COLOR[y++]]};
						h[0]=h[1]=(b&128)<<1; *t++=p=q[ b>>6   ]; *t++=p; *t++=p; *t++=p;
						h[2]=h[3]=(b& 32)<<3; *t++=p=q[(b>>4)&3]; *t++=p; *t++=p; *t++=p;
						h[4]=h[5]=(b&  8)<<5; *t++=p=q[(b>>2)&3]; *t++=p; *t++=p; *t++=p;
						h[6]=h[7]=(b&  2)<<7; *t++=p=q[(b   )&3]; *t++=p; *t++=p; *t++=p;
					}
					break;
				case 4: // HI-RES EXTENDED
					for (BYTE x=0;x<40;++x,h+=8)
					{
						BYTE a=vicii_cache[x],b=vicii_bitmap[(a&63)*8+vicii_eighth];
						VIDEO_UNIT q0=video_clut[(a>>6)+17],q1=video_clut[VICII_COLOR[y++&1023]];
						*t++=p=(h[0]=(b&128)<<1)?q1:q0; *t++=p;
						*t++=p=(h[1]=(b& 64)<<2)?q1:q0; *t++=p;
						*t++=p=(h[2]=(b& 32)<<3)?q1:q0; *t++=p;
						*t++=p=(h[3]=(b& 16)<<4)?q1:q0; *t++=p;
						*t++=p=(h[4]=(b&  8)<<5)?q1:q0; *t++=p;
						*t++=p=(h[5]=(b&  4)<<6)?q1:q0; *t++=p;
						*t++=p=(h[6]=(b&  2)<<7)?q1:q0; *t++=p;
						*t++=p=(h[7]=(b&  1)<<8)?q1:q0; *t++=p;
					}
					break;
				case 16+0: // IDLE HI-RES CHARACTER
				case 16+1: // IDLE LO-RES CHARACTER
				case 16+4: // IDLE HI-RES EXTENDED
					for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x,h+=8)
					{
						*t++=p=(h[0]=(b&128)<<1)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(h[1]=(b& 64)<<2)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(h[2]=(b& 32)<<3)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(h[3]=(b& 16)<<4)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(h[4]=(b&  8)<<5)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(h[5]=(b&  4)<<6)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(h[6]=(b&  2)<<7)?video_clut[0]:video_clut[17]; *t++=p;
						*t++=p=(h[7]=(b&  1)<<8)?video_clut[0]:video_clut[17]; *t++=p;
					}
					break;
				case 16+3: // IDLE LO-RES GRAPHICS
					for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x,h+=8)
					{
						h[0]=h[1]=(b&128)<<1; *t++=p=(b&192)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
						h[2]=h[3]=(b& 32)<<3; *t++=p=(b& 48)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
						h[4]=h[5]=(b&  8)<<5; *t++=p=(b& 12)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
						h[6]=h[7]=(b&  2)<<7; *t++=p=(b&  3)?video_clut[0]:video_clut[17]; *t++=p; *t++=p; *t++=p;
					}
					break;
				case 16+2: // IDLE HI-RES GRAPHICS
				case 5:
				case 6:
				case 7: // UNDEFINED MODES! (always black!?!?!?)
				case 16+5:
				case 16+6:
				case 16+7: // IDLE UNDEFINED MODES!
				default: // BLACK BACKGROUND!
					p=video_clut[0]; for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x,h+=8)
					{
						h[0]=(b&128)<<1; *t++=p; *t++=p;
						h[1]=(b& 64)<<2; *t++=p; *t++=p;
						h[2]=(b& 32)<<3; *t++=p; *t++=p;
						h[3]=(b& 16)<<4; *t++=p; *t++=p;
						h[4]=(b&  8)<<5; *t++=p; *t++=p;
						h[5]=(b&  4)<<6; *t++=p; *t++=p;
						h[6]=(b&  2)<<7; *t++=p; *t++=p;
						h[7]=(b&  1)<<8; *t++=p; *t++=p;
					}
			}
			p=video_clut[17]; for (BYTE i=(VIDEO_PIXELS_X-640)/2-vicii_horizon*2;i--;) *t++=p;
			// render the sprites and handle their collision bits
			t=u+(VIDEO_PIXELS_X/2)-320; { int k; for (BYTE i=8;i--;) if (k=vicii_sprite_k[i]) vicii_draw_sprite(t,i,k); }
		}
	}
	else if (vicii_cow&&!(vicii_nosprites|vicii_noimpacts)) // wrong scanline or frame, do the collission bitmap and nothing else
	{
		memset( vicii_hits     ,0,sizeof(vicii_hits)/4);
		memset(&vicii_hits[384],0,sizeof(vicii_hits)/4);
		WORD *h=&vicii_hits[(512-320)/2+vicii_horizon],y=vicii_cursor;
		switch (vicii_mode) // render the background's collision bitmap in a single go!
		{
			case 0: // HI-RES CHARACTER
				for (BYTE x=0;x<40;++x,h+=8)
				{
					BYTE b=vicii_bitmap[vicii_cache[x]*8+vicii_eighth];
					h[0]=(b&128)<<1;
					h[1]=(b& 64)<<2;
					h[2]=(b& 32)<<3;
					h[3]=(b& 16)<<4;
					h[4]=(b&  8)<<5;
					h[5]=(b&  4)<<6;
					h[6]=(b&  2)<<7;
					h[7]=(b&  1)<<8;
				}
				break;
			case 1: // LO-RES CHARACTER
				for (BYTE x=0;x<40;++x,h+=8)
				{
					BYTE b=vicii_bitmap[vicii_cache[x]*8+vicii_eighth];
					if (VICII_COLOR[y++&1023]>=8) // LO-RES?
					{
						h[0]=h[1]=(b&128)<<1;
						h[2]=h[3]=(b& 32)<<3;
						h[4]=h[5]=(b&  8)<<5;
						h[6]=h[7]=(b&  2)<<7;
					}
					else // HI-RES!
					{
						h[0]=(b&128)<<1;
						h[1]=(b& 64)<<2;
						h[2]=(b& 32)<<3;
						h[3]=(b& 16)<<4;
						h[4]=(b&  8)<<5;
						h[5]=(b&  4)<<6;
						h[6]=(b&  2)<<7;
						h[7]=(b&  1)<<8;
					}
				}
				break;
			case 2: // HI-RES GRAPHICS
				for (BYTE x=0;x<40;++x,h+=8,++y)
				{
					BYTE b=vicii_bitmap[(y&1023)*8+vicii_eighth];
					h[0]=(b&128)<<1;
					h[1]=(b& 64)<<2;
					h[2]=(b& 32)<<3;
					h[3]=(b& 16)<<4;
					h[4]=(b&  8)<<5;
					h[5]=(b&  4)<<6;
					h[6]=(b&  2)<<7;
					h[7]=(b&  1)<<8;
				}
				break;
			case 3: // LO-RES GRAPHICS
				for (BYTE x=0;x<40;++x,h+=8,++y)
				{
					BYTE b=vicii_bitmap[(y&1023)*8+vicii_eighth];
					h[0]=h[1]=(b&128)<<1;
					h[2]=h[3]=(b& 32)<<3;
					h[4]=h[5]=(b&  8)<<5;
					h[6]=h[7]=(b&  2)<<7;
				}
				break;
			case 4: // HI-RES EXTENDED
				for (BYTE x=0;x<40;++x,h+=8)
				{
					BYTE b=vicii_bitmap[(vicii_cache[x]&63)*8+vicii_eighth];
					h[0]=(b&128)<<1;
					h[1]=(b& 64)<<2;
					h[2]=(b& 32)<<3;
					h[3]=(b& 16)<<4;
					h[4]=(b&  8)<<5;
					h[5]=(b&  4)<<6;
					h[6]=(b&  2)<<7;
					h[7]=(b&  1)<<8;
				}
				break;
			case 5:
			case 6:
			case 7: // UNDEFINED MODES!
				if (vicii_mode&1) // LO-RES?
					for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x,h+=8)
					{
						h[0]=h[1]=(b&128)<<1;
						h[2]=h[3]=(b& 32)<<3;
						h[4]=h[5]=(b&  8)<<5;
						h[6]=h[7]=(b&  2)<<7;
					}
				else // HI-RES!
					for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x,h+=8)
					{
						h[0]=(b&128)<<1;
						h[1]=(b& 64)<<2;
						h[2]=(b& 32)<<3;
						h[3]=(b& 16)<<4;
						h[4]=(b&  8)<<5;
						h[5]=(b&  4)<<6;
						h[6]=(b&  2)<<7;
						h[7]=(b&  1)<<8;
					}
				break;
			default: // BACKGROUND!
				for (BYTE x=0,b=vicii_memory[(vicii_mode&4)?0X39FF:0X3FFF];x<40;++x,h+=8)
				{
					h[0]=(b&128)<<1;
					h[1]=(b& 64)<<2;
					h[2]=(b& 32)<<3;
					h[3]=(b& 16)<<4;
					h[4]=(b&  8)<<5;
					h[5]=(b&  4)<<6;
					h[6]=(b&  2)<<7;
					h[7]=(b&  1)<<8;
				}
		}
		// handle the sprites' collision bits
		{ int k; for (BYTE i=8;i--;) if (k=vicii_sprite_k[i]) vicii_draw_sprite(NULL,i,k); }
	}
	else
		MEMZERO(vicii_sprite_k); // nuke sprites!
}
void vicii_draw_border(void) // render a single line of the border, again relying on extant datas
{
	if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
	{
		VIDEO_UNIT p,*t=video_target-video_pos_x+VIDEO_OFFSET_X;
		if (UNLIKELY(vicii_mode&32)) // VIC-II off or drawing the top/bottom border?
			for (int i=VIDEO_OFFSET_X>>4;p=video_clut[vicii_copy_border[i]],i<((VIDEO_OFFSET_X+VIDEO_PIXELS_X)>>4);++i)
				for (char x=16;x;--x) *t++=p;
		else // render the borders
		{
			VIDEO_UNIT *u=t; int i,j,k; if (vicii_port_22&8)
				k=j=(VIDEO_PIXELS_X-640)/2;
			else
				k=4+(j=(VIDEO_PIXELS_X-640)/2+14); // left border 7 px, right border 9 px
			if (vicii_copy_border_l)
				for (p=video_clut[vicii_copy_border[VIDEO_OFFSET_X>>4]],u=t;j--;) *u++=p;
			if (vicii_copy_border_r)
				for (p=video_clut[vicii_copy_border[((VIDEO_OFFSET_X+VIDEO_PIXELS_X)>>4)-1]],u=t+VIDEO_PIXELS_X-k;k--;) *u++=p;
		}
	}
}

//void video_main(int t) // the VIC-II updates the video on its own!

// CPU: MOS 6510 MICROPROCESSOR ===================================== //

// interrupt queue operations; notice that the 6510 can temporarily decrease these counters
#define M6510_IRQ_SET() do{ if (m6510_i_t<-9) m6510_i_t=-2; }while(0)
#define M6510_NMI_SET() do{ if (m6510_n_t<-9) m6510_n_t=-2; }while(0)

void m6510_tape_main(void) // refill the tape counter
{
	if (((cia_port_13[0]|=16)&CIA_TABLE_0[13]&31)&&cia_port_13[0]<128)
		{ cia_port_13[0]+=128; M6510_IRQ_SET(); }
	while ((tape_t+=tape_feed())<0) ;
}
#define M6510_TAPE_N_DISC(zzz) do{ if (tape&&!tape_disabled) { if ((tape_t-=zzz)<0) m6510_tape_main(); } else if (!disc_disabled) m6502_main(zzz); }while(0)
#define M6510_LOAD_SPRITE(i) do{ WORD z=vicii_attrib[1016+i]*64+vicii_sprite_y[i]; vicii_sprite_k[i]=vicii_memory[z+2]+(vicii_memory[z+1]<<8)+(vicii_memory[z]<<16); }while(0)

int m6510_tick(int q) // handles the essential events that happen on each tick, according to the threshold `q`; returns the resulting timing (1 if no delays happen, >1 otherwise)
{
	for (int t=1;;++t)
	{
		vicii_copy_border[(BYTE)(video_pos_x>>4)]=VICII_TABLE[32];
		video_target+=16,video_pos_x+=16; // just for debugging purposes
		++m6510_i_t,++m6510_n_t; // this must happen before any IRQ/NMI events
		if (UNLIKELY(++vicii_pos_x>=vicii_irq_x)) // update the VIC-II clock
			vicii_pos_x=-VICII_MINUS; // the "logical" ticker wraps before the "physical" one; this makes NTSC and PAL effectively equivalent, as their differences lay after the canvas and before the sprites
		switch (vicii_pos_x)
		{
			case -8: // equivalent to 55 on PAL, and to 56 or 57 on NTSC depending on the horizontal timing (64 or 65 T)
				vicii_cow=0; for (BYTE i=8,j=128;i--;j>>=1)
				{
					vicii_sprite_x^=VICII_TABLE[23]&j; // toggle height step
					if (VICII_TABLE[21]&j) // enabled sprite?
						if ((vicii_pos_y&255)==VICII_TABLE[1+i*2]) // new sprite?
							vicii_sprite_y[i]=((vicii_spritexy&j)&&vicii_pos_y>=256)?vicii_sprite_y[i]:0, // reload!
							vicii_sprite_x=(VICII_TABLE[23]&j)+(vicii_sprite_x&~j); // set height!
					if (vicii_sprite_y[i]<63)
						vicii_cow|=j; // tag sprite for fetching!
				}
				vicii_spritexy=0;
				vicii_takeover=vicii_cow&  1;
				break;
			case -7:
				if (!(VICII_TABLE[22]&8)) vicii_mode|=+8; // borderless 38-char mode
				break;
			case -6:
				if ( (VICII_TABLE[22]&8)) vicii_mode|=+8; // borderless 40-char mode
				vicii_copy_border_r=vicii_mode&8;
				if (vicii_cow&  2) vicii_takeover=1;
				break;
			case -5:
				if (vicii_eighth==7)
					vicii_mode|=+16,vicii_backup=vicii_cursor+vicii_dmadelay,vicii_dmadelay=0;
				if (vicii_badline||!(vicii_mode&16))
					vicii_mode&=~16,vicii_eighth=(vicii_eighth+1)&7;
				break;
			case -4:
				if (vicii_cow&  4) vicii_takeover=1;
				break;
			case -3:
				if (vicii_cow&  1) { M6510_LOAD_SPRITE(0); if (!(vicii_cow&  6)) vicii_takeover=0; }
				break;
			case -2:
				if (vicii_cow&  8) vicii_takeover=1;
				break;
			case -1:
				if (vicii_cow&  2) { M6510_LOAD_SPRITE(1); if (!(vicii_cow& 12)) vicii_takeover=0; }
				break;
			case +0:
				if (vicii_cow& 16) vicii_takeover=1;
				vicii_draw_border();
				if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y) video_drawscanline(); // scanline is complete!
				video_nextscanline(0); // scanline event!
				// update other devices, if required
				{
					int tt=vicii_len_x/multi_t;
					if (!audio_disabled) audio_main(tt); // even the sharpest samples eat more than one scanline per signal
					if ((cia_serials[0]-=cia_xchange[0])<=0&&cia_xchange[0])
						if (cia_xchange[0]=cia_serials[0]=0,CIA_TABLE_0[14]&64)
							if (((cia_port_13[0]|=8)&CIA_TABLE_0[13]&31)&&cia_port_13[0]<128)
								{ cia_port_13[0]+=128; M6510_IRQ_SET(); }
					if ((cia_serials[1]-=cia_xchange[1])<=0&&cia_xchange[1])
						if (cia_xchange[1]=cia_serials[1]=0,CIA_TABLE_1[14]&64)
							if (((cia_port_13[1]|=8)&CIA_TABLE_1[13]&31)&&cia_port_13[1]<128)
								{ cia_port_13[1]+=128; M6510_NMI_SET(); }
				}
				M6510_TAPE_N_DISC(16);
				break;
			case +1:
				if (vicii_cow&  4) { M6510_LOAD_SPRITE(2); if (!(vicii_cow& 24)) vicii_takeover=0; }
				if (++vicii_pos_y<vicii_len_y)
				{
					if (vicii_pos_y==vicii_irq_y) // the RASTER IRQ doesn't happen here on line 0!
						VICII_TABLE[25]|=1;
					if ((VICII_TABLE[25]&VICII_TABLE[26]&7)&&VICII_TABLE[25]<128) // kludge: check both RASTER (+1) and COLLISION (+2+4) here
						{ VICII_TABLE[25]+=128; M6510_IRQ_SET(); }
					if (vicii_pos_y==48)
						vicii_ready=VICII_TABLE[17]&16; // DEN bit: allow badlines (and enable bitmap)
					if (vicii_badline=vicii_pos_y>=48&&vicii_pos_y<248&&vicii_ready&&!((vicii_pos_y-VICII_TABLE[17])&7))
						vicii_mode&=~16; // we're in a badline, thus we must NOT display the background!
				}
				else
					vicii_frame=--vicii_pos_y; // line 311 must last one extra tick, but the CPU must never see "312" here!
				break;
			case +2:
				if (vicii_frame)
				{
					//if (video_pos_y>=VIDEO_LENGTH_Y)
					//{
						if (!video_framecount) video_endscanlines(); // frame is complete!
						video_newscanlines(video_pos_x,0); session_signal|=SESSION_SIGNAL_FRAME+session_signal_frames; // frame event!
					//}
					// handle decimals, seconds, minutes, hours and alarm in a single block;
					// bit 30 keeps the clock from updating (f.e. while setting the time)
					#define cia_alarmclock(xxx,yyy,zzz) \
						do{ if (((cia_port_13[yyy]|=4)&xxx[13]&31)&&cia_port_13[yyy]<128) { cia_port_13[yyy]+=128; zzz; } }while(0)
					#define CIAS_24HOURS(x,y,z) do{ if (!(x&0X40000000)) { if ((++x&15)>=10) \
						if (!(~(x+= 256 - 10 )&0X00000A00)) if (!(~(x+=0X00000600)&0X00006000)) \
						if (!(~(x+=0X0000A000)&0X000A0000)) if (!(~(x+=0X00060000)&0X00600000)) \
						{ if (!(~(x+=0X00A00000)&0X13000000)) x-=0X92000000; \
						else if (!(~(x&0X0A000000))) x+=0X06000000; } \
						if (equalsiiii(y,x)) z; } }while(0)
					static char decimal=0; if (--decimal<=0)
					{
						decimal=vicii_len_y<288?6:5; // i.e. NTSC versus PAL
						if (!cia_port_11[0]) CIAS_24HOURS(cia_hhmmssd[0],&CIA_TABLE_0[8],cia_alarmclock(CIA_TABLE_0,0,M6510_IRQ_SET()));
						if (!cia_port_11[1]) CIAS_24HOURS(cia_hhmmssd[1],&CIA_TABLE_1[8],cia_alarmclock(CIA_TABLE_1,1,M6510_NMI_SET()));
					}
					if (mmu_fly&&!--mmu_fly) mmu_out&=0X37; // unused bits slowly fade away
					// reset VIC-II in-frame counters
					vicii_pos_y=vicii_backup=vicii_frame=0;
					if (!vicii_irq_y) // the RASTER IRQ happens 1 T later on line 0!
						if (((VICII_TABLE[25]|=1)&VICII_TABLE[26]&31)&&VICII_TABLE[25]<128)
							{ VICII_TABLE[25]+=128; M6510_IRQ_SET(); }
				}
				if (vicii_cow& 32) vicii_takeover=1;
				break;
			case +3:
				if (vicii_cow&  8) { M6510_LOAD_SPRITE(3); if (!(vicii_cow& 48)) vicii_takeover=0; }
				break;
			case +4:
				if (vicii_cow& 64) vicii_takeover=1;
				break;
			case +5:
				if (vicii_cow& 16) { M6510_LOAD_SPRITE(4); if (!(vicii_cow& 96)) vicii_takeover=0; }
				break;
			case +6:
				if (vicii_cow&128) vicii_takeover=1;
				break;
			case +7:
				if (vicii_cow& 32) { M6510_LOAD_SPRITE(5); if (!(vicii_cow&192)) vicii_takeover=0; }
				break;
			case +9:
				if (vicii_cow& 64) { M6510_LOAD_SPRITE(6); if (!(vicii_cow&128)) vicii_takeover=0; }
				break;
			case 11:
				if (vicii_cow&128) { M6510_LOAD_SPRITE(7); vicii_takeover=0; }
				break;
			case 12:
				if (vicii_takeover=vicii_badline) vicii_mode&=~16;
				break;
			case 14:
				if (vicii_takeover=vicii_badline) vicii_mode&=~16,vicii_eighth=0;
				vicii_cursor=vicii_backup;//,vicii_dmadelay=0; // *!* redundant!?
				break;
			case 16:
				for (BYTE i=8,j=128;i--;j>>=1) // sprites move to next line
					if (vicii_sprite_y[i]<63)
						if (!(vicii_sprite_x&j))
							vicii_sprite_y[i]=(vicii_sprite_y[i]+3)&63;
				vicii_copy_border_l=vicii_mode&8;
				vicii_mode&=~8; // here the left border ends and the bitmap begins, even if the 38-char mode hides the first char
				if (VICII_TABLE[17]&8)
					{ if (vicii_pos_y==51&&vicii_ready) vicii_mode&=~32; else if (vicii_pos_y==251) vicii_mode|=+32; }
				else
					{ if (vicii_pos_y==55&&vicii_ready) vicii_mode&=~32; else if (vicii_pos_y==247) vicii_mode|=+32; }
				if (vicii_badline)
				{
					//vicii_mode&=~16; // *!* redundant!?
					WORD y=vicii_cursor; for (BYTE x=0;x<40;++x) vicii_cache[x]=vicii_attrib[y++&1023];
					if (vicii_takeover>1) vicii_cache[0]=vicii_cache[1]=vicii_cache[2]=255; // the "light grey" bug, see below
				}
				// no `break`!
			case 48: // evenly distributed
				M6510_TAPE_N_DISC(16);
				break;
			case 18: // the VIC-II 6569/8565 barrier is 18/19
				vicii_horizon=VICII_TABLE[22]&7; // the canvas needs this right now!
				vicii_port_22=VICII_TABLE[22]; // the borders will need this later
				vicii_copy_hits=VICII_TABLE[31]*256,VICII_TABLE[30]; // custom IC
				break;
			case 18+11: // empyrical middle point: 18+10 is the minimum value where "DISCONNECT" works, but 18+13 breaks "BOX CHECK TEST"
				if (cia_oldglue)
					{ if (vicii_mode&32) MEMZERO(vicii_sprite_k); else vicii_draw_canvas(); if (!(vicii_mode&16)) vicii_cursor+=40; }
				break;
			case 18+14: // empyrical middle point: the cylinder of "REWIND-TEMPEST" needs 18+11..18+18, but "BOX CHECK TEST" breaks at 18+17
				if (!cia_oldglue)
					{ if (vicii_mode&32) MEMZERO(vicii_sprite_k); else vicii_draw_canvas(); if (!(vicii_mode&16)) vicii_cursor+=40; }
				// no `break`!
			//case 32: // evenly distributed
				M6510_TAPE_N_DISC(16);
				break;
		}
		// handle pending CIA events before updating the counters
		#define CIA_DO_STATE(xxx,yyy,zzz,aaa) \
			case +0+4+2+0: yyy=+0+0+2+0; break; \
			case +8+0+0+1: yyy=+0+0+0+1; xxx=mgetii(&zzz); break; \
			case +8+0+2+0: yyy=+0+0+2+0; xxx=mgetii(&zzz); break; \
			case +8+4+2+0: yyy=+0+4+2+0; if (xxx==1) goto aaa; xxx=mgetii(&zzz); break; \
			case +0+0+2+1: yyy=+0+0+0+1; case +0+0+2+0:
		// `goto` is a terrible reserved word, but it simplifies the work here :-(
		#define CIA_DO_CNT_A(xxx,yyy,zzz,aaa) \
			if (cia_minor_a[zzz]&&UNLIKELY(!--cia_count_a[zzz])) \
				{ if (cia_state_a[zzz]!=+0+0+0+1) { \
					aaa: if (!(cia_count_a[zzz]=mgetii(&yyy[4]))) cia_count_a[zzz]=65536; \
					if (((cia_port_13[zzz]|=1)&yyy[13]&31)&&cia_port_13[zzz]<128) \
						{ cia_port_13[zzz]+=128; xxx; } \
					cia_state_a[zzz]=(yyy[14]&8)?yyy[14]&=~1,cia_event_a[zzz]&=~1,+8+0+0+1:+8+0+2+0; \
				} cia_carry=cia_major_b[zzz]; }
		#define CIA_DO_CNT_B(xxx,yyy,zzz,aaa) \
			if ((cia_minor_b[zzz]|cia_carry)&&UNLIKELY(!--cia_count_b[zzz])) \
				{ if (cia_state_b[zzz]!=+0+0+0+1) { \
					aaa: if (!(cia_count_b[zzz]=mgetii(&yyy[6]))) cia_count_b[zzz]=65536; \
					if (((cia_port_13[zzz]|=2)&yyy[13]&31)&&cia_port_13[zzz]<128) \
						{ cia_port_13[zzz]+=128; xxx; } \
					cia_state_b[zzz]=(yyy[15]&8)?yyy[15]&=~1,cia_event_b[zzz]&=~1,+8+0+0+1:+8+0+2+0; \
				} }
		// the macros thus cover both halves of the CIA logic: the state machine and the control listener
		#define CIA_DO_EVENT(xxx,yyy,zzz) \
			if (UNLIKELY(xxx>=0)) switch (yyy) { \
				case +0+0+0+1: case +8+0+0+1: \
					if (xxx&1) yyy=xxx&16?+8+4+2+0:+0+4+2+0; \
					else if (xxx&16) yyy=+8+0+0+1; \
					zzz=xxx&~16; xxx=-1; break; \
				case +0+0+2+0: \
					if (xxx&1) { if (xxx&16) yyy=8+4+2+0; } \
					else yyy=xxx&16?+8+0+0+1:+0+0+2+1; \
					zzz=xxx&~16; xxx=-1; break; \
				case +8+0+2+0: case +0+4+2+0: \
					if (xxx&1) { \
						if (xxx&8) --xxx,yyy=+0+0+0+1; \
						else if (xxx&16) yyy=+8+4+2+0; \
					} else yyy=+0+0+0+1; \
					zzz=xxx&~16; xxx=-1; }
		char cia_carry=0; switch (cia_state_a[0])
		{
			CIA_DO_STATE(cia_count_a[0],cia_state_a[0],CIA_TABLE_0[4],cia_goto0_a);
			CIA_DO_CNT_A(M6510_IRQ_SET(),CIA_TABLE_0,0,cia_goto0_a);
		}
		CIA_DO_EVENT(cia_event_a[0],cia_state_a[0],CIA_TABLE_0[14]);
		switch (cia_state_b[0])
		{
			CIA_DO_STATE(cia_count_b[0],cia_state_b[0],CIA_TABLE_0[6],cia_goto0_b);
			CIA_DO_CNT_B(M6510_IRQ_SET(),CIA_TABLE_0,0,cia_goto0_b);
		}
		CIA_DO_EVENT(cia_event_b[0],cia_state_b[0],CIA_TABLE_0[15]);
		cia_carry=0; switch (cia_state_a[1])
		{
			CIA_DO_STATE(cia_count_a[1],cia_state_a[1],CIA_TABLE_1[4],cia_goto1_a);
			CIA_DO_CNT_A(M6510_NMI_SET(),CIA_TABLE_1,1,cia_goto1_a);
		}
		CIA_DO_EVENT(cia_event_a[1],cia_state_a[1],CIA_TABLE_1[14]);
		switch (cia_state_b[1])
		{
			CIA_DO_STATE(cia_count_b[1],cia_state_b[1],CIA_TABLE_1[6],cia_goto1_b);
			CIA_DO_CNT_B(M6510_NMI_SET(),CIA_TABLE_1,1,cia_goto1_b);
		}
		CIA_DO_EVENT(cia_event_b[1],cia_state_b[1],CIA_TABLE_1[15]);
		// freeze the CPU if required, return otherwise
		if (vicii_takeover<=q) return main_t+=t,t;
	}
}

/*void m6510_sync(int t) // handles the hardware events that can gather multiple clock ticks, set here as `t`
{
	static int r=0; //main_t+=t;
	int tt=(r+=t)/multi_t; // calculate base value of `t`
	r-=(t=tt*multi_t); // adjust `t` and keep remainder
	if (t>0)
	{
		if (tt>0)
		{
			if (audio_dirty&&!audio_disabled)
				audio_main(audio_queue+tt),audio_dirty=audio_queue=0;
			else
				audio_queue+=tt;
		}
	}
}*/

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

BYTE sid_detection; // kludge for "BOX CHECK TEST" :-(
#define m6510_sid_extras0() (sid_multichip1=sid_multichip2=0)
BYTE sid_multichip1=0; // kludge for multi SID chips at $D420 and $D440 :-(
BYTE sid_multichip2=0; // kludge for multi SID chips at $DE00 and $DF00 :-(
void m6510_sid_extras1(WORD w,BYTE b) { sid_multichip1=(w==24&&b==128)?4:0; }
void m6510_sid_extras2(WORD w,BYTE b) { sid_multichip2=(w==24&&b==128)?6:0; }

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
						if (!(CIA_TABLE_0[13]&16)) ++tape_polling; // autodetect non-IRQ tape loaders ("TWIN WORLD")
						w=cia_port_13[0]; cia_port_13[0]=0; if (VICII_TABLE[25]<128) m6510_i_t=-1<<20;
						// this satisfies "BOX CHECK TEST" but there are more differences (PC64 CIA IRQ test)
						if (cia_old6526&&cia_state_a[0]==0+0+2+0&&CIA_TABLE_0[4]+CIA_TABLE_0[5]*256-1==cia_count_a[0]) ++cia_count_a[0];
						return w;
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
						w=cia_port_13[1]; cia_port_13[1]=0; m6510_n_t=-1<<20;
						// this satisfies "BOX CHECK TEST" but there are more differences (PC64 CIA NMI test)
						if (cia_old6526&&cia_state_a[1]==0+0+2+0&&CIA_TABLE_1[4]+CIA_TABLE_1[5]*256-1==cia_count_a[1]) ++cia_count_a[1];
						return w;
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
						if (b&16)
						{
							if (vicii_pos_y==48)
								vicii_ready=1; // we can enable the display if we're still on line $30
							else if (vicii_pos_y>=48&&vicii_pos_y<248&&!vicii_ready)
								vicii_mode&=~32; // remove border but DON'T enable display: "SWIRL BY DNP"
						}
						if (vicii_pos_y>=48&&vicii_pos_y<248/*&&vicii_pos_x!=0*/&&vicii_ready&&((b-VICII_TABLE[17])&7))
						{
							// we must satisfy many different and seemingly contradictory cases:
							// - "CREATURES" ($23B1, Y=$032) scrolls the screen;
							// - "REWIND-TEMPEST" ($85D1, Y=$03B) scrolls the screen;
							// - "SMR-PAF" ($0A38, Y=$030) scrolls the screen;
							// - "DAWNFALL" ($B25A, Y=$034,$03C,$044...) reloads the attributes;
							// - "MS PACMAN" ($09E2..., Y=$02F...) reloads the attributes;
							// - "FUNNY RASTERS" ($C15F, Y=$03A,$03B...) freezes the cursor;
							// ...and all we know is that the VIC-II dominant range is X=12..55
							if (vicii_badline=!((b-vicii_pos_y)&7))
								if (vicii_pos_x>=12&&vicii_pos_x<55)
								{
									vicii_takeover=vicii_pos_x==14?2:1; // "MS PACMAN" and other FLI pictures force an attribute reload without the 3-T pause
									if ((vicii_dmadelay=55-1-vicii_pos_x)>40||!(vicii_mode&16)) vicii_dmadelay=0; // "CREATURES" scrolls with delays of 0..40
								}
						}
						// the remainder of the consequences of modifying this register are handled in the VIC-II ticker, not here
					case 18: // $D012: RASTER POSITION
						VICII_TABLE[w]=b;
						if (w==17) vicii_setmode(),vicii_setmaps();
						w=(VICII_TABLE[17]&128)*2+VICII_TABLE[18];
						if (vicii_irq_y!=w)
							if (vicii_pos_y==(vicii_irq_y=w))
								if (((VICII_TABLE[25]|=1)&VICII_TABLE[26]&31)&&VICII_TABLE[25]<128)
									{ VICII_TABLE[25]+=128; M6510_IRQ_SET(); }
						break;
					case 22: // $D016: CONTROL REGISTER 2
						VICII_TABLE[22]=b&31; vicii_setmode();
						break;
					case 23: // $D017: SPRITE Y EXPANSION
						vicii_spritexy|=b; // cfr. the bottom border of "FOR YOUR SPRITES ONLY"
						vicii_sprite_x&=VICII_TABLE[23]=b;
						break;
					case 24: // $D018: MEMORY CONTROL REGISTER
						VICII_TABLE[24]=b&~1; vicii_setmaps();
						break;
					case 25: // $D019: INTERRUPT REQUEST REGISTER
						if ((VICII_TABLE[25]&=(~b&15))&VICII_TABLE[26]&15)
							{ if (VICII_TABLE[25]<128) { VICII_TABLE[25]+=128; M6510_IRQ_SET(); } } // throw pending VIC-II IRQ
						else
							{ VICII_TABLE[25]&=127; if (cia_port_13[0]<128) m6510_i_t=-1<<20; } // acknowledge VIC-II IRQ
						break;
					case 26: // $D01A: INTERRUPT REQUEST MASK
						if (VICII_TABLE[25]&(VICII_TABLE[26]=b&15))
							{ if (VICII_TABLE[25]<128) { VICII_TABLE[25]+=128; M6510_IRQ_SET(); } } // throw pending VIC-II IRQ
						else
							{ VICII_TABLE[25]&=127; if (cia_port_13[0]<128) m6510_i_t=-1<<20; } // acknowledge VIC-II IRQ
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
				if (w==0XD400+18&&b==0X20&&!(~SID_TABLE_0[18]&0X7F))
					sid_detection=sid_nouveau?-1:0; // "BOX CHECK TEST" modifies this port; see above
				SID_TABLE_0[w&=31]=b,sid_reg_update(0,w),m6510_sid_extras0(); if (w<21&&b) tape_song=(tape_disabled||(audio_disabled&1))?0:240;
			}
	else
		if (w<0XDD00)
			if (w<0XDC00) // color configuration, $D800-$DBFF
				VICII_COLOR[w-0XD800]=b&15;
			else // CIA #1 configuration, $DC00-$DC0F
				switch (w&=15)
				{
					case  5: // reload TIMER A
						CIA_TABLE_0[5]=b; if (!(CIA_TABLE_0[14]&1)) // *!* set TIMER LOAD flag?
							if (!(cia_count_a[0]=CIA_TABLE_0[4]+b*256)) cia_count_a[0]=65536;
						//if (((CIA_TABLE_0[14]&65)==65)&&!cia_xchange[0]) cia_xchange[0]=3,cia_serials[0]=cia_count_a[0];
						break;
					case  7: // reload TIMER B
						CIA_TABLE_0[7]=b; if (!(CIA_TABLE_0[15]&1)) // *!* set TIMER LOAD flag?
							if (!(cia_count_b[0]=CIA_TABLE_0[6]+b*256)) cia_count_b[0]=65536;
						break;
					case  8: // decimals
						cia_port_11[0]=0; if (CIA_TABLE_0[15]&128) CIA_TABLE_0[ 8]=b&15;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0XFFFFFF00)+ (b& 15);
						if (equalsiiii(&CIA_TABLE_0[8],cia_hhmmssd[0])) cia_alarmclock(CIA_TABLE_0,0,M6510_IRQ_SET()); // "HAMMERFIST" needs this to run (cfr. $A7D0)
						break;
					case  9: // seconds
						cia_port_11[0]=1; if (CIA_TABLE_0[15]&128) CIA_TABLE_0[ 9]=b&63;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0XFFFF00FF)+((b& 63)<< 8);
						break;
					case 10: // minutes
						cia_port_11[0]=1; if (CIA_TABLE_0[15]&128) CIA_TABLE_0[10]=b&63;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0XFF00FFFF)+((b& 63)<<16);
						break;
					case 11: // hours + AM/PM bit
						cia_port_11[0]=1; if (CIA_TABLE_0[15]&128) CIA_TABLE_0[11]=b&143;
							else cia_hhmmssd[0]=(cia_hhmmssd[0]&0X00FFFFFF)+((b&143)<<24);
						break;
					/*case 12: // serial data transfer
						if (((CIA_TABLE_0[14]&65)==65)&&!cia_xchange[0]) cia_xchange[0]=3,cia_serials[0]=39;
						break;*/
					case 13: // CIA #1 int.mask
						if (b&128) CIA_TABLE_0[13]|=b&31;
							else CIA_TABLE_0[13]&=~(b&31);
						if ((CIA_TABLE_0[13]&cia_port_13[0]&31)&&cia_port_13[0]<128) // throw pending CIA #1 IRQ?
							{ cia_port_13[0]+=128; M6510_IRQ_SET(); }
						break;
					case 14: // CIA #1 TIMER A control
						// SERIAL PORT: temporary placeholder, just to let "Arkanoid" launch as expected...
						if (b&64)
							cia_port_13[0]|=+8;
						else
							cia_port_13[0]&=~8;
						// setting bit 3 shouldn't happen instantly, but where does the delay come from?
						cia_event_a[0]=b;
						cia_port_14(0,b);
						break;
					case 15: // CIA #1 TIMER B control
						cia_event_b[0]=b;
						cia_port_15(0,b);
						break;
					default:
						CIA_TABLE_0[w]=b;
				}
		else
			if (w<0XDE00) // CIA #2 configuration, $DD00-$DD0F
				switch (w&=15)
				{
					case  0:
						if (cia_oldglue) // *!* kludge for glue logic: unlike IC (Integrated Circuit), DL (Discrete Logic) delays collisions on page switch
							if ((CIA_TABLE_1[0]^b)&3) VICII_TABLE[31]=vicii_copy_hits>>8,VICII_TABLE[30]=vicii_copy_hits;
						// no `break`! (btw, it's more probable that IC performs vicii_setmaps() here, while DL does it once per scanline)
					case  2:
						CIA_TABLE_1[w]=b; vicii_setmaps();
						break;
					case  5: // reload TIMER A
						CIA_TABLE_1[5]=b; if (!(CIA_TABLE_1[14]&1)) // *!* set TIMER LOAD flag?
							if (!(cia_count_a[1]=CIA_TABLE_1[4]+b*256)) cia_count_a[1]=65536;
						//if (((CIA_TABLE_1[14]&65)==65)&&!cia_xchange[1]) cia_xchange[1]=3,cia_serials[1]=cia_count_a[1];
						break;
					case  7: // reload TIMER B
						CIA_TABLE_1[7]=b; if (!(CIA_TABLE_1[15]&1)) // *!* set TIMER LOAD flag?
							if (!(cia_count_b[1]=CIA_TABLE_1[6]+b*256)) cia_count_b[1]=65536;
						break;
					case  8: // decimals
						cia_port_11[1]=0; if (CIA_TABLE_1[15]&128) CIA_TABLE_1[ 8]=b&15;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0XFFFFFF00)+ (b& 15);
						if (equalsiiii(&CIA_TABLE_1[8],cia_hhmmssd[1])) cia_alarmclock(CIA_TABLE_1,1,M6510_NMI_SET());
						break;
					case  9: // seconds
						cia_port_11[1]=1; if (CIA_TABLE_1[15]&128) CIA_TABLE_1[ 9]=b&63;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0XFFFF00FF)+((b& 63)<< 8);
						break;
					case 10: // minutes
						cia_port_11[1]=1; if (CIA_TABLE_1[15]&128) CIA_TABLE_1[10]=b&63;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0XFF00FFFF)+((b& 63)<<16);
						break;
					case 11: // hours + AM/PM bit
						cia_port_11[1]=1; if (CIA_TABLE_1[15]&128) CIA_TABLE_1[11]=b&143;
							else cia_hhmmssd[1]=(cia_hhmmssd[1]&0X00FFFFFF)+((b&143)<<24);
						break;
					/*case 12: // serial data transfer
						if (((CIA_TABLE_1[14]&65)==65)&&!cia_xchange[1]) cia_xchange[1]=3,cia_serials[1]=39;
						break;*/
					case 13: // CIA #2 int.mask
						if (b&128) CIA_TABLE_1[13]|=b&31;
							else CIA_TABLE_1[13]&=~(b&31);
						if ((CIA_TABLE_1[13]&cia_port_13[1]&31)&&cia_port_13[1]<128) // throw pending CIA #2 NMI!
							{ cia_port_13[1]+=128; M6510_NMI_SET(); }
						break;
					case 14: // CIA #2 TIMER A control
						/* // RS232 PORT: "Athena" uses it to trigger NMIs, but how does it exactly happen?
						if (b&64) // SERIAL PORT???
							cia_port_13[1]|=8;
						else
							cia_port_13[1]&=~8;
						*/
						cia_event_a[1]=b;
						cia_port_14(1,b);
						break;
					case 15: // CIA #2 TIMER B control
						cia_event_b[1]=b;
						cia_port_15(1,b);
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
#define M65XX_LOCAL BYTE m6510_aux1,m6510_aux2
#define M65XX_PEEKSYNC (0) // (_t_-=m65xx_t, m6510_sync(m65xx_t), m65xx_t-=0)
#define M65XX_POKESYNC (0) // do{ if (m6510_aux1>=0XD4&&m6510_aux1<0XD8) audio_dirty=1; M65XX_PEEKSYNC; }while(0)
#define M65XX_PAGE(x) (m6510_aux2=mmu_bit[m6510_aux1=x])
#define M65XX_PEEK(x) ((m6510_aux2&1)?M65XX_PEEKSYNC,m6510_recv(x):mmu_rom[m6510_aux1][x])
#define M65XX_POKE(x,o) do{ if (m6510_aux2&2) { M65XX_POKESYNC; m6510_send(x,o); } else mem_ram[x]=o; }while(0) // simplify ROM-RAM fall back
#define M65XX_PEEKZERO(x) (x<0X0002?mmu_cfg_get(x):mem_ram[x]) // ZEROPAGE is simpler, we only need to filter the first two bytes
#define M65XX_POKEZERO(x,o) do{ if (x<0X0002) mmu_cfg_set(x,o); else mem_ram[x]=o; }while(0) // ditto
#define M65XX_PULL(x) (mem_ram[256+(x)]) // stack operations are simpler, they're always located on the same RAM area
#define M65XX_PUSH(x,o) (mem_ram[256+(x)]=o) // ditto
// the MOS 6510 "dumb" operations sometimes have an impact on the hardware of the C64
#define M65XX_DUMBPAGE(x) (m6510_aux2=mmu_bit[m6510_aux1=x])
#define M65XX_DUMBPEEK(x) ((m6510_aux2&4)&&(M65XX_PEEKSYNC,m6510_recv(x)))
#define M65XX_DUMBPOKE(x,o) do{ if (m6510_aux2&8) { M65XX_POKESYNC; m6510_send(x,o); } }while(0)
#define M65XX_DUMBPEEKZERO(x) (0)
#define M65XX_DUMBPOKEZERO(x,o) (0)
#define M65XX_DUMBPULL(x) (0)
// When the VIC-II announces that it needs the MOS 6510 to wait, it allows up to three writing operations at the beginning;
// this is equivalent to writing operations being non-blocking because the 6510 never does more than three writes in a row.
#define M65XX_TICK (m65xx_t+=m6510_tick(2)) // we use 2 because `vicii_takeover` can be 0 (idle), 1 (busy normal) and 2 (busy rushed)
#define M65XX_WAIT (m65xx_t+=m6510_tick(0)) // and the comparison is against the highest acceptable value of the takeover status

#define M65XX_START (mgetii(&mem_rom[0X1FFC]))
#define M65XX_RESET m6510_reset
#define M65XX_MAIN m6510_main
#define M65XX_SYNC(x) (x)
#define M65XX_IRQ_ACK 0
#define M65XX_NMI_ACK (m6510_n_t=-1<<20)
#define M65XX_PC m6510_pc
#define M65XX_A m6510_a
#define M65XX_X m6510_x
#define M65XX_Y m6510_y
#define M65XX_S m6510_s
#define M65XX_P m6510_p
#define M65XX_I_T m6510_i_t
#define M65XX_N_T m6510_n_t

BYTE m6510_xef=0XEE; // by default we assume 0XEE, but the user must be able to choose
#define M65XX_XEE 0XEE
#define M65XX_XEF m6510_xef
#define M65XX_RDY (!vicii_takeover) // FELLAS-EXT.PRG checks this; it needs "instable illegal opcode mode" in MICRO64!
#define M65XX_SHW (vicii_pos_x!=1-VICII_MINUS||!vicii_badline||(vicii_cow&1)) // (sort of) kludge: FELLAS-EXT checks this too to detect when M65XX_RDY goes true
#define M65XX_TRAP_0X20 if (M65XX_PC.w==0XF53A&&mmu_mcr==m6510_t64ok) { t64_loadfile(); M65XX_X=mem_ram[0X00AC],M65XX_Y=mem_ram[0X00AD],M65XX_PC.w=0XF8D1; } // load T64 file
#define M65XX_TRAP_0XF0 if (M65XX_PC.w==0XFD87&&mmu_mcr==m6510_power) mem_ram[0XC2]=0X9F; // power-up boost: skip RAM test

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
	sprintf(DEBUG_INFOZ( 1)+4,"[%02X:%02X] %08X",mmu_cfg[0],mmu_cfg[1],debug_info_binary(mmu_out));
	if (!(q&1)) // 1/2
	{
		sprintf(DEBUG_INFOZ( 2),"CIA #1: %02X %04X:%04X",cia_port_13[0],cia_count_a[0]&0XFFFF,cia_count_b[0]&0XFFFF);
		if (cia_port_13[0] &128) debug_hilight2(DEBUG_INFOZ( 2)+4+4);
		sprintf(DEBUG_INFOZ( 3),"%c%c%c",cia_state_a[0]&8?'L':'-',cia_state_a[0]&4?'W':'-',cia_state_a[0]&2?'*':cia_state_a[0]&1?'-':'?'); byte2hexa(DEBUG_INFOZ( 3)+4,&CIA_TABLE_0[0],8);
		sprintf(DEBUG_INFOZ( 4),"%c%c%c",cia_state_b[0]&8?'L':'-',cia_state_b[0]&4?'W':'-',cia_state_b[0]&2?'*':cia_state_b[0]&1?'-':'?'); byte2hexa(DEBUG_INFOZ( 4)+4,&CIA_TABLE_0[8],8);
		sprintf(DEBUG_INFOZ( 5),"CIA #2: %02X %04X:%04X",cia_port_13[1],cia_count_a[1]&0XFFFF,cia_count_b[1]&0XFFFF);
		if (cia_port_13[1] &128) debug_hilight2(DEBUG_INFOZ( 5)+4+4);
		sprintf(DEBUG_INFOZ( 6),"%c%c%c",cia_state_a[1]&8?'L':'-',cia_state_a[1]&4?'W':'-',cia_state_a[1]&2?'*':cia_state_a[1]&1?'-':'?'); byte2hexa(DEBUG_INFOZ( 6)+4,&CIA_TABLE_1[0],8);
		sprintf(DEBUG_INFOZ( 7),"%c%c%c",cia_state_b[1]&8?'L':'-',cia_state_b[1]&4?'W':'-',cia_state_b[1]&2?'*':cia_state_b[1]&1?'-':'?'); byte2hexa(DEBUG_INFOZ( 7)+4,&CIA_TABLE_1[8],8);
		sprintf(DEBUG_INFOZ( 8),"TOD %07X//%07X",(cia_hhmmssd[0]>>4)+(cia_hhmmssd[0]&15),(cia_hhmmssd[1]>>4)+(cia_hhmmssd[1]&15)); // CIA #1+#2 time-of-day
		sprintf(DEBUG_INFOZ( 9),"VIC-II: %02X %02X%c%03X %02X",VICII_TABLE[25],(video_pos_x>>4)&255,vicii_badline?'*':'-',vicii_pos_y&4095,vicii_mode);
		if (VICII_TABLE[25]&128) debug_hilight2(DEBUG_INFOZ( 9)+4+4);
		for (q=0;q<4;++q)
			byte2hexa(DEBUG_INFOZ(10+q)+4,&VICII_TABLE[q*8],8);
		sprintf(DEBUG_INFOZ( 9)+21,"ATTR:%04X",vicii_attrib-mem_ram);
		sprintf(DEBUG_INFOZ(10)+21,"BASE:%04X",vicii_backup&1023);
		sprintf(DEBUG_INFOZ(11)+21,"Y:%2d:%04X",vicii_eighth&63,vicii_cursor&1023);
		sprintf(DEBUG_INFOZ(12)+21,"IEC1:%02X %c",cia_serials[0]&255,cia_xchange[0]?'*':'-');
		sprintf(DEBUG_INFOZ(13)+21,"IEC2:%02X %c",cia_serials[1]&255,cia_xchange[1]?'*':'-');
		byte2hexa(DEBUG_INFOZ(14)+4,vicii_sprite_y,8); // the 6-bit sprite offset counters
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
VIDEO_UNIT grafx_show2b[4]={0,0X0080FF,0XFF8000,0XFFFFFF};
WORD grafx_show(VIDEO_UNIT *t,int g,int n,BYTE m,WORD w,int o)
{
	while (n-->0)
	{
		BYTE b=mem_ram[w++];
		if (o&1)
		{
			VIDEO_UNIT p;
			*t++=p=grafx_show2b[ b>>6   ]; *t++=p;
			*t++=p=grafx_show2b[(b>>4)&3]; *t++=p;
			*t++=p=grafx_show2b[(b>>2)&3]; *t++=p;
			*t++=p=grafx_show2b[ b    &3]; *t++=p;
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
						for (int zz=0;zz<8;zz+=2)
							tt[xx*8+zz]=tt[xx*8+zz+1]=grafx_show2b[(vicii_memory[ii]>>(6-zz))&3];
					}
				else
					for (int xx=0;xx<3;++xx,++ii)
						for (int zz=0;zz<8;++zz)
							tt[xx*8+zz]=(vicii_memory[ii]<<zz)&128?0XFFFFFF:0;//video_clut[23+i]:video_clut[17];
		}
}
#include "cpcec-m6.h"
#undef DEBUG_HERE

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
			if (m6510_pc.w>=0XE000&&autorun_type("\022\025\016",3)) break; // we must NOT retry if we're already running code!!
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
	m6510_i_t=m6510_n_t=m6502_i_t=-1<<20; debug_clear(),debug_reset();
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
// - a single instance of byte X is stored as it is, "X";
// - N=2..256 instances of byte X become the series "X,X,N-2";
// - the end marker is "~X,~X,255" where X is the last source byte.
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
	unsigned char *u=t,j; short l; while (i>=3)
	{
		j=*s; if (j!=*++s) --i,l=1; else if ((l=s[1])<255) i-=3,s+=2,l+=2; else return t-u;
		if ((o-=l)<0) return -1; do *t++=j; while (--l); // ERROR: target is too short!
	}
	return -2; // ERROR: source is too short!
}

// uncompressed snapshots store 0 as the compressed size (Intel WORD at offset 16)
int snap_save(char *s) // saves snapshot file `s`; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"wb"); if (!f) return 1;
	BYTE header[512]; MEMZERO(header);
	strcpy(header,snap_magic8);
	int i,q=snap_extended?bin2rle(session_scratch,(1<<16)-1,mem_ram,1<<16):0;
	if (q>0) mputii(&header[0x10],q); else q=0;
	// CPU #1
	header[0x20]=m6510_pc.b.l;
	header[0x21]=m6510_pc.b.h;
	header[0x22]=m6510_p;
	header[0x23]=m6510_a;
	header[0x24]=m6510_x;
	header[0x25]=m6510_y;
	header[0x26]=m6510_s;
	header[0x27]=(m6510_n_t<-9?0:128)+(VICII_TABLE[25]<128?0:1)+(cia_port_13[0]<128?0:2);
	header[0x30]=mmu_cfg[0];
	header[0x31]=mmu_cfg[1];
	//header[0x32]=mmu_out; // this value is normally identical to mmu_cfg[1], but not always, cfr. CPUPORT.PRG
	// CIA #1
	memcpy(&header[0x40],CIA_TABLE_0,0X10);
	mputii(&header[0x54],cia_count_a[0]);
	mputii(&header[0x56],cia_count_b[0]);
	i=cia_hhmmssd[0]|(cia_port_11[0]?64<<24:0); mputiiii(&header[0x58],i);
	header[0x5D]=cia_port_13[0];
	// CIA #2
	memcpy(&header[0x60],CIA_TABLE_1,0X10);
	mputii(&header[0x74],cia_count_a[1]);
	mputii(&header[0x76],cia_count_b[1]);
	i=cia_hhmmssd[1]|(cia_port_11[1]?64<<24:0); mputiiii(&header[0x78],i);
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
	m6510_n_t=(header[0x27]&128)?-2:-1<<20;
	m6510_i_t=(header[0x27]&  3)?-2:-1<<20;
	mmu_cfg[0]=header[0x30];
	mmu_cfg[1]=header[0x31];
	//if (!(mmu_out=header[0x32])) mmu_out=0XFF; // *!* temporary bugfix
	// CIA #1
	memcpy(CIA_TABLE_0,&header[0x40],0X10);
	if (!(cia_count_a[0]=mgetii(&header[0x54]))) cia_count_a[0]=65536;
	if (!(cia_count_b[0]=mgetii(&header[0x56]))) cia_count_b[0]=65536;
	cia_event_a[0]=cia_event_b[0]=-1;
	cia_port_14(0,CIA_TABLE_0[14]); cia_state_a[0]=(CIA_TABLE_0[14]&1)?+0+0+2+0:+0+0+0+1;
	cia_port_15(0,CIA_TABLE_0[15]); cia_state_b[0]=(CIA_TABLE_0[15]&1)?+0+0+2+0:+0+0+0+1;
	cia_port_11[0]=CIA_TABLE_0[11]&64; CIA_TABLE_0[11]&=143; // bit 30 is the "locked" flag
	cia_hhmmssd[0]=mgetiiii(&header[0x58]); cia_port_13[0]=header[0x5D];
	mmu_update(); // CPU #1 ports $00 and $01 handle the tape together with CIA #1
	// CIA #2
	memcpy(CIA_TABLE_1,&header[0x60],0X10);
	if (!(cia_count_a[1]=mgetii(&header[0x74]))) cia_count_a[1]=65536;
	if (!(cia_count_b[1]=mgetii(&header[0x76]))) cia_count_b[1]=65536;
	cia_event_a[1]=cia_event_b[1]=-1;
	cia_port_14(1,CIA_TABLE_1[14]); cia_state_a[1]=(CIA_TABLE_1[14]&1)?+0+0+2+0:+0+0+0+1;
	cia_port_15(1,CIA_TABLE_1[15]); cia_state_b[1]=(CIA_TABLE_1[15]&1)?+0+0+2+0:+0+0+0+1;
	cia_port_11[1]=CIA_TABLE_1[11]&64; CIA_TABLE_1[11]&=143; // bit 30 is the "locked" flag
	cia_hhmmssd[1]=mgetiiii(&header[0x78]); cia_port_13[1]=header[0x7D];
	debug_reset();
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
	video_clut_update();
	// COLOR TABLE
	fread1(header,512,f);
	for (int i=0;i<512;++i) VICII_COLOR[i*2]=header[i]>>4,VICII_COLOR[1+i*2]=header[i]&15;
	// 64K RAM
	q?rle2bin(mem_ram,1<<16,session_scratch,fread1(session_scratch,q,f)):fread1(mem_ram,1<<16,f);
	// ... future blocks will go here ...
	//if ((VICII_TABLE[25]&VICII_TABLE[26]&31)||(cia_port_13[0]&CIA_TABLE_0[13]&31)) m6510_i_t=-2; // pending IRQ workaround?
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
	//"0x4800 Play tape\tCtrl+Shift+F8\n"
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
	"0x8501 Reset ANE bit0\n"
	"0x8502 CIA 6526A revision\n"
	"0x8503 C64C IC glue logic\n"
	"0x8504 SID 8580 revision\n"
	"0x8505 Internal SID filter\n"
	"0x8506 Disable VIC-II sprites\n"
	"0x8507 Disable sprite collisions\n"
	"=\n"
	//"0x0600 Raise 6510 speed\tCtrl+F6\n"
	//"0x4600 Lower 6510 speed\tCtrl+Shift+F6\n"
	"0x0601 1x CPU clock\n"
	"0x0602 2x CPU clock\n"
	"0x0603 3x CPU clock\n"
	"0x0604 4x CPU clock\n"
	"0x0605 Power-up boost\n"
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
	//"0x852F Printer output..\n"
	"0x851F Strict snapshots\n"
	/*
	"0x8510 Disc controller\n"
	"0x8590 Strict disc writes\n"
	"0x8591 Read-only disc by default\n"
	*/
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
	session_menucheck(0x0605,m6510_power>=0);
	session_menucheck(0x8502,!cia_old6526);
	session_menucheck(0x8503,!cia_oldglue);
	session_menucheck(0x8501,!(m6510_xef&1));
	session_menucheck(0x8507,vicii_noimpacts);
	session_menucheck(0x8506,vicii_nosprites);
	session_menucheck(0x8505,sid_filters);
	session_menucheck(0x8504,sid_nouveau);
	session_menuradio(0x8508+sid_extras,0X8508,0X850C);
	//session_menucheck(0x850D,sid_muted[0]);
	//session_menucheck(0x850E,sid_muted[1]);
	//session_menucheck(0x850F,sid_muted[2]);
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
	vicii_len_y=312; // PAL/NTSC vertical configuration
	vicii_irq_x=(vicii_len_x=63*multi_t)-VICII_MINUS; // PAL/NTSC horizontal
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
		case 0x8501: // MOS 6510 $EE/$EF SWITCH
			m6510_xef^=1;
			break;
		case 0x8502: // CIA 6526/652A SWITCH
			cia_old6526=!cia_old6526;
			break;
		case 0x8503: // CIA DISCRETE/CUSTOM IC SWITCH
			cia_oldglue=!cia_oldglue;
			break;
		case 0x8507: // DISABLE SPRITE COLLISIONS
			vicii_noimpacts=!vicii_noimpacts;
			VICII_TABLE[31]=VICII_TABLE[30]=0;
			break;
		case 0x8506: // DISABLE VIC-II SPRITES
			vicii_nosprites=!vicii_nosprites;
			VICII_TABLE[31]=VICII_TABLE[30]=0;
			break;
		case 0x8505: // DISABLE SID FILTER
			sid_filters=!sid_filters;
			sid_reg_update(0,23);
			sid_reg_update(1,23);
			sid_reg_update(2,23);
			break;
		case 0x8504: // SID 8580/6581
			sid_nouveau=!sid_nouveau;
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
		case 0x0605: // POWER-UP BOOST
			m6510_power=m6510_power>=0?-1:0X07;
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
		case 0x0800: // ^F8: REMOVE OR PLAY TAPE
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
			m6510_n_t=-2;
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
	else if (!strcasecmp(session_parmtr,"type")) disc_disabled=!(*s&1),cia_old6526=!!(*s&2),cia_oldglue=!!(*s&4);
	else if (!strcasecmp(session_parmtr,"fdcw")) disc_filemode=*s&3;
	else if (!strcasecmp(session_parmtr,"bank")) reu_depth=*s&7,reu_depth=reu_depth>length(reu_kbyte)?0:reu_depth;
	else if (!strcasecmp(session_parmtr,"joy1")) key2joy_flag=(*s&1);
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
		"type %d\nfdcw %d\nbank %d\njoy1 %d\nsidx %d\nmisc %d\n"
		"file %s\nsnap %s\ntape %s\ndisc %s\nprog %s\ncard %s\n"
		"vjoy %s\npalette %d\ncasette %d\ndebug %d\n",
		(~disc_disabled&1)+(cia_old6526?2:0)+(cia_oldglue?4:0),disc_filemode,reu_depth,!!key2joy_flag,(sid_nouveau?1:0)+sid_extras*2,(snap_extended?1:0),
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
			MEMLOAD(kbd_bit0,autorun_kbd); // the active keys are fully virtual
		else
		{
			for (int i=0;i<length(kbd_bit0);++i) kbd_bit0[i]=kbd_bit[i]|joy_bit[i]; // mix keyboard + joystick bits
			if (kbd_bit[ 8]) kbd_bit0[0]|=kbd_bit[ 8],kbd_bit0[1]|=128; // LEFT SHIFT + right side KEY combos (1/2)
			if (kbd_bit[15]) kbd_bit0[7]|=kbd_bit[15],kbd_bit0[1]|=128; // LEFT SHIFT + left  side KEY combos (2/2)
			if (!(~kbd_bit0[9]&3)) kbd_bit0[9]-=3; if (!(~kbd_bit0[9]&12)) kbd_bit0[9]-=12; // catch illegal UP+DOWN and LEFT+RIGHT joystick bits
		}
		while (!session_signal)
		{
			m6510_main( // clump MOS 6510 instructions together to gain speed...
			((VIDEO_LENGTH_X+15-video_pos_x)>>4)
			*multi_t); // ...without missing any deadlines!
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
				int q=tape_disabled?0:128;
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
					if (autorun_mode)
						onscreen_bool(-5,-7,3,1,autorun_t>1),
						onscreen_bool(-5,-4,3,1,autorun_t>1),
						onscreen_bool(-6,-6,1,5,autorun_t>1),
						onscreen_bool(-2,-6,1,5,autorun_t>1);
					else
					{
						onscreen_bool(-5,-6,3,1,kbd_bit_tst(kbd_joy[0]));
						onscreen_bool(-5,-2,3,1,kbd_bit_tst(kbd_joy[1]));
						onscreen_bool(-6,-5,1,3,kbd_bit_tst(kbd_joy[2]));
						onscreen_bool(-2,-5,1,3,kbd_bit_tst(kbd_joy[3]));
						onscreen_bool(-4,-4,1,1,kbd_bit_tst(kbd_joy[4]));
					}
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
			video_clut[31]=video_clut[video_pos_z&7]; // DEBUG COLOUR!
			if (m6510_i_t>0) m6510_i_t=0; else if (m6510_i_t<-9) m6510_i_t=-1<<20; // catch IRQ overflows; notice that the 6510 can temporarily turn -2 into -3
			if (m6510_n_t>0) m6510_n_t=0; else if (m6510_n_t<-9) m6510_n_t=-1<<20; // catch NMI overflows
			if (m6510_pc.w>=0XE4E2&&m6510_pc.w<0XE4EB&&mmu_mcr==7&&tape_fastload) m6510_pc.w=0XE4EB; // tape "FOUND FILENAME" boost
			if (!(mmu_cfg[1]&32)&&(CIA_TABLE_0[13]&16)) tape_enabled|=+1; // IRQ tape:
			else tape_enabled&=~1; // "SIDEWIZE" proves that mmu_cfg[1]&32 matters, not mmu_out&32
			if (tape_polling>500) tape_enabled|=+2; // non-IRQ tape: is the machine polling the port fast enough?
			else if (tape_polling<50) tape_enabled&=~2; // not fast enough, apparently not a tape loader
			if (!tape_fastload) tape_song=0/*,tape_loud=1*/;
			else if (tape_song) /*tape_loud=0,*/--tape_song;
			//else tape_loud=1; // expect song to play for several frames
			tape_polling=0;
			if (tape_signal)
			{
				if (tape_signal<2) tape_enabled=0; // stop tape if required
				tape_signal=0,session_dirty=1; // update config
			}
			/*tape_skipping=audio_queue=0;*/ // reset tape and audio flags
			if (tape&&tape_filetell<tape_filesize&&tape_skipload&&!session_filmfile&&!tape_disabled&&!tape_song)
				video_framelimit|=(MAIN_FRAMESKIP_MASK+1),session_fast|=2,video_interlaced|=2,audio_disabled|=2; // abuse binary logic to reduce activity
			else
				video_framelimit&=~(MAIN_FRAMESKIP_MASK+1),session_fast&=~2,video_interlaced&=~2,audio_disabled&=~2; // ditto, to restore normal activity
			session_update();
			//if (!audio_disabled) audio_main(1+(video_pos_x>>4)); // preload audio buffer
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
