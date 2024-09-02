 // #:  #:   ###:   #:  #:  #####:    ##:  ------------------------- //
//  ##:##:  #:  #:  #:  #:  #:       #: #:  MSXEC, small standard MSX //
//  #:#:#:  #:       #:#:   #:      #:      emulator, once again made //
//  #:#:#:   ###:     #:    ####:   #:      on top of CPCEC's modules //
//  #:  #:      #:   #:#:   #:      #:      by Cesar Nicolas-Gonzalez //
//  #:  #:  #:  #:  #:  #:  #:       #: #:  since 2023-01-27 till now //
 // #:  #:   ###:   #:  #:  #####:    ##:  ------------------------- //

#define MY_CAPTION "MSXEC"
#define my_caption "msxec"
#define MY_LICENSE "Copyright (C) 2019-2024 Cesar Nicolas-Gonzalez"

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

// As already stated in the description of ZXSEC and CSFEC, the goal
// of this emulator is to provide a reasonable performance of a 1988
// MSX2+ (compatible with 1983 MSX and 1985 MSX2) while still reusing
// bits from the previously written CPCEC and its supporting modules.

// This file focuses on the MSX-specific features: configuration, MMU
// logic, VDP video, Z80 timings and quirks, snapshots, options...

#include <stdio.h> // printf()...
#include <stdlib.h> // strtol()...
#include <string.h> // strcpy()...

// General MSX metrics and constants defined as general types ------- //

//#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (44<<4)
#define VIDEO_LENGTH_Y (39<<4)
#ifndef VIDEO_BORDERLESS
#define VIDEO_OFFSET_X (11<<2)
#define VIDEO_OFFSET_Y ( 5<<4)
#define VIDEO_PIXELS_X (40<<4)
#define VIDEO_PIXELS_Y (30<<4)
#else
#define VIDEO_OFFSET_X (27<<2) // the default MSX2 512x424 screen without the border
#define VIDEO_OFFSET_Y (27<<2)
#define VIDEO_PIXELS_X (32<<4)
#define VIDEO_PIXELS_Y (53<<3)
//#define VIDEO_OFFSET_Y (32<<2) // original MSX1 512x384
//#define VIDEO_PIXELS_Y (24<<4)
#endif
#define VIDEO_RGB2Y(r,g,b) ((r)*3+(g)*6+(b)) // generic RGB-to-Y expression

#if defined(SDL2)||!defined(_WIN32)
unsigned short session_icon32xx16[32*32] = {
	#include "msxec-a4.h"
	};
#endif

// The MSX1 keyboard had 88 keys; MSX2 models optionally added a numeric pad. Mind the OCTAL values!
// +--------------------------------------------+ +---------------------------+ +-------------------+
// | F1/F6  | F2/F7  | F3/F8  | F4/F9  | F5/F10 | | HOME | INS. | DEL. | STOP | |NUM7|NUM8|NUM9|NUM-|
// | 065    | 066    | 067    | 070    | 071    | | 101  | 102  | 103  | 074  | |124 |125 |126 |127 |
// +--------------------------------------------------------------------------+ +-------------------+
// | ESC| 1! | 2@ | 3# | 4$ | 5% | 6^ | 7& | 8* | 9( | 0) | -_ | =+ | \| | <- | |NUM4|NUM5|NUM6|NUM/|
// | 072|001 |002 |003 |004 |005 |006 |007 |010 |011 |000 |012 |013 |014 |075 | |117 |120 |121 |112 |
// +--------------------------------------------------------------------------+ +-------------------+
// | TAB  | Q  | W  | E  | R  | T  | Y  | U  | I  | O  | P  | [{ | ]} | INTRO | |NUM1|NUM2|NUM3|NUM*|
// | 073  |046 |054 |032 |047 |051 |056 |052 |036 |044 |045 |015 |016 |  077  | |114 |115 |116 |110 |
// +-------------------------------------------------------------------+      | +-------------------+
// | CTRL  | A  | S  | D  | F  | G  | H  | J  | K  | L  | :; | '" | `~ |      | |NUM0|NUM,|NUM.|NUM+|
// | 061   |026 |050 |031 |033 |034 |035 |037 |040 |041 |021 |020 |017 |      | |113 |122 |123 |111 |
// +--------------------------------------------------------------------------| +-------------------+
// | SHIFT   | Z  | X  | C  | V  | B  | N  | M  | ,< | .> | /? | SHIFT | DEAD | |C.LT|   C.UP  |C.RT|
// | 060     |057 |055 |030 |053 |027 |043 |042 |022 |023 |024 | 060   | 025  | |104 |   105   |107 |
// +--------------------------------------------------------------------------+ |    +---------+    |
//        | CAPS  | CODE  | SPACE                      | GRAPH |SELECT |        |    |   C.DN  |    |
//        | 063   | 064   | 100                        | 062   | 076   |        |    |   106   |    |
//        +------------------------------------------------------------+        +-------------------+
// Notice that the layout differs across models, f.e. "DEAD" becomes '`^" in non-US machines;
// similarly, the top row can be STOP, SELECT, F1-F5, HOME, INS and DEL, f.e. Sharp Hotbit.

#define KBD_JOY_UNIQUE 6 // four sides + two fires
unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1-fire4
	{ 0160,0161,0162,0163,0164,0165,0164,0165 }; // side bits are hard-wired, but the fire bits can be chosen
#define DEBUG_LONGEST 4 // Z80 opcodes can be up to 4 bytes long
//#define MAUS_EMULATION // emulation can examine the mouse
//#define MAUS_LIGHTGUNS // lightguns are emulated with the mouse
//#define VIDEO_LO_X_RES // MSX2 modes T2, G5 and G6 are hi-res (2, 4 and 16 colours/pixel)
#define SHA1_CALCULATOR // used by the cartridge loader
#define RUNLENGTH_ENCODING // snap_load/snap_save (old!)
#define LEMPELZIV_ENCODING // snap_load/snap_save
#define PNG_OUTPUT_MODE 0 // PNG_OUTPUT_MODE implies DEFLATE_RFC1950 and forbids QOI
#define POWER_BOOST1 1 // power_boost default value (enabled)
#define POWER_BOOST0 0
#include "cpcec-rt.h" // emulation framework!

const unsigned char kbd_map_xlt[]=
{
	// control keys (range 0X81..0XBF)
	KBCODE_F1	,0x81,	KBCODE_F2	,0x82,	KBCODE_F3	,0x83,	KBCODE_F4	,0x84,
	KBCODE_F5	,0x85,	KBCODE_F6	,0x86,	KBCODE_F7	,0x87,	KBCODE_F8	,0x88,
	KBCODE_F9	,0x89,	KBCODE_HOLD	,0x8F,	KBCODE_F11	,0x8B,	KBCODE_F12	,0x8C,
	KBCODE_X_ADD	,0x91,	KBCODE_X_SUB	,0x92,	KBCODE_X_MUL	,0x93,	KBCODE_X_DIV	,0x94,
	// actual keys; again, notice the octal coding
	KBCODE_ESCAPE	,0072,	KBCODE_TAB	,0073,	KBCODE_CAPSLOCK	,0063,	KBCODE_L_SHIFT	,0060,
	KBCODE_1	,0001,	KBCODE_Q	,0046,	KBCODE_A	,0026,	KBCODE_Z	,0057,
	KBCODE_2	,0002,	KBCODE_W	,0054,	KBCODE_S	,0050,	KBCODE_X	,0055,
	KBCODE_3	,0003,	KBCODE_E	,0032,	KBCODE_D	,0031,	KBCODE_C	,0030,
	KBCODE_4	,0004,	KBCODE_R	,0047,	KBCODE_F	,0033,	KBCODE_V	,0053,
	KBCODE_5	,0005,	KBCODE_T	,0051,	KBCODE_G	,0034,	KBCODE_B	,0027,
	KBCODE_6	,0006,	KBCODE_Y	,0056,	KBCODE_H	,0035,	KBCODE_N	,0043,
	KBCODE_7	,0007,	KBCODE_U	,0052,	KBCODE_J	,0037,	KBCODE_M	,0042,
	KBCODE_8	,0010,	KBCODE_I	,0036,	KBCODE_K	,0040,	KBCODE_CHR4_1	,0022,
	KBCODE_9	,0011,	KBCODE_O	,0044,	KBCODE_L	,0041,	KBCODE_CHR4_2	,0023,
	KBCODE_0	,0000,	KBCODE_P	,0045,	KBCODE_CHR3_1	,0017,	KBCODE_CHR4_3	,0024, // "\|" is the third key after "L"
	KBCODE_CHR1_1	,0012,	KBCODE_CHR2_1	,0015,	KBCODE_CHR3_2	,0020,	KBCODE_CHR4_4	,0025, // "DEAD" is the key before "Z"; it's missing in 104-key layouts!
	KBCODE_CHR1_2	,0013,	KBCODE_CHR2_2	,0016,	KBCODE_CHR3_3	,0014,	KBCODE_CHR4_5	,0021, // "`~" is the key before "1"
	KBCODE_BKSPACE	,0075,	KBCODE_ENTER	,0077,	KBCODE_SPACE	,0100,	KBCODE_L_CTRL	,0061,
	KBCODE_R_SHIFT	,0060,	KBCODE_R_CTRL	,0061,	// mirrors of KBCODE_L_SHIFT and KBCODE_L_CTRL; OPENMSX uses KBCODE_R_CTRL for "DEAD"
	KBCODE_UP	,0105,	KBCODE_DOWN	,0106,	KBCODE_LEFT	,0104,	KBCODE_RIGHT	,0107,
	// the MSX function keys are relocated to the number pad, following "INSERT", "DELETE" and "HOME"
	KBCODE_INSERT	,0102,	KBCODE_HOME	,0101,	KBCODE_PRIOR	,0064, // "CODE" is PAGE UP
	KBCODE_DELETE	,0103,	KBCODE_END	,0074,	KBCODE_NEXT	,0062, // "STOP" is END, "GRAPH" is PAGE DOWN
	KBCODE_X_1	,0065,	KBCODE_X_2	,0066,	KBCODE_X_3	,0067, // "F1-F5" are NUM.1-NUM.5
	KBCODE_X_4	,0070,	KBCODE_X_5	,0071,	KBCODE_X_6	,0076, // "SELECT" is NUM.6; NUM.0 might be better
};

VIDEO_UNIT video_table[16+24]= // colour table, 0xRRGGBB style, according to https://en.wikipedia.org/wiki/TMS9918
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // VDP palette, to be filled later
	#if 1 // MSX2 V9938 additive GREEN, RED and BLUE channels (linear RGB)
	0X000000,0X002400,0X004900,0X006D00,0X009200,0X00B600,0X00DB00,0X00FF00, // G
	0X000000,0X240000,0X490000,0X6D0000,0X920000,0XB60000,0XDB0000,0XFF0000, // R
	0X000000,0X000024,0X000049,0X00006D,0X000092,0X0000B6,0X0000DB,0X0000FF, // B
	#else // MSX2 V9938 additive GREEN, RED and BLUE channels (gamma: 1.6)
	0X000000,0X004B00,0X007400,0X009600,0X00B400,0X00CF00,0X00E800,0X00FF00, // G
	0X000000,0X4B0000,0X740000,0X960000,0XB40000,0XCF0000,0XE80000,0XFF0000, // R
	0X000000,0X00004B,0X000074,0X000096,0X0000B4,0X0000CF,0X0000E8,0X0000FF, // B
	#endif
};
VIDEO_UNIT video_xlat[16]; // static colours only (dynamic V9938 colours go elsewhere)

char palette_path[STRMAX]="";
int video_table_load(char *s) // shared with CSFEC, based on VICE's palette files, but ink #0 is unused
{
	FILE *f=puff_fopen(s,"r"); if (!f) return -1;
	unsigned char t[STRMAX],n=1; VIDEO_UNIT p[16];
	while (fgets(t,STRMAX,f)&&n<=16) if (*t>'#') // skip "# comment" and others
		{ unsigned int r,g,b; if (sscanf(UTF8_BOM(t),"%X%X%X",&r,&g,&b)!=3) n=16; else if (n<16) p[n]=((r&255)<<16)+((g&255)<<8)+(b&255); ++n; }
	puff_fclose(f); if (n!=16) return -1;
	STRCOPY(palette_path,s); for (n=1;n<16;++n) video_table[n]=p[n]; return 0;
}
void video_table_reset(void)
{
	VIDEO_UNIT const p[]={
		// https://en.wikipedia.org/wiki/TMS9918 (NTSC rather than PAL?)
		0X000000,0X000000,0X0AAD1E,0X34C84C,0X2B2DE3,0X514BFB,0XBD2925,0X1EE2EF,
		0XFB2C2B,0XFF5F4C,0XBDA22B,0XD7B454,0X0A8C18,0XAF329A,0XB2B2B2,0XFFFFFF,
		// https://github.com/mamedev/mame/blob/master/src/devices/video/tms9928a.cpp (NTSC only) // = above but brighter
		//0X000000,0X000000,0X21C842,0X5EDC78,0X5455ED,0X7D76FC,0XD4524D,0X42EBF5,
		//0XFC5554,0XFF7978,0XD4C154,0XE6CE80,0X21B03B,0XC95BBA,0XCCCCCC,0XFFFFFF,
		// https://www.msx.org/wiki/VDP_Color_Palette_Registers (MSX2 default palette)
		//0X000000,0X000000,0X24DB24,0X6DFF6D,0X2424FF,0X496DFF,0XB62424,0X49DBFF,
		//0XFF2424,0XFF6D6D,0XDBDB24,0XDBDB92,0X249224,0XDB49B6,0XB6B6B6,0XFFFFFF,
	};
	for (int n=0;n<16;++n) video_table[n]=p[n];
}

// GLOBAL DEFINITIONS =============================================== //

#define TICKS_PER_LINE 228 // TRUE IN PAL; ALSO TRUE IN NTSC?
int LINES_PER_FRAME;// 313 // 50 HZ (PAL) 262 // 60 HZ (NTSC)
int TICKS_PER_FRAME;// (TICKS_PER_LINE*LINES_PER_FRAME)
int TICKS_PER_SECOND;// (TICKS_PER_FRAME*VIDEO_PLAYBACK)
// Everything in the MSX hardware is tuned to a 3.58 MHz clock,
// using simple binary divisors to adjust the devices' timings.
int multi_t=0,multi_u=0; // overclocking shift+bitmask

// HARDWARE DEFINITIONS ============================================= //

#define ram_maxcfg 6 // 0 = 64K w/o mapper, 1 = 64K, 2 = 128K, 3 = 256K, 4 = 512K, 5 = 1024K, 6 = 2048K
#define bad_ram (&mem_ram[32<<14]) // dummy bank: write-only area for void writes
#define bad_rom (&mem_rom[8<<14]) // dummy bank: read-only area filled with 0XFF
BYTE mem_ram[129<<14],mem_rom[9<<14]; // memory: 129*16k RAM (=2048K+JUNK) and 8*16k ROM (BIOS+BAS.+LOGO+XYZ1+EXT.+KAN1+KAN2+XYZ2+JUNK)
BYTE *mmu_ram[16],*mmu_rom[16]; // memory is divided in 16x 4k banks
#define PEEK(x) mmu_rom[(x)>>12][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>12][x] // WARNING, x cannot be `x=EXPR`!
BYTE mmu_bit[16]; // special behaviors in banks: +1 PEEK, +2 POKE
#define i18n_ntsc (mem_rom[0X2B]<128) // not exactly the right way to calculate it, but it works...
#define i18n_kana (mem_rom[0X2B]<32) // likewise, !(mem_rom[0X2B]&32) would be more accurate...

BYTE type_id=2; // the $002D BIOS byte: 0 = 1983 MSX/"MSX1", 1 = 1985 MSX2, 2 = 1988 MSX2+/"MSX2P", 3 = 1990 MSX TURBO R/"MSXTR"...?
BYTE disc_disabled=0; // disables the disc drive altogether as well as its related logic; +1 = manual, +2 = automatic
BYTE disc_filemode=1; // +1 = read-only by default instead of read-write; +2 = relaxed disc write errors instead of strict
VIDEO_UNIT video_clut[16+16+256]; // palettes: 16-colour (MSX1 static, MSX2 dynamic) + MSX2 G7 16-colour sprite + MSX2 G7 256-colour bitmap
VIDEO_UNIT video_wide_clut[32*64*64]; // the V9958 true colour modes YJK and YAE are too heavy for runtime calculations :-(

// Z80 registers: the hardware and the debugger must be allowed to "spy" on them!

HLII z80_af,z80_bc,z80_de,z80_hl; // Accumulator+Flags, BC, DE, HL
HLII z80_af2,z80_bc2,z80_de2,z80_hl2,z80_ix,z80_iy; // AF', BC', DE', HL', IX, IY
HLII z80_pc,z80_sp,z80_iff,z80_ir; // Program Counter, Stack Pointer, Interrupt Flip-Flops, IR pair
BYTE z80_imd,z80_r7; // Interrupt Mode // low 7 bits of R, required by several `IN X,(Y)` operations

int z80_irq; // IRQ status: +1 IE0 (VDP frame event) +2 IE1 (VDP line event)
int z80_int; // Z80 INT flag (IFF1 latch + HALT mode)

char audio_dirty; int audio_queue=0; // used to clump audio updates together to gain speed

// behind the PIO: TAPE --------------------------------------------- //

#define tape_enabled (!tape_disabled)
#define tape_disabled (pio_port_c&16)
//#define TAPE_TAP_FORMAT // useless outside Spectrum
#define TAPE_CAS_FORMAT // required!
#define TAPE_TZX_KANSAS // required!
//#define FASTTAPE_DUMPER // overkill!
#include "cpcec-k7.h"

// 0XA8,0XA9,0XAA,0AXB: PIO 8255 ------------------------------------ //

BYTE pio_port_a,pio_port_b,pio_port_c,pio_control; // port A: PSLOT (MMU)
void pio_reset(void)
	{ pio_port_a=pio_port_b=pio_port_c=tape_output=0,pio_control=0X82; } // notice that the PIO 8255 in the MSX is hard-wired to the MMU

FILE *printer=NULL; int printer_p=0; BYTE printer_z,printer_t[256];
void printer_flush(void) { fwrite1(printer_t,printer_p,printer),printer_p=0; }
void printer_close(void) { printer_flush(),fclose(printer),printer=NULL; }

// 0XA0,0XA1,0XA2: AY-3-8910 PSG ------------------------------------ //

#define PSG_MAX_VOICE 10922 // 32768/3= 10922
#define PSG_TICK_STEP 16 // 3.58 MHz /2 /16 = 111875 Hz
#define PSG_KHZ_CLOCK 1750 // compare with the 2000 kHz YM3 standard
#define PSG_MAIN_EXTRABITS 0 // does any title need nonzero here at all?
#define PSG_PLAYCITY 1 // the SECOND PSG card contains one chip...
#define PSG_PLAYCITY_XLAT(x) psg_outputs[(x)] // ...playing at the PSG's same intensity
#define playcity_hiclock TICKS_PER_SECOND // the SECOND PSG clock is fixed
#define playcity_loclock (AUDIO_PLAYBACK*16)
#define PSG_PLAYCITY_RESET ((void)0) // nothing special to do!
int playcity_disabled=1; // this chip is an extension (disabled by default)

#include "cpcec-ay.h"

#include "cpcec-ym.h"
void ym3_write(void) { ym3_write_ay(psg_table,&psg_hard_log,PSG_KHZ_CLOCK); }

int dac_extra=0,dac_delay=0,dac_voice=0; // tape_loud=1,tape_song=0: do any MSX titles play music while loading from tape!?
#define dac_frame() ((dac_extra>>=1)?0:dac_delay>0?dac_voice=dac_voice*63/64:++dac_delay) // soften signal as time passes

BYTE joystick_bit=0; // alternate joystick ports; they use registers 14 and 15 of the PSG
BYTE key2joy_flag=0; // alternate joystick buttons

// behind the MMU: KONAMI SCC-051649 -------------------------------- //

BYTE sccplus_table[256],sccplus_wave[5]; // where sccplus_table[255] is the SCCPLUS MODE register
BYTE sccplus_ready=0,sccplus_debug=0,sccplus_ram[1<<16]; // the two-in-one 64K banks used by "Snatcher" (lower 64K) and "SD Snatcher" (upper 64K)
// SCC- mode (base $98XX..$9FXX)
// 00-1F: wave 1	// 20-3F: wave 2	// 40-5F: wave 3	// 60-7F: waves 4 and 5
// 80-81: tone 1	// 82-83: tone 2	// 84-85: tone 3	// 86-87: tone 4	// 88-89: tone 5
// 8A-8E: vol. 1-5	// 8F: mixer (bits 0-4 are voices 1-5)
// 90-9F: R/W mirror of 80-8F			// A0-BF: R/O mirror of 60-7F
// C0-DF: test ?	// E0-FF: unused ?
// SCC+ mode (base $B8XX..$BFXX)
// 00-1F: wave 1	// 20-3F: wave 2	// 40-5F: wave 3	// 60-7F: wave 4	// 80-9F: wave 5
// A0-A1: tone 1	// A2-A3: tone 2	// A4-A5: tone 3	// A6-A7: tone 4	// A8-A9: tone 5
// AA-AE: vol. 1-5	// AF: mixer (bits 0-4 are voices 1-5)
// B0-BF: R/W mirror of A0-AF
// C0-DF: test ?	// E0-FF: unused ?
BYTE sccplus_playing; // zero on reset, one when a channel is first enabled
int sccplus_tone[5],sccplus_ampl[5]; // these values are too big for 8 bits
#if AUDIO_CHANNELS > 1
int sccplus_stereo[5][2]; // the five channels' LEFT and RIGHT weights
#endif

void sccplus_reset(void)
{
	MEMZERO(sccplus_table); MEMZERO(sccplus_tone); MEMZERO(sccplus_ampl); sccplus_playing=sccplus_debug=0;
	dac_extra=dac_voice=0; // "KONAMI'S SYNTHESIZER" also goes here, as it uses the same unsigned byte samples (no DMA or mixer tho')
	for (int i=0;i<32*5;++i) sccplus_table[i]=(i&16)?-128:127; // dummy square waves; implied in the "BATMAN" remake for MSX2 (42CD: [00]=7F,[10]=80)
}
#define sccplus_amplis(c,b) (sccplus_playing|=sccplus_ampl[c]=((b)&15)*32768/9600) // see also psg_outputs[]; 9600=15 levels *5 channels <<7 bits
void sccplus_update(void)
	{ for (int n=0;n<5;++n) sccplus_amplis(n,sccplus_table[n+0XAA]); }

void sccplus_minus_send(BYTE r,BYTE b) // send byte `b` to register `r` in "SCC-" mode
{
	if (session_shift) cprintf("SCC- %04X:%02X=%02X\n",z80_pc.w,r,b);
	sccplus_debug=0; switch (r>>4)
	{
		case  0: case  1: case  2: case  3: case  4: case  5: // waves 1 to 3
			sccplus_table[r]=b; break;
		case  6: case  7: // wave 4 overwrites wave 5!
			sccplus_table[r]=sccplus_table[r+32]=b; break;
		case  8: // mirror of tones, vols. and mixer
			r+=16; // no `break`!
		case  9: // tones, vols. and mixer
			if (r>=0X9A&&r<=0X9E) sccplus_amplis(r-0X9A,b); // amplitude register?
			//else if (r==0X9F) sccplus_playing|=b&31; // mixer register
			sccplus_table[r+16]=b; break;
		case 14: case 15: // deformation
			sccplus_table[192]=b; break;
		//case 12: case 13: // test + unused
	}
}
BYTE sccplus_minus_recv(BYTE r) // request a byte from register `r` in "SCC-" mode
{
	if (r>=0XE0) sccplus_table[192]=255; // reading these bytes causes a write!
	return r>=0XA0&&r<=0XBF?sccplus_table[r-0X40]:sccplus_table[r]; // *!* undefined areas!?
}

void sccplus_table_send(BYTE r,BYTE b) // send byte `b` to register `r` in "SCC+" mode
{
	if (session_shift) cprintf("SCC+ %04X:%02X=%02X\n",z80_pc.w,r,b);
	sccplus_debug=1; switch (r>>4)
	{
		case 11: // mirror of tones, vols. and mixer
			r-=16; // no `break`!
		case 10: // tones, vols. and mixer
			if (r>=0XAA&&r<=0XAE) sccplus_amplis(r-0XAA,b); // amplitude register?
			//else if (r==0XAF) sccplus_playing|=b&31; // mixer register
			// no `break`!
		case  0: case  1: case  2: case  3: case  4: case  5: case  6: case  7: case  8: case  9: // waves 1 to 5
			sccplus_table[r]=b; break;
		case 12: case 13: // deformation
			sccplus_table[192]=b; break;
		//case 14: case 15: // test + unused
	}
}
BYTE sccplus_table_recv(BYTE r) // request a byte from register `r` in "SCC+" mode
{
	if (r>=0XC0&&r<=0XDF) sccplus_table[192]=255; // reading these bytes causes a write!
	return r>=0XB0&&r<=0XBF?sccplus_table[r-16]:sccplus_table[r]; // *!* undefined areas!?
}

#define SCCPLUS_MAIN_EXTRABITS 0 // oversampling isn't very useful, SCCPLUS samples have low granularity

void sccplus_main(AUDIO_UNIT *t,int l) // "piggyback" the audio output for nonzero `l` samples!
{
	if ((sccplus_table[192]&3)) sccplus_playing=0; // bits 0 and 1 of TEST turn all frequencies into ultrasounds :-(
	int s[5]={(sccplus_table[161+0]&15)*256+sccplus_table[160+0],
		(sccplus_table[161+2]&15)*256+sccplus_table[160+2],
		(sccplus_table[161+4]&15)*256+sccplus_table[160+4],
		(sccplus_table[161+6]&15)*256+sccplus_table[160+6],
		(sccplus_table[161+8]&15)*256+sccplus_table[160+8]};
	do
	{
		#if AUDIO_CHANNELS > 1
		static int o0=0,o1=0;
		#else
		static int o=0;
		#endif
		#if SCCPLUS_MAIN_EXTRABITS
		for (char n=0;n<(1<<SCCPLUS_MAIN_EXTRABITS);++n) // remember, the old SCC- is a subset of the SCC+
		#endif
		{
			static int i=0; i+=TICKS_PER_SECOND; // AFAIK the SCCPLUS clock is 3.58 MHz, like the Z80
			int m=i/(AUDIO_PLAYBACK<<SCCPLUS_MAIN_EXTRABITS),j; i%=(AUDIO_PLAYBACK<<SCCPLUS_MAIN_EXTRABITS);
			for (char c=0;c<5;++c) // update and mix channels; divisions let us skip many iterations
				if (sccplus_table[175]&(1<<c))
					if ((j=s[c])>1) // ignore ultrasounds = avoid divisions by zero!
					{
						int k=(sccplus_tone[c]+=m)/j; sccplus_wave[c]+=k,sccplus_tone[c]%=j;
						j=sccplus_wave[c]; //if (!sccplus_table[192]) j^=sccplus_wave[c]>>5;
						int p=sccplus_ampl[c]*(INT8)sccplus_table[c*32+(j&31)];
						#if AUDIO_CHANNELS > 1
							o0+=sccplus_stereo[c][0]*p,
							o1+=sccplus_stereo[c][1]*p;
						#else
							o+=p;
						#endif
					}
		}
		#if AUDIO_CHANNELS > 1
		*t=((o0>>(SCCPLUS_MAIN_EXTRABITS+8))+*t)>>1; ++t; o0&=(1<<SCCPLUS_MAIN_EXTRABITS+8)-1;
		*t=((o1>>(SCCPLUS_MAIN_EXTRABITS+8))+*t)>>1; ++t; o1&=(1<<SCCPLUS_MAIN_EXTRABITS+8)-1;
		#else
		*t=((o >> SCCPLUS_MAIN_EXTRABITS   )+*t)>>1; ++t; o &=(1<<SCCPLUS_MAIN_EXTRABITS  )-1;
		#endif
	}
	while (--l>0);
}

// behind the MMU: YAMAHA OPLL YM2413 ------------------------------- //

#define OPLL_TICK_STEP 72 // the OPLL updates all channels every 72 Z80-T
#define OPLL_MAIN_EXTRABITS 0 // oversampling isn't very useful here either, songs never play high pitches
#define OPLL_MAX_VOICE 3640 // =32768/9 ; but beware of noise!!
BYTE opll_internal=0,opll_poke_i_o=0;
#include "cpcec-yo.h"

// behind the MMU: DISC CONTROLLER ---------------------------------- //

// we emulate the WD1793-like Philips WD2793-02 chip, more widespread (and simpler!) than the NEC765-like Panasonic-Toshiba TC8566AF chip

#define DISKETTE_PAGE 512
#define DISKETTE_SECTORS 9
#define DISKETTE_TRACK99 99
#define DISKETTE_PC (z80_pc.w)
#define diskette_filemode disc_filemode // as used by the WD1793
#define diskette_drives 2 // even if this is 1 "? fre(0)" shows 23432 (PHILIPS) instead of 24990 unless we boot with CONTROL down :-/ NATIONAL always shows 23430 :-(
#include "cpcec-wd.h"

#define disc_closeall() (disc_close(0),disc_close(1))

#define DISC_MAXTRACKS 80 // aren't all MSX-DOS discs limited to 80 tracks?

FILE *disc[4]={NULL,NULL,NULL,NULL}; char disc_path[STRMAX]=""; // file handles and current path

int disc_close(int d) // remove disc from drive `d` (0=A:), if any
{
	if (diskette_size[d]<0)
		fseek(disc[d],0,SEEK_SET),fwrite1(diskette_mem[d],-diskette_size[d],disc[d]),fsetsize(disc[d],-diskette_size[d]);
	if (diskette_mem[d])
		free(diskette_mem[d]),diskette_mem[d]=NULL;
	if (disc[d])
		puff_fclose(disc[d]),disc[d]=NULL;
	return diskette_size[d]=0;
}
int disc_create(char *s) // create a 720k disc file at `s`; !0 ERROR
{
	FILE *f=fopen(s,"wb"); if (!f) return 1; // cannot create disc!
	BYTE t[1<<9]; MEMZERO(t); // a whole sector
	//memcpy(t,"\353\376\220SINGLE80\000\002\002\001\000\002\160\000\320\002\370\002\000\011\000\001\000\000\000\320\307",32); // 1-sided 360k boot sector
	memcpy(t,"\353\376\220DOUBLE80\000\002\002\001\000\002\160\000\240\005\371\003\000\011\000\002\000\000\000\320\311",32); // 2-sided 720k boot sector
	fwrite1(t,sizeof(t),f); // notice that the Z80 code in this dummy boot sector is simply the opcode $C9 (0311), "RET"; the disc is ignored!
	MEMZERO(t); memcpy(t,"\371\377\377",3); fwrite1(t,sizeof(t),f); // 0371 is $F9, the 720k marker; see above
	MEMZERO(t); for (int n=0;n<12;++n) fwrite1(t,sizeof(t),f); // FAT backups and file directories
	memset(t,0XE5,sizeof(t)); for (int n=0;n<1440-1-1-12;++n) fwrite1(t,sizeof(t),f);
	return fclose(f);
}
int disc_open(char *s,int d,int q) // insert a MSX-DOS disc from path `s` in drive `d`; !0 ERROR
{
	disc_close(d);
	if (!(diskette_mem[d]=malloc(DISC_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE)))
		return 1; // memory error!
	memset(diskette_mem[d],0XE5,DISC_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE); // sanitize space
	if (!(disc[d]=puff_fopen(s,(diskette_canwrite[d]=q)?"rb+":"rb")))
		return disc_close(d),1; // file error!
	int i=fread1(diskette_mem[d],(DISC_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE),disc[d]);
	i+=fread1(diskette_mem[d],(DISC_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE),disc[d]);
	if ((i!=180*1024&&i!=360*1024&&i!=720*1024)//||!equalsii(&diskette_mem[d],0XFEEB) // signature $EB $FE [$90]
		||!memcmp(diskette_mem[d],"MV - CPC",8)||!memcmp(diskette_mem[d],"EXTENDED",8)) // Amstrad CPC and Spectrum +3 discs!
		return disc_close(d),1; // not a valid MSX-DOS disc!
	//diskette_sides[d]=(diskette_mem[d][0X15]&1)+1; // FAT:- F8 - F9 - FC - FD
	//diskette_tracks[d]=(diskette_mem[d][0X15]&4)?40:80; // 80x1 80x2 40x1 40x2
	diskette_tracks[d]=i>180*1024?80:40; // check nothing but the file size:
	diskette_sides[d]=i>360*1024?2:1; // the boot sector isn't reliable :-(
	cprintf("DISKETTE format: %d tracks and %d sides.\n",diskette_tracks[d],diskette_sides[d]);
	STRCOPY(disc_path,s);
	return diskette_size[d]=diskette_tracks[d]*diskette_sides[d]*DISKETTE_SECTORS*DISKETTE_PAGE,0;
}

// behind the MMU: MSX-DISK ROM and other devices ------------------- //

BYTE msx2p_flags,ioctl_flags,openmsx_log=0; // `system control` (port $F5), MSX-SYSTEM/MSX-ENGINE chipset, OPENMSX debug log
int kanji_p=0; // index between 0 and 0X1FFFF (128K JIS1) or 0X3FFFF (256K JIS2)
BYTE kanji_rom[1<<18]; // 256K JIS2 (128K JIS1 is a subset) filled with 255 if missing or disabled (JIS1 expects $00000=$00; JIS2, $20000=$00)
BYTE disc_rom[2<<14]; // 32K, the second half must be filled with 0XFF, but stay distinct from bad_rom: MSX-DISK code is limited to $4000-$7FFF,
BYTE *disc_mapping[4]={&disc_rom[0X4000-0X0000],&disc_rom[0X0000-0X4000],&disc_rom[0X4000-0X8000],&disc_rom[0X4000-0XC000]}; // as stated above.
BYTE opll_rom[1<<14]; // 16K, but similarly to MSX-DISK, it's read-only yet sensible to memory writes!
const BYTE opll_txt[32]="AB\020@\000\000\000\000\000\000\000\000\000\000\000\000\311/DUMMY/APRLOPLL"; // MSX-MUSIC OPLL dummy ROM
// bootable discs f.e. CHOPPER2.DSK expect ports to appear on the $BFFX range, but the DISK ROM must not be visible in that bank;
// otherwise free memory plummets into 20308 bytes (out of 28815) instead of the expected 23432 (normal boot) or 24990 (CONTROL down, f.e. ABADIA16.DSK)

void ioctl_setup(void) // notice that this setup goes beyond the disc logic; perhaps we should call this function "extensions_setup"
{
	FILE *f; diskette_setup(); opll_setup();
	MEMBYTE(kanji_rom,-1);	if ((f=fopen(strcat(strcpy(session_substr,session_path),"msxjis2.rom"),"rb"))
		||(f=fopen(strcat(strcpy(session_substr,session_path),"msxjis1.rom"),"rb"))) fread1(kanji_rom,sizeof(kanji_rom),f),fclose(f);
	MEMBYTE(disc_rom,-1); if (f=fopen(strcat(strcpy(session_substr,session_path),"msxdisk.rom"),"rb")) fread1(disc_rom,1<<14,f),fclose(f);
	if (f=fopen(strcat(strcpy(session_substr,session_path),"msxmusic.rom"),"rb")) fread1(opll_rom,1<<14,f),fclose(f);
		else MEMBYTE(opll_rom,0XC9),MEMSAVE(opll_rom,opll_txt); // MSX-MUSIC OPLL dummy ROM, no jump address, all code is RETs
}
#define disc_reset() (diskette_reset())
#define ioctl_reset() (disc_reset(),opll_reset(),opll_poke_i_o=msx2p_flags=kanji_p=0,ioctl_flags=~0) // hard resets clear "msx2p_flags"; soft resets keep it

// 0XFC,0XFD,0XFE,0XFF: MEMORY MAPPER UNIT (MMU) -------------------- //

BYTE ram_cfg[4]; // RAM MAPPER for pages 0, 1, 2 and 3; beware, bits 0 and 1 are negative: pages 0..3 are almost always set to N+3..0!
BYTE rom_cfg[4]; // ROM CONFIG at offset $FFFF: SSLOT (MMU)
BYTE ram_map=1,ram_depth=0,ram_bit=3,ram_dirty=3;
void ram_setcfg(int x) { ram_depth=((ram_map=x)?x-1:0); ram_bit=(4<<ram_depth)-1; } // 0 = 64K W/O MAPPER, 1 = 64K, 2 = 128K, etc.
int ram_getcfg(void) { return ram_map?ram_depth+1:0; }

BYTE *cart=NULL; // external cartridge, empty by default
#define CART_BITS 22 // 1<<22 = 4 MEGABYTES! "Sword of Ianna" is 704k; late Koei titles are 1M; "King's Valley Enhanced" is almost 4M!
#define CART_MASK ((1<<CART_BITS)-1)
#define CART_IDS 10 // supported types, see `mmu_update` below

BYTE sram[1<<15]; // the biggest SRAM chip is 32K long: KOEI SRAM cartridges!
char sram_path[STRMAX]=""; BYTE sram_dirt=0,sram_cart=0,sram_mask=0; // path and bits of the SRAM data dump
int sram_makepath(char *s) // build `sram_path` using string `s` as a base; NONZERO OK!
{
	strcpy(sram_path,s); char *t=strrchr(sram_path,'.'),*u=strrchr(sram_path,PATHCHAR);
	return t&&(!u||u<t)?strcpy(t,".srm"),1:0; // "dir/name.old" => "dir/name.new"
}

char cart_path[STRMAX]="";
unsigned int cart_sha1_size=0,cart_sha1_list[2<<10][6];
BYTE cart_bank[4],cart_id=0,cart_log=0; // cart_id is the mapper type, cart_log is the ceiling of the binary logarithm of the cartridge size
BYTE cart_big=0; // overrides cart_id if nonzero: 1: 32K/48K/64K cartridges that "invade" $0000-$3FFF; 2: 16K/32K/48K cartridges that start at $4000
int cart_insert(char *s) // insert a cartridge; zero OK, nonzero error!
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1; // cannot open file!
	int i,o; char q;
	/**/ if (fgetii(f)==0X4241) // normal case, "AB" appears at the beginning
		q=2; // cart_big is either 0 or 2
	else if (fseek(f,+0X4000,SEEK_SET),fgetii(f)==0X4241) // "AB" appears at offset +16K (f.e. the "Knight Lore MSX2" remake)
		q=1; // cart_big is always 1
	else if (fseek(f,-0X4000,SEEK_END),fgetii(f)==0X4241) // "AB" appears at offset -16K, we've spotted the fearsome "R-Type"
		q=0; // cart_big is always 0
	else return puff_fclose(f),1; // missing "AB" ID!
	i=fgetii(f); o=fgetii(f); // "Super Lode Runner" stores $0000 in both fields :-(
	if ((i|o)&&(i<0X4004||i>0XBFFF)&&(o<0X4004||o>0XBFFF)) return puff_fclose(f),1; // wrong CART.INIT value! ("Fantasy Zone 2" uses $4004)
	fseek(f,0,SEEK_END); i=ftell(f); if (i<32||i>(1<<CART_BITS)) return puff_fclose(f),1; // too short (dummy SCCPLUS cart is 32B) or too long!
	if (!cart) { if (!(cart=malloc(1<<CART_BITS))) return puff_fclose(f),1; } // memory full!
	memset(cart,255,1<<CART_BITS);
	fseek(f,0,SEEK_SET); fread1(cart,i,f); puff_fclose(f);
	cart_log=12; while ((1<<++cart_log)<i) {}
	if (i<=(1<<14)) // mirror pages; 16K is special!
	{
		// "Nausicaa" (incompatible with MSX-DISK!), "3D Golf Simulation", etc. are tokenized BASIC cartridges (no $4000-$7FFF range); the header is diagnostic
		memcpy(&cart[1<<14],cart,1<<14); if (!(cart[3]|cart[5]|cart[7])&&cart[9]>=0X80&&cart[9]<0XC0) memset(cart,255,1<<14); // 3D GOLF: $8010; NAUSICAA: $9010
	}
	for (int j=1<<15;j<(1<<CART_BITS);j<<=1) if (i<=j) memcpy(&cart[j],cart,j); // mirror pages, if required: 32K, 64K, 128K, etc.
	cart_big=q<2?q:i>0XC000?0:2; // cartridges where "AB" appears in the second 16K "invade" $0000-$3FFF; above 48K, they carry custom banking; otherwise PSLOT is enough
	sccplus_ready=sram_dirt=sram_cart=sram_mask=0; MEMBYTE(sram,-1); MEMBYTE(sccplus_ram,-1);
	if (sram_makepath(s)) // load SRAM if possible, but don't tag it as dirty yet
		if (f=puff_fopen(sram_path,"rb")) fread1(sram,sizeof(sram),f),puff_fclose(f);
	// cartridge type detection based on a pre-made SHA-1 list with types: "012356789abcdef012356789abcdef0123567 4" (roughly compatible with FMSX)
	o=0; sha1_init(); while (o+64<=i) sha1_hash(&cart[o]),o+=64; sha1_exit(&cart[o],i&63);
	cprintf("SHA-1: %08X%08X%08X%08X%08X",sha1_o[0],sha1_o[1],sha1_o[2],sha1_o[3],sha1_o[4]);
	for (o=0;o<cart_sha1_size;++o)
		if (!memcmp(sha1_o,cart_sha1_list[o],sizeof(sha1_o)))
			{ cart_id=cart_sha1_list[o][5]; break; }
	cprintf(":%X (%d)\n",cart_id,o<cart_sha1_size?o:-1); // not sure what to do when the type is unknown...
	/**/ if (cart_id==6) // KONAMI SRAM configuration seems to be always the same
		sram_cart=16; // 8K: "GAME MASTER 2"
	else if (cart_id==7) // KOEI SRAM configuration is tied to the cartridge size
	{
		if (equalsmmmm(&cart[0X14],0X72647279)) sram_cart=128; // special case: "WIZARDRY" 512K +8K
		else if (i>=(1<<20)) sram_cart=128,sram_mask=3; // 1 MB +32K: "NOBUNAGA NO YABOU 1991"
		else if (i>=(1<<19)) sram_cart=192; // 512K +8K: (not +16K?) "L'EMPEREUR"
		else /* (i>=(1<<18*/ sram_cart=224; // 256K +8K: "GENGHIS KHAN MSX1"
	}
	else if (cart_id==8) // ASCII 16K SRAM is sometimes limited to 2K, but it has no impact here
		sram_cart=16; // 8K: "HYDLIDE 2", "HARRY FOX: MSX SPECIAL"...
	else if (cart_id==2)
		sccplus_ready=!strcmp(&cart[0X14],"+ dummy"); // "SCCPLUS.ROM" enables the SCC+ RAM banks!
	STRCOPY(cart_path,s); return 0;
}
void cart_reset(void)
{
	cart_bank[0]=cart_bank[1]=cart_bank[2]=cart_bank[3]=0; // default to zero
	if (!cart||cart_big)
		; // nothing to bank!
	else if (cart_id==5) // ASCII 16K: "Gall Force" requires the second bank to boot as 0; "Eggerland 2" suggests that this is normal
	{
		if (equalsmmmm(&cart[0X2804C],0X4259444F))
			cart_bank[0]=15; // special case: "R-Type" is ASCII 16K, but the $4000-$7FFF bank boots as 15 instead of as zero!
	}
	else if (cart_id<8)
		cart_bank[0]=0,cart_bank[1]=1,cart_bank[2]=2,cart_bank[3]=3; // normal boot values for Generic, Konami and ASCII 8K carts
	else
		; // types 8 and 9 (ASCII 16K + SRAM: "A-TRAIN"; "HARRY FOX: YUKI NO MAOH", "SUPER LODE RUNNER", "CROSS BLAIM") use zeros
}
void cart_remove(void) // remove the cartridge
{
	int i=sram_dirt?sram_mask+1:0; sccplus_ready=sram_dirt=cart_log=cart_big=0; if (cart)
	{
		free(cart),cart=NULL,cart_reset();
		if (i&&sram_cart) if (sram_makepath(cart_path))
			{ FILE *f=puff_fopen(sram_path,"wb"); if (f) fwrite1(sram,i<<13,f),puff_fclose(f); }
	}
}
void cart_setup(void) // load the cartridge list, if available
{
	strcat(strcpy(session_substr,session_path),my_caption ".sha"); // sorta compatible with "carts.sha" from FMSX
	cart_sha1_size=0; FILE *f=fopen(session_substr,"r"); if (f)
	{
		while (cart_sha1_size<length(cart_sha1_list)&&fgets(session_substr,STRMAX,f))
		{
			char *s=UTF8_BOM(session_substr); int k; unsigned int h[5]={0,0,0,0,0};
			while ((k=eval_hex(*s))>=0) // build a 40-nibble value, even if the string isn't 40 chars!
				h[0]=h[0]<<4|((h[1]>>28)&15),
				h[1]=h[1]<<4|((h[2]>>28)&15),
				h[2]=h[2]<<4|((h[3]>>28)&15),
				h[3]=h[3]<<4|((h[4]>>28)&15),
				h[4]=h[4]<<4|((k>>0)&15),++s;
			if (s!=session_substr&&(k=eval_hex(*++s))>=0&&k<CART_IDS) // valid and supported?
				cart_sha1_list[cart_sha1_size][0]=h[0]&0XFFFFFFFF,
				cart_sha1_list[cart_sha1_size][1]=h[1]&0XFFFFFFFF,
				cart_sha1_list[cart_sha1_size][2]=h[2]&0XFFFFFFFF,
				cart_sha1_list[cart_sha1_size][3]=h[3]&0XFFFFFFFF,
				cart_sha1_list[cart_sha1_size][4]=h[4]&0XFFFFFFFF,
				cart_sha1_list[cart_sha1_size][5]=k,
				//cprintf("%08X%08X%08X%08X%08X:%X\n",h[0],h[1],h[2],h[3],h[4],k), // perhaps too much
				++cart_sha1_size;
		}
		cprintf("SHA-1 list: %d entries.\n",cart_sha1_size); // sufficient
		fclose(f);
	}
	else cprintf("Cannot load SHA-1 list!"); // should we show an actual warning?
}
int cart_hotfix(char *s) // patch cartridge according to IPS file `s`; zero OK, nonzero error!
{
	if (!cart) return 1; // no cartridge, nothing to do
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1; // cannot open file!
	int i=fgetmmmm(f),j,k,l=1<<cart_log; if (i!=0X50415443||fgetc(f)!=0X48) return puff_fclose(f),1; // wrong file!
	while ((i=fgetmmm(f)),(j=fgetmm(f))>=0&&i>=0)
		if (j) // normal patch?
			if (i+j>l) break; else cprintf("IPS:%06X:%04X\n",i,j),fread1(cart+i,j,f); // quit if the patch overflows!
		else // RLE compression
			if (i+(j=fgetmm(f))>l) break; else k=fgetc(f),cprintf("IPS:%06X:%04X,%02X\n",i,j,k),memset(cart+i,k,j);
	return puff_fclose(f),j>=0||i!=0X454F46; // "EOF"
}

BYTE ps_slot[4];
BYTE slots_ram=0x32,slots_1st=0x10,slots_2nd=0x20,slots_dsk=0x20; // original MSX1 slots
BYTE slots_sub=0x31,slots_mus=0x33; // later MSX2, MSX2P and MSXTR slots
// some slots are compatible with each other; for example SUB is compatible with DSK or MUS in MSX2 but not in MSX2P

void mmu_update(void) // the MMU requires the PIO because PORT A is the PSLOT bitmask!
{
	if (!type_id) rom_cfg[3]=2+8+32+128; // MSX1: no SSLOT logic!
	if (type_id<2) rom_cfg[0]=rom_cfg[1]=rom_cfg[2]=0; // MSX2: SSLOT logic limited to PSLOT3!
	BYTE z=(ram_cfg[0]|ram_cfg[1]|ram_cfg[2]|ram_cfg[3]|3)&ram_bit,*s,*t;
	if (ram_dirty<z) session_dirty=ram_dirty=z;
	MEMZERO(mmu_bit); // by default no PEEK or POKE events are raised
	// range $0000-$3FFF
	s=&bad_rom[0-0X0000]; t=&bad_ram[0-0X0000]; z=(pio_port_a>>0)&3; ps_slot[0]=z=((rom_cfg[z]>>0)&3)+z*16;
	/**/ if (!z) s=&mem_rom[0X00000-0X0000]; // BIOS!
	else if (z==slots_ram) s=t=&mem_ram[(((ram_cfg[0]^3)&ram_bit)<<14)-0X0000];
	else if (z==slots_1st) // normal cartridge (1/4)
		{ if (cart) if (cart_big==1) s=&cart[0X0000-0X0000]; }
	else if (z==slots_sub) // EXT.! (MSX2)
		{ /*if (equalsii(&mem_rom[0X10000],0X4443))*/ s=&mem_rom[0X10000-0X0000]; }
	mmu_rom[ 0]=mmu_rom[ 1]=mmu_rom[ 2]=mmu_rom[ 3]=s;
	mmu_ram[ 0]=mmu_ram[ 1]=mmu_ram[ 2]=mmu_ram[ 3]=t;
	// range $4000-$7FFF
	s=&bad_rom[0-0X4000]; t=&bad_ram[0-0X4000]; z=(pio_port_a>>2)&3; ps_slot[1]=z=((rom_cfg[z]>>2)&3)+z*16;
	/**/ if (!z) s=&mem_rom[0X04000-0X4000]; // BAS.!
	else if (z==slots_ram) s=t=&mem_ram[(((ram_cfg[1]^3)&ram_bit)<<14)-0X4000];
	else if (z==slots_1st) // normal cartridge (2/4) // "KONAMI'S SYNTHESIZER" DAC
		{ if (cart) s=cart_big==1?&cart[0X4000-0X4000]:&cart[0X0000-0X4000]; }
	else if (z==slots_dsk) // DISK without C-BIOS LOGO?
		{ if (!disc_disabled&&!equalsii(&mem_rom[0X08000],0X2D43)) s=disc_mapping[1],mmu_bit[ 7]=3; } // WD1793 handler (2/4)
	else if (z==slots_sub)
		{ /*if (equalsii(&mem_rom[0X10000],0X4443))*/ s=&mem_rom[0X14000-0X4000]; } // KANJI1! (MSX2P)
	else if (z==slots_mus)
		{ if (opll_internal) s=&opll_rom[0-0X4000],mmu_bit[7]=2; } // pass WRITE operations to the OPLL handler
	mmu_rom[ 4]=mmu_rom[ 5]=mmu_rom[ 6]=mmu_rom[ 7]=s;
	mmu_ram[ 4]=mmu_ram[ 5]=mmu_ram[ 6]=mmu_ram[ 7]=t;
	// range $8000-$BFFF
	s=&bad_rom[0-0X8000]; t=&bad_ram[0-0X8000]; z=(pio_port_a>>4)&3; ps_slot[2]=z=((rom_cfg[z]>>4)&3)+z*16;
	/**/ if (!z) { /*if (!equalsii(&mem_rom[0X18000],0X4443))*/ s=&mem_rom[0X08000-0X8000]; } // LOGO! (C-BIOS)
	else if (z==slots_ram) s=t=&mem_ram[(((ram_cfg[2]^3)&ram_bit)<<14)-0X8000];
	else if (z==slots_1st) // normal cartridge (3/4)
		{ if (cart) s=cart_big==1?&cart[0X8000-0X8000]:&cart[0X4000-0X8000]; }
	else if (z==slots_dsk) // DISK without C-BIOS LOGO?
		{ if (!disc_disabled&&!equalsii(&mem_rom[0X08000],0X2D43)) s=disc_mapping[2],mmu_bit[11]=3; } // WD1793 handler (3/4)
	else if (z==slots_sub)
		{ /*if (equalsii(&mem_rom[0X10000],0X4443))*/ s=&mem_rom[0X18000-0X8000]; } // KANJI2! (MSX2P)
	mmu_rom[ 8]=mmu_rom[ 9]=mmu_rom[10]=mmu_rom[11]=s;
	mmu_ram[ 8]=mmu_ram[ 9]=mmu_ram[10]=mmu_ram[11]=t;
	// range $C000-$FFFF
	mmu_bit[15]=type_id&&(pio_port_a>=0XC0)?3:0; // detect/ignore PEEK/POKE at $FFFF (SSLOT) (MSX2/MSX1)
	s=&bad_rom[0-0XC000]; t=&bad_ram[0-0XC000]; z=(pio_port_a>>6)&3; ps_slot[3]=z=((rom_cfg[z]>>6)&3)+z*16;
	/**/ if (!z) ;//s=&mem_rom[0X0C000-0X0000]; // XYZ1! (is there any machine where this is NOT like bad_rom?)
	else if (z==slots_ram) s=t=&mem_ram[(((ram_cfg[3]^3)&ram_bit)<<14)-0XC000];
	else if (z==slots_1st) // normal cartridge (4/4)
		{ if (cart) s=cart_big==1?&cart[0XC000-0XC000]:&cart[0X8000-0XC000]; }
	else if (z==slots_sub) // XYZ2! (MSX2) (ditto, is there any machine where this is NOT like bad_rom?)
		;//{ /*if (equalsii(&mem_rom[0X10000],0X4443))*/ s=&mem_rom[0X1C000-0X0000]; }
	mmu_rom[12]=mmu_rom[13]=mmu_rom[14]=mmu_rom[15]=s;
	mmu_ram[12]=mmu_ram[13]=mmu_ram[14]=mmu_ram[15]=t;
	if (!cart) // no cartridge?
	{
		// nothing!
	}
	else if (cart_big) // small special cartridges go here
	{
		if (ps_slot[1]==slots_1st)
		{
			if (equalsmmmm(&cart[0X7F64],0X37343120)) mmu_bit[4]=2; // "KONAMI'S SYNTHESIZER" DAC: POKE $4000,NN
		}
	}
	// cartridges bigger than 48k require more logic: mappers!
	else switch (cart_id) // always last!
	{
		case 0: // Generic 8K
			if (ps_slot[1]==slots_1st)
				//mmu_ram[ 4]=mmu_ram[ 5]=mmu_ram[ 6]=mmu_ram[ 7]=&bad_ram[0-0X4000],
				mmu_rom[ 4]=mmu_rom[ 5]=&cart[((cart_bank[0]<<13)&CART_MASK)-0X4000],
				mmu_rom[ 6]=mmu_rom[ 7]=&cart[((cart_bank[1]<<13)&CART_MASK)-0X6000],
				mmu_bit[ 4]=mmu_bit[ 5]=mmu_bit[ 6]=mmu_bit[ 7]=2; // detect POKE
			if (ps_slot[2]==slots_1st)
				//mmu_ram[ 8]=mmu_ram[ 9]=mmu_ram[10]=mmu_ram[11]=&bad_ram[0-0X8000],
				mmu_rom[ 8]=mmu_rom[ 9]=&cart[((cart_bank[2]<<13)&CART_MASK)-0X8000],
				mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[3]<<13)&CART_MASK)-0XA000],
				mmu_bit[ 8]=mmu_bit[ 9]=mmu_bit[10]=mmu_bit[11]=2; // detect POKE
			break;
		case 1: // Generic 16K
			if (ps_slot[1]==slots_1st)
				//mmu_ram[ 4]=mmu_ram[ 5]=mmu_ram[ 6]=mmu_ram[ 7]=&bad_ram[0-0X4000],
				mmu_rom[ 4]=mmu_rom[ 5]=
				mmu_rom[ 6]=mmu_rom[ 7]=&cart[((cart_bank[0]<<14)&CART_MASK)-0X4000],
				mmu_bit[ 4]=mmu_bit[ 5]=mmu_bit[ 6]=mmu_bit[ 7]=2; // detect POKE
			if (ps_slot[2]==slots_1st)
				//mmu_ram[ 8]=mmu_ram[ 9]=mmu_ram[10]=mmu_ram[11]=&bad_ram[0-0X8000],
				mmu_rom[ 8]=mmu_rom[ 9]=
				mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[1]<<14)&CART_MASK)-0X8000],
				mmu_bit[ 8]=mmu_bit[ 9]=mmu_bit[10]=mmu_bit[11]=2; // detect POKE
			break;
		case 2: // Konami SCC
			if (ps_slot[1]==slots_1st)
			{
				if (sccplus_ready)
				{
					mmu_rom[ 4]=mmu_rom[ 5]=&sccplus_ram[((cart_bank[0]<<13)&(length(sccplus_ram)-1))-0X4000],
					mmu_rom[ 6]=mmu_rom[ 7]=&sccplus_ram[((cart_bank[1]<<13)&(length(sccplus_ram)-1))-0X6000];
					if (!(sccplus_table[255]&(1+16)))
						mmu_bit[ 5]=2; // detect POKE!
					else
						mmu_ram[ 4]=mmu_ram[ 5]=mmu_rom[ 4];
					if (!(sccplus_table[255]&(2+16)))
						mmu_bit[ 7]=2; // detect POKE!
					else
						mmu_ram[ 6]=mmu_ram[ 7]=mmu_rom[ 6];
				}
				else
					mmu_rom[ 4]=mmu_rom[ 5]=&cart[((cart_bank[0]<<13)&CART_MASK)-0X4000],
					mmu_rom[ 6]=mmu_rom[ 7]=&cart[((cart_bank[1]<<13)&CART_MASK)-0X6000],
					mmu_bit[ 5]=mmu_bit[ 7]=2; // detect POKE at $5000-$77FF
			}
			if (ps_slot[2]==slots_1st)
			{
				if (sccplus_ready)
				{
					mmu_rom[ 8]=mmu_rom[ 9]=&sccplus_ram[((cart_bank[2]<<13)&(length(sccplus_ram)-1))-0X8000],
					mmu_rom[10]=mmu_rom[11]=&sccplus_ram[((cart_bank[3]<<13)&(length(sccplus_ram)-1))-0XA000];
					if (!(sccplus_table[255]&(4+16)))
						mmu_bit[ 9]=3; // detect POKE+PEEK: SCC- I/O goes here!
					else
						mmu_ram[ 8]=mmu_ram[ 9]=mmu_rom[ 8];
					mmu_bit[11]=3; // SCC+ I/O and SCCPLUS MODE register!
				}
				else
					mmu_rom[ 8]=mmu_rom[ 9]=&cart[((cart_bank[2]<<13)&CART_MASK)-0X8000],
					mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[3]<<13)&CART_MASK)-0XA000],
					mmu_bit[ 9]=mmu_bit[11]=3; // detect POKE+PEEK at $9000-$BFFF
			}
			break;
		case 6: // Konami 8K + SRAM (16K SRAM set by BIT 4 and selected by BIT 5)
		case 3: // Konami 8K (no SRAM at all)
			if (ps_slot[1]==slots_1st)
			{
				mmu_bit[ 6]=2; // detect $6000-$6FFF, ignore $4000-$4FFF
				if (!sram_cart) mmu_bit[ 7]=2; // detect $6000-$7FFF, ignore $4000-$5FFF
				//mmu_rom[ 4]=mmu_rom[ 5]=&cart[0-0X4000]; // this is always BANK 0!
				if (cart_bank[1]&sram_cart)
					s=&sram[(cart_bank[1]&32)<<7],
					mmu_rom[ 7]=(mmu_rom[ 6]=s-0X6000)-0X1000;
				else
					mmu_rom[ 6]=mmu_rom[ 7]=&cart[((cart_bank[1]<<13)&CART_MASK)-0X6000];
			}
			if (ps_slot[2]==slots_1st)
			{
				mmu_bit[ 8]=mmu_bit[10]=2; // detect POKE at $8000-$8FFF and $A000-$AFFF
				if (!sram_cart) mmu_bit[ 9]=mmu_bit[11]=2; // detect POKE at $8000-$BFFF
				if (cart_bank[2]&sram_cart)
					s=&sram[(cart_bank[2]&32)<<7],
					mmu_rom[ 9]=(mmu_rom[ 8]=s-0X8000)-0X1000;
				else
					mmu_rom[ 8]=mmu_rom[ 9]=&cart[((cart_bank[2]<<13)&CART_MASK)-0X8000];
				if (cart_bank[3]&sram_cart)
					s=&sram[(cart_bank[3]&32)<<7],sram_dirt=1,
					mmu_ram[11]=mmu_rom[11]=(mmu_rom[10]=s-0XA000)-0X1000;
				else
					mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[3]<<13)&CART_MASK)-0XA000];
			}
			break;
		case 8: // ASCII 16K + SRAM (2/8K SRAM set by BIT 4)
		case 5: // ASCII 16K (no SRAM at all)
			if (ps_slot[1]==slots_1st)
			{
				if (cart_bank[0]&sram_cart)
					mmu_rom[ 6]=mmu_rom[ 7]=(mmu_rom[ 4]=mmu_rom[ 5]=&sram[0X0000-0X4000])-0X2000;
				else
					mmu_rom[ 4]=mmu_rom[ 5]=mmu_rom[ 6]=mmu_rom[ 7]=&cart[((cart_bank[0]<<14)&CART_MASK)-0X4000];
				mmu_bit[ 6]=mmu_bit[ 7]=2; // detect POKE at $6000-$7FFF
			}
			if (ps_slot[2]==slots_1st)
			{
				if (cart_bank[1]&sram_cart)
					mmu_ram[10]=mmu_ram[11]=mmu_rom[10]=mmu_rom[11]=(mmu_ram[ 8]=mmu_ram[ 9]=mmu_rom[ 8]=mmu_rom[ 9]=&sram[0X0000-0X8000])-0X2000,sram_dirt=1;
				else
					mmu_rom[ 8]=mmu_rom[ 9]=mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[1]<<14)&CART_MASK)-0X8000];
				//mmu_ram[ 8]=mmu_ram[ 9]=mmu_ram[10]=mmu_ram[11]=&bad_ram[0-0X8000],
			}
			break;
		case 7: // ASCII 8K + SRAM / KOEI SRAM (8/16/32K SRAM set by BIT 5/6/7)
		case 4: // ASCII 8K (no SRAM at all)
		{
			if (ps_slot[1]==slots_1st)
			{
				//mmu_ram[ 4]=mmu_ram[ 5]=mmu_ram[ 6]=mmu_ram[ 7]=&bad_ram[0-0X4000],
				mmu_rom[ 4]=mmu_rom[ 5]=(cart_bank[0]&sram_cart)?&sram[((sram_mask&cart_bank[0])<<13)-0X4000]:&cart[((cart_bank[0]<<13)&CART_MASK)-0X4000];
				mmu_rom[ 6]=mmu_rom[ 7]=(cart_bank[1]&sram_cart)?&sram[((sram_mask&cart_bank[1])<<13)-0X6000]:&cart[((cart_bank[1]<<13)&CART_MASK)-0X6000];
				mmu_bit[ 6]=mmu_bit[ 7]=2; // detect POKE at $6000-$7FFF
			}
			if (ps_slot[2]==slots_1st)
			{
				if (cart_bank[2]&sram_cart)
					t=s=&sram[((sram_mask&cart_bank[2])<<13)-0X8000],sram_dirt=1;
				else
					t=&bad_ram[0-0X8000],s=&cart[((cart_bank[2]<<13)&CART_MASK)-0X8000];
				mmu_ram[ 8]=mmu_ram[ 9]=t,mmu_rom[ 8]=mmu_rom[ 9]=s;
				if (cart_bank[3]&sram_cart)
					t=s=&sram[((sram_mask&cart_bank[3])<<13)-0XA000],sram_dirt=1;
				else
					t=&bad_ram[0-0X8000],s=&cart[((cart_bank[3]<<13)&CART_MASK)-0XA000];
				mmu_ram[10]=mmu_ram[11]=t,mmu_rom[10]=mmu_rom[11]=s;
			}
			break;
		}
		case 9: // Miscellaneous
			/**/ if (equalsmmmm(&cart[0X05350],0X64426553)) // "CROSS BLAIM"
			{
				if (ps_slot[1]==slots_1st)
					mmu_bit[ 4]=2; // detect POKE at $4045
				if (ps_slot[2]==slots_1st)
					mmu_rom[ 8]=mmu_rom[ 9]=
					mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[1]<<14)&CART_MASK)-0X8000];
			}
			else if (equalsmmmm(&cart[0X1FA74],0X4D453150)) // "SUPER LODE RUNNER"
			{
				if (ps_slot[1]==slots_1st)
					mmu_rom[ 4]=mmu_rom[ 5]=mmu_rom[ 6]=mmu_rom[ 7]=&bad_rom[0-0X4000];
				if (ps_slot[2]==slots_1st)
					mmu_bit[ 0]=2, // detect POKE at $0000
					mmu_rom[ 8]=mmu_rom[ 9]=
					mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[1]<<14)&CART_MASK)-0X8000];
			}
			else if (equalsmmmm(&cart[0X004E4],0X384C7C78)) // "HARRY FOX: YUKI NO MAOH"
			{
				if (ps_slot[1]==slots_1st)
					mmu_bit[ 6]=mmu_bit[ 7]=2, // detect POKE at $6000-$7FFF
					mmu_rom[ 4]=mmu_rom[ 5]=mmu_rom[ 6]=mmu_rom[ 7]=&cart[((cart_bank[0]<<15)&CART_MASK)-0X4000];
				if (ps_slot[2]==slots_1st)
					mmu_rom[ 8]=mmu_rom[ 9]=mmu_rom[10]=mmu_rom[11]=&cart[((cart_bank[1]<<15)&CART_MASK)+0X4000-0X8000];
			}
			/*
			else if (equalsmmmm(&cart[0X00018],0X50414332)) // "PAC2" <= "PAC2OPLL"
			{
				if (ps_slot[1]==slots_1st)
				{
					if (equalsmm(&sram[0X1FFE],0X4D69)) // the FM-PAC SRAM is visible when the chars "Mi" are poked
						mmu_ram[ 5]=mmu_rom[ 5]=mmu_ram[ 4]=mmu_rom[ 4]=&sram[0X0000-0X4000];
					else // ROM only, but still catches writes to 5FFE and 5FFF that reveal the SRAM
						mmu_bit[5]=2; // detect POKE at $5000-$5FFF (actually $5FFE-$5FFF)
				}
				//if (ps_slot[2]==slots_1st)
					//mmu_rom[ 8]=mmu_rom[ 9]=mmu_rom[10]=mmu_rom[11]=&bad_rom[0-0X8000];
			}
			*/
			break;
	}
}

void mmu_reset(void) // notice that pio_reset() must happen first on a cold boot!
{
	ram_dirty=ram_cfg[0]=3,ram_cfg[1]=2,ram_cfg[2]=1,ram_cfg[3]=
	rom_cfg[0]=rom_cfg[1]=rom_cfg[2]=rom_cfg[3]=0; mmu_update();
}
void mmu_slowpoke(WORD w,BYTE b) // notice that the caller already filters out invalid ranges and conditions
{
	/**/ if (w>=0XF000) // SSLOT!
		{ if (w==0XFFFF&&type_id>(pio_port_a<192?1:0)) rom_cfg[pio_port_a>>6]=b,mmu_update(); else POKE(w)=b; }
	else if (mmu_rom[w>>12]==disc_mapping[w>>14]) switch(w&0X3FFF) // MSX-DISK I/O?
	{
		case 0X3FB8: case 0X3FF8: diskette_send_command(b); break;
		case 0X3FB9: case 0X3FF9: diskette_send_track(b); break;
		case 0X3FBA: case 0X3FFA: diskette_send_sector(b); break;
		case 0X3FBB: case 0X3FFB: diskette_send_data(b); break;
		// NATIONAL/PANASONIC:
		// port +4 stores the motor (BIT 3: 0 = OFF, 1 = ON), the side (BIT 2: 0 = SIDE A, 1 = SIDE B)
		// and the drive (BITS 1-0: 0 = ?, 1 = A:, 2 = B:, 3 = ?)
		case 0X3FBC: diskette_side=(b>>2)&1; diskette_motor=(diskette_drive=(b-1)&3)<diskette_drives?(b>>3)&1:0; diskette_send_busy(0); break; // invalid drive = motor OFF!
		//case 0X3FBD: break; // *!*
		//case 0X3FBE: break; // *!*
		//case 0X3FBF: break; // *!*
		// PHILIPS, SONY, etc:
		// port +4 stores the side in BIT 0: 0 = SIDE A, 1 = SIDE B
		// port +5 stores the motor (BIT 7: 0 = OFF, 1 = ON) and the drive (BIT 0: 0 = A:, 1 = B:)
		case 0X3FFC: diskette_side=b&1; break;
		case 0X3FFD: diskette_motor=b>>7; diskette_drive=b&1; break; // not `b&3`!
		//case 0X3FFE: break; // *!*
		//case 0X3FFF: break; // *!*
	}
	else if (mmu_rom[ 7]==&opll_rom[0-0X4000]&&(w&0XFFFC)==0X7FF4) switch(w&~8) // MSX-MUSIC MREQ I/O?
	{
		case 0X7FF4: if (opll_poke_i_o) opll_setindex(b); break;
		case 0X7FF5: if (opll_poke_i_o) opll_sendbyte(b); break;
		case 0X7FF6: opll_poke_i_o=b&1; break;
	}
	else if (!cart) // no cartridge?
	{
		// nothing!
	}
	else if (cart_big) // small special cartridges go here
	{
		if (w==0X4000&&equalsmmmm(&cart[0X7F64],0X37343120))
			{ dac_extra=1<<7,dac_delay=0,dac_voice=((b-128)*10922)>>8; } // "KONAMI'S SYNTHESIZER" DAC: unsigned 8-bit sample; see also psg_outputs[]
	}
	else switch (cart_id) // always last!
	{
		case 6: // Konami 8K + SRAM (GAME MASTER 2)
		case 3: // Konami 8K: HML------------- HML=(0..3)+2
		case 0: // Generic 8K: HML------------- HML=(0..3)+2
			cart_bank[(w>>13)- 2]=b,mmu_update();
			break;
		case 1: // Generic 16K: HL-------------- HL=(0..1)+1
			cart_bank[(w>>14)- 1]=b,mmu_update();
			break;
		case 2: // Konami SCC: HML10----------- HML=(0..3)+2
			/**/ if ((w&0X1800)==0X1000)
				cart_bank[(w>>13)- 2]=b,mmu_update();
			else if (sccplus_ready&&w>=0XBFFE&&w<=0XBFFF) // SCCPLUS MODE register!
			{
				sccplus_table[255]=b,mmu_update();
			}
			else if (w>=0X9800&&w<=0X9FFF) // SCC-: 10011-----------
			{
				if (!(~cart_bank[2]&63)) // Konami SCC-?
					if (!(sccplus_table[255]&32))
						sccplus_minus_send(w,b);
			}
			else if (w>=0XB800&&w<=0XBFFF) // SCC+: 10111-----------
			{
				if ((cart_bank[3]&128)) // Konami SCC+?
					if ( (sccplus_table[255]&32))
						sccplus_table_send(w,b);
			}
			break;
		case 7: // ASCII 8K + SRAM (KOEI SRAM)
		case 4: // ASCII 8K: 011HL----------- HL=0..3
			cart_bank[(w>>11)-12]=b,mmu_update();
			break;
		case 8: // ASCII 16K + SRAM
		case 5: // ASCII 16K: 011L0----------- L=0..1
			if (!(w&0X0800)) // does any cartridge write to $6800-$6FFF or $7800-$7FFF?
				cart_bank[(w>>12)- 6]=b,mmu_update();
			break;
		case 9: // Miscellaneous
			/**/ if (w==0X4045)
			{
				if (equalsmmmm(&cart[0X05350],0X64426553)) // "CROSS BLAIM"
					cart_bank[1]=b,mmu_update();
			}
			else if (!w)//==0X0000
			{
				if (equalsmmmm(&cart[0X1FA74],0X4D453150)) // "SUPER LODE RUNNER"
					cart_bank[1]=b,mmu_update();
			}
			else if (!(w&0X0FFF)) // AFAIK only $6000 and $7000 are used here
			{
				if (equalsmmmm(&cart[0X004E4],0X384C7C78)) // "HARRY FOX: YUKI NO MAOH"
					cart_bank[(w>>12)- 6]=b,mmu_update();
			}
			/*
			else if (w>=0X5FFE&&w<=0X5FFF) // only $5FFE and $5FFF can make the FM-PAC SRAM visible
			{
				if (equalsmmmm(&cart[0X00018],0X50414332)) // "PAC2" <= "PAC2OPLL"
					{ if (sram[w-0X4000]!=b) sram[w-0X4000]=b,mmu_update(); }
			}
			*/
			//else POKE(w)=b; // redundant
			break;
	}
}
BYTE mmu_slowpeek(WORD w) // ditto, we can only reach this function if the MMU_BIT fields are right and the address isn't
{
	/**/ if (w>=0XF000) // SSLOT!
		return (w==0XFFFF&&type_id>(pio_port_a<192?1:0))?~rom_cfg[pio_port_a>>6]:PEEK(w);
	else if (mmu_rom[w>>12]==disc_mapping[w>>14]) switch(w&0X3FFF) // MSX-DISK I/O?
	{
		case 0X3FB8: case 0X3FF8: return diskette_recv_status();
		case 0X3FB9: case 0X3FF9: return diskette_recv_track();
		case 0X3FBA: case 0X3FFA: return diskette_recv_sector();
		case 0X3FBB: case 0X3FFB: return diskette_recv_data();
		// NATIONAL/PANASONIC:
		// port +4 returns the current status:
		// 0X80/0XC0? STOP, 0X00 FEED, 0X40 BUSY
		case 0X3FBC: return diskette_recv_busy()>0?0X00:0X80; // TODO: emulate BUSY
		//case 0X3FBD:
		//case 0X3FBE:
		//case 0X3FBF: return 255; // *!*
		// PHILIPS, SONY, etc:
		// ports +4 and +5 simply reflect the last byte sent to them
		// port +7 returns the current status: 0X80/0X00? STOP, 0X40 FEED, 0XC0 BUSY
		case 0X3FFC: return diskette_side;
		case 0X3FFD: return (diskette_motor<<7)+diskette_drive+64+4;
		//case 0X3FFE: return 255; // *!*
		case 0X3FFF: return diskette_recv_busy()>0?0X40:0X80; // TODO: emulate BUSY
		default: return PEEK(w);
	}
	else if (w<0X8000)
	{
		; // nothing?
	}
	else //(w>=0X8000)
	{
		/**/ if (w>=0X9800&&w<=0X9FFF)
		{
			if (cart_id==2&&!(~cart_bank[2]&63)) // Konami SCC-?
				if (!(sccplus_table[255]&32))
					return sccplus_minus_recv(w);
		}
		else if (w>=0XB800&&w<=0XBFFF)
		{
			if (cart_id==2&&( cart_bank[3]&128)) // Konami SCC+?
				if ( (sccplus_table[255]&32))
					return sccplus_table_recv(w);
		}
	}
	return PEEK(w);
}
#define mmu_poke(w,b) (mmu_slowpoke(w,b))
#define mmu_peek(w) (mmu_slowpeek(w))

// 0XB4,0XB5: RICOH RP-5C01 RTC+CMOS -------------------------------- //

BYTE cmos_table[55+1]={ // default values; remove the "cmos" line in the configuration file to force a reset!
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	10, 0, 0, 0, 8, 2,15, 4, 4, 0, 0, 0, 0, // defaults to "WIDTH 40 (=2*16+8)" and "COLOR 15,4,4"
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0}, // the last nibble is the last value sent to SELECT port; it also makes the length even
	cmos_index=0,cmos_count=0;
BYTE cmos_mask[55+1]={
	15, 7,15, 7,15, 3, 7,15, 3,15, 1,15,15, // block 0
	 0, 0,15, 7,15, 3, 7,15, 3, 0, 3, 3, 0, // block 1
	15,15,15,15,15,15,15,15,15,15,15,15,15, // block 2
	15,15,15,15,15,15,15,15,15,15,15,15,15, // block 3
	15,15,15, 0}; // control

void cmos_table_select(BYTE b) // select entry in CMOS table
	{ cmos_index=(cmos_table[55]=(b&=15))<13?(cmos_table[52]&3)*13+b:52-13+b; }
BYTE cmos_table_recv(void) // request a byte from CMOS table
	{ return cmos_index<55?cmos_table[cmos_index]:0; } // MSX2EXT uses register 26 to test the RTC!
void cmos_table_send(BYTE b) // send a byte to CMOS table
	{ if (cmos_index<55) cmos_table[cmos_index]=b&cmos_mask[cmos_index]; }

BYTE cmos_dayz[16]={0,31,28,31,30,31,30,31,31,30,31,30,31,0,0,0}; // February must NOT be constant!
void cmos_test(void) // update TIMER once per scanline
{
	if (cmos_table[53]&1) // seconds?
		if (++cmos_table[0]>9) // seconds units
			cmos_table[0]=0,cmos_table[1]=(cmos_table[1]+1)&7; // seconds tens
	if (cmos_table[53]&2) // minutes?
		if (++cmos_table[2]>9) // minutes units
			cmos_table[2]=0,cmos_table[3]=(cmos_table[3]+1)&7; // minutes tens
	if (cmos_table[53]&4) // hours?
		if (++cmos_table[4]>9) // hours units
			cmos_table[4]=0,cmos_table[5]=(cmos_table[5]+1)&3; // hours tens
	if (cmos_table[53]&8) // days?
		if (++cmos_table[7]>9) // days units
			cmos_table[7]=0,cmos_table[8]=(cmos_table[8]+1)&3; // days tens
}
void cmos_tick(void) // update CLOCK once every second
{
	if (++cmos_table[0]>9) // seconds units
		if (cmos_table[0]=0,++cmos_table[1]>5) // seconds tens
			if (cmos_table[1]=0,++cmos_table[2]>9) // minutes units
				if (cmos_table[2]=0,++cmos_table[3]>5) // minutes tens
				{
					if (cmos_table[3]=0,++cmos_table[4]>9) cmos_table[4]=0,++cmos_table[5]; // hours units + tens
					if (cmos_table[5]>=2&&cmos_table[4]>=4) // end of the day? (AM-PM mode isn't used in the MSX)
					{
						if (cmos_table[4]=cmos_table[5]=0,++cmos_table[6]>7) cmos_table[6]=0; // day of the week
						if (++cmos_table[7]>9) cmos_table[7]=0,++cmos_table[8]; // day of the month
						BYTE y=cmos_table[12]*10+cmos_table[11]; cmos_dayz[2]=(y&3)?28:29; // 100 years = Julian calendar; should `y&3` be cmos_table[24]?
						if (cmos_table[8]*10+cmos_table[7]>cmos_dayz[cmos_table[10]*10+cmos_table[9]])  // end of the month?
						{
							if (cmos_table[7]=1,cmos_table[8]=0,++cmos_table[9]>9) // months units
								cmos_table[9]=0,++cmos_table[10]; // months tens
							if (cmos_table[10]>=1&&cmos_table[9]>=2) // end of the year?
								if (cmos_table[9]=1,cmos_table[10]=0,++cmos_table[11]>9)  // years units
									if (cmos_table[11]=0,++cmos_table[12]>9) // years tens
										cmos_table[12]=0; // 100 years = 00..99
						}
					}
				}
}

#define cmos_export(t) (nibble2hexa0((t),cmos_table,55)) // export CMOS into the string `t`; returns `t`
#define cmos_import(s) (hexa2nibble(cmos_table,(s),55)) // import the string `t` into CMOS; 0 OK, !0 ERROR
#define cmos_shrink(t) (nibble2byte((t),cmos_table,28)) // shrink CMOS data into a 28-byte block
#define cmos_expand(s) (byte2nibble(cmos_table,(s),28)) // expand a 28-byte block into CMOS data

// 0X98,0X99,0X9A,0X9B: TMS9918/V9938/V9958 VDP --------------------- //

BYTE vdp_ram[8<<14]; // 128K in MSX2, but only the first 16K are used in MSX1
BYTE vdp_table[64],vdp_latch,vdp_state[16],vdp_palette[32]; int vdp_where,vdp_which,vdp_smash;
//const BYTE vdp_palette0[32]={0X00,0,0X00,0,0X11,6,0X33,7,0X17,1,0X27,3,0X51,1,0X27,6,0X71,1,0X73,3,0X61,6,0X64,6,0X11,4,0X65,2,0X55,5,0X77,7}; // MSX2 MSX1-like palette
const BYTE vdp_palette7[32]={0X00,0,0X03,0,0X30,0,0X33,0,0X00,3,0X03,3,0X30,3,0X33,3,
	0X73,4,0X07,0,0X70,0,0X77,0,0X00,7,0X07,7,0X70,7,0X77,7}; // MSX2 G7 sprite palette

// VDP maps: PATTERN NAME X2, COLOUR TABLE X2, PATTERN GENERATOR X2, SPRITE ATTRIBUTE X2, SPRITE GENERATOR
BYTE *vdp_map_pn1,*vdp_map_pn2,*vdp_map_ct1,*vdp_map_ct2,*vdp_map_pg1,*vdp_map_pg2,*vdp_map_sa1,*vdp_map_sa2,*vdp_map_sa7,*vdp_map_sg1,*vdp_map_sg7;
int vdp_bit_pg2,vdp_bit_ct2,vdp_bit_bmp,vdp_bit_bm1,vdp_map_k32,vdp_map_bm4,vdp_map_bm8; // MSX2 additional values
BYTE vdp_memtype; // 0 in MSX1 video modes (16K linear), 1 in MSX2 SPRITE MODE 2 modes minus G6/G7 (128K linear), 2 in MSX2 G6/G7 (128K planar)
BYTE vdp_spritel,vdp_sprite1; // sprite length and mask
char vdp_legalsprites=0,vdp_finalsprite=32,vdp_impactsprite=32; // sprite limits and options
int vdp_finalraster; // MSX1: 192 always; MSX2: 192 or 212
int vdp_flash=0; // counter for ODD and EVEN blinking effects
BYTE vdp_table_last=99; // last modified register, for debug purposes
#define video_clut_r3g3b3(v,i) (video_xlat_rgb(video_table[16+( v[i*2+1]    &7)]+video_table[24+((v[i*2+0]>>4)&7)]+video_table[32+( v[i*2+0]    &7)]))
void video_xlat_clut(void) // update the entire colour palette according to user choice and VDP modes
{
	for (int i=0;i<16;++i)
		video_clut[i+ 0]=type_id?video_clut_r3g3b3(vdp_palette,i):video_xlat[i]; // palette is dynamic in MSX2 and built-in in MSX1
}
void video_wide_xlat(void) // update the static high-colour palettes according to user choice alone
{
	for (int i=0;i<16;++i) // MSX2: V9938 G7 mode (sprite)
		video_clut[i+16]=video_clut_r3g3b3(vdp_palette7,i);
	for (int i=0;i<256;++i) // MSX2: V9938 G7 mode (bitmap)
		video_clut[i+32]=video_xlat_rgb(video_table[(i>>5)+16]+video_table[((i>>2)&7)+24]+ // GREEN and RED are straightforward 0..7
			video_table[(i&3)*7/3+32]); // BLUE: 0/1/2/3 => 0/2/4/7 (NOT 0/2/5/7! f.e. the MSX2 "ABADIA" title)
	if (type_id>1) for (char y=0;y<32;++y) for (char j=0;j<64;++j) for (char k=0;k<64;++k) // MSX2P: V9958 lossy 15-bit colour modes YJK and YAE
	{
		char r=j<32?j:j-64,g=k<32?k:k-64,b=(y*5-r*2-g)>>2; if (b<0) b=0; else if (b>31) b=31;
		if ((g+=y)<0) g=0; else if (g>31) g=31; if ((r+=y)<0) r=0; else if (r>31) r=31;
		video_wide_clut[(y<<12)+(j<<6)+k]=video_xlat_rgb((((r*33)>>2)<<16)+(((g*33)>>2)<<8)+((b*33)>>2));
	}
	//FILE *f; if (f=fopen("wideclut","wb")) fwrite1(video_wide_clut,sizeof(video_wide_clut),f),fclose(f);
}

#define video_clut_modify(i) (video_clut[i]=video_clut_r3g3b3(vdp_palette,i)) // update entry `i` (0..15) in the MSX2 VDP palette
#define vdp_next_where() do{ if (!(vdp_where=(vdp_where+1)&0X3FFF)&&vdp_memtype) vdp_table[14]=(vdp_table[14]+1)&7; }while(0)

BYTE vdp_raster_mode=0; // the video mode (0..31) and its flags (+32 sprites, +64 border!)
#define VDP_BLIT_OVER() (vdp_blit_t|=-8*2) // the blitter stepping is always 8 T long
#define VDP_BLIT_GAIN(t) (t) // *!* TODO: fine-tune the blitter budget calculations
#define VDP_BLIT_PAIN(t) (vdp_blit_t-=(t)*2) // yes, it's all pain and gain...
int vdp_blit_budget; // blitter budget in MSX2 modes where the blitter is active
void vdp_mode_update(void) // recalculate video mode and blitter budget on each new scanline
{
	vdp_raster_mode=((vdp_table[0]>>1)&7)+(vdp_table[1]&24); // dumb way to store the mode in a byte: 0..5 are G1..G6, 7 is G7, 8 is MC, 16 is T1, 18 is T2.
	if ((~vdp_table[1]|vdp_state[2])&64) // screen is off? border is on?
		vdp_raster_mode+=64, // draw border! (screen can be disabled)
		vdp_blit_budget=12; // best blitter performance
	else // notice that some definitions are redundant: the blitter only operates in G4-G7 modes
	{
		if ((vdp_raster_mode&16)|(vdp_table[8]&2)) // mode is T1 or T2? sprites are disabled?
			vdp_blit_budget=VDP_BLIT_GAIN(11); // average blitter performance
		else
			vdp_raster_mode+=32, // +32 means sprites are enabled in this line
			vdp_blit_budget=VDP_BLIT_GAIN(10); // worst blitter performance
	}
}

void vdp_reload(void) // recalculate values that aren't exclusively tied to vdp_table[]
{
	vdp_legalsprites=vdp_legalsprites<32?vdp_memtype?8:4:32;
	// alternating even/odd screens? "MAZE OF GALIOUS MSX2" uses this in water screens!
	vdp_map_k32=((vdp_table[9]&4)?vdp_state[2]&2:vdp_flash<0)?~0X8000:~0; // see vdp_frame()
}
void vdp_update(void) // recalculate bitmap and sprite values
{
	if ((vdp_table[1]&24)==0&&(vdp_table[0]&10)==10) // i.e. if ((vdp_raster_mode&29)==5) ...
		vdp_memtype=2; // G6/G7: memory is planar!
	else vdp_memtype=(vdp_table[0]&12)&&!(vdp_table[1]&24); // memory is linear
	int i=((vdp_table[11]&  3)<<15)+(vdp_table[5]<<7);
	vdp_map_sa1=&vdp_ram[i]; // SPRITES MODE 1
	vdp_map_sa2=&vdp_ram[i&0X1FC00]; // MODE 2
	vdp_map_sa7=&vdp_ram[(i&0X1FC00)>>1]; // PLANAR
	vdp_map_sg1=&vdp_ram[(vdp_table[ 6]& 63)<<11]; // LINEAR
	vdp_map_sg7=&vdp_ram[(vdp_table[ 6]& 63)<<10]; // PLANAR
	if (vdp_table[1]&2) // sprite size and mask
		vdp_spritel=16,vdp_sprite1=-4;
	else
		vdp_spritel= 8,vdp_sprite1=-1;
	vdp_finalraster=vdp_table[9]<128?192:212;
	// PATTERN NAME
	i=((vdp_table[ 2]&127)<<10);
	vdp_bit_bm1=i&1024;
	vdp_map_pn1=&vdp_ram[i&-2048];
	vdp_map_pn2=&vdp_ram[i&-0X2000];
	// COLOUR TABLE
	i=((vdp_table[10]&  7)<<14)+(vdp_table[3]<<6);
	vdp_map_ct1=&vdp_ram[i];
	vdp_map_ct2=&vdp_ram[i&-0X2000];
	// PATTERN GENERATOR
	i=((vdp_table[ 4]& 63)<<11);
	vdp_map_pg1=&vdp_ram[i];
	vdp_map_pg2=&vdp_ram[i&-0X2000];
	// MSX1 G2 + MSX2 G3
	vdp_bit_ct2=((vdp_table[3]&127)<< 6)+  63;
	vdp_bit_pg2=((vdp_table[4]&  3)<<11)+2047;
	// MSX2 MODES
	vdp_bit_bmp=(vdp_table[2]&31)*8+7;
	vdp_map_bm4=(vdp_table[2]&96)<<10;
	vdp_map_bm8=(vdp_table[2]&32)<<10;
	vdp_reload();
}
void vdp_frame(void)
{
	if (vdp_flash<0) // update blink and flash attributes, once per frame
		{ if (!++vdp_flash) vdp_flash=(vdp_table[13]>>4)*10; }
	else
		{ if (--vdp_flash<=0) vdp_flash=(vdp_table[13]&15)*-10; }
	if (video_interlaces) // MSX2 HI-RES G4-G7: use the runtime's implicit scanlines as the reference
		vdp_state[2]|=+2;
	else
		vdp_state[2]&=~2;
}
void vdp_reset(void)
{
	MEMZERO(vdp_ram); MEMZERO(vdp_state); MEMZERO(vdp_table);
	vdp_state[2]=0X0C; // bits 2 and 3 are always set!
	vdp_state[6]=0XFC; // set all unused bits
	vdp_state[4]=vdp_state[9]=0XFE; // set all unused bits
	//vdp_table[2]=vdp_table[3]=vdp_table[4]=vdp_table[5]=0XFF;//???
	//vdp_table[1]=0X10; // machine launches in 40x24 text mode!?
	const BYTE p0[32]={ 0x00,0,0x00,0,0x11,6,0x33,7,0x17,1,0x27,3,0x51,1,0x27,6,0x71,1,0x73,3,0x61,6,0x64,6,0x11,4,0x65,2,0x55,5,0x77,7};
	MEMLOAD(vdp_palette,p0); // we don't need to explicitly reset the blitter: if (!table[46]&&!(status[1]&1)) the blitter is completely asleep
	vdp_map_k32=vdp_palette[0]=vdp_where=vdp_smash=0; vdp_which=-1; vdp_update(); vdp_mode_update(); video_xlat_clut();
}
BYTE vdp_ram_send(BYTE b)
{
	int i=((vdp_table[14]&7)<<14)+vdp_where; // linear offset
	if (vdp_memtype>1) i=((i>>1)+(i<<16))&0X1FFFF; // planar modes (G6, G7 and their YJK/YAE variants) are different!
	vdp_ram[i]=b; vdp_next_where(); return b;
}
BYTE vdp_ram_recv(void)
{
	int i=((vdp_table[14]&7)<<14)+vdp_where; // linear offset
	if (vdp_memtype>1) i=((i>>1)+(i<<16))&0X1FFFF; // planar modes (G6, G7 and their YJK/YAE variants) are different!
	i=vdp_ram[i]; vdp_next_where(); return i;
}

// The MSX2 is equipped with a blitter: it receives parameters and commands
// through the registers #32..#46, and runs in parallel to the Z80.

// s* = source, d* = destination, n* = number of dots, z* = logical limits
unsigned int vdp_blit_nx; // they MUST overflow! ZERO always means MAXIMUM!
unsigned int vdp_blit_sx,vdp_blit_dx; // SY and DY are kept in their registers!
int vdp_blit_nz,vdp_blit_t; // counter (used by command LINE and Z80-to-VDP waits) and timer (where operations take their clock ticks)
INT8 vdp_blit_ax,vdp_blit_ay; // step (add to value), either +1 or -1
INT8 vdp_blit_case,vdp_blit_addx; BYTE vdp_blit_step,vdp_blit_bits,vdp_blit_mask; // precalc's that avoid repeating the same calculations
int vdp_blit_xl,vdp_blit_yl,vdp_blit_xh,vdp_blit_yh; // AND (255/511/1023) and NAND (-256/-512/-1024) masks of the screen coordinates

int vdp_blit_update(void) // set MASK, GAPS and BITS the current mode; >=0 OK, <0 ERROR
{
	vdp_blit_ay=(vdp_table[45]&8)?-1:1; vdp_blit_ax=(vdp_table[45]&4)?-1:1;
	switch (vdp_raster_mode&31) // only G4 to G7 are valid in the V9938; other modes can be optionally handled by the V9958 as G7(G6?)
	{
		case 3: vdp_blit_xh=~(vdp_blit_xl=255); vdp_blit_yh=~(vdp_blit_yl=1023); vdp_blit_mask= 15;
			vdp_blit_addx=vdp_blit_ax*(vdp_blit_step=2); return vdp_blit_case=0; // G4: 256x4-bit
		case 4: vdp_blit_xh=~(vdp_blit_xl=511); vdp_blit_yh=~(vdp_blit_yl=1023); vdp_blit_mask=  3;
			vdp_blit_addx=vdp_blit_ax*(vdp_blit_step=4); return vdp_blit_case=1; // G5: 512x2-bit
		case 5: vdp_blit_xh=~(vdp_blit_xl=511); vdp_blit_yh=~(vdp_blit_yl= 511); vdp_blit_mask= 15;
			vdp_blit_addx=vdp_blit_ax*(vdp_blit_step=2); return vdp_blit_case=2; // G6: 512x4-bit
		default:
			if (!(vdp_table[25]&64)) break; // the V9958 can handle modes G1, G2 and G3 if this bit (CMD) is set
		case 6: // ?? equivalent to G7?
		case 7: vdp_blit_bits=0; // this is always zero in G7
			vdp_blit_xh=~(vdp_blit_xl=255); vdp_blit_yh=~(vdp_blit_yl= 511); vdp_blit_mask=255;
			vdp_blit_addx=vdp_blit_ax*(vdp_blit_step=1); return vdp_blit_case=3; // G7: 256x8-bit
	}
	vdp_table[46]&=15,vdp_state[2]&=126; // illegal mode! cancel everything!
	return vdp_blit_case=-1; // the user shouldn't change modes during operation... let's hope he doesn't!
}
BYTE *vdp_blit_offs(int x,int y) // get current offset from X and Y; they must be within their valid ranges!
{
	switch (vdp_blit_case)
	{
		case 0: vdp_blit_bits=(1&~x)<<2; return &vdp_ram[(y<<7)+(x>>1)]; // linear
		case 1: vdp_blit_bits=(3&~x)<<1; return &vdp_ram[(y<<7)+(x>>2)]; // linear
		case 2: vdp_blit_bits=(1&~x)<<2; return &vdp_ram[(y<<7)+(x>>2)+((x&2)<<15)]; // planar
		default: // this never happens, but we must satisfy the compiler
		case 3: return &vdp_ram[(y<<7)+(x>>1)+((x&1)<<16)]; // planar
	}
}
void vdp_blit_logo(int x,int y,BYTE b) // perform a logical operation on a pixel (LINE, PSET, LOGICAL MOVE); once again, X and Y must be within range!
{
	if ((b&=vdp_blit_mask)||!(vdp_table[46]&8)) // TIMP, TAND, TOR, TXOR and TNOT do nothing if `b` is ZERO!
	{
		BYTE *o=vdp_blit_offs(x,y); // this sets `vdp_blit_bits`!
		BYTE m=vdp_blit_mask<<vdp_blit_bits;
		switch (vdp_table[46]&7)
		{
			case 1: *o&=(~m)+(b<<vdp_blit_bits); break; // AND
			case 2: *o|=(b<<vdp_blit_bits); break; // OR
			case 3: *o^=(b<<vdp_blit_bits); break; // XOR
			default: // all +4 are NOT!?
			case 4: b=vdp_blit_mask&~b; // NOT: no `break`! f.e. menu of "DAISENRYAKU"
			case 0: *o=(*o&~m)+(b<<vdp_blit_bits); // IMP
		}
	}
}
#define vdp_blit_test(x,y) ((*vdp_blit_offs(x,y))>>vdp_blit_bits)

#define VDP_BLIT_GET_SX() (((vdp_table[33]<<8)+vdp_table[32])&vdp_blit_xl)
#define VDP_BLIT_GET_SY() (((vdp_table[35]<<8)+vdp_table[34])&vdp_blit_yl)
#define VDP_BLIT_GET_DX() (((vdp_table[37]<<8)+vdp_table[36])&vdp_blit_xl)
#define VDP_BLIT_GET_DY() (((vdp_table[39]<<8)+vdp_table[38])&vdp_blit_yl)
#define VDP_BLIT_GET_NX() (((vdp_table[41]<<8)+vdp_table[40])&vdp_blit_xl)
#define VDP_BLIT_GET_NY(v) (((vdp_table[43]<<8)+vdp_table[42]+(v))&vdp_blit_yl)
#define VDP_BLIT_SET_SY(v) (vdp_table[35]=((v)>>8)&3,vdp_table[34]=(v))
#define VDP_BLIT_SET_DY(v) (vdp_table[39]=((v)>>8)&3,vdp_table[38]=(v))
#define VDP_BLIT_SET_NY(v) (vdp_table[43]=((v)>>8)&3,vdp_table[42]=(v))
#define VDP_BLIT_EXIT_X(v) (vdp_state[4]=((v)>>8)|254,vdp_state[3]=(v))
#define VDP_BLIT_EXIT_Y(v) (vdp_state[6]=((v)>>8)|252,vdp_state[5]=(v))
#define VDP_BLIT_EXIT_Z(v) (vdp_state[9]=((v)>>8)|254,vdp_state[8]=(v))

void vdp_blit_launch(void) // tells the blitter to start a new command!
{
	if (vdp_blit_update()<0) // set the internal options according to the mode... or exit if the mode's wrong!
		vdp_table[46]&=15; // force STOP!
	vdp_state[2]=(vdp_state[2]&110)+1; // BUSY (+1) but not QUEUE (+128) or FOUND (+16)!
	vdp_blit_sx=VDP_BLIT_GET_SX();
	vdp_blit_dx=VDP_BLIT_GET_DX();
	if (vdp_table[46]<128) // LINE is independent of the video mode!
		vdp_blit_nz=((vdp_blit_nx=mgetii(&vdp_table[40])&1023)-1)>>1;
	else
		vdp_blit_nx=VDP_BLIT_GET_NX(),vdp_blit_nz=0; // C-BIOS2 abuses N (C0: 000x0212 and F0: 000x0085)
	if (session_shift) cprintf("%08X: VDP=%02X:%02X S=%04d,%04d D=%04d,%04d N=%04dx%04d\n",z80_pc.w,vdp_table[46],vdp_table[45],
		vdp_blit_sx,VDP_BLIT_GET_SY(),vdp_blit_dx,VDP_BLIT_GET_DY(),vdp_blit_nx,VDP_BLIT_GET_NY(0));
}
void vdp_blit_main(int t) // warning: `t` is defined in 3.58 MHz clock ticks, but the blitter runs at x6! (228 Z80 T vs 1368 VDP T)
{
	vdp_blit_t+=t*vdp_blit_budget; while (vdp_blit_t>=0) // perform one step of blitter command logic until we run out of time budget
	{
		if (!(vdp_state[2]&1)) // stopped?
			VDP_BLIT_OVER(), // cancel time budget!
			vdp_table[46]&=15;//,vdp_state[2]&=127; // "Andorogynus" proves that we must NOT reset QUEUE here, but in vdp_state_recv(); see below!
		else switch (vdp_table[46]>>4)
		{
			int s,d,n; // temporary regs for SY, DY and NY
			case 15: // HMMC
				VDP_BLIT_PAIN(8); // always faster than OUT ($NN),A!
				if (vdp_blit_nz) // not the first time?
					{ VDP_BLIT_OVER(); break; } // wait for Z80 SEND!
				d=VDP_BLIT_GET_DY();
				*vdp_blit_offs(vdp_blit_dx,d)=vdp_table[44]; // copy one byte!
				vdp_blit_dx+=vdp_blit_addx;
				if ((vdp_blit_nx-=vdp_blit_step)<vdp_blit_step||(vdp_blit_dx&vdp_blit_xh)) // horizontal end?
					goto go_to_exit15;
				vdp_blit_nz=vdp_state[2]|=128; // QUEUE!
				break;
			case 14: // YMMM*
				VDP_BLIT_PAIN(40+24);
				s=VDP_BLIT_GET_SY();
				d=VDP_BLIT_GET_DY();
				*vdp_blit_offs(vdp_blit_dx,d)=*vdp_blit_offs(vdp_blit_dx,s); // copy one byte!
				if ((vdp_blit_dx+=vdp_blit_addx)&vdp_blit_xh) // horizontal end?
					goto go_to_exit14;
				break;
			case 13: // HMMM
				VDP_BLIT_PAIN(64+24);
				s=VDP_BLIT_GET_SY();
				d=VDP_BLIT_GET_DY();
				*vdp_blit_offs(vdp_blit_dx,d)=*vdp_blit_offs(vdp_blit_sx,s); // copy one byte!
				vdp_blit_sx+=vdp_blit_addx;
				vdp_blit_dx+=vdp_blit_addx;
				if ((vdp_blit_nx-=vdp_blit_step)<vdp_blit_step||((vdp_blit_sx|vdp_blit_dx)&vdp_blit_xh)) // horizontal end?
					goto go_to_exit13;
				break;
			case 12: // HMMV
				VDP_BLIT_PAIN(48);
				d=VDP_BLIT_GET_DY();
				*vdp_blit_offs(vdp_blit_dx,d)=vdp_table[44]; // copy one byte!
				vdp_blit_dx+=vdp_blit_addx;
				if ((vdp_blit_nx-=vdp_blit_step)<vdp_blit_step||(vdp_blit_dx&vdp_blit_xh)) // horizontal end?
				{
					VDP_BLIT_PAIN(56);
					goto go_to_exit12;
				}
				break;
			case 11: // LMMC
				VDP_BLIT_PAIN(8); // always faster than OUT ($NN),A!
				if (vdp_blit_nz) // not the first time?
					{ VDP_BLIT_OVER(); break; } // wait for Z80 SEND!
				d=VDP_BLIT_GET_DY();
				vdp_blit_logo(vdp_blit_dx,d,vdp_table[44]);
				vdp_blit_dx+=vdp_blit_ax;
				if (!--vdp_blit_nx||(vdp_blit_dx&vdp_blit_xh)) // horizontal end?
				{
					go_to_exit15: // *!* GOTO!
					vdp_blit_dx=VDP_BLIT_GET_DX();
					vdp_blit_nx=VDP_BLIT_GET_NX();
					d+=vdp_blit_ay; VDP_BLIT_SET_DY(d);
					n=VDP_BLIT_GET_NY(-1); VDP_BLIT_SET_NY(n);
					if (!n||(d&vdp_blit_yh)) // vertical end?
						{ --vdp_state[2]; break; } // stopping! skip QUEUE tho'!
				}
				vdp_blit_nz=vdp_state[2]|=128; // QUEUE!
				break;
			case 10: // LMCM
				VDP_BLIT_PAIN(8); // always faster than OUT ($NN),A!
				vdp_state[2]|=128; // QUEUE!
				VDP_BLIT_OVER(); break; // wait for Z80 RECV!
			case  9: // LMMM
				VDP_BLIT_PAIN(64+32+24);
				s=VDP_BLIT_GET_SY();
				d=VDP_BLIT_GET_DY();
				vdp_blit_logo(vdp_blit_dx,d,vdp_blit_test(vdp_blit_sx,s)); // paint a pixel!
				vdp_blit_sx+=vdp_blit_ax;
				vdp_blit_dx+=vdp_blit_ax;
				if (!--vdp_blit_nx||((vdp_blit_sx|vdp_blit_dx)&vdp_blit_xh)) // horizontal end?
				{
					go_to_exit13: // *!* GOTO!
					VDP_BLIT_PAIN(64);
					vdp_blit_sx=VDP_BLIT_GET_SX();
					vdp_blit_nx=VDP_BLIT_GET_NX();
					go_to_exit14: // *!* GOTO!
					vdp_blit_dx=VDP_BLIT_GET_DX();
					s+=vdp_blit_ay; VDP_BLIT_SET_SY(s);
					d+=vdp_blit_ay; VDP_BLIT_SET_DY(d);
					n=VDP_BLIT_GET_NY(-1); VDP_BLIT_SET_NY(n);
					if (!n||((s|d)&vdp_blit_yh)) // vertical end?
						--vdp_state[2]; // stopping!
				}
				break;
			case  8: // LMMV
				VDP_BLIT_PAIN(72+24);
				d=VDP_BLIT_GET_DY();
				vdp_blit_logo(vdp_blit_dx,d,vdp_table[44]); // paint a pixel!
				vdp_blit_dx+=vdp_blit_ax;
				if (!--vdp_blit_nx||(vdp_blit_dx&vdp_blit_xh)) // horizontal end?
				{
					VDP_BLIT_PAIN(64);
					go_to_exit12: // *!* GOTO!
					vdp_blit_dx=VDP_BLIT_GET_DX();
					vdp_blit_nx=VDP_BLIT_GET_NX();
					d+=vdp_blit_ay; VDP_BLIT_SET_DY(d);
					n=VDP_BLIT_GET_NY(-1); VDP_BLIT_SET_NY(n);
					if (!n||(d&vdp_blit_yh)) // vertical end?
						--vdp_state[2]; // stopping!
				}
				break;
			case  7: // LINE ftr. Bresenham's algorithm
				VDP_BLIT_PAIN(88+24);
				d=VDP_BLIT_GET_DY();
				vdp_blit_logo(vdp_blit_dx,d,vdp_table[44]);
				if (!vdp_blit_nx) // major end? (the decrement is done later: there are nx+1 major points in a line)
					{ --vdp_state[2]; break; } // stopping! quit now!
				if (vdp_table[45]&1) // MAJOR is Y?
					d+=vdp_blit_ay;
				else // MAJOR is X!
					vdp_blit_dx+=vdp_blit_ax;
				if ((vdp_blit_nz-=(mgetii(&vdp_table[42])&511))<0) // minor end?
				{
					VDP_BLIT_PAIN(32);
					vdp_blit_nz+=mgetii(&vdp_table[40])&1023; // not VDP_BLIT_GET_NX()!
					if (vdp_table[45]&1) // MINOR is X?
						vdp_blit_dx+=vdp_blit_ax;
					else // MINOR is Y!
						d+=vdp_blit_ay;
				}
				VDP_BLIT_SET_DY(d);
				if ((vdp_blit_dx&vdp_blit_xh)||(d&vdp_blit_yh)) // overflow?
					--vdp_state[2]; // stopping!
				else
					--vdp_blit_nx; // decrement!
				break;
			case  6: // SRCH, essential to the in-game map of KOEI'S "NOBUNAGA NO YABOU 1991"!
				t=(vdp_blit_test(vdp_blit_sx,VDP_BLIT_GET_SY())^vdp_table[44])&vdp_blit_mask;
				if ((vdp_table[45]&2)?t:!t) // a matching pixel?
					VDP_BLIT_EXIT_Z(vdp_blit_sx),
					vdp_state[2]=(vdp_state[2]&110)+16; // STOP + SUCCESS!
				else if ((vdp_blit_sx+=vdp_blit_ax)&vdp_blit_xh) // horizontal end?
					VDP_BLIT_EXIT_Z(vdp_blit_sx),
					vdp_state[2]&=110; // STOP w/o SUCCESS!
				break;
			case  5: // PSET
				vdp_blit_logo(vdp_blit_dx,VDP_BLIT_GET_DY(),vdp_table[44]);
				--vdp_state[2]; // stopping!
				break;
			case  4: // POINT/TEST
				vdp_state[7]=vdp_blit_test(vdp_blit_sx,VDP_BLIT_GET_SY())&vdp_blit_mask;
				// no `break`!
			default: --vdp_state[2]; // stopping!
		}
	}
}
// the Z80 hits register 44; refresh blitter commands HMMC and LMMC! (15/$F and 11/$B)
#define VDP_BLIT_XMMC() do{ if (vdp_table[46]>=0XF0||(vdp_table[46]>=0XB0&&vdp_table[46]<=0XBF)) vdp_blit_nz=0; }while(0) // announce Z80 SEND!
// the Z80 fetches status 7; refresh blitter command LMCM! (10/$A)
#define VDP_BLIT_LMCM() do{ if (vdp_table[46]>=0XA0&&vdp_table[46]<=0XAF) vdp_blit_lmcm(); }while(0)
void vdp_blit_lmcm(void)
{
	int s=VDP_BLIT_GET_SY();
	vdp_state[2]&=127; // not QUEUE!
	vdp_state[7]=vdp_blit_test(vdp_blit_sx,s)&vdp_blit_mask;
	vdp_blit_sx+=vdp_blit_ax;
	if (!--vdp_blit_nx||(vdp_blit_sx&vdp_blit_xh)) // horizontal end?
	{
		vdp_blit_sx=VDP_BLIT_GET_SX();
		vdp_blit_nx=VDP_BLIT_GET_NX();
		s+=vdp_blit_ay; VDP_BLIT_SET_SY(s);
		int n=VDP_BLIT_GET_NY(-1); VDP_BLIT_SET_NY(n);
		if (!n||(s&vdp_blit_yh)) // vertical end?
			--vdp_state[2]; // stopping!
	}
}

// `vdp_count_x` is effectively identical to `video_pos_x`!
int vdp_count_y=0,vdp_raster=-64; // worst possible cases by default

void vdp_table_send(BYTE r,BYTE b) // send byte `b` to register `r` (0-63, although only 0-7 are valid in MSX1 and 0-46 on MSX2)
{
	if (!type_id) // fewer and shorter registers in MSX1!
		{ const char x[8]={2,-5,15,-1,7,127,7,-1}; b&=x[r&=7]; }
	BYTE z; if ((vdp_table_last=r)<47) switch (z=vdp_table[r],vdp_table[r]=b,r)
	{
		case  0:
			if (!(b&16)) // IE1 line event IRQ?
				z80_irq&=+1;//~2
			if (z!=b) vdp_update(); // reduce clobbering
			break;
		case  1:
			if (vdp_state[0]&128) // IE0 frame event IRQ?
			{
				if (b&32)
					z80_irq|=+1;
				else
					z80_irq&=+2;//~1
			}
			// no `break`!
		case  2: // PN
		case  3: // CT
		case  4: // PG
		case  5: // SA
		case  6: // SG
		case  9: // LN
		case 10: // CT2
		case 11: // SA2
		case 14: // 16K
			if (z!=b) vdp_update(); // reduce clobbering
			break;
		case 16:
			vdp_palette[(b&15)*2+0]&=127; // reset special bit 7
			break;
		case 19: // the line event IRQ parameter
			z80_irq&=+1;//~2
			break;
		case 23:
			if (!(vdp_table[0]&16)) z80_irq&=+1;//~2
			break;
		case 25: // V9958 SPECIAL FLAGS
		case 26: // HSCROLL CHAR OFFSET
		case 27: // HSCROLL PIXEL DELAY
			if (type_id<2) vdp_table[r]=0; // V9958 only!
			break;
		case 44:
			if (vdp_state[2]&128) VDP_BLIT_XMMC();
			break;
		case 46:
			vdp_blit_launch();
			//break;
	}
}
BYTE vdp_state_recv(void) // receive byte `b` from status `r` (0-15, although only 0 is valid on MSX1 and 0-9 on MSX2)
{
	BYTE r=vdp_table[15]&15; //if (r>9) return 255; // is this true?
	BYTE b=vdp_state[r]; switch (r) // several registers induce changes
	{
		case 0:
			z80_irq&=+2;//~1
			vdp_state[0]&=31; // reset bits 7 (+128: VERTICAL SCAN INTERRUPT/IE0), 6 (+64: ILLEGAL SPRITE) and 5 (+32: COLLISION)
			break;
		case 1:
			if (type_id>1) b|=+4; // MSX2P: V9958 instead of V9938
			z80_irq&=+1;//~2
			vdp_state[1]&=~1; // reset bit 0 (+1: HORIZONTAL SCAN INTERRUPT/IE1)
			break;
		case 2:
			// from the OpenMSX version log, 2004-09-26 Maarten ter Huurne: "... At end of HMMC and LMMC, CE is reset immediately, but TR is reset the next time S#2 is read. Fixes Andorogynus ..."
			if (!(vdp_state[2]&1)) vdp_state[2]&=127; // TR is bit 7 (+128), CE is bit 0 (+1)
			break;
		case 5:
			vdp_state[3]=vdp_state[5]=0,vdp_state[4]=0XFE,vdp_state[5]=0XFC; // reading status 5 resets 3-6!
			break;
		case 7:
			if (vdp_state[2]&128) VDP_BLIT_LMCM();
			//break;
	}
	return b;
}

// CPU-HARDWARE-VIDEO-AUDIO INTERFACE =============================== //

BYTE vdp_impact[32+256+32]; // the impact bitmap doubles as a sprite priority check and a colour scratch
int vdp_spritetop1=0,vdp_spritetop2=0,vdp_spritetmp[128]; // sprite counter and buffer (0..31 bitmap, 32..63 attrib, 64..95 extend, 96..127 offset)
BYTE vdp_spritepos[32];
void video_sprites_test(BYTE y) // look for illegal sprites in scanline `y`. This happens one scanline in advance!
{
	char m=0,n=0; int i=0,j; MEMZERO(vdp_spritepos); for (;i<vdp_finalsprite;++i)
	{
		if (vdp_memtype)
			{ if ((j=(vdp_memtype>1?vdp_map_sa7[i*2+256]:vdp_map_sa2[i*4+512]))==216) break; } // SPRITE PLANAR? // SPRITE MODE 2?
		else
			if ((j=vdp_map_sa1[i*4+0])==208) break; // SPRITE MODE 1
		BYTE a=y-j; if (vdp_table[1]&1) a>>=1; if (a>=vdp_spritel) // zoom x2? active sprite?
			;
		else if (++n<=vdp_legalsprites)
			vdp_spritepos[i]=a+1;
		else
			{ m=i; break; } // illegal sprite!
	}
	// legal sprite code shared by both modes
	vdp_spritetop1=i; if (m) // illegal sprites (5S); sprite #0 will never be illegal!
		vdp_state[0]=(vdp_state[0]&160)+64+m; // "DRAGON QUEST II MSX2" (title) and "IO" (setup) rely on this!
	else if (i<vdp_finalsprite) vdp_state[0]=(vdp_state[0]&160)+i; // end marker
	else vdp_state[0]=(vdp_state[0]&160)+31; // sprites are okay; notice that we can't tell apart between 31 and 32

}
void video_sprites_calc(void) // fetch the next scanline sprites. Call this early in the scanline!
{
	int i=0,j; MEMZERO(vdp_spritetmp); vdp_spritetop2=vdp_spritetop1;
	if (vdp_memtype) { if (vdp_memtype>1) for (;i<vdp_spritetop1;++i) // SPRITE PLANAR?
	{
		if (!(j=vdp_spritepos[i])) continue;
		--j,j=(j&1)*65536+(j>>1),vdp_spritetmp[64+i]=vdp_map_sa7[i*8+j],j+=(vdp_map_sa7[i*2+257]&vdp_sprite1)*4;
		if (vdp_spritetmp[96+i]=vdp_map_sa7[i*2+256+65536],vdp_spritel>8)
			vdp_spritetmp[32+i]=128<<8,vdp_spritetmp[i]=(vdp_map_sg7[j]<<8)+vdp_map_sg7[j+8]; // bigger sprites!
		else
			vdp_spritetmp[32+i]=128,vdp_spritetmp[i]=vdp_map_sg7[j];
	}
	else for (;i<vdp_spritetop1;++i) // SPRITE MODE 2?
	{
		if (!(j=vdp_spritepos[i])) continue;
		--j,vdp_spritetmp[64+i]=vdp_map_sa2[i*16+j],j=(vdp_map_sa2[i*4+514]&vdp_sprite1)*8+j;
		if (vdp_spritetmp[96+i]=vdp_map_sa2[i*4+513],vdp_spritel>8)
			vdp_spritetmp[32+i]=128<<8,vdp_spritetmp[i]=(vdp_map_sg1[j]<<8)+vdp_map_sg1[j+16]; // bigger sprites!
		else
			vdp_spritetmp[32+i]=128,vdp_spritetmp[i]=vdp_map_sg1[j];
	} }
	else for (;i<vdp_spritetop1;++i) // SPRITE MODE 1
	{
		if (!(j=vdp_spritepos[i])) continue;
		--j,j=(vdp_map_sa1[i*4+2]&vdp_sprite1)*8+j;
		if (vdp_spritetmp[96+i]=vdp_map_sa2[i*4+513],vdp_spritel>8)
			vdp_spritetmp[32+i]=128<<8,vdp_spritetmp[i]=(vdp_map_sg1[j]<<8)+vdp_map_sg1[j+16]; // bigger sprites!
		else
			vdp_spritetmp[32+i]=128,vdp_spritetmp[i]=vdp_map_sg1[j];
	}
}
void video_sprites_draw(VIDEO_UNIT *t,BYTE y) // render the scanline `y` sprites. `t` is the video buffer's EC=1 X=0, NULL if we only have to update pointers and calculate impacts. Call this late in the scanline!
{
	BYTE z=((vdp_table[8]&32)&&vdp_memtype)?16:0; // colour 0 can be visible in some cases of SPRITE MODE 2
	int h=0,i=0,j,k; MEMZERO(vdp_impact); if (vdp_memtype) for (;i<vdp_spritetop2;++i) // SPRITE MODE 2?
	{
		if (j=vdp_spritetmp[i]) // do we have a bitmap?
		{
			BYTE *hh=&vdp_impact[vdp_spritetmp[96+i]],c=vdp_spritetmp[64+i];
			if (!(c&128)) hh+=32; // EARLY CLOCK shifts 32 pixels left
			k=vdp_spritetmp[32+i];
			if (c&64) // render masked sprite?
				if (c=(c&15)+z,vdp_table[1]&1) // zoom x2?
					do
						if (j&k) *hh|=c,hh[1]|=c;
					while (hh+=2,k>>=1);
				else
					do
						if (j&k) *hh|=c;
					while (++hh,k>>=1);
			else // render normal sprite?
				if (c=(c&15)+z,vdp_table[1]&1) // zoom x2?
					do
						if (j&k) { if (*hh) { if (!h) h=hh-vdp_impact; } else *hh=c; if (hh[1]) { if (!h) h=hh-vdp_impact; } else hh[1]=c; }
					while (hh+=2,k>>=1);
				else
					do
						if (j&k) { if (*hh) { if (!h) h=hh-vdp_impact; } else *hh=c; }
					while (++hh,k>>=1);
		}
	}
	else for (;i<vdp_spritetop2;++i) // SPRITE MODE 1
	{
		if (j=vdp_spritetmp[i]) // do we have a bitmap?
		{
			BYTE *hh=&vdp_impact[vdp_map_sa1[i*4+1]],c=vdp_map_sa1[i*4+3];
			if (!(c&128)) hh+=32; // EARLY CLOCK shifts 32 pixels left
			if (!(c&=15)) c=16; //else c+=64; // invisible sprites cause collisions!
			if (k=vdp_spritetmp[32+i],vdp_table[1]&1) // zoom x2?
				do
					if (j&k) { if (*hh) ++h; else *hh=c; if (hh[1]) ++h; else hh[1]=c; }
				while (hh+=2,k>>=1);
			else
				do
					if (j&k) { if (*hh) ++h; else *hh=c; }
				while (++hh,k>>=1);
		}
	}
	// sprite collision code shared by both modes
	if (h) // did any sprites hit each other?
		if (!(vdp_state[0]&32)) // MSX2 keeps the first X and Y
		{
			h+=12,VDP_BLIT_EXIT_X(h),y+=8,VDP_BLIT_EXIT_Y(y); // according to manual
			vdp_state[0]|=vdp_impactsprite; // store impact, nothing if disabled
		}
	if (t) // shall we render actual pixels?
	{
		z+=15; // if `z` was 16, colour 0 becomes visible
		VIDEO_UNIT *v=!(~vdp_table[0]&14)?video_clut+16:video_clut; // G7 sprites use their own palette
		if (t+=64,(vdp_raster_mode&31)==4)
			{ for (i=0;i<256;t+=2,++i) if ((h=vdp_impact[32+i])&z) t[0]=v[(h>>2)&3],t[1]=v[h&3]; } // "G5" limits colours to 2 bits
		else
			{ for (i=0;i<256;t+=2,++i) if ((h=vdp_impact[32+i])&z) t[1]=t[0]=v[h&15]; } // all other modes enable all 4 bit colours
	}
}

// the YAMAHA V9938 handbook:
// STAGE	 VDP	  Z80
// -----------	----	------
// Synchronise	 100	 16.67 \,
// Left Erase	 102	 17.00 |-- 43.00
// Left Border	  56	  9.33 /'
// Display	1024	170.67 \.
// Right Border	  59	  9.83 |- 185.00
// Right Erase	  27	  4.50 /'
// -----------	----	------
// TOTAL	1368	228.00

// if 1 narrow pixel is 2 VDP T:
//		T1/T2	other
// Synchro	  44	  44
// Left Edge	  80	  64
// Display	 480	 512
// Right Edge	  80	  64
// ----------	----	----
// TOTAL	 684	 684

BYTE vdp_raster_add8=0; // this is where (vdp_raster+vdp_table[23])&255 is kept
INLINE void video_main(int t) // render video output for `t` Z80 clock ticks; t is always nonzero!
{
	#define VDP_LIMIT_X_H  44 // HSYNC totally invisible
	#define VDP_LIMIT_X_6  80 // 480-px modes' edge
	#define VDP_LIMIT_X_8  64 // 512-px modes' edge
	#define VDP_LIMIT_X_V 684 // =228*3
	static int vdp_limit_x_l=VDP_LIMIT_X_H+VDP_LIMIT_X_8,vdp_limit_x_r=VDP_LIMIT_X_V-VDP_LIMIT_X_8,vdp_limit_x_z=0,vdp_hscroll_p=0;
	static BYTE a,b,c,n,x,h; // character caches, counter and index
	static VIDEO_UNIT *lz=NULL,pz,rz; static int mz; // the V9958 LEFT MASK location and colour
	// update the BACKDROP and BORDER colours, not always identical; cfr. "TERRORPODS"
	int i=vdp_table[7];
	if ((vdp_raster_mode&31)==7) // MSX2 G7: the border is the whole byte in R#7
		rz=video_clut[i+32];
	else if (vdp_table[8]&32) // reg.8 bit 5 (TP): MSX2 LOGO and parts of METAL LIMIT set it; most games reset it!
		video_clut[0]=video_clut_r3g3b3(vdp_palette,0),rz=video_clut[i&((vdp_raster_mode&31)==4?3:15)]; // "G5" limits borders to 2 colour bits!
	else // inherited from MSX1: backdrop and border are one and the same; ZERO is transparent, in practice BLACK
		rz=video_clut[0]=(i&=15)?video_clut[i]:type_id?video_clut_r3g3b3(vdp_palette,0):video_clut[1]; // MSX2: "OUTRUN" (G3), "CASTLEVANIA" and C-BIOS2 LOGO (G4), MSX2 LOGO (G5)...
	int w=video_pos_x,z=w+t*3+vdp_limit_x_z; // the "goal" we must reach. Remember that each Z80 T becomes 3 UNITS!
	while (w<z)
		if (w<vdp_limit_x_l)
			if (w<VDP_LIMIT_X_H) // HBLANK?
			{
				t=z<VDP_LIMIT_X_H?z:VDP_LIMIT_X_H; // last possible point within block
				i=VDP_LIMIT_X_H-0; if (w<i&&t>=i) // what's the exact value here?
				{
					; // ...insert events within hblank here...
				}
				video_target+=t-w,w=t; // move cursors without drawing
				if (w>=VDP_LIMIT_X_H) // HBLANK OFF event?
				{
					// "IO" expects MSX2 to raise this signal later, but why?
					if (vdp_raster>=-1&&vdp_raster<vdp_finalraster-1)
						video_sprites_calc(),video_sprites_test(vdp_raster_add8);
					vdp_reload();//vdp_update();
					if ((i=(vdp_table[18]&15)*2)&16) i-=32; // horizontal offset is a signed nibble of square pixels!
					if (vdp_raster_mode&16)
						vdp_limit_x_l=VDP_LIMIT_X_H+VDP_LIMIT_X_6-i,
						vdp_limit_x_r=VDP_LIMIT_X_V-VDP_LIMIT_X_6-i;
					else
						vdp_limit_x_l=VDP_LIMIT_X_H+VDP_LIMIT_X_8-i,
						vdp_limit_x_r=VDP_LIMIT_X_V-VDP_LIMIT_X_8-i;
					if (!(vdp_raster_mode&24)) // i.e. modes from 0 ("G1") to 7 ("G7")
						vdp_limit_x_l+=mz=((vdp_table[27])&7)<<1; // V9958 register 27 sets the HORIZONTAL PIXEL DELAY
				}
			}
			else // LEFT BORDER!
			{
				t=z<vdp_limit_x_l?z:vdp_limit_x_l; // last possible point within block
				i=vdp_limit_x_l-0; if (w<i&&t>=i) // what's the exact value here?
				{
					; // ...insert events within left border here...
				}
				if ((frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)/*&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X)*/)
					do VIDEO_NEXT=rz,VIDEO_NEXT=rz; while ((w+=2)<t);
				else
					video_target+=t-w,w=t; // move cursors without drawing
				if (w>=vdp_limit_x_l) // BORDER OFF event?
				{
					if (~vdp_table[0]&16) // is this true? it avoids the sporadic "ZANAC-EX" menu bugs without hurting "METAL LIMIT" parts 2 (punk+bean) and 9 (blocky logo)
						vdp_state[1]&=~1,z80_irq&=+1;//~2
					vdp_state[2]&=~32; // end of horizontal border
					if (!vdp_raster) // start of bitmap?
						vdp_state[2]&=~64;
					else if (vdp_raster==vdp_finalraster) // end of bitmap? (overscan tricks will disable this event!)
					{
						vdp_state[2]|=+64;
						vdp_state[0]|=128; // INT status bit: the IE0 frame event
						if (vdp_table[1]&32) z80_irq|=1; // bit 0 (+1) handles the frame interrupt...
					}
					vdp_mode_update(); // beware, this updates `vdp_raster_mode`
					if ((frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)&&(vdp_table[25]&2))
						pz=rz,lz=video_target-mz; else lz=NULL; // BIT 1 of V9958 register 25 enables the 8-square-pixel LEFT MASK
				}
			}
		else
			if (w<vdp_limit_x_r) // BITMAP?
			{
				t=z<vdp_limit_x_r?z:vdp_limit_x_r; // last possible point within block
				i=vdp_limit_x_r-0; if (w<i&&t>=i) // what's the exact value here?
				{
					; // ...insert events within bitmap here...
				}
				if ((frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)/*&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X)*/)
				{
					if (vdp_raster_mode>=64) // border?
						do VIDEO_NEXT=rz,VIDEO_NEXT=rz; while ((w+=2)<t);
					else
					{
						VIDEO_UNIT p; BYTE y=vdp_raster_add8;
						switch (vdp_raster_mode&31)
						{
							case  8: // MSX1: "MC" (MULTICOLOUR) DOTS
								i=(y>>3)*32; y=(y>>2)&7;
								do
								{
									if (!--n)
									{
										n=8,b=vdp_map_pg1[(vdp_map_pn1[vdp_bit_bm1+i+x]<<3)+y];
										x=(x+1)&31;
									}
									else if (n==4)
										b<<=4;
									VIDEO_NEXT=p=video_clut[b>>4]; VIDEO_NEXT=p;
								}
								while ((w+=2)<t); break;
							case 16: // MSX1: "T1" (TEXT1) 40x6 CHARS
							{
								i=(y>>3)*40; y&=7;
								VIDEO_UNIT p0=video_clut[0],p1=video_clut[vdp_table[7]>>4];
								do
								{
									if (!--n)
									{
										n=6,b=vdp_map_pg1[(vdp_map_pn1[vdp_bit_bm1+i+x]<<3)+y];
										x=(x+1)&63;
									}
									VIDEO_NEXT=p=(b&128)?p1:p0; VIDEO_NEXT=p; b<<=1;
								}
								while ((w+=2)<t); break;
							}
							case 17: // MSX1: T1+G2 HYBRID 40x6 CHARS
							{
								i=(y>>3)*40; int k=(y&192)<<2; y&=7;
								VIDEO_UNIT p0=video_clut[0],p1=video_clut[vdp_table[7]>>4];
								do
								{
									if (!--n)
									{
										n=6,b=vdp_map_pg2[(((k+vdp_map_pn1[vdp_bit_bm1+i+x])<<3)+y)&vdp_bit_pg2];
										x=(x+1)&63;
									}
									VIDEO_NEXT=p=(b&128)?p1:p0; VIDEO_NEXT=p; b<<=1;
								}
								while ((w+=2)<t); break;
							}
							case 18: // MSX2: "T2" (TEXT2) 80x3 CHARS
							{
								i=((y>>3)*80)&2047; y&=7;
								VIDEO_UNIT p0=video_clut[0],p1=video_clut[vdp_table[7]>>4];
								VIDEO_UNIT p3,p2; if (vdp_flash<0) // blink?
									p3=video_clut[vdp_table[12]>>4],p2=video_clut[vdp_table[12]&15];
								else
									p3=p1,p2=p0;
								do
								{
									VIDEO_UNIT p5,p4;
									if (!--n)
									{
										n=3,
										c=vdp_map_ct1[(i+x)>>3]<<(x&7), // blink attributes
										b=vdp_map_pg1[(vdp_map_pn2[i+x]<<3)+y]; // range 0..2047
										x=(x+1)&127;
									}
									if (c&128) p4=p2,p5=p3; else p4=p0,p5=p1; // blink bit?
									VIDEO_NEXT=(b&128)?p5:p4; VIDEO_NEXT=(b& 64)?p5:p4; b<<=2;
								}
								while ((w+=2)<t); break;
							}
							case  0: // MSX1: "G1" (GRAPHIC1) 32x8 CHARS
							{
								i=(y>>3)*32; y&=7; if (vdp_hscroll_p) vdp_hscroll_p=1024;
								do
								{
									if (!--n)
									{
										n=8,a=vdp_map_pn1[(vdp_bit_bm1^vdp_hscroll_p)+i+x],b=vdp_map_pg1[(a<<3)+y],c=vdp_map_ct1[a>>3]; // range 0..1023
										if (!(x=(x+1)&31)) if (h) vdp_hscroll_p^=1024;
									}
									VIDEO_UNIT p0=video_clut[c&15],p1=video_clut[c>>4];
									VIDEO_NEXT=p=(b&128)?p1:p0; VIDEO_NEXT=p; b<<=1;
								}
								while ((w+=2)<t); break;
							}
							case  1: // MSX1: "G2" (GRAPHIC2) 32x8 TILES
							case  2: // MSX2: "G3" (GRAPHIC3) 32x8 TILES
							{
								i=(y>>3)*32; int j,k=(y&192)<<2; y&=7; if (vdp_hscroll_p) vdp_hscroll_p=1024;
								do
								{
									if (!--n)
									{
										n=8,j=((k+vdp_map_pn1[(vdp_bit_bm1^vdp_hscroll_p)+i+x])<<3)+y,
										b=vdp_map_pg2[j&vdp_bit_pg2],c=vdp_map_ct2[j&vdp_bit_ct2];
										if (!(x=(x+1)&31)) if (h) vdp_hscroll_p^=1024;
									}
									VIDEO_UNIT p0=video_clut[c&15],p1=video_clut[c>>4];
									VIDEO_NEXT=p=(b&128)?p1:p0; VIDEO_NEXT=p; b<<=1;
								}
								while ((w+=2)<t); break;
							}
							case  3: // MSX2: "G4" (GRAPHIC4) 256x4-BIT PIXELS
							{
								i=(y&vdp_bit_bmp)<<7; // undoc'd R#2 mask!
								int j=vdp_map_bm4&vdp_map_k32; if (vdp_hscroll_p) vdp_hscroll_p=32768;
								do
								{
									if (!--n)
									{
										n=2,b=vdp_ram[(j^vdp_hscroll_p)+i+x];
										VIDEO_NEXT=p=video_clut[b>>4]; VIDEO_NEXT=p;
										if (!(x=(x+1)&127)) if (h) vdp_hscroll_p^=32768;
									}
									else
									{
										VIDEO_NEXT=p=video_clut[b&15]; VIDEO_NEXT=p;
									}
								}
								while ((w+=2)<t); break;
							}
							case  4: // MSX2: "G5" (GRAPHIC5) 512x2-BIT PIXELS
							{
								i=(y&vdp_bit_bmp)<<7; // undoc'd R#2 mask!
								int j=vdp_map_bm4&vdp_map_k32; if (vdp_hscroll_p) vdp_hscroll_p=32768;
								do
								{
									if (!--n)
									{
										n=2,b=vdp_ram[(j^vdp_hscroll_p)+i+x];
										VIDEO_NEXT=video_clut[ b>>6   ];
										VIDEO_NEXT=video_clut[(b>>4)&3];
										if (!(x=(x+1)&127)) if (h) vdp_hscroll_p^=32768;
									}
									else
									{
										VIDEO_NEXT=video_clut[(b>>2)&3];
										VIDEO_NEXT=video_clut[ b    &3];
									}
								}
								while ((w+=2)<t); break;
							}
							case  5: // MSX2: "G6" (GRAPHIC6) 512x4-BIT PIXELS
							{
								i=(y&vdp_bit_bmp)<<7; // undoc'd R#2 mask?
								int j=vdp_map_bm8&vdp_map_k32; if (vdp_hscroll_p) vdp_hscroll_p=32768;
								do
								{
									y=vdp_ram[(j^vdp_hscroll_p)+i+(x>>1)+(x&1)*65536]; // planar :-(
									VIDEO_NEXT=video_clut[y>>4];
									VIDEO_NEXT=video_clut[y&15];
									if (!(x=(x+1)&255)) if (h) vdp_hscroll_p^=32768;
								}
								while ((w+=2)<t); break;
							}
							case  6: // MSX2: "??" (unused? equivalent to G7?)
							case  7: // MSX2: "G7" (GRAPHIC7) 256x8-BIT PIXELS
							{
								i=(y&vdp_bit_bmp)<<7; // undoc'd R#2 mask?
								int j=vdp_map_bm8&vdp_map_k32; if (vdp_hscroll_p) vdp_hscroll_p=32768;
								if (!(vdp_table[25]&8)) // V9938 256-colour palette
									do
									{
										VIDEO_NEXT=p=video_clut[32+vdp_ram[(j^vdp_hscroll_p)+i+(x>>1)+(x&1)*65536]]; VIDEO_NEXT=p; // planar :-(
										if (!(x=(x+1)&255)) if (h) vdp_hscroll_p^=32768;
									}
									while ((w+=2)<t);
								else if (vdp_table[25]&16) // V9958 YAE hybrid mode
									do
									{
										static int jk=0; static unsigned int yyyy=0;
										if (!--n)
										{
											BYTE *vvvv=&vdp_ram[(j^vdp_hscroll_p)+i+(x>>1)]; // fetch four bytes at once
											n=4,yyyy=(vvvv[65537]<<24)+(vvvv[1]<<16)+(vvvv[65536]<<8)+vvvv[0]; // planar :-(
											jk=((yyyy>>15)&(7<<9))+((yyyy>>10)&(7<<6))+((yyyy>>5)&(7<<3))+(yyyy&7);
										}
										VIDEO_NEXT=p=((y=yyyy>>3)&1)?video_clut[y>>1]:video_wide_clut[(y<<12)+jk]; VIDEO_NEXT=p;
										if (yyyy>>=8,!(x=(x+1)&255)) if (h) vdp_hscroll_p^=32768;
									}
									while ((w+=2)<t);
								else // V9958 YJK high colour; what does vdp_table[25]&32 (VDS) do by the way!?
									do
									{
										static int jk=0; static unsigned int yyyy=0;
										if (!--n)
										{
											BYTE *vvvv=&vdp_ram[(j^vdp_hscroll_p)+i+(x>>1)]; // fetch four bytes at once
											n=4,yyyy=(vvvv[65537]<<24)+(vvvv[1]<<16)+(vvvv[65536]<<8)+vvvv[0]; // planar :-(
											jk=((yyyy>>15)&(7<<9))+((yyyy>>10)&(7<<6))+((yyyy>>5)&(7<<3))+(yyyy&7);
										}
										VIDEO_NEXT=p=video_wide_clut[((yyyy<<9)&(31<<12))+jk]; VIDEO_NEXT=p;
										if (yyyy>>=8,!(x=(x+1)&255)) if (h) vdp_hscroll_p^=32768;
									}
									while ((w+=2)<t);
								break;
							}
							default: // unknown mode (fall back to backdrop)
								p=video_clut[0];
								do
									VIDEO_NEXT=p,VIDEO_NEXT=p;
								while ((w+=2)<t); break;
						}
					}
				}
				else
					video_target+=t-w,w=t; // move cursors without drawing
				if (w>=vdp_limit_x_r) // BORDER ON event?
				{
					// these settings are updated later than it looks
					x=vdp_table[26]&31,n=1,h=(vdp_table[2]&32)&&(vdp_table[25]&1); // V9958 register 26 sets the HORIZONTAL CHARACTER OFFSET
					vdp_hscroll_p=h&&!(vdp_table[26]&32); // reset sub-character count and caracter index!
					switch (vdp_raster_mode&31)
					{
						case 16: // "T2"
							x*=2; break;
						case 3: // "G4"
						case 4: // "G5"
							x*=4; break;
						case 5: // "G6"
						case 6: // "??"
						case 7: // "G7"
							x*=8; break;
					}
					if (vdp_raster_mode>=32&&vdp_raster_mode<64) // sprites aren't allowed in T1+T2 modes
					{
						if ((frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)/*&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X)*/)
						{
							memcpy(session_scratch,video_target-(512+64),sizeof(VIDEO_UNIT[64])); // protect left border!
							video_sprites_draw(video_target-(512+64),vdp_raster_add8);
							memcpy(video_target-(512+64),session_scratch,sizeof(VIDEO_UNIT[64])); // restore left border!
						}
						else
							video_sprites_draw(NULL,vdp_raster_add8); // we don't have to draw anything, but we still have to check the sprites' impacts
					}
					if (lz) for (int zz=16;zz;--zz) *lz++=pz; // V9958 LEFT MASK
					vdp_state[2]|=+32; // horizontal border starts
					if (vdp_raster_add8==vdp_table[19]&&vdp_raster>=0&&vdp_raster<(i18n_ntsc?vdp_table[9]<128?235:245:256)) // trigger BEFORE increase!
					{
						vdp_state[1]|=+1; // FH status bit: the IE1 line event sets this bit when IL==DL
						if (vdp_table[0]&16) z80_irq|=+2; // ...while bit 1 (+2) handles the scanline interrupt.
					}
				}
			}
			else // RIGHT BORDER!
			{
				t=z<VDP_LIMIT_X_V?z:VDP_LIMIT_X_V; // last possible point within block
				i=VDP_LIMIT_X_V-0; if (w<i&&t>=i) // what's the exact value here?
				{
					; // ...insert events within right border here...
				}
				if ((frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)/*&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X)*/)
					do VIDEO_NEXT=rz,VIDEO_NEXT=rz; while ((w+=2)<t);
				else
					video_target+=t-w,w=t; // move cursors without drawing
				if (w>=VDP_LIMIT_X_V) // HBLANK ON event?
				{
					video_pos_x=w; // the video_XXX() routines expect a valid `video_pos_x`
					w-=VDP_LIMIT_X_V,z-=VDP_LIMIT_X_V; // next scanline requires new values!
					// scanline events
					if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y&&frame_pos_y==video_pos_y) video_drawscanline();
					video_nextscanline(w); // scanline event!
					vdp_raster_add8=++vdp_raster+vdp_table[23];
					if (!(cmos_table[52]&8)) cmos_test(); // CMOS TIMER enabled?
					if (++vdp_count_y>=LINES_PER_FRAME) // end of frame?
					{
						vdp_frame(); // refresh VDP registers (clobbering them is a bad idea!)
						if (cmos_table[52]&8) if (++cmos_count>=VIDEO_PLAYBACK) cmos_count=0,cmos_tick(); // CMOS TIMER disabled?
						if ((vdp_raster=vdp_table[18]>>4)&8) vdp_raster-=16; // extend sign; mid-frame changes of R#18 high nibble don't work
						vdp_raster+=(vdp_finalraster-(i18n_ntsc?270:320))>>1; // PAL 64/54 lines above, 192/212 pixels, 4<n<64 lines below; NTSC 37/27 lines above, 192/212 pixels, 4<n<64 lines below
						vdp_count_y=0; video_newscanlines(0,i18n_ntsc?25*2:0); // end of frame!
					}
					else if (vdp_count_y*2==LINES_PER_FRAME) audio_dirty|=sccplus_playing|opll_playing|!playcity_disabled; // force audio updates, possibly redundant
					//opll_frame(); // handle OPLL ADSR
				}
			}
	vdp_limit_x_z=z-(video_pos_x=w);
}

void audio_main(int t) // render audio output for `t` clock ticks; t is always nonzero!
{
	int z=audio_pos_z; // unlike other sound chips, the Konami SCCPLUS requires a fine-grained emulation (f.e. "F1 SPIRIT")
	psg_main(t,((tape_status^tape_output)<<12)+dac_voice); // merge tape signals and beeper: x<<(7+5)=x<<12
	if (z<audio_pos_z)
	{
		#ifdef PSG_PLAYCITY
		if (!playcity_disabled)
			playcity_main(&audio_frame[z*AUDIO_CHANNELS],audio_pos_z-z);
		#endif
		if (sccplus_playing)
			sccplus_main(&audio_frame[z*AUDIO_CHANNELS],audio_pos_z-z);
		else if (opll_playing) // can SCCPLUS and OPLL coexist!?
			opll_main(&audio_frame[z*AUDIO_CHANNELS],audio_pos_z-z);
	}
}

// autorun runtime logic -------------------------------------------- //

BYTE snap_done; // avoid accidents with ^F2, see all_reset()
char autorun_path[STRMAX]="",*autorun_s=NULL,autorun_t=0;
//BYTE autorun_kbd[16]; // automatic keypresses
#define autorun_next() ((void)0) // handle AUTORUN

// Z80-hardware procedures ------------------------------------------ //

// the MSX hands the Z80 a mainly empty data bus value
#define Z80_IRQ_BUS 255
// the MSX doesn't obey the Z80 IRQ ACK signal
#define Z80_IRQ_ACK ((void)0)
int z80_skip; // V9958 can induce delays on the Z80 I/O to prevent buffer overruns
// these delays are "relaxed": z80_sync(N) must NOT be performed with z80_skip+=N

void z80_sync(int t) // the Z80 asks the hardware/video/audio to catch up
{
	static int r=0; main_t+=t;
	vdp_blit_main(t); // it must be pegged to the Z80!
	//diskette_main(t); // ???
	if (tape_enabled&&tape)
		audio_dirty=1/*|=tape_loud*/,tape_main(t); // echo the tape signal thru sound!
	t=(r+=t)>>multi_t; r&=multi_u; // calculate base value of `t` and keep remainder
	if (t>0)
	{
		if (audio_queue+=t,audio_dirty&&audio_required)
			audio_main(audio_queue),audio_dirty=audio_queue=0;
		video_main(t);
	}
}

// the emulator includes two methods to speed tapes up:
// * tape_skipload controls the physical method (disabling realtime, raising frameskip, etc. during tape operation)
// * tape_fastload controls the logical method (detecting tape loaders and feeding them data straight from the tape)
char tape_skipload=1,tape_fastload=0; int tape_skipping=0;

#if 1 // TAPE_FASTLOAD
BYTE z80_tape_index[1<<16]; // full Z80 16-bit cache, 255 when unused
BYTE z80_tape_fastload[][32] = { // codes that read pulses : <offset, length, data> x N) -------------------------------------------------------------- MAXIMUM WIDTH //
/*  0 */ {  -5,   7,0X0C,0X28,0X0A,0XDB,0XA2,0XAB,0XF2,-128,  -9 }, // MSX1 FIRMWARE: $1B25
/*  1 */ {  -0,   2,0XAB,0XF2,-128,  +8,  -0,  14,0X7B,0X2F,0X5F,0X0C,0X10,0XF4,0X79,0XC9,0X00,0X00,0X00,0X00,0X10,0XEC }, // MSX1 FIRMWARE: $1B09
/*  2 */ {  -3,   6,0XD8,0XDB,0XA2,0X07,0X30,0XF7 }, // MSX1 FIRMWARE: $1AC0
/*  3 */ {  -3,   6,0XD8,0XDB,0XA2,0X07,0X38,0XF7 }, // MSX1 FIRMWARE: $1AC9,$1B34
/*  4 */ {  -7,   4,0X04,0X00,0XC8,0X3E,  +3,   5,0X2F,0XA9,0XE6,0X80,0XCA,-128, -14 }, // ZX SPECTRUM FIRMWARE + TOPO
/*  5 */ {  -6,   3,0X04,0XC8,0X3E,  +3,   6,0X1F,0XA9,0XE6,0X40,0X20,0XF4 }, // IBER
/*  6 */ {  -6,  12,0X04,0XC8,0X00,0X00,0XDB,0XA2,0X1F,0XA9,0XE6,0X40,0X28,0XF4 }, // "PACMANIA", "LASER SQUAD"...
/*  7 */ {  -6,   3,0X04,0XC8,0X3E,  +3,   6,0X1F,0XA9,0XE6,0X40,0X28,0XF4 }, // ZYDROLOAD
/*  8 */ {  -6,   3,0X04,0XC8,0X3E,  +3,   7,0X1F,0XA9,0XD8,0XE6,0X40,0X28,0XF3 }, // "TIME SCANNER"
/*  9 */ {  -4,  11,0X04,0XC8,0XDB,0XA2,0X1F,0X1F,0XA9,0XE6,0X20,0X28,0XF5 }, // RED POINT
/* 10 */ {  -6,   3,0X04,0XC8,0X3E,  +3,   5,0X2F,0XA9,0XE6,0X80,0XCA,-128, -13 }, // "SURVIVOR"
/* 11 */ {  -6,   3,0X04,0XC8,0X3E,  +3,   7,0XBF,0XC0,0XA9,0XE6,0X80,0X28,0XF3 }, // "SUPER MSX" SERIES
/* 12 */ {  -6,   3,0X04,0XC8,0X3E,  +3,   7,0X1F,0XD0,0XA9,0XE6,0X40,0X28,0XF3 }, // "DRAKKAR"
/* 13 */ {  -4,   6,0X24,0XC8,0XDB,0XA2,0XA9,0XF2,-128,  -8 }, // FLASHLOAD ("SORCERY")
};
BYTE z80_tape_fastfeed[][32] = { // codes that build bytes
/*  0 */ {  -0,  12,0XFE,0X04,0X3F,0XD8,0XFE,0X02,0X3F,0XCB,0X1A,0X79,0X0F,0XD4,  +2,   1,0XCD,  +2,   2,0X2D,0XC2,-128, -24 }, // MSX1 FIRMWARE: $1AE9
/*  1 */ {  -0,   3,0XFE,0X04,0XD2,  +2,   8,0XFE,0X02,0X3F,0XCB,0X1A,0X79,0X0F,0XD4,  +2,   1,0XCD,  +2,   2,0X2D,0XC2,-128, -25 }, // MR. MICRO LOADER ("SPACE SHUTTLE")
/*  2 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -14 }, // ZX SPECTRUM FIRMWARE + TOPO
/*  3 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0X3E,0X04,0XDA,-128,  +2,   0,   3,0X3E,0X01,0X32,  +2,   3,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -24 }, // "SURVIVOR"
/*  4 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   1,0XD2,-128, -14 }, // FLASHLOAD ("SORCERY")
};
#ifdef FASTTAPE_DUMPER
BYTE z80_tape_fastdump[][32] = { // codes that fill blocks
/*  0 */ {  -0,   7,0X77,0XE7,0X28,0X03,0X23,0X18,0XF6 }, // MSX1 FIRMWARE: $7032
/*  1 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +19,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCA }, // ZX SPECTRUM FIRMWARE
/*  2 */ { -29,   8,0X08,0X20,0X05,0XDD,0X75,0X00,0X18,0X0A, +10,   3,0XDD,0X23,0X1B, +19,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD1 }, // TOPO
/*  3 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +19,   6,0X7C,0XAD,0X67,0X7A,0XB3,0XCA,  +2,   1,0XC3,-128, -58 }, // ZYDROLOAD
};
#endif

WORD z80_tape_spystack(WORD d) { d+=z80_sp.w; int i=PEEK(d); ++d; return (PEEK(d)<<8)+i; } // must keep everything 16-bit!
void z80_tape_hitstack(WORD d,WORD w) { d+=z80_sp.w; POKE(d)=w; ++d; POKE(d)=w>>8; } // must keep everything 16-bit again!
int z80_tape_testfeed(WORD p)
{
	int i; if ((i=z80_tape_index[p])>length(z80_tape_fastfeed))
	{
		for (i=0;i<length(z80_tape_fastfeed)&&!fasttape_test(z80_tape_fastfeed[i],p);++i) {}
		z80_tape_index[p]=i; cprintf("FASTFEED: %04X=%02d\n",p,(i<length(z80_tape_fastfeed))?i:-1);
	}
	return i;
}
#ifdef FASTTAPE_DUMPER
int z80_tape_testdump(WORD p)
{
	int i; if ((i=z80_tape_index[p-1])>length(z80_tape_fastdump)) // the offset avoid conflicts with TABLE2
	{
		for (i=0;i<length(z80_tape_fastdump)&&!fasttape_test(z80_tape_fastdump[i],p);++i) {}
		z80_tape_index[p-1]=i; cprintf("FASTDUMP: %04X=%02d\n",p,(i<length(z80_tape_fastdump))?i:-1);
	}
	return i;
}
#else
#define z80_tape_testdump(p) (0)
#endif

void z80_tape_trap(void)
{
	int i,j; BYTE k; if ((i=z80_tape_index[z80_pc.w])>length(z80_tape_fastload))
	{
		for (i=0;i<length(z80_tape_fastload)&&!fasttape_test(z80_tape_fastload[i],z80_pc.w);++i) {}
		z80_tape_index[z80_pc.w]=i; cprintf("FASTLOAD: %04X=%02d\n",z80_pc.w,(i<length(z80_tape_fastload))?i:-1);
	}
	if (i>=length(z80_tape_fastload)) return; // only known methods can reach here!
	if (!tape_skipping) tape_skipping=-1;
	switch (i) // handle tape analysis upon status
	{
		case  0: // MSX1 FIRMWARE: $1B28
			fasttape_add8(z80_de.b.l>>7,41,&z80_bc.b.l,1); // TONE, BIT0
			break;
		case  1: // MSX1 FIRMWARE: $1B09
			if (z80_hl.b.l==0x08&&FASTTAPE_CAN_KFEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==1))
			{
				#ifdef FASTTAPE_DUMPER
				if ((i=z80_tape_spystack(8))>=0X4000&&(k=z80_tape_testdump(z80_tape_spystack(10)))==0)
				{
					j=z80_tape_spystack(6); //cprintf("{%04X:%04X} ",i,j);
					while (i<j&&FASTTAPE_CAN_KDUMP())
						POKE(i)=fasttape_kdump(),++i;
					z80_tape_hitstack(8,i);
				}
				#endif
				tape_skipping=z80_hl.b.l=z80_bc.b.h=1,k=fasttape_kfeed(z80_de.b.l>>7,62),z80_de.b.h=k<<1,z80_bc.b.l=k>>7;
			}
			else
				fasttape_sub8(z80_de.b.l>>7,62,&z80_bc.b.h,1); // BIT1
			break;
		case  2: // MSX1 FIRMWARE: $1AC4
		case  3: // MSX1 FIRMWARE: $1ACD,$1B38
			k=0; fasttape_add8(i&1,126,&k,1); // wait for 1 or 0
			break;
		case  4: // ZX SPECTRUM FIRMWARE + TOPO SOFT ("ALE HOP")
		case  5: // IBER ("KE RULEN LOS PETAS", "MANTIS 2")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==2)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==1||k==2)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1/*&&((WORD)(z80_ix.w-z80_sp.w)>2)*/)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(~z80_bc.b.l>>7,62),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(~z80_bc.b.l>>7,62,&z80_bc.b.h,1);
			break;
		case  6: // "PACMANIA", "LASER SQUAD"...
		case  7: // ZYDROLOAD ("HOSTAGES")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==2)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==1||k==2||k==3)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1/*&&((WORD)(z80_ix.w-z80_sp.w)>2)*/)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>6,62),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>6,62,&z80_bc.b.h,1);
			break;
		case  8: // "TIME SCANNER"
		case 12: // "DRAKKAR"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==2)
				k=fasttape_feed(z80_bc.b.l>>6,68),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>6,68,&z80_bc.b.h,1);
			break;
		case  9: // RED POINT ("MR.DO'S WILDRIDE")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==2)
				k=fasttape_feed(z80_bc.b.l>>5,59),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>5,59,&z80_bc.b.h,1);
			break;
		case 10: // "SURVIVOR"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==3)
				k=fasttape_feed(~z80_bc.b.l>>7,60),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(~z80_bc.b.l>>7,60,&z80_bc.b.h,1);
			break;
		case 11: // "SUPER MSX" SERIES
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==2)
				k=fasttape_feed(z80_bc.b.l>>7,68),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>7,68,&z80_bc.b.h,1);
			break;
		case 13: // FLASHLOAD ("SORCERY")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==4)
				k=fasttape_feed(z80_bc.b.l>>7,39),tape_skipping=z80_hl.b.l=128+(k>>1),z80_hl.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>7,39,&z80_hl.b.h,1);
			break;
	}
}
#endif

void z80_send(WORD p,BYTE b) // the Z80 sends a byte to a hardware port
{
	z80_skip=0; switch (p&=0XFF)
	{
		#ifdef PSG_PLAYCITY
		case 0X10: // SECOND PSG PORT #0
			if (!playcity_disabled) playcity_select(0,b);
			break;
		case 0X11: // SECOND PSG PORT #1
			if (!playcity_disabled) playcity_send(0,b);
			break;
		//case 0X12: // SECOND PSG PORT #2
		#endif
		#ifdef DEBUG
		case 0X2E: // OPENMSX DEBUG PORT #0 (f.e. CBIOS-MUSIC.ROM)
			if (openmsx_log)
				{ if (!b) openmsx_log=0,cputs("`\n"); }
			else
				{ if (b==0X23) openmsx_log=1,cputs("OPENMSX: `"); }
			break; // '#'
		case 0X2F: // OPENMSX DEBUG PORT #1
			if (openmsx_log) cputchar(b);
			break;
		#endif
		case 0X7C: // OPLL PORT #0
			if (opll_internal) opll_setindex(b);
			break;
		case 0X7D: // OPLL PORT #1
			if (opll_internal) opll_sendbyte(b);
			break;
		case 0X90: // PRINTER PORT #0: send $00, then $FF to print last character sent to #1
			if (printer&&printer_z<b)
				if (++printer_p>=length(printer_t))
					printer_flush();
			printer_z=b; break;
		case 0X91: // PRINTER PORT #1: ASCII character goes here
			printer_t[printer_p]=b; break;
		case 0X9C: // VDP PORT #0 (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) break;
		case 0X98: // VDP PORT #0
			if (vdp_table[25]&4) z80_skip+=0; // the V9958 flag WTE/WAIT ENABLE can delay I/O *!* how many T? does it actually do anything on a 3.6 MHz Z80?
			// TODO: detect whether the S1985 (MSX-SYSTEM chipset) is related to the byte loss: early MSX1 machines suffered it more easily than late S1985-based ones
			if ((main_t&63)||(vdp_raster_mode&64)||(type_id|ram_map)||multi_t>0||(session_fast&-2)||(main_t-vdp_smash>25)) // "MEGAPHOENIX": byte loss in early models!
				vdp_ram_send(vdp_latch=b); // "IO" is more careful and only rarely triggers this at $5208: "OUT ($98),A" four times, the last three are dummy writes
			else if (session_shift) cprintf("%08X: T=%02d! ",z80_pc.w,main_t-vdp_smash); // "CHASE HQ" shouldn't trigger byte loss AFAIK: "OUT ($98),A: DJNZ $-2" = 26 T
			// TODO: strings of "OUT ($98),A" (12 T) are the only way to cause byte loss in a MSX2; are they worth emulating? does any software rely on such behavior?
			vdp_which=-1,vdp_smash=main_t;
			break;
		case 0X9D: // VDP PORT #1 (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) break;
		case 0X99: // VDP PORT #1
			if (vdp_which<0) // first half?
				vdp_which=b;
			else
			{
				p=vdp_which+(b<<8); vdp_which=-1;
				if (p&0X8000) // VDP register?
					vdp_table_send(b&63,p);
				else if (vdp_where=p&0X3FFF,!(p&0X4000)) // read/write mode?
					vdp_latch=vdp_ram_recv();
			}
			break;
		case 0X9E: // VDP PORT #2 (MSX-SYSTEM MIRROR)
			//if (!(type_id|ram_map)) break;
		case 0X9A: // VDP PORT #2
			if (type_id)
			{
				p=vdp_table[16]&15; if (vdp_palette[p*2+0]<128) // first half? (can't use vdp_which here: C-BIOS2 mixes writes to ports $98 and $9A)
					vdp_palette[p*2+0]=b|128;
				else
					vdp_palette[p*2+0]&=0X77,vdp_palette[p*2+1]=b&7, // mask to 0X0777
					video_clut_modify(p),vdp_table[16]=(p+1)&15;
			}
			break;
		case 0X9F: // VDP PORT #3 (MSX-SYSTEM MIRROR)
			//if (!(type_id|ram_map)) break;
		case 0X9B: // VDP PORT #3
			if (type_id)
			{
				if ((p=vdp_table[17]&63)!=17)
					vdp_table_send(p,b); // don't overwrite register 17!
				if (vdp_table[17]<128)
					vdp_table[17]=(p+1)&63; // auto-increase register 17
			}
			break;
		case 0XA0: // PSG PORT #0
			psg_table_select(b); break;
		case 0XA1: // PSG PORT #1
			psg_table_send(b); break;
		//case 0XA2: // PSG PORT #2
		case 0XAC: // PIO PORT A (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) break;
		case 0XA8: // PIO PORT A
			pio_port_a=b; mmu_update(); break;
		case 0XAD: // PIO PORT B (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) break;
		case 0XA9: // PIO PORT B
			pio_port_b=b; break; // useless on MSX!
		case 0XAE: // PIO PORT C (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) break;
		case 0XAA: // PIO PORT C
			pio_port_c=b;
			go_to_pio_port_c: // *!* GOTO!
			if (!dac_extra)
				{ static BYTE bbb=0; BYTE bb=pio_port_c&128; if (bbb!=bb) dac_delay=0,dac_voice=(bbb=bb)?10922:0; } // see also psg_outputs[]
			tape_output=(pio_port_c&32)&&tape_type<0; break;
		case 0XAF: // PIO CONTROL (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) break;
		case 0XAB: // PIO CONTROL
			if (b&128)
				//pio_port_a=pio_port_b=pio_port_c=0, // true on MSX?
				pio_control=b; // useless on MSX!
			else
			{	if (b&1)
					pio_port_c|=+(1<<((b>>1)&7));
				else
					pio_port_c&=~(1<<((b>>1)&7));
				goto go_to_pio_port_c;
			}
			break;
		case 0XB4: // RTC PORT #0
			cmos_table_select(b); break;
		case 0XB5: // RTC PORT #1
			cmos_table_send(b); break;
		case 0XDA: // KANJI PORT #0 (JIS2)
			if (!kanji_rom[0X20000]) kanji_p=(kanji_p&0X1F800)+(b&63)*32+0X20000; // JIS2-able?
			break;
		case 0XDB: // KANJI PORT #1 (JIS2)
			if (!kanji_rom[0X20000]) kanji_p=(kanji_p&0X7E0)+(b&63)*2048+0X20000; // JIS2-able?
			break;
		case 0XD8: // KANJI PORT #0 (JIS1)
			kanji_p=(kanji_p&0X1F800)+(b&63)*32; break;
		case 0XD9: // KANJI PORT #1 (JIS1)
			kanji_p=(kanji_p&0X7E0)+(b&63)*2048; break;
		//case 0XF3: // MSX2P DISPLAY MODE
		//case 0XF4: // MSX2P SYSTEM FLAGS
		case 0XF5: // MSX2 SYSTEM CONTROL
			ioctl_flags=b; break; // breakpoint at $03D1 before MSX2 firmware OUT ($F5),A; at $03D3 after it
		case 0XFC: // RAM MAPPER $0000-$3FFF
		case 0XFD: // RAM MAPPER $4000-$7FFF
		case 0XFE: // RAM MAPPER $8000-$BFFF
		case 0XFF: // RAM MAPPER $C000-$FFFF
			if (ram_map) ram_cfg[p-0XFC]=b,mmu_update(); // ignore changes on 64K ("AAARGH" and its buggy OUT $FE,$00/$10)
	}
}

BYTE z80_recv(WORD p) // the Z80 receives a byte from a hardware port
{
	z80_skip=0; switch (p&=0XFF)
	{
		#ifdef PSG_PLAYCITY
		//case 0X10: // SECOND PSG PORT #0
		//case 0X11: // SECOND PSG PORT #1
		case 0X12: // SECOND PSG PORT #2
			return playcity_disabled?255:playcity_recv(0);
		#endif
		case 0X90: // PRINTER PORT #0: BIT1 = !READY (?)
			return printer?253:255;
		//case 0X91: // PRINTER PORT #1
		case 0X9C: // VDP PORT #0 (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) return 255;
		case 0X98: // VDP PORT #0
			if (vdp_table[25]&4) z80_skip+=0; // the V9958 flag WTE/WAIT ENABLE can delay I/O *!* how many T?
			p=vdp_latch; vdp_latch=vdp_ram_recv();
			vdp_which=-1; return p;
		case 0X9D: // VDP PORT #1 (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) return 255;
		case 0X99: // VDP PORT #1
			p=vdp_state_recv(); // notice that register 15 is always zero in MSX1!
			vdp_which=-1; return p;
		//case 0X9E: // VDP PORT #2 (MSX-SYSTEM MIRROR)
		//case 0X9A: // VDP PORT #2
		//case 0X9F: // VDP PORT #3 (MSX-SYSTEM MIRROR)
		//case 0X9B: // VDP PORT #3
		//case 0XA0: // PSG PORT #0
		//case 0XA1: // PSG PORT #1
		case 0XA2: // PSG PORT #2
			if (psg_index==14)
			{
				BYTE b=(psg_table[15]>>6)^~joystick_bit; if (b&=1) b=kbd_bit[14];
				#if 1 // TAPE_FASTLOAD
				if (tape_fastload&&tape/*_loud*/) z80_tape_trap(); // handle tape analysis upon status
				#endif
				return (tape_enabled&&tape_status?191:63)+((mem_rom[0X2B]&32)*2)-b; // 64 is the !JAPANESE KEYBOARD BIT
			}
			return psg_table_recv();
		case 0XAC: // PIO PORT A (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) return 255;
		case 0XA8: // PIO PORT A
			return pio_port_a;
		case 0XAD: // PIO PORT B (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) return 255;
		case 0XA9: // PIO PORT B
			return ~kbd_bit[pio_port_c&15];
		case 0XAE: // PIO PORT C (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) return 255;
		case 0XAA: // PIO PORT C
			return pio_port_c;
		case 0XAF: // PIO CONTROL (MSX-SYSTEM MIRROR)
			if (!(type_id|ram_map)) return 255;
		case 0XAB: // PIO CONTROL
			return pio_control;
		//case 0XB4: // RTC PORT #0
		case 0XB5: // RTC PORT #1
			return cmos_table_recv();
		//case 0XDA: // KANJI PORT #0 (JIS2)
		case 0XDB: // KANJI PORT #1 (JIS2)
			if (!kanji_rom[0X20000]) return 255; // JIS2-able?
		//case 0XD8: // KANJI PORT #0 (JIS1)
		case 0XD9: // KANJI PORT #1 (JIS1)
			if (!i18n_kana/*||!(ioctl_flags&((p&2)?2:1))*/) return 255; // skip non-Japanese machines!
			p=kanji_rom[kanji_p&(sizeof(kanji_rom)-1)]; if (!(++kanji_p&31)) kanji_p-=32; // wrap!
			return p;
		//case 0XF3: // MSX2P DISPLAY MODE
		case 0XF4: // MSX2P SYSTEM FLAGS
			// this field is used to store 1.- whether this is a cold or warm boot, and 2.- the state of the R800 in MSXTR machines;
			// PANASONIC and SANYO store the COMPLEMENT rather than the original byte, and MSX2P C-BIOS expects this behavior as well.
			// other extant ROM files are identical to the SANYO BIOS, with the CPL instructions related to this port turned into NOP.
			p=msx2p_flags,msx2p_flags|=128; if (PEEK(z80_pc.w)==0X2F) p=~p; return p; // check CPL to please the BIOS, complement or not!
		//case 0XF5: // MSX2 SYSTEM CONTROL
		case 0XFC: // RAM MAPPER $0000-$3FFF
		case 0XFD: // RAM MAPPER $4000-$7FFF
		case 0XFE: // RAM MAPPER $8000-$BFFF
		case 0XFF: // RAM MAPPER $C000-$FFFF
			if (ram_map) return ram_cfg[p-0XFC]|~ram_bit; // unused bits are ONE
		default:
			return 255;
	}
}

// CPU: ZILOG Z80 MICROPROCESSOR ==================================== //

const BYTE z80_delays[]= // precalc'd coarse timings
{
	// +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F //// +0x000 -
	 5,11, 8, 7, 5, 5, 8, 5, 5,12, 8, 7, 5, 5, 8, 5, // base: 1/2
	 9,11, 8, 7, 5, 5, 8, 5,13,12, 8, 7, 5, 5, 8, 5, // 0x10-0x1F
	 8,11,14, 7, 5, 5, 8, 5, 8,12,17, 7, 5, 5, 8, 5, // 0x20-0x2F
	 8,11,14, 7,12,12,11, 5, 8,12,14, 7, 5, 5, 8, 5, // 0x30-0x3F
	 5, 5, 5, 5, 5, 5, 8, 5, 5, 5, 5, 5, 5, 5, 8, 5, // 0x40-0x4F
	 5, 5, 5, 5, 5, 5, 8, 5, 5, 5, 5, 5, 5, 5, 8, 5, // 0x50-0x5F
	 5, 5, 5, 5, 5, 5, 8, 5, 5, 5, 5, 5, 5, 5, 8, 5, // 0x60-0x6F
	 8, 8, 8, 8, 8, 8, 5, 8, 5, 5, 5, 5, 5, 5, 8, 5, // 0x70-0x7F
	 5, 5, 5, 5, 5, 5, 8, 5, 5, 5, 5, 5, 5, 5, 8, 5, // 0x80-0x8F
	 5, 5, 5, 5, 5, 5, 8, 5, 5, 5, 5, 5, 5, 5, 8, 5, // 0x90-0x9F
	 5, 5, 5, 5, 5, 5, 8, 5, 5, 5, 5, 5, 5, 5, 8, 5, // 0xA0-0xAF
	 5, 5, 5, 5, 5, 5, 8, 5, 5, 5, 5, 5, 5, 5, 8, 5, // 0xB0-0xBF
	 6,11,11,11,11, 8, 8,12, 6,11,11, 5,11,18, 8,12, // 0xC0-0xCF
	 6,11,11, 8,11, 8, 8,12, 6, 5,11, 8,11, 5, 8,12, // 0xD0-0xDF
	 6,11,11,17,11, 8, 8,12, 6, 5,11, 5,11, 5, 8,12, // 0xE0-0xEF
	 6,11,11, 5,11, 8, 8,12, 6, 7,11, 5,11, 5, 8,12, // 0xF0-0xFF
	// +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F //// +0x100 -
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // base: 2/2
	 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10-0x1F
	 5, 0, 3, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, // 0x20-0x2F
	 5, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, // 0x30-0x3F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x40-0x4F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x50-0x5F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x60-0x6F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x70-0x7F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x80-0x8F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x90-0x9F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xA0-0xAF
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xB0-0xBF
	 6, 0, 0, 0, 7, 4, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0, // 0xC0-0xCF
	 6, 0, 0, 4, 7, 4, 0, 0, 6, 0, 0, 4, 7, 0, 0, 0, // 0xD0-0xDF
	 6, 0, 0, 3, 7, 4, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0, // 0xE0-0xEF
	 6, 0, 0, 0, 7, 4, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0, // 0xF0-0xFF
	// +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F //// +0x200 -
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // EDxx: 1/2
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x10-0x1F
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x20-0x2F
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x30-0x3F
	 9, 5,12,13, 5,11, 5, 6, 9, 5,12,17, 5,11, 5, 6, // 0x40-0x4F
	 9, 5,12,13, 5,11, 5, 6, 9, 5,12,17, 5,11, 5, 6, // 0x50-0x5F
	 9, 5,12,13, 5,11, 5,15, 9, 5,12,17, 5,11, 5,15, // 0x60-0x6F
	 9, 5,12,13, 5,11, 5, 6, 9, 5,12,17, 5,11, 5, 6, // 0x70-0x7F
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x80-0x8F
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x90-0x9F
	 9,13,13,13, 5, 5, 5, 5, 9,13,13,13, 5, 5, 5, 5, // 0xA0-0xAF
	 9,13,13,13, 5, 5, 5, 5, 9,13,13,13, 5, 5, 5, 5, // 0xB0-0xBF
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0xC0-0xCF
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0xD0-0xDF
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0xE0-0xEF
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0xF0-0xFF
	// +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F //// +0x300 -
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // EDxx: 2/2
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10-0x1F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x20-0x2F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x30-0x3F
	 0, 4, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, // 0x40-0x4F
	 0, 4, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, // 0x50-0x5F
	 0, 4, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, // 0x60-0x6F
	 0, 4, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, // 0x70-0x7F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x80-0x8F
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x90-0x9F
	 4, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, // 0xA0-0xAF
	 9, 5, 5, 5, 0, 0, 0, 0, 9, 5, 5, 5, 0, 0, 0, 0, // 0xB0-0xBF
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xC0-0xCF
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xD0-0xDF
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xE0-0xEF
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xF0-0xFF
	// +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F //// +0x400 -
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // CB prefix
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0x10-0x1F
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0x20-0x2F
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0x30-0x3F
	 5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 5, 5, 9, 5, // 0x40-0x4F
	 5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 5, 5, 9, 5, // 0x50-0x5F
	 5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 5, 5, 9, 5, // 0x60-0x6F
	 5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 5, 5, 9, 5, // 0x70-0x7F
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0x80-0x8F
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0x90-0x9F
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0xA0-0xAF
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0xB0-0xBF
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0xC0-0xCF
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0xD0-0xDF
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0xE0-0xEF
	 5, 5, 5, 5, 5, 5,12, 5, 5, 5, 5, 5, 5, 5,12, 5, // 0xF0-0xFF
	// +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F //// +0x500 -
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // XY+CB set
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0x10-0x1F
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0x20-0x2F
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0x30-0x3F
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12, // 0x40-0x4F
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12, // 0x50-0x5F
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12, // 0x60-0x6F
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12, // 0x70-0x7F
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0x80-0x8F
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0x90-0x9F
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0xA0-0xAF
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0xB0-0xBF
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0xC0-0xCF
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0xD0-0xDF
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0xE0-0xEF
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, // 0xF0-0xFF
	// +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F //// +0x600 -
	 0, 0, 0, 5, 0, 0, 0, 0, 0,12, 0, 5, 0, 0, 0, 0, // XY prefix
	 0, 0, 0, 5, 0, 0, 0, 0, 0,12, 0, 5, 0, 0, 0, 0, // 0x10-0x1F
	 0,11,14, 7, 5, 5, 8, 0, 0,12,17, 7, 5, 5, 8, 0, // 0x20-0x2F
	 0, 0, 0, 5,20,20,16, 0, 0,12, 0, 5, 0, 0, 0, 0, // 0x30-0x3F
	 0, 0, 0, 0, 5, 5,16, 0, 0, 0, 0, 0, 5, 5,16, 0, // 0x40-0x4F
	 0, 0, 0, 0, 5, 5,16, 0, 0, 0, 0, 0, 5, 5,16, 0, // 0x50-0x5F
	 5, 5, 5, 5, 5, 5,16, 5, 5, 5, 5, 5, 5, 5,16, 5, // 0x60-0x6F
	16,16,16,16,16,16, 0,16, 0, 0, 0, 0, 5, 5,16, 0, // 0x70-0x7F
	 0, 0, 0, 0, 5, 5,16, 0, 0, 0, 0, 0, 5, 5,16, 0, // 0x80-0x8F
	 0, 0, 0, 0, 5, 5,16, 0, 0, 0, 0, 0, 5, 5,16, 0, // 0x90-0x9F
	 0, 0, 0, 0, 5, 5,16, 0, 0, 0, 0, 0, 5, 5,16, 0, // 0xA0-0xAF
	 0, 0, 0, 0, 5, 5,16, 0, 0, 0, 0, 0, 5, 5,16, 0, // 0xB0-0xBF
	 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 5, 0, 0, 0, 0, // 0xC0-0xCF
	 9, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, // 0xD0-0xDF
	 9,11, 0,17, 0, 8, 0, 0, 9, 5, 0, 0, 0, 0, 0, 0, // 0xE0-0xEF
	 9, 0, 0, 0, 0, 0, 0, 0, 9, 7, 0, 0, 0, 0, 0, 0, // 0xF0-0xFF
};

// input/output
#define Z80_SYNC() ( _t_-=z80_t, z80_sync(z80_t), z80_t=0 )
#define Z80_SYNC_IO ( _t_-=z80_t, z80_sync(z80_t) ) // see Z80_STRIDE_IO for the missing "z80_t=0"
#define Z80_PRAE_RECV(w) Z80_SYNC_IO
#define Z80_RECV z80_recv
#define Z80_POST_RECV(w) z80_t+=z80_skip//,_t_-=z80_loss
#define Z80_PRAE_SEND(w) do{ if (!(w&0x0054)) audio_dirty=1; Z80_SYNC_IO; }while(0) // PSG and PIO can play sound!
#define Z80_SEND z80_send
#define Z80_POST_SEND(w) z80_t+=z80_skip//,_t_-=z80_loss
// fine timings
#define Z80_LOCAL // it must stick between macros :-/
#define Z80_MREQ(t,w)
#define Z80_MREQ_1X(t,w)
#define Z80_MREQ_NEXT(t)
#define Z80_MREQ_1X_NEXT(t)
#define Z80_WAIT(t)
#define Z80_WAIT_IR1X(t)
#define Z80_DUMB_M1(w) ((void)0) // dumb 4-T MREQ
#define Z80_DUMB Z80_DUMB_M1 // dumb 3-T MREQ
#define Z80_NEXT_M1 PEEK // 4-T PC FETCH
#define Z80_NEXT PEEK // 3-T PC FETCH
#define Z80_PEEK(w) ((mmu_bit[w>>12]&1)?mmu_peek(w):mmu_rom[w>>12][w])
#define Z80_PEEK0 PEEK // untrappable single read, use with care
#define Z80_PEEK1WZ Z80_PEEK // 1st twin read from LD rr,($hhll)
#define Z80_PEEK2WZ Z80_PEEK // 2nd twin read
#define Z80_PEEK1SP Z80_PEEK0 // 1st twin read from POP rr
#define Z80_PEEK2SP Z80_PEEK0 // 2nd twin read
#define Z80_PEEK1EX Z80_PEEK0 // 1st twin read from EX rr,(SP)
#define Z80_PEEK2EX Z80_PEEK0 // 2nd twin read
#define Z80_PRAE_NEXTXY PEEK // special DD/FD PEEK (1/2)
#define Z80_POST_NEXTXY // special DD/FD PEEK (2/2)
#define Z80_POKE(w,b) do{ if (mmu_bit[w>>12]&2) mmu_poke(w,b); else POKE(w)=(b); }while(0) // trappable single write; be careful, too
#define Z80_PEEKPOKE(w,b) do{ if (mmu_bit[w>>12]&2) mmu_poke(w,b); else POKE(w)=(b); }while(0) // a POKE that follows a same-address PEEK, f.e. INC (HL)
#define Z80_POKE0(w,b) (POKE(w)=(b)) // untrappable single write, use with care
#define Z80_POKE1WZ Z80_POKE // 1st twin write from LD ($hhll),rr
#define Z80_POKE2WZ Z80_POKE // 2nd twin write
#define Z80_POKE1SP Z80_POKE0 // 1st twin write from PUSH rr
#define Z80_POKE2SP Z80_POKE0 // 2nd twin write
#define Z80_POKE1EX Z80_POKE0 // 1st twin write from EX rr,(SP)
#define Z80_POKE2EX Z80_POKE0 // 2nd twin write
// coarse timings
#define Z80_STRIDE(o) (z80_t+=z80_delays[o])
#define Z80_STRIDE_ZZ(o) (z80_t+=z80_delays[o]) // no slow/fast ACK quirk on MSX
#define Z80_STRIDE_IO(o) (z80_t=z80_delays[o]) // "z80_t=XXX" makes "z80_t=0" redundant in Z80_SYNC_IO
#define Z80_LIST_PEEKXY(o) z80_delays[o+0x600] // defined, this makes the Z80 DD/FD boolean list redundant
#define Z80_SLEEP(t) z80_t+=(t) // `t` must be nonzero!
#define Z80_HALT_STRIDE 5 // i.e. optimal HALT step, handled in a single go
// Z80 quirks
BYTE z80_xcf=0; // the XCF quirk flag
#define Z80_QUIRK(x) (z80_xcf=(x)&1) // XCF quirk only
#define Z80_QUIRK_M1 // nothing to do!
#define Z80_XCF_BUG (z80_xcf?0:z80_af.b.l) // replicate the XCF quirk -- seen on Spectrum, not sure on MSX
#define Z80_0XED71 (0) // whether OUT (C) sends 0 (NMOS) or 255 (CMOS)
#define Z80_0XED5X (1) // whether LD A,I/R checks the IRQ in-opcode

#define bios_magick() (debug_point[0X10CB]=debug_point[0X030D]=debug_point[0X0370]=debug_point[0X045C]=debug_point[0X04BF]=debug_point[0X2C76]=debug_point[0X7BC1]=debug_point[0X7D12]=debug_point[0X7D68]=DEBUG_MAGICK)
void z80_magick(void) // virtual magick!
{
	//cprintf("MAGICK:%08X ",z80_pc.w);
	/**/ if (pio_port_a& 15) { if (z80_pc.w==0X2C76&&ram_bit+1==z80_hl.w&&mmu_rom[2]==&mem_rom[0X10000-0X0000]&&equalsmm(&mem_rom[0X12C76],0XE53E)) ram_dirty=3; } // MSX2P: clean extended RAM "dirty" status after internal test
	// all the other traps are BIOS & BASIC only!
	else if (z80_pc.w==0X10CB) { if (autorun_t>0) { { if (mem_rom[0X10CB]==0XE5) z80_af.b.h=*autorun_s++,z80_pc.w=0XFD9F; } if (!--autorun_t) disc_disabled&=1; } } // AUTORUN + end of AUTORUN
	else if (!power_boosted) ; // power-up boost only!
	else if (z80_pc.w==0X030D||z80_pc.w==0X0370) // MSX1: power-up boost (hardware test)
		{ if (!type_id&&mem_rom[z80_pc.w]==0X2C) z80_hl.b.l=255; }
	else if (z80_pc.w==0X045C||z80_pc.w==0X04BF) // MSX2: power-up boost (hardware test) (almost the same as above)
		{ if ( type_id&&mem_rom[z80_pc.w]==0X2C) z80_hl.b.l=255; }
	else if (z80_pc.w==0X7BC1) { if (mem_rom[0X7BC1]==0X2B) z80_hl.w=1; } // MSX2: power-up boost (NOT present on JP machines!)
	else if (z80_pc.w==0X7D12) { if (mem_rom[0X7D12]==0X10&&z80_bc.b.h>2) z80_bc.b.h=2; } // MSX1: power-up boost (longer on non-JP machines)
	else if (z80_pc.w==0X7D68) { if (mem_rom[0X7D68]==0X2C) z80_hl.b.l=255; } // MSX-BASIC: power-up boost (memory test)
}

#define DEBUG_HERE
#define DEBUG_INFOX 20 // panel width
BYTE debug_pook(int q,WORD w) { return q?vdp_ram[((vdp_table[14]&4)<<14)+w]:PEEK(w); } // show either VDP or Z80 space (hacky, can't write on VDP)
//BYTE debug_pook(int q,WORD w) { return PEEK(w); } // `q` doesn't matter on MSX! empty banks are unreadable AND unwritable!
void debug_info(int q)
{
	if (cart&&!cart_big)
		sprintf(DEBUG_INFOZ( 0),"MAP:   %c %02X:%02X:%02X:%02X",hexa1[cart_id],cart_bank[0],cart_bank[1],cart_bank[2],cart_bank[3]);
	else
		sprintf(DEBUG_INFOZ( 0),"MAP:   - --:--:--:--");
	sprintf(DEBUG_INFOZ( 1)+4,"PIO%c %02X:%02X:%02X:%02X",cart?'*':'-',pio_port_a,pio_port_b,pio_port_c,pio_control);
	sprintf(DEBUG_INFOZ( 2)+5,"RAM %02X:%02X:%02X:%02X",ram_cfg[0],ram_cfg[1],ram_cfg[2],ram_cfg[3]);
	sprintf(DEBUG_INFOZ( 3)+6,"PS %02X:%02X:%02X:%02X",ps_slot[0],ps_slot[1],ps_slot[2],ps_slot[3]);
	if (!(q&1)) // 1/2
	{
		sprintf(DEBUG_INFOZ( 3),"    IO:%02X JIS%c:%05X",ioctl_flags,/*kanji_rom[0X20000]?128:256,*/kanji_rom[0]?'-':kanji_rom[0X20000]?'1':'2',kanji_p&0X3FFFF);
		sprintf(DEBUG_INFOZ( 4),"DSK:");
			sprintf(DEBUG_INFOZ( 5)+4,"%02X%02X%02X--%02X%02X-- %c",
				diskette_command,diskette_track,diskette_sector,diskette_side,diskette_motor*128+diskette_drive,diskette_length?'*':'-');
		sprintf(DEBUG_INFOZ( 7)+4,"%04X:%02X   %02X:%03X",vdp_where,vdp_latch,(video_pos_x/3)&255,vdp_raster&511);
		const char m[][3]={"G1","G2","G3","G4","G5","G6","??","G7"};
		sprintf(DEBUG_INFOZ( 6),"VDP: V%cS%c %d%d%d%d%d [%s]",
			(vdp_table[1]&64)?'*':'-',(vdp_table[8]&2)?'-':'*',
			(vdp_table[0]>>3)&1,(vdp_table[0]>>2)&1,(vdp_table[0]>>1)&1,(vdp_table[1]>>3)&1,(vdp_table[1]>>4)&1,
			(vdp_table[1]&8)?"MC":(vdp_table[1]&16)?(vdp_table[0]&4)?"T2":"T1":m[(vdp_table[0]>>1)&7]);
		byte2hexa(DEBUG_INFOZ( 8)+4,&vdp_table[0],8);
		if (type_id)
		{
			sprintf(DEBUG_INFOZ(10),"%02X=",vdp_raster_add8);
			byte2hexa(DEBUG_INFOZ( 9)+4,&vdp_table[ 8],8);
			byte2hexa(DEBUG_INFOZ(10)+4,&vdp_table[16],8);
			byte2hexa(DEBUG_INFOZ(11)+4,&vdp_table[32],8);
			byte2hexa(DEBUG_INFOZ(12)+4,&vdp_table[40],7);
			byte2hexa(DEBUG_INFOZ(13)+4,&vdp_state[ 0],8);
			if ((q=vdp_table_last)<24) // not 127!
				debug_hilight2(DEBUG_INFOZ( 8+(q>>3))+4+(q&7)*2);
			else if (q>=32&&q<47)
				debug_hilight2(DEBUG_INFOZ( 7+(q>>3))+4+(q&7)*2);
			if ((q=vdp_table[15]&15)<8)
				debug_hilight2(DEBUG_INFOZ(13)+4+q*2);
			const char o[][5]={"STOP","STOP","STOP","STOP","TEST","PSET","SRCH","LINE","LMMV","LMMM","LMCM","LMMC","HMMV","HMMM","YMMM","HMMC"};
			sprintf(DEBUG_INFOZ(14)+4,"%s %03X%c%03X%c%03X",o[vdp_table[46]>>4],vdp_blit_sx&0XFFF,(vdp_state[2]&1)?'*':'-',vdp_blit_dx&0XFFF,(vdp_state[2]&128)?'*':'-',vdp_blit_nx&0XFFF);
			if (type_id>1) // the extra V9958 registers: SPECIAL FLAGS, HSCROLL CHAR OFFSET and PIXEL DELAY
				sprintf(DEBUG_INFOZ(11)+0,"+%02X",vdp_table[25]&127),
				sprintf(DEBUG_INFOZ(12)+0,"%02X%c",vdp_table[26]&63,(vdp_table[27]&7)+'0');
		}
		else
			sprintf(DEBUG_INFOZ( 9)+4,"STATUS: %02X",vdp_state[0]);
	}
	else // 2/2
	{
		sprintf(DEBUG_INFOZ( 3),"RTC:  %X%X/%X%X %X%X:%X%X:%X%X",cmos_table[10],cmos_table[9],cmos_table[8],cmos_table[7],cmos_table[5],cmos_table[4],cmos_table[3],cmos_table[2],cmos_table[1],cmos_table[0]);
		for (int y=0;y<4;++y) // reg blocks 0-3
		{
			for (int x=0;x<13;++x) // block regs 0-12
				*(DEBUG_INFOZ(4+y)+4+x)=hexa1[cmos_table[y*13+x]&15];
		}
		sprintf(DEBUG_INFOZ( 7)+17,"%X%X%X",cmos_table[52]&15,cmos_table[53]&15,cmos_table[54]&15); // major regs 13-15
		sprintf(DEBUG_INFOZ( 4)+18,"%02d",cmos_count);
		if (cmos_index<52)
			*(DEBUG_INFOZ( 4+cmos_index/13)+4+(cmos_index%13))+=128;
		else if (cmos_index<55)
			*(DEBUG_INFOZ( 7)+cmos_index-52+17)+=128;
		sprintf(DEBUG_INFOZ( 8),"PSG:");
		sprintf(DEBUG_INFOZ( 9),"1ST");
		sprintf(DEBUG_INFOZ(11),"2ND%c",playcity_disabled?'-':'*');
		sprintf(DEBUG_INFOZ(13),"SCC%c",sccplus_ready?'+':'-');
		byte2hexa(DEBUG_INFOZ( 9)+4,&psg_table[0],8);
		byte2hexa(DEBUG_INFOZ(10)+4,&psg_table[8],8);
		debug_hilight2(DEBUG_INFOZ( 9+((psg_index&15)>>3))+4+(psg_index&7)*2);
		byte2hexa(DEBUG_INFOZ(11)+4,&playcity_table[0][0],8);
		byte2hexa(DEBUG_INFOZ(12)+4,&playcity_table[0][8],8);
		debug_hilight2(DEBUG_INFOZ(11+(playcity_index[0]&15)/8)+4+(playcity_index[0]&7)*2);
		byte2hexa(DEBUG_INFOZ(14)+0,&sccplus_table[192],1);
		if (sccplus_debug)
			byte2hexa(DEBUG_INFOZ(14)+2,&sccplus_table[255],1);
		else
			sprintf(DEBUG_INFOZ(14)+2,"--");
		byte2hexa(DEBUG_INFOZ(13)+4,&sccplus_table[160],8); // remember, we handle SCC- as a subset of SCC+,
		byte2hexa(DEBUG_INFOZ(14)+4,&sccplus_table[168],8); // and thus we must use the offsets of the later
	}
}
int grafx_mask(void) { return debug_mode?0X1FFFF:0XFFFF; }
BYTE grafx_peek(int w) { return debug_mode?vdp_ram[w&0X1FFFF]:debug_peek(w); }
void grafx_poke(int w,BYTE b) { if (debug_mode) vdp_ram[w&0X1FFFF]=b; else debug_poke(w,b); }
int grafx_size(int i) { return i*8; }
int grafx_show(VIDEO_UNIT *t,int g,int n,int w,int o)
{
	const VIDEO_UNIT p0=0,p1=0XFFFFFF; BYTE z=-(o&1); g-=8; do
	{
		BYTE b=grafx_peek(w)^z; // Z80 or VDP!
		*t++=b&128?p1:p0; *t++=b& 64?p1:p0;
		*t++=b& 32?p1:p0; *t++=b& 16?p1:p0;
		*t++=b&  8?p1:p0; *t++=b&  4?p1:p0;
		*t++=b&  2?p1:p0; *t++=b&  1?p1:p0;
	}
	while (++w,t+=g,--n); return w&grafx_mask();
}
void grafx_info(VIDEO_UNIT *t,int g,int o) // draw the palette and the current sprites
{
	t-=16*8; for (int y=12;--y>=0;)
		for (int x=0;x<16*8;++x)
			t[y*g+x]=video_clut[(x/8)/*+(y/12)*16*/];
	o=(o&1)?video_clut[15]:video_clut[0]; // sprite background color
	int i=0,p=0; BYTE z=0; if (vdp_memtype) // MODE 2 sprites?
		for (int y=0;y<4*16;++y)
			for (int x=0;x<8*16;++x) // always assuming 16x16
			{
				if (!(x&7))
				{
					i=((y>>4)*8+(x>>4))*4;
					p=vdp_map_sa2[i+514]&-4;
					z=vdp_map_sg1[p*8+(y&15)+(x&8)*2];
					p=video_clut[vdp_map_sa2[i*4+(y&15)]&15];
				}
				t[(y+1*12)*g+x]=((z<<(x&7))&128)?p:o;
			}
	else // MODE 1 sprites!
		for (int y=0;y<4*16;++y)
			for (int x=0;x<8*16;++x) // ditto
			{
				if (!(x&7))
				{
					i=((y>>4)*8+(x>>4))*4;
					p=vdp_map_sa1[i+  2]&-4;
					z=vdp_map_sg1[p*8+(y&15)+(x&8)*2];
					p=video_clut[vdp_map_sa1[i  +  3   ]&15];
				}
				t[(y+1*12)*g+x]=((z<<(x&7))&128)?p:o;
			}
}
#include "cpcec-z8.h"
#undef DEBUG_HERE

// EMULATION CONTROL ================================================ //

char txt_error_any_load[]="Cannot open file!";
char txt_error_bios[]="Cannot load firmware!";

// emulation setup and reset operations ----------------------------- //

void all_setup(void) // setup everything!
{
	video_table_reset(); // user palette
	bios_magick(); // MAGICK follows DEBUG!
	ioctl_setup();
	psg_setup();
	z80_setup();
	cart_setup();
}
void all_reset(void) // reset everything!
{
	const int k=128; // tested on a real MSX by Imulilla; the "Space Shuttle" tape needs 0s and 255s
	for (int i=0;i<0X10000;i+=k) memset(&mem_ram[i],0,k),i+=k,memset(&mem_ram[i],-1,k);
	memset(&mem_ram[0X10000],0,sizeof(mem_ram)-0X10000); // extra memory is filled with $00 AFAIK
	//MEMZERO(mem_ram);
	//MEMZERO(autorun_kbd);
	pio_reset();
	mmu_reset();
	vdp_reset();
	ioctl_reset();
	tape_reset();
	psg_reset();
	z80_reset();
	cart_reset();
	debug_reset();
	sccplus_reset();
	//MEMBYTE(mem_ram,-1); // clean memory AND remove "AB" boot traps (not MEMZERO?)
	disc_disabled&=1,z80_irq=snap_done=/*autorun_mode=*/autorun_t=0; // avoid accidents!
	MEMBYTE(z80_tape_index,-1); // TAPE_FASTLOAD, avoid false positives!
}

// firmware ROM file handling operations ---------------------------- //

BYTE old_type_id=99; // the last loaded BIOS type
char bios_path[STRMAX]="",bios_system[][10]={"msx.rom","msx2.rom","msx2p.rom"/*,"msxtr.rom"*/};

int bios_load(char *s) // load ROM. `s` path; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1;
	//int i=fgetii(f); if (i!=0XC3F3)
	int i=fgetiiii(f); if (fgetiiii(f)!=0X98981BBF) // official MSX and opensource C-BIOS firmwares use these values!
		return puff_fclose(f),1; // MSX1 firmware must begin with DI: JP $NNNN!
	MEMBYTE(mem_rom,-1); // fill both `bad_rom` and unused space with 0XFF
	fseek(f,0,SEEK_SET); i=fread1(mem_rom,sizeof(mem_rom),f);
	i+=fread1(mem_rom,sizeof(mem_rom),f); puff_fclose(f);
	// generally speaking, the layout of single-file firmwares is as follows:
	// MSX ....... BIOS+BAS.
	// MSX2 ...... BIOS+BAS.+EXT.
	// MSX2P ..... BIOS+BAS.+EXT.+KAN1+KAN2
	// CBIOS1 .... BIOS+BAS.+LOGO
	// CBIOS2 .... BIOS+BAS.+EXT.+LOGO
	// CBIOS2P ... BIOS+BAS.+EXT.+LOGO
	// as a result, some shuffling is required to match the built-in order BIOS+BAS.+LOGO+XYZ1+EXT.+KAN1+KAN2+XYZ2
	if (i<32*1024||i>80*1024||mem_rom[0X2D]>=length(bios_system))
		return old_type_id=99; // reject wrong sizes and IDs!
	else if (i>64*1024) // 80K? MSX2P! (MSXTR too?)
		memcpy(&mem_rom[0X14000],&mem_rom[0X0C000],0X8000),
		memcpy(&mem_rom[0X10000],&mem_rom[0X08000],0X4000),
		memset(&mem_rom[0X08000],-1,0X8000);
	else if (i>48*1024) // 64K? C-BIOS2 or C-BIOS3!
		memcpy(&mem_rom[0X10000],&mem_rom[0X08000],0X4000),
		memcpy(&mem_rom[0X08000],&mem_rom[0X0C000],0X4000),
		memset(&mem_rom[0X0C000],-1,0X4000);
	else if (i>32*1024) // 48K? detect type...
	{
		if (equalsii(&mem_rom[0X08000],0X4443)) // "CD"? official MSX2 ROM set!
			memcpy(&mem_rom[0X10000],&mem_rom[0X08000],0X4000),
			memset(&mem_rom[0X08000],-1,0X4000);
		//else ; // no "CD": C-BIOS1!
	}
	//else ; // 32K, official MSX1 ROM set!
	if (session_substr!=s) STRCOPY(bios_path,s);
	if ((type_id=mem_rom[0X2D])!=old_type_id) // take the ID value from the BIOS
		if ((old_type_id=type_id)>1)
			msx2p_flags=0,video_wide_xlat(); // MSX2P!
	return 0;
}
#define bios_reload() (old_type_id==type_id?0:bios_load(strcat(strcpy(session_substr,session_path),bios_system[type_id]))) // ditto, but from the current type_id

// snapshot file handling operations -------------------------------- //

char snap_pattern[]="*.stx",snap_magic16[]="MSX SNAPSHOT V1\032";
char snap_path[STRMAX]="",snap_extended=1; // flexible behavior (i.e. compression)

int snap_bin2x(BYTE *t,int o,BYTE *s,int i)
{
	#ifdef LEMPELZIV_ENCODING
	return bin2lzf(t,o,s,i);
	#else
	return bin2rlf(t,o,s,i);
	#endif
}
void snap_savechunk(BYTE *s,int l,FILE *f) // save an optionally compressed block
{
	int i=snap_extended?snap_bin2x(session_scratch,l,s,l):0;
	if (i>0) fputiiii(i,f),fwrite1(session_scratch,i,f); else fputiiii(l,f),fwrite1(s,l,f);
}
int snap_x2bin(BYTE *t,int o,BYTE *s,int i,int q) // notice that `q` will be zero without LEMPELZIV_ENCODING
{
	return
	#ifdef LEMPELZIV_ENCODING
	q?lzf2bin(t,o,s,i):
	#endif
	rlf2bin(t,o,s,i);
}
int snap_loadchunk(BYTE *t,int l,FILE *f,int i,int q) // load an optionally compressed block; 0 OK, !0 ERROR
	{ return (i<l?snap_x2bin(t,l,session_scratch,fread1(session_scratch,i,f),q):fread1(t,l,f))-l; }

int snap_save(char *s) // save a snapshot `s`; zero OK, nonzero error!
{
	FILE *f=puff_fopen(s,"wb"); if (!f) return 1;
	BYTE header[256]; MEMZERO(header); strcpy(header,snap_magic16);
	#ifdef LEMPELZIV_ENCODING
	if (snap_extended) header[14]='2';
	#endif
	int i=type_id?1<<16:(1<<14),j,k;
	if ((j=snap_extended?snap_bin2x(session_scratch,i,vdp_ram,i):0)>0) mputii(&header[16],j); else j=0;
	if ((k=snap_extended?snap_bin2x(&session_scratch[j],1<<16,mem_ram,1<<16):0)>0) mputii(&header[18],k); else k=0;
	header[20]=(mem_rom[0X2B]&128)+(mem_rom[0X2B]&32)*2+type_id; // !NTSC (+128) + !JAPANESE (+64) + TYPE (0..3)
	// Z80
	mputii(&header[32+ 0],z80_af .w);
	mputii(&header[32+ 2],z80_bc .w);
	mputii(&header[32+ 4],z80_hl .w);
	mputii(&header[32+ 6],z80_de .w);
	mputii(&header[32+ 8],z80_af2.w);
	mputii(&header[32+10],z80_bc2.w);
	mputii(&header[32+12],z80_hl2.w);
	mputii(&header[32+14],z80_de2.w);
	mputii(&header[32+16],z80_ix .w);
	mputii(&header[32+18],z80_iy .w);
	mputii(&header[32+20],z80_sp .w);
	mputii(&header[32+22],z80_pc .w);
	header[32+24]=z80_ir.b.h;
	header[32+25]=z80_ir.b.l;
	header[32+26]=header[32+27]=z80_iff.b.l&1; // should we be more precise here?
	header[32+28]=z80_imd;
	// PSG
	memcpy(&header[64+ 0],psg_table,16);
	header[64+16]=psg_index;
	// PIO
	header[64+20]=pio_port_a ;
	header[64+21]=pio_port_b ;
	header[64+22]=pio_port_c ;
	header[64+23]=pio_control;
	// MMU
	memcpy(&header[64+24],ram_cfg,4);
	if (type_id) memcpy(&header[64+28],rom_cfg,4);
	// VDP
	if (vdp_which>=0) header[96+0]=vdp_which,header[96+1]=1; // vdp_which will be -1 ALMOST always!
	header[96+2]=vdp_latch;
	mputii(&header[96+4],vdp_where);
	mputii(&header[96+8],video_pos_x<<1); //header[96+8]=vdp_count_x;
	mputii(&header[96+10],vdp_count_y);
	if (type_id)
		memcpy(&header[96+16],vdp_table,   24), // 8 bytes MSX1 + 24 bytes MSX2 w/o BLITTER
		mputii(&header[96+40],vdp_blit_sx    ), // BLITTER counters SX, DX, etc. in the unused registers #24-#31
		mputii(&header[96+42],vdp_blit_dx    ),
		mputii(&header[96+44],vdp_blit_nx    ),
		mputii(&header[96+46],vdp_blit_nz    ),
		memcpy(&header[96+48],vdp_table+32,16), // 16 bytes MSX2 BLITTER
		memcpy(&header[96+64],vdp_palette, 32), // 16 palette entries of 2 bytes (0RRR0BBB+00000GGG) each
		memcpy(&header[96+96],vdp_state,   10), // only the first 10 STATUS bytes are used
		memcpy(&header[96+106],vdp_table+25,3), // extra V9958 registers (ZERO unless we're emulating the MSX2P!)
		header[96+111]=msx2p_flags, // MSX2P attributes, if present: ZERO on reset, +128 after the cold boot, etc
		cmos_shrink(&header[208]),header[208+30]=cmos_index,header[208+31]=cmos_count; // RTC is optional in MSX1
	else
		memcpy(&header[96+16],vdp_table,8), // the MSX1 VDP is smaller and simpler
		header[96+96]=vdp_state[0]; // just one STATUS byte!
	// CARTRIDGE (if any)
	memcpy(&header[240],cart_bank,4); // cartridge configuration bytes, all ZERO if no cartridge is present
	fwrite1(header,256,f);
	// VDP 16K/64K + 64K RAM
	j?fwrite1(session_scratch,j,f):fwrite1(vdp_ram,i,f);
	k?fwrite1(&session_scratch[j],k,f):fwrite1(mem_ram,1<<16,f);
	if (type_id) // MSX2: VDP 128K
		kputmmmm(0X56445031,f), // "VDP1"
		snap_savechunk(&vdp_ram[1<<16],1<<16,f);
	j=ram_dirty>>2; for (i=1;i<=j;++i) // don't save unused memory
		fputmmmm((i>9?0X52414D37:0X52414D30)+i,f), // "RAM1".."RAM9", "RAMA".."RAMZ"
		snap_savechunk(&mem_ram[i<<16],1<<16,f);
	if (sram_dirt) // active 8K SRAM pages
		for (j=0X53524D30,i=0;i<=sram_mask;++j,++i)
			fputmmmm(j,f), // "SRM0".."SRM7"
			snap_savechunk(&sram[i<<13],1<<13,f);
	if (sccplus_playing)
		kputmmmm(0X5343432B,f), // "SCC+", the SCC+ status
		snap_savechunk(sccplus_table,256,f); //fputiiii(256,f); fwrite1(sccplus_table,256,f);
	if (sccplus_ready)
	{
		kputmmmm(0X53434330,f), // "SCC0", the SCC+ lower RAM bank ("Snatcher")
		snap_savechunk(sccplus_ram,65536,f);
		if (length(sccplus_ram)>65536)
			kputmmmm(0X53434331,f), // "SCC1", the SCC+ upper RAM bank ("SD Snatcher")
			snap_savechunk(sccplus_ram+65536,65536,f);
	}
	if (opll_playing)
		kputmmmm(0X4F504C4C,f), // "OPLL", the OPLL status
		snap_savechunk(opll_table,64,f); //fputiiii(256,f); fwrite1(sccplus_table,256,f);
	if (!playcity_disabled)
		kputmmmm(0X50534732,f), // "PSG2", the SECOND PSG status
		kputiiii(16+1,f),
		fwrite1(playcity_table[0],16,f),fputc(playcity_index[0],f);
	STRCOPY(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}
int snap_load(char *s) // load a snapshot `s`; zero OK, nonzero error!
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1;
	BYTE header[256]; int i,q;
	if ((fread1(header,256,f)!=256)||(i=header[20]&15)>=length(bios_system)||(
		#ifdef LEMPELZIV_ENCODING
		q=header[14]-'1')>1||(header[14]='1',
		#else
		q=0,
		#endif
		memcmp(header,snap_magic16,16))) return puff_fclose(f),1;
	type_id=i; bios_reload(); int j=mgetii(&header[16]),k=mgetii(&header[18]); // `j` and `k` are zero when the blocks aren't compressed
	// Z80
	z80_af .w=mgetii(&header[32+ 0]);
	z80_bc .w=mgetii(&header[32+ 2]);
	z80_hl .w=mgetii(&header[32+ 4]);
	z80_de .w=mgetii(&header[32+ 6]);
	z80_af2.w=mgetii(&header[32+ 8]);
	z80_bc2.w=mgetii(&header[32+10]);
	z80_hl2.w=mgetii(&header[32+12]);
	z80_de2.w=mgetii(&header[32+14]);
	z80_ix .w=mgetii(&header[32+16]);
	z80_iy .w=mgetii(&header[32+18]);
	z80_sp .w=mgetii(&header[32+20]);
	z80_pc .w=mgetii(&header[32+22]);
	z80_ir.b.h=header[32+24];
	z80_ir.b.l=header[32+25];
	z80_iff .w=header[32+26]&257; // should we be more precise here?
	z80_imd=header[32+28]&3;
	// PSG
	memcpy(psg_table,&header[64+ 0],16);
	psg_table_select(header[64+16]); psg_all_update();
	// PIO
	pio_port_a =header[64+20];
	pio_port_b =header[64+21];
	pio_port_c =header[64+22];
	pio_control=header[64+23];
	// MMU
	memcpy(ram_cfg,&header[64+24],4);
	if (type_id) memcpy(rom_cfg,&header[64+28],4);
	// VDP
	vdp_which=(header[96+1]&1)?header[96+0]:-1; // vdp_which will be -1 ALMOST always!
	vdp_latch=header[96+2];
	vdp_where=mgetii(&header[96+4]);
	video_target-=video_pos_y*VIDEO_LENGTH_X+video_pos_x; frame_pos_y-=video_pos_y; // rewind!
	video_pos_x=mgetii(&header[96+8])>>1; //vdp_count_x=header[96+8];
	vdp_count_y=mgetii(&header[96+10]);
	video_target+=video_pos_y*VIDEO_LENGTH_X+video_pos_x; frame_pos_y+=video_pos_y; // adjust!
	MEMZERO(vdp_table),MEMZERO(vdp_state); // invalidate all the unused registers and states!
	if (type_id)
		memcpy(vdp_table,   &header[96+16],24), // 8 bytes MSX1 + 24 bytes MSX2 w/o BLITTER
		vdp_blit_sx=mgetii (&header[96+40]   ), // BLITTER counters SX, DX, etc. in the unused registers #24-#31
		vdp_blit_dx=mgetii (&header[96+42]   ),
		vdp_blit_nx=mgetii (&header[96+44]   ),
		vdp_blit_nz=mgetii (&header[96+46]   ),
		memcpy(vdp_table+32,&header[96+48],16), // 16 bytes MSX2 BLITTER
		memcpy(vdp_palette ,&header[96+64],32), // 16 palette entries of 2 bytes (0RRR0BBB+00000GGG) each
		memcpy(vdp_state   ,&header[96+96],10), // only the first 10 STATUS bytes are used
		memcpy(vdp_table+25,&header[96+106],3), // extra V9958 registers (ZERO unless we're emulating the MSX2P!)
		msx2p_flags=header[96+111], // MSX2P attributes, if present: ZERO on reset, +128 after the cold boot, etc
		cmos_expand(&header[208]),cmos_index=header[208+30],cmos_count=header[208+31]; // RTC is optional in MSX1
	else
		memcpy(vdp_table,&header[96+16],8), // the MSX1 VDP is smaller and simpler
		vdp_state[0]=header[96+96]; // just one STATUS byte!
	// setting `z80_irq` above was redundant: we're better off calculating it here as follows:
	z80_irq=((vdp_state[0]&128)&&(vdp_table[1]&32)?1:0)+((vdp_state[1]&  1)&&(vdp_table[0]&16)?2:0);
	z80_int=0; // avoid nasty surprises!..?
	// CARTRIDGE (if any)
	memcpy(cart_bank,&header[240],4); // if any byte here isn't zero we have to insert a cartridge!
	if (!cart) // we must also check whether the Z80 PC is located on a cartridge, i.e. PSLOT[PC>>14] is 1
		if ((cart_bank[0]|cart_bank[1]|cart_bank[2]|cart_bank[3])||((pio_port_a>>((z80_pc.w>>14)*2))&3)==1)
			if (cart_insert(cart_path)) MEMZERO(cart_bank); // nuke these variables on failure :-(
	// VDP 16K/64K + 64K RAM
	if (j>=(i=type_id?1<<16:(1<<14))) j=0; // old snapshots wrote "16K" in the MSX1 VDP size
	j?snap_x2bin(vdp_ram,i,session_scratch,fread1(session_scratch,j,f),q):fread1(vdp_ram,i,f);
	k?snap_x2bin(mem_ram,1<<16,session_scratch,fread1(session_scratch,k,f),q):fread1(mem_ram,1<<16,f);
	// extra RAM, VDP and others
	j=1<<16; sccplus_playing=opll_playing=sram_dirt=0; playcity_disabled=1;
	while ((k=fgetmmmm(f),i=fgetiiii(f))>=0)
	{
		/**/ if (k==0X56445031&&i<=65536) // "VDP1", upper VDP 64K page
		{
			i=snap_loadchunk(&vdp_ram[1<<16],1<<16,f,i,q);
		}
		else if (k>=0X52414D30&&k<=0X52414D5A&&i<=65536) // "RAM1..RAM9", "RAMA..RAMZ", upper 64K RAM pages
		{
			i=snap_loadchunk(&mem_ram[(k-=(k>0X52414D39?0X52414D37:0X52414D30))<<16],1<<16,f,i,q);
			if (j<(k=(k+1)<<16)) j=k; // remember highest page
		}
		else if (k>=0X53524D30&&k<=0X53524D37&&i<=8192) // "SRM0..SRM7", active 8K SRAM pages
		{
			sram_dirt=1; sram_mask|=(k&=7); // cart_insert() will get sram_mask wrong if the cartridge doesn't match
			i=snap_loadchunk(&sram[k<<13],1<<13,f,i,q);
		}
		else if (k==0X5343432B&&i<=256) // "SCC+", the SCC+ status
		{
			i=snap_loadchunk(sccplus_table,256,f,i,q); //fread1(sccplus_table,256,f); i-=256;
		}
		else if (k==0X53434330&&i<=65536) // "SCC0", the SCC+ lower RAM bank ("Snatcher")
		{
			sccplus_ready=1,i=snap_loadchunk(sccplus_ram,65536,f,i,q);
		}
		else if (k==0X53434331&&i<=65536) // "SCC1", the SCC+ upper RAM bank ("SD Snatcher")
		{
			if (length(sccplus_ram)>65536)
				sccplus_ready=1,i=snap_loadchunk(sccplus_ram+65536,65536,f,i,q);
		}
		else if (k==0X4F504C4C&&i<= 64) // "OPLL", the OPLL status
		{
			i=snap_loadchunk(opll_table, 64,f,i,q); //fread1(sccplus_table,256,f); i-=256;
		}
		else if (k==0X50534732&&i== 17) // "PSG2", the SECOND PSG status
		{
			playcity_disabled=0,fread1(playcity_table[0],16,f),playcity_index[0]=fgetc(f),i-=17;
		}
		// ... future blocks will go here ...
		else cprintf("SNAP %08X:%08X?\n",k,i); // unknown type:size
		{ if (i<0) return puff_fclose(f),1; } fseek(f,i,SEEK_CUR); // abort on error!
	}
	ram_dirty=(j>>14)-1; i=0; while (j>65536) ++i,j>>=1;
	if (i>ram_maxcfg) i=ram_maxcfg; // sanity check!
	if (ram_depth<i) ram_setcfg(i+1); // update extended RAM config
	mmu_update(); vdp_update(); vdp_mode_update(); vdp_blit_update(); video_xlat_clut(); if (sccplus_playing) sccplus_update(); if (opll_playing) opll_update();

	debug_reset();
	MEMBYTE(z80_tape_index,-1); // TAPE_FASTLOAD, avoid false positives!
	STRCOPY(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

// "autorun" file and logic operations ------------------------------ //

int any_load(char *s,int q) // load a file regardless of format. `s` path, `q` autorun; 0 OK, !0 ERROR
{
	autorun_t=0; if (!s||!*s) return -1; // cancel any autoloading yet; reject invalid paths
	if (!video_table_load(s)) video_main_xlat(),video_xlat_clut(); else if (!cart_hotfix(s)) {} else // colour palette, cartridge patch!
	if (snap_load(s))
		if (disc_open(s,0,!(disc_filemode&1))) // use same setting as DEFAULT READ-ONLY
			if (tape_open(s))
				if (bios_load(s))
					if (cart_insert(s))
						return bios_reload(),1; // everything failed!
					else
					{
						if (q&&s[strlen(s)-1]=='2'&&!type_id) { type_id=1; bios_reload(); } // ".mx2" file? switch from MSX1 to MSX2
						//else if (q&&s[strlen(s)-1]=='1'&&type_id) { type_id=0; bios_reload(); } // ditto, but ".mx1" (...bad idea?)
						all_reset(); // a new cartridge forces a reset
					}
				else
					cart_remove(),all_reset(); // BIOS removes cartridge and resets
			else
			{
				if (q) // disable disc drive and write RUN"CAS: or BLOAD"CAS:",R
				{
					cart_remove(),all_reset(),disc_disabled|=2;
					if (globbing("*cload*",s,1)) // least likely (<10%)
						autorun_t=10,autorun_s="cload\015run\015";
					else if (globbing("*bload*",s,1)) // halfway (>30%)
						autorun_t=14,autorun_s="bload\042cas:\042,r\015";
					else // most likely (>50%!)
						autorun_t=10,autorun_s="run\042cas:\042\015";
				}
			}
		else
			{ if (q) cart_remove(),all_reset(),disc_disabled=0; } // enable disc drive, the MSX firmware will do the autorun on its own (?)
	if (q) STRCOPY(autorun_path,s);
	return 0;
}

char txt_error_snap_save[]="Cannot save snapshot!";
char file_pattern[]="*.cas;*.csw;*.dsk;*.ips;*.mx1;*.mx2;*.rom;*.stx;*.tsx;*.vpl;*.wav"; // from A to Z

char session_menudata[]=
	"File\n"
	"0x8300 Open any file..\tF3\n"
	"0xC300 Load snapshot..\tShift-F3\n"
	"0x0300 Load last snapshot\tCtrl-F3\n"
	"0x8200 Save snapshot..\tF2\n"
	"0x0200 Save last snapshot\tCtrl-F2\n"
	"=\n"
	"0x8700 Insert disc into A:..\tF7\n"
	"0x8701 Create disc in A:..\n"
	//"0x0701 Flip disc sides in A:\n"
	"0x0700 Remove disc from A:\tCtrl-F7\n"
	"0xC700 Insert disc into B:..\tShift-F7\n"
	"0xC701 Create disc in B:..\n"
	//"0x4701 Flip disc sides in B:\n"
	"0x4700 Remove disc from B:\tCtrl-Shift-F7\n"
	"=\n"
	"0x8800 Insert tape..\tF8\n"
	"0xC800 Record tape..\tShift-F8\n"
	"0x8801 Browse tape..\n"
	"0x0800 Remove tape\tCtrl-F8\n"
	//"0x4800 Play tape\tCtrl-Shift-F8\n"
	"0x4801 Flip tape polarity\n"
	"=\n"
	"0xC500 Insert cartridge..\tShift-F5\n"
	"0x4500 Remove cartridge\tCtrl-Shift-F5\n"
	"0x0510 Patch cartridge..\n"
	"=\n"
	"0x0080 E_xit\n"
	"Edit\n"
	"0x8500 Select firmware..\tF5\n"
	"0x0500 Reset emulation\tCtrl-F5\n"
	"0x8F00 Pause\tPause\n"
	"0x8900 Debug\tF9\n"
	"=\n"
	"0x8511 64K RAM w/o mapper\n"
	"0x8512 64K RAM\n"
	"0x8513 128K RAM\n"
	"0x8514 256K RAM\n"
	"0x8515 512K RAM\n"
	"0x8516 1024K RAM\n"
	"0x8517 2048K RAM\n"
	"0x8501 Generic 8K mapper\n"
	"0x8502 Generic 16K mapper\n"
	"0x8504 Konami 8K mapper\n" // a.k.a. Konami4
	"0x8507 Konami 8K + SRAM\n" // "GAME MASTER 2"
	"0x8503 Konami SCC mapper\n" // a.k.a. Konami5
	"0x8505 ASCII 8K mapper\n"
	"0x8508 ASCII 8K + SRAM\n" // a.k.a. KOEI SRAM
	"0x8506 ASCII 16K mapper\n"
	"0x8509 ASCII 16K + SRAM\n" // "HYDLIDE 2"...
	"0x850A Miscellaneous mapper\n"
	"0x850E MSX-MUSIC (YM2413)\n"
	"0x850F Play 2ND PSG audio\n"
	"=\n"
	"0x8B08 Custom VDP palette..\n"
	"0x8B18 Reset VDP palette\n"
	"0x850B Disable sprites\n"
	"0x850D Show \"illegal\" sprites\n"
	"0x850C Cancel sprite collisions\n"
	"Settings\n"
	"0x8601 1" I18N_MULTIPLY " real speed\n"
	"0x8602 2" I18N_MULTIPLY " real speed\n"
	"0x8603 3" I18N_MULTIPLY " real speed\n"
	"0x8604 4" I18N_MULTIPLY " real speed\n"
	"0x8600 Run at full throttle\tF6\n"
	//"0x0600 Raise Z80 speed\tCtrl-F6\n"
	//"0x4600 Lower Z80 speed\tCtrl-Shift-F6\n"
	"0x0601 1" I18N_MULTIPLY " CPU clock\n"
	"0x0602 2" I18N_MULTIPLY " CPU clock\n"
	"0x0603 4" I18N_MULTIPLY " CPU clock\n"
	"0x0604 8" I18N_MULTIPLY " CPU clock\n"
	"=\n"
	"0x0400 Virtual joystick\tCtrl-F4\n"
	"0x0401 Redefine virtual joystick\n"
	"0x4400 Flip joystick ports\tCtrl-Shift-F4\n"
	"0x4401 Flip joystick buttons\n"
	"=\n"
	"0x851F Strict snapshots\n"
	"0x852F Printer output..\n"
	"0x8510 Enable disc drives\tShift-F6\n"
	"0x8590 Strict disc writes\n"
	"0x8591 Read-only disc by default\n"
	//"=\n"
	"0x0900 Tape speed-up\tCtrl-F9\n"
	"0x4900 Tape analysis\tCtrl-Shift-F9\n"
	"0x0901 Tape auto-rewind\n"
	"0x0605 Power-up boost\n"
	"Audio\n"
	"0x8400 Sound playback\tF4\n"
	"0x8A01 No acceleration\n"
	"0x8A02 Light acceleration\n"
	"0x8A03 Middle acceleration\n"
	"0x8A04 Heavy acceleration\n"
	#if AUDIO_CHANNELS > 1
	"0xC401 0% stereo\n"
	"0xC402 25% stereo\n"
	"0xC403 50% stereo\n"
	"0xC404 100% stereo\n"
	#endif
	"=\n"
	"0x8401 No filtering\n"
	"0x8402 Light filtering\n"
	"0x8403 Middle filtering\n"
	"0x8404 Heavy filtering\n"
	"=\n"
	"0x4C00 Record YM file\tCtrl-Shift-F12\n"
	"0x0C00 Record WAV file\tCtrl-F12\n"
	#if AUDIO_BITDEPTH > 8
	"0x8C03 High wavedepth\n"
	#endif
	"Video\n"
	"0x8901 Onscreen status\tShift-F9\n"
	"0x8B01 Monochrome\n"
	"0x8B02 Dark palette\n"
	"0x8B03 Normal palette\n"
	"0x8B04 Light palette\n"
	"0x8B05 Green screen\n"
	//"0x8B00 Next palette\tF11\n"
	//"0xCB00 Prev. palette\tShift-F11\n"
	"=\n"
	"0x8907 Microwave static\n"
	"0x8903 X-masking\n"
	"0x8902 Y-masking\n"
	//"0x0B00 Next scanline\tCtrl-F11\n"
	//"0x4B00 Prev. scanline\tCtrl-Shift-F11\n"
	"0x0B01 All scanlines\n"
	"0x0B03 Simple interlace\n"
	"0x0B04 Double interlace\n"
	"0x0B02 Average scanlines\n"
	"0x8904 X-blending\n"
	"0x8905 Y-blending\n"
	"0x8906 Gigascreen\n"
	#ifdef VIDEO_FILTER_BLUR0
	"0x8909 Fine Giga/X-blend\n"
	#endif
	"=\n"
	"0x9100 Raise frameskip\tNum.+\n"
	"0x9200 Lower frameskip\tNum.-\n"
	"0x9300 Full frameskip\tNum.*\n"
	"0x9400 No frameskip\tNum./\n"
	"=\n"
	"0x8C00 Save screenshot\tF12\n"
	"0x8C04 Output " SESSION_SCRN_EXT " format\n"
	"0xCC00 Record film\tShift-F12\n"
	"0x8C02 High framerate\n"
	"0x8C01 High resolution\n"
	"Window\n"
	"0x8A00 Blitter acceleration*\n"
	"0x8A10 Full screen\tAlt+Return\n"
	"0x8A11 100% zoom\n"
	"0x8A12 150% zoom\n"
	"0x8A13 200% zoom\n"
	"0x8A14 250% zoom\n"
	"0x8A15 300% zoom\n"
	"Help\n"
	"0x8100 Help..\tF1\n"
	"0x0100 About..\tCtrl-F1\n"
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
	#if AUDIO_BITDEPTH > 8
	session_menucheck(0x8C03,!session_wavedepth);
	#endif
	session_menucheck(0x8C04,session_scrn_flag);
	session_menucheck(0xCC00,!!session_filmfile);
	session_menucheck(0x0C00,!!session_wavefile);
	session_menucheck(0x4C00,!!ym3_file);
	session_menucheck(0x8700,!!disc[0]);
	session_menucheck(0xC700,!!disc[1]);
	//session_menucheck(0x0701,disc_flip[0]);
	//session_menucheck(0x4701,disc_flip[1]);
	session_menucheck(0x8800,tape_type>=0&&tape);
	session_menucheck(0xC800,tape_type<0&&tape);
	//session_menucheck(0x4800,tape_enabled);
	session_menucheck(0x4801,tape_polarity);
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	session_menucheck(0x0400,session_key2joy);
	session_menucheck(0x4400,joystick_bit);
	session_menucheck(0x4401,key2joy_flag);
	session_menuradio(0x0601+multi_t,0x0601,0x0604);
	session_menucheck(0x0605,power_boost-POWER_BOOST0);
	//session_menucheck(0x4400,litegun);
	session_menucheck(0x8590,!(disc_filemode&2));
	session_menucheck(0x8591,disc_filemode&1);
	session_menucheck(0x8510,!(disc_disabled&1));
	session_menucheck(0x850D,vdp_legalsprites>=32);
	session_menucheck(0x850B,vdp_finalsprite<32);
	session_menucheck(0x850C,!vdp_impactsprite);
	session_menucheck(0x850E,opll_internal);
	session_menuradio(0x8511+ram_getcfg(),0x8511,0x8517);
	session_menuradio(0x8501+cart_id,0x8501,0x8501+CART_IDS-1);
	#ifdef PSG_PLAYCITY
	session_menucheck(0x850F,!playcity_disabled);
	#endif
	session_menucheck(0xC500,!!cart);
	session_menucheck(0x851F,!(snap_extended));
	session_menucheck(0x852F,!!printer);
	session_menucheck(0x8901,onscreen_flag);
	session_menucheck(0x8902,video_filter&VIDEO_FILTER_MASK_Y);
	session_menucheck(0x8903,video_filter&VIDEO_FILTER_MASK_X);
	session_menucheck(0x8904,video_filter&VIDEO_FILTER_MASK_Z);
	session_menucheck(0x8905,video_lineblend);
	session_menucheck(0x8906,video_pageblend);
	session_menucheck(0x8907,video_microwave);
	#ifdef VIDEO_FILTER_BLUR0
	session_menucheck(0x8909,video_fineblend);
	#endif
	session_menuradio(0x8A10+(session_fullblit?0:1+session_zoomblit),0X8A10,0X8A15);
	session_menucheck(0x8A00,!session_softblit);
	session_menuradio(0x8A01+session_softplay,0x8A01,0x8A04);
	session_menuradio(0x8B01+video_type,0x8B01,0x8B05);
	session_menuradio(0x0B01+video_scanline,0x0B01,0x0B04);
	kbd_joy[4]=kbd_joy[6]=0164+key2joy_flag;
	kbd_joy[5]=kbd_joy[7]=0165-key2joy_flag;
	#if AUDIO_CHANNELS > 1
	session_menuradio(0xC401+audio_mixmode,0xC401,0xC404);
	// according to https://www.msx.org/wiki/Category:PSG most brands doing stereo chose MIDDLE:LEFT:RIGHT
	psg_stereo[0][0]=256+audio_stereos[audio_mixmode][1],psg_stereo[0][1]=256-audio_stereos[audio_mixmode][1];
	psg_stereo[1][0]=256+audio_stereos[audio_mixmode][0],psg_stereo[1][1]=256-audio_stereos[audio_mixmode][0];
	psg_stereo[2][0]=256+audio_stereos[audio_mixmode][2],psg_stereo[2][1]=256-audio_stereos[audio_mixmode][2];
	#ifdef PSG_PLAYCITY
	for (int i=0;i<3;++i)
		playcity_stereo[0][i][0]=psg_stereo[i][0],playcity_stereo[0][i][1]=psg_stereo[i][1]; // TURBO SOUND chip shares stereo with the PSG
	#endif
	// is the Konami SCC stereo? let's assume it's M:L:R:LM:MR -- voices 4 and 5 must be in opposite sides, and "F1 SPIRIT" uses voice 1 as the engine
	sccplus_stereo[0][0]=psg_stereo[0][0];
	sccplus_stereo[0][1]=psg_stereo[0][1];
	sccplus_stereo[3][0]=((sccplus_stereo[1][0]=psg_stereo[1][0])+psg_stereo[0][0])>>1;
	sccplus_stereo[3][1]=((sccplus_stereo[1][1]=psg_stereo[1][1])+psg_stereo[0][1])>>1;
	sccplus_stereo[4][0]=((sccplus_stereo[2][0]=psg_stereo[2][0])+psg_stereo[0][0])>>1;
	sccplus_stereo[4][1]=((sccplus_stereo[2][1]=psg_stereo[2][1])+psg_stereo[0][1])>>1;
	// OPLL was either fully mono or assigned one side to instruments and the other side to drums :-/
	for (int c=0;c<length(opll_stereo);++c)
		opll_stereo[c][0]=psg_stereo[0][0],opll_stereo[c][1]=psg_stereo[0][1];
	#endif
	video_resetscanline(),debug_dirty=1; sprintf(session_info,"%d:%dK %s %s %0.1fMHz"
		,16*(ram_dirty+1),16*(ram_bit+1)
		,type_id>1?"MSX2+":type_id?"MSX2":"MSX"
		,session_ntsc(i18n_ntsc)?i18n_kana?"JPN":"USA":"PAL" // always call session_ntsc()!
		,3.6*(1<<multi_t)); // actually 3.58 :-P
	TICKS_PER_SECOND=(TICKS_PER_FRAME=(LINES_PER_FRAME=i18n_ntsc?262:313)*TICKS_PER_LINE)*VIDEO_PLAYBACK;
}
void session_user(int k) // handle the user's commands
{
	char *s; switch (k)
	{
		case 0x8100: // F1: HELP..
			session_message(
				//"Key\t\t" MESSAGEBOX_WIDETAB "+Control\n"
				"F1\tHelp..\t" MESSAGEBOX_WIDETAB
				"^F1\tAbout..\t"
				"\n"
				"F2\tSave snapshot.." MESSAGEBOX_WIDETAB
				"^F2\tSave last snapshot"
				"\n"
				"F3\tOpen any file.." MESSAGEBOX_WIDETAB
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
				"\t(shift: load cart..)" "\t"
				"\t(shift: eject cart)"
				"\n"
				"F6\tToggle realtime" MESSAGEBOX_WIDETAB
				"^F6\tNext CPU speed"
				"\n"
				"\t(shift: ..disc cnt.)\t"
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
				//"\t(shift: play/stop)" // "\t"
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
				"\n" // "\n"
				"Num.+\tUpper frameskip" MESSAGEBOX_WIDETAB
				"Num.*\tFull frameskip"
				"\n"
				"Num.-\tLower frameskip" MESSAGEBOX_WIDETAB
				"Num./\tNo frameskip"
				"\n"
				"Pause\tPause/continue" MESSAGEBOX_WIDETAB
				"*Return\tMaximize/restore" "\t" // this `t` sets the window width
				"\n"
				"*Up\tIncrease zoom" MESSAGEBOX_WIDETAB
				"*Down\tDecrease zoom"
				"\n"
				"*Right\tSpeed action up" MESSAGEBOX_WIDETAB
				"*Left\tSlow action down"
				// "\n" "^Key =\tControl+Key" MESSAGEBOX_WIDETAB "*Key =\tAlt+Key\n"
			,"Help");
			break;
		case 0x0100: // ^F1: ABOUT..
			session_aboutme(
				"MSX 1/2/2+ emulator written by Cesar Nicolas-Gonzalez\n"
				"(UNED 2019 Master's Degree in Computer Engineering)\n"
				"\nnews and updates: http://cngsoft.no-ip.org/cpcec.htm"
				"\n\n" MY_LICENSE "\n\n" GPL_3_INFO
			,session_caption);
			break;
		case 0x8200: // F2: SAVE SNAPSHOT..
			if (s=puff_session_newfile(snap_path,snap_pattern,"Save snapshot"))
			{
				if (snap_save(s))
					session_message(txt_error_snap_save,txt_error);
				else STRCOPY(autorun_path,s);
			}
			break;
		case 0x0200: // ^F2: RESAVE SNAPSHOT
			if (snap_done&&*snap_path)
				if (snap_save(snap_path))
					session_message(txt_error_snap_save,txt_error);
			break;
		case 0x8300: // F3: OPEN ANY FILE.. // LOAD SNAPSHOT..
			if (puff_session_getfile(session_shift?snap_path:autorun_path,session_shift?snap_pattern:file_pattern,session_shift?"Load snapshot":"Load file"))
		case 0x8000: // DRAG AND DROP
			{
				if (multiglobbing(puff_pattern,session_parmtr,1)) // ZIP archive, pick a file
					if (!puff_session_zipdialog(session_parmtr,file_pattern,"Load file"))
						break; // user aborted the procedure!
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
			#if AUDIO_CHANNELS > 1
			if (session_shift) // next stereo mode
				{ if (!session_filmfile) { if (++audio_mixmode>=length(audio_stereos)) audio_mixmode=0; } }
			else
			#endif
			if (session_audio)
				audio_disabled^=1;
			break;
		case 0x8401:
		case 0x8402:
		case 0x8403:
		case 0x8404:
			#if AUDIO_CHANNELS > 1
			if (session_shift) // SET STEREO MODE
				{ if (!session_filmfile) audio_mixmode=k-0x8401; }
			else
			#endif
			audio_filter=k-0x8401;
			break;
		case 0x0400: // ^F4: TOGGLE JOYSTICK
			session_shift?(joystick_bit=!joystick_bit):(session_key2joy=!session_key2joy);
			break;
		case 0x0401: // REDEFINE VIRTUAL JOYSTICK
			if (session_shift) { key2joy_flag=!key2joy_flag; break; }
			{
				int t[KBD_JOY_UNIQUE];
				if ((t[0]=session_scan("UP"))>=0)
				if ((t[1]=session_scan("DOWN"))>=0)
				if ((t[2]=session_scan("LEFT"))>=0)
				if ((t[3]=session_scan("RIGHT"))>=0)
				if ((t[4]=session_scan("FIRE 1"))>=0)
				if ((t[5]=session_scan("FIRE 2"))>=0)
					for (int i=0;i<KBD_JOY_UNIQUE;++i) kbd_k2j[i]=t[i];
			}
			break;
		case 0x850D:
			vdp_legalsprites=vdp_legalsprites>=32?0:32; // i.e. show 4 or 8 sprites per line or all 32
			break;
		case 0x850B:
			vdp_finalsprite=vdp_finalsprite?0:32; // i.e. render no sprites at all, or all 32
			break;
		case 0x850C:
			vdp_impactsprite=vdp_impactsprite?0:32; // i.e. impact mask (bit 5) off or on
			break;
		case 0x8590: // STRICT DISC WRITES
			disc_filemode^=2;
			break;
		case 0x8591: // DEFAULT READ-ONLY
			disc_filemode^=1;
			break;
		case 0x851F:
			snap_extended=!snap_extended;
			break;
		case 0x8500: // F5: LOAD FIRMWARE.. // INSERT CARTRIDGE..
			if (session_shift)
			{
				if (s=puff_session_getfile(cart_path,"*.mx1;*.mx2;*.rom","Insert cartridge"))
					if (cart_insert(s))
						session_message("Cannot insert cartridge!",txt_error);
					else
						all_reset(); // setup and reset!
			}
			else if (s=puff_session_getfile(bios_path,"*.rom","Load firmware"))
			{
				if (bios_load(s)) // error? warn and undo!
					session_message(txt_error_bios,txt_error),bios_reload(); // reload valid firmware, if required
				else
					all_reset(); // setup and reset
			}
			break;
		case 0x0500: // ^F5: RESET EMULATION // REMOVE CARTRIDGE
			if (session_shift)
				{ if (cart) cart_remove(); }
			//else // redundant!
			all_reset();
			break;
		case 0x0510: // PATCH CARTRIDGE
			if (cart) // no cartridge, nothing to do
				if (s=puff_session_getfile(cart_path,"*.ips","Patch cartridge"))
					if (cart_hotfix(s))
						session_message("Cannot patch cartridge!",txt_error);
			break;
		case 0x8511: // 64K RAM W/O MAPPER
		case 0x8512: // 64K RAM
		case 0x8513: // 128K RAM
		case 0x8514: // 256K RAM
		case 0x8515: // 512K RAM
		case 0x8516: // 1024K RAM
		case 0x8517: // 2048K RAM
			ram_setcfg(k-0x8511); mmu_update();
			break;
		case 0x8501: // GENERIC 8K MAPPER
		case 0x8502: // GENERIC 16K MAPPER
		case 0x8503: // KONAMI SCC MAPPER
		case 0x8504: // KONAMI 8K MAPPER
		case 0x8505: // ASCII 8K MAPPER
		case 0x8506: // ASCII 16K MAPPER
		case 0x8507: // KONAMI 8K + SRAM (GAME MASTER 2)
		case 0x8508: // ASCII 8K + SRAM (KOEI SRAM)
		case 0x8509: // ASCII 16K + SRAM MAPPER
		case 0x850A: // MISCELLANEOUS MAPPER
			if (cart_id!=(k-=0x8501)) { cart_id=k; if (cart&&!cart_big) all_reset(); } // don't reset if nothing changes
			break;
		case 0x850E: // MSX-MUSIC OPLL
			opll_internal=!opll_internal; opll_playing=0;
			break;
		#ifdef PSG_PLAYCITY
		case 0x850F: // SECOND PSG (PORT $10)
			if (playcity_disabled=!playcity_disabled)
				playcity_reset();
			break;
		#endif
		case 0x852F: // PRINTER
			if (printer) printer_close(); else
				if (s=session_newfile(NULL,"*.txt","Printer output"))
					if (printer_p=0,!(printer=fopen(s,"wb")))
						session_message("Cannot record printer!",txt_error);
			break;
		case 0x8600: // F6: TOGGLE REALTIME
			if (!session_shift)
				{ session_fast^=1; break; }
			// +SHIFT: no `break`!
		case 0x8510: // DISC DRIVE
			disc_disabled^=1; // disabling the disc drive while the firmware is on will freeze the machine!
			break;
		case 0x8B08: // LOAD VDP PALETTE..
			if (s=puff_session_getfile(palette_path,"*.vpl","Custom VDP palette"))
			{
				if (video_table_load(s))
					session_message("Cannot load VDP palette!",txt_error);
				else
					video_main_xlat(),video_xlat_clut(); // no need for video_wide_xlat()
			}
			break;
		case 0x8B18: // RESET VDP PALETTE
			video_table_reset(); video_main_xlat(),video_xlat_clut(); // no need for video_wide_xlat() here either
			break;
		case 0x8601: // REALTIME x1
		case 0x8602: // REALTIME x2
		case 0x8603: // REALTIME x3
		case 0x8604: // REALTIME x4
			session_rhythm=k-0x8600-1; session_fast&=~1;
			break;
		case 0x0600: // ^F6: TOGGLE TURBO Z80
			multi_u=(1<<(multi_t=(multi_t+(session_shift?-1:1))&3))-1;
			//mmu_update(); // turbo cancels all contention!
			break;
		case 0x0601: // CPU x1
		case 0x0602: // CPU x2
		case 0x0603: // CPU x3
		case 0x0604: // CPU x4
			multi_u=(1<<(multi_t=k-0x0601))-1;
			break;
		case 0x0605: // POWER-UP BOOST
			power_boost^=POWER_BOOST1^POWER_BOOST0;
			break;
		case 0x8701: // CREATE DISC..
			if (!disc_disabled)
				if (s=puff_session_newfile(disc_path,"*.dsk",session_shift?"Create disc in B:":"Create disc in A:"))
				{
					if (disc_create(s))
						session_message("Cannot create disc!",txt_error);
					else
						disc_open(s,session_shift,1);
				}
			break;
		case 0x8700: // F7: INSERT DISC..
			if (!disc_disabled)
				if (s=puff_session_getfilereadonly(disc_path,"*.dsk",session_shift?"Insert disc into B:":"Insert disc into A:",disc_filemode&1))
					if (disc_open(s,session_shift,!session_filedialog_get_readonly()))
						session_message("Cannot open disc!",txt_error);
			break;
		case 0x0700: // ^F7: EJECT DISC
			disc_close(session_shift);
			break;
		case 0x8800: // F8: INSERT OR RECORD TAPE..
			if (session_shift)
			{
				if (s=puff_session_newfile(tape_path,"*.csw","Record tape"))
					if (tape_create(s))
						session_message("Cannot create tape!",txt_error);
			}
			else if (s=puff_session_getfile(tape_path,"*.cas;*.csw;*.tsx;*.wav","Insert tape"))
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
				;//tape_enabled=!tape_enabled;
			else
				tape_close();
			break;
		case 0x0801: // TAPE POLARITY
			if (session_shift)
				tape_polarity=!tape_polarity;
			break;
		case 0x8900: // F9: DEBUG
			if (!session_shift)
				{ session_signal=SESSION_SIGNAL_DEBUG^(session_signal&~SESSION_SIGNAL_PAUSE),debug_clean(); break; }
			// no `break`!
		case 0x8901:
			onscreen_flag^=1;
			break;
		case 0x8902: // Y-MASKING
			if (!session_filmfile) video_filter^=VIDEO_FILTER_MASK_Y;
			break;
		case 0x8903: // X-MASKING
			if (!session_filmfile) video_filter^=VIDEO_FILTER_MASK_X;
			break;
		case 0x8904: // X-BLENDING
			video_filter^=VIDEO_FILTER_MASK_Z;
			break;
		case 0x8905: // Y-BLENDING
			video_lineblend^=1;
			break;
		case 0x8906: // FRAME BLENDING (GIGASCREEN)
			video_pageblend^=1;
			break;
		case 0x8907: // MICROWAVES
			video_microwave^=1;
			break;
		#ifdef VIDEO_FILTER_BLUR0
		case 0x8909: // FINE/COARSE X-BLENDING
			video_fineblend^=1;
			break;
		#endif
		case 0x0900: // ^F9: TOGGLE FAST LOAD OR FAST TAPE
			if (session_shift)
				tape_fastload^=1;
			else
				tape_skipload^=1;
			break;
		case 0x0901:
			tape_rewind^=1;
			break;
		case 0x8A10: // FULL SCREEN
			session_fullblit^=1; session_resize();
			break;
		case 0x8A11: // WINDOW SIZE 100%, etc
		case 0x8A12: case 0x8A14:
		case 0x8A13: case 0x8A15:
			session_fullblit=0,session_zoomblit=k-0x8A11; session_resize();
			break;
		case 0x8A00: // VIDEO ACCELERATION / SOFTWARE RENDER (*needs restart)
			session_softblit^=1;
			break;
		case 0x8A01: case 0x8A02: case 0x8A03:
		case 0x8A04: // AUDIO ACCELERATION (*needs restart)
			session_softplay=k-0x8A01;
			break;
		case 0x8B01: // MONOCHROME
		case 0x8B02: // DARK PALETTE
		case 0x8B03: // NORMAL PALETTE
		case 0x8B04: // LIGHT PALETTE
		case 0x8B05: // GREEN SCREEN
			video_type=k-0x8B01;
			video_main_xlat(),video_wide_xlat(),video_xlat_clut();
			break;
		case 0x8B00: // F11: PALETTE
			video_type=(video_type+(session_shift?4:1))%5;
			video_main_xlat(),video_wide_xlat(),video_xlat_clut();
			break;
		case 0x0B01: // ALL SCANLINES
		case 0x0B03: // SIMPLE INTERLACE
		case 0x0B04: // DOUBLE INTERLACE
		case 0x0B02: // AVG. SCANLINES
			if (!session_filmfile) video_scanline=k-0x0B01;
			break;
		case 0x0B00: // ^F11: SCANLINES
			if (session_filmfile) {} else if (session_shift)
				{ if (!(video_filter=(video_filter+1)&7)) video_lineblend^=1; }
			else if ((video_scanline=video_scanline+1)>3)
				{ if (video_scanline=0,video_pageblend^=1) video_microwave^=1; }
			break;
		case 0x8C01:
			if (!session_filmfile)
				session_filmscale^=1;
			break;
		case 0x8C02:
			if (!session_filmfile)
				session_filmtimer^=1;
			break;
		#if AUDIO_BITDEPTH > 8
		case 0x8C03:
			if (!session_filmfile&&!session_wavefile)
				session_wavedepth^=1;
			break;
		#endif
		case 0X8C04: // OUTPUT AS BMP
			session_scrn_flag^=1;
			break;
		case 0x8C00: // F12: SAVE SCREENSHOT OR RECORD FILM
			if (!session_shift)
				session_savebitmap();
			else if (session_closefilm())
					session_createfilm();
			break;
		case 0x0C00: // ^F12: RECORD WAVEFILE OR YM FILE
			if (session_shift)
			{
				if (ym3_close()) // toggles recording
					if (ym3_nextfile=session_savenext("%s%08u.ym",ym3_nextfile))
						ym3_create(session_parmtr);
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
			video_framelimit|=+MAIN_FRAMESKIP_MASK;
			break;
		case 0x9400: // ^NUM./
		case 0x1400: // NUM./: MINIMUM FRAMESKIP
			video_framelimit&=~MAIN_FRAMESKIP_MASK;
			break;
	}
}

void session_configreadmore(char *s) // parse a pre-processed configuration line: `session_parmtr` keeps the value name, `s` points to its value
{
	int i; char *t=UTF8_BOM(session_parmtr); if (!s||!*s||!session_parmtr[0]) {} // ignore if empty or internal!
	else if (i=eval_hex(*s)&31,!strcasecmp(t,"type")) { if (i<length(bios_system)) type_id=i; }
	else if (!strcasecmp(t,"joy1")) joystick_bit=i&1,key2joy_flag=(i>>1)&1;
	else if (!strcasecmp(t,"bank")) { if (i<=ram_maxcfg) ram_setcfg(i); }
	else if (!strcasecmp(t,"unit")) disc_filemode=i&3,disc_disabled=(i>>2)&1;
	else if (!strcasecmp(t,"misc")) snap_extended=i&1,playcity_disabled=(~i>>1)&1,opll_internal=(i>>2)&1;
	else if (!strcasecmp(t,"cmos")) cmos_import(s);
	else if (!strcasecmp(t,"file")) strcpy(autorun_path,s);
	else if (!strcasecmp(t,"snap")) strcpy(snap_path,s);
	else if (!strcasecmp(t,"tape")) strcpy(tape_path,s);
	else if (!strcasecmp(t,"disc")) strcpy(disc_path,s);
	else if (!strcasecmp(t,"bios")) strcpy(bios_path,s);
	else if (!strcasecmp(t,"cart")) strcpy(cart_path,s);
	else if (!strcasecmp(t,"rgbs")) strcpy(palette_path,s);
	else if (!strcasecmp(t,"vjoy")) { if (!hexa2byte(session_parmtr,s,KBD_JOY_UNIQUE)) usbkey2native(kbd_k2j,session_parmtr,KBD_JOY_UNIQUE); }
	else if (!strcasecmp(t,"palette")) { if (i<5) video_type=i; }
	else if (!strcasecmp(t,"casette")) tape_rewind=i&1,tape_skipload=(i>>1)&1,tape_fastload=(i>>2)&1;
	else if (!strcasecmp(t,"debug")) debug_configread(strtol(s,NULL,10));
}
void session_configwritemore(FILE *f) // update the configuration file `f` with emulator-specific names and values
{
	native2usbkey(kbd_k2j,KBD_JOY_UNIQUE); fprintf(f,"type %d\njoy1 %d\nbank %d\nunit %d\nmisc %d\ncmos %s\n"
		"file %s\nsnap %s\ntape %s\ndisc %s\nbios %s\ncart %s\nrgbs %s\n"
		"vjoy %s\npalette %d\ncasette %d\ndebug %d\n",
		type_id,(joystick_bit&1)+(key2joy_flag&1)*2,ram_getcfg(),(disc_disabled&1)*4+disc_filemode,snap_extended+(playcity_disabled?0:2)+(opll_internal&1)*4,cmos_export(&session_parmtr[KBD_JOY_UNIQUE*2+2]),
		autorun_path,snap_path,tape_path,disc_path,bios_path,cart_path,palette_path,
		byte2hexa0(session_parmtr,kbd_k2j,KBD_JOY_UNIQUE),video_type,tape_rewind+tape_skipload*2+tape_fastload*4,debug_configwrite());
}

// START OF USER INTERFACE ========================================== //

int main(int argc,char *argv[])
{
	session_prae(ARGVZERO); all_setup(); all_reset();
	int i=0,j,k=0; while (++i<argc)
		if (argv[i][0]=='-')
		{
			j=1; do
				switch (argv[i][j++])
				{
					case 'c':
						video_scanline=(BYTE)(argv[i][j++]-'0');
						if (video_scanline<0||video_scanline>7)
							i=argc; // help!
						else
							video_pageblend=video_scanline&1,video_scanline>>=1;
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
						cart_id=(BYTE)(argv[i][j++]-'0');
						if (cart_id<0||cart_id>=CART_IDS)
							i=argc; // help!
						break;
					//case 'i': break;
					//case 'I': break;
					case 'j':
						session_key2joy=1;
						break;
					case 'J':
						session_stick=0;
						break;
					case 'k':
						ram_depth=(BYTE)(argv[i][j++]-'0');
						if (ram_depth<0||ram_depth>ram_maxcfg)
							i=argc; // help!
						else
							ram_setcfg(ram_depth);
						break;
					case 'K':
						ram_setcfg(0);
						break;
					case 'm':
						k=1; type_id=(BYTE)(argv[i][j++]-'0');
						if (type_id<0||type_id>=length(bios_system))
							i=argc; // help!
						break;
					case 'o':
						onscreen_flag=1;
						break;
					case 'O':
						onscreen_flag=0;
						break;
					case 'p':
						playcity_disabled=0;
						break;
					case 'P':
						playcity_disabled=1;
						break;
					case 'r':
						video_framelimit=(BYTE)(argv[i][j++]-'0');
						if (video_framelimit<0||video_framelimit>9)
							i=argc; // help!
						break;
					case 'R':
						session_fast=1;
						break;
					case 's':
						session_audio=1;
						break;
					case 'S':
						session_audio=0;
						break;
					case 't':
						audio_mixmode=length(audio_stereos)-1;
						break;
					case 'T':
						audio_mixmode=0;
						break;
					case 'w':
						session_fullblit=0;
						break;
					case 'W':
						session_fullblit=1;
						break;
					case 'x':
						disc_disabled=0;
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
						session_zoomblit=0;
						break;
					case '$':
						session_hidemenu=1;
						break;
					case '!':
						session_softblit=1,session_softplay=0;
						break;
					default:
						i=argc; // help!
				}
			while ((i<argc)&&(argv[i][j]));
		}
		else if (any_load(puff_makebasepath(argv[i]),1))
			i=argc; // help!
	if (i>argc)
		return
			printfusage("usage: " my_caption " [option..] [file..]\n"
			"  -cN\tscanline type (0..7)\n"
			"  -CN\tcolour palette (0..4)\n"
			"  -d\tdebug mode\n"
			"  -gN\tset cartridge mapper (0..9)\n" // cfr. CART_IDS
			"  -j\tenable joystick keys\n"
			"  -J\tdisable joystick\n"
			"  -k0\t64K RAM w/o mapper\n"//"-K"
			"  -k1\t64K RAM\n"
			"  -k2\t128K RAM\n"
			"  -k3\t256K RAM\n"
			"  -k4\t512K RAM\n"
			"  -k5\t1024K RAM\n"
			"  -k6\t2048K RAM\n"
			"  -m0\tMSX firmware\n"
			"  -m1\tMSX2 firmware\n"
			"  -m2\tMSX2+ firmware\n"
			//"  -m3\tMSX TURBO R firmware\n"
			"  -J\tdisable joystick\n"
			"  -o/O\tenable/disable onscreen status\n"
			"  -p/P\tenable/disable second PSG\n"
			"  -rN\tset frameskip (0..9)\n"
			"  -R\tdisable realtime\n"
			"  -S\tdisable sound\n"
			"  -t/T\tenable/disable stereo\n"
			"  -W\tfullscreen mode\n"
			"  -x/X\tenable/disable disc drives\n"
			"  -y/Y\tenable/disable tape analysis\n"
			"  -z/Z\tenable/disable tape speed-up\n"
			"  -!\tforce software render\n"
			"  -+\tdefault window size\n"
			//"\t-$\talternative user interface\n"//
			),1;
	if (bios_reload())
		return printferror(txt_error_bios),1;
	// ... if (!*disc_rom&&!disc_disabled) disc_disabled=1,mmu_update(); // can't enable the disc drive without its ROM!
	if (k) all_reset(); // reset machine again if required
	char *s=session_create(session_menudata); if (s)
		return sprintf(session_scratch,"Cannot create session: %s!",s),printferror(session_scratch),1;
	session_kbdreset();
	session_kbdsetup(kbd_map_xlt,length(kbd_map_xlt)/2);
	video_target=&video_frame[video_pos_y*VIDEO_LENGTH_X+video_pos_x]; audio_target=audio_frame;
	video_main_xlat(),video_wide_xlat(),video_xlat_clut(); session_resize();
	// it begins, "alea jacta est!"
	for (audio_disabled=!session_audio;!session_listen();)
	{
		while (!session_signal)
			z80_main( // clump Z80 instructions together to gain speed...
				UNLIKELY(vdp_raster_add8==vdp_table[19]||vdp_raster==vdp_finalraster)?0: // IRQ events
				((VDP_LIMIT_X_V-video_pos_x)<<multi_t)/3); // ...without missing any IRQ and ULA deadlines!
		if (session_signal&SESSION_SIGNAL_FRAME) // end of frame?
		{
			if (audio_required)
			{
				if (audio_pos_z<AUDIO_LENGTH_Z) audio_main(TICKS_PER_FRAME); // fill sound buffer to the brim!
				audio_playframe();
			}
			if (video_required&&onscreen_flag)
			{
				if (disc_disabled)
					onscreen_text(+1,-3,"--\t--",0);
				else
				{
					if (k=diskette_length) onscreen_char(+3,-3,diskette_target>=0?'W':'R');
					onscreen_byte(+1,-3,diskette_cursor[0],k&&(diskette_drive==0));
					onscreen_byte(+4,-3,diskette_cursor[1],k&&(diskette_drive==1));
				}
				k=tape_disabled?0:128;
				if (tape_filesize<=0||tape_type<0)
					onscreen_text(+7,-3,tape?"REC":"---",k);
				else
				{
					if (tape_skipping) onscreen_char(+6,-3,(tape_skipping>0?'*':'+')+k);
					j=(long long int)tape_filetell*999/tape_filesize;
					onscreen_char(+7,-3,'0'+j/100+k);
					onscreen_byte(+8,-3,j%100,k);
				}
				if (session_stick|session_key2joy)
				{
					if (autorun_t) // big letter "A"
						onscreen_bool(-7,-7,3,1,autorun_t>0),
						onscreen_bool(-7,-4,3,1,autorun_t>0),
						onscreen_bool(-8,-6,1,5,autorun_t>0),
						onscreen_bool(-4,-6,1,5,autorun_t>0);
					else
					{
						onscreen_bool(-6,-8,1,2,kbd_bit_tst(kbd_joy[0])),
						onscreen_bool(-6,-5,1,2,kbd_bit_tst(kbd_joy[1])),
						onscreen_bool(-8,-6,2,1,kbd_bit_tst(kbd_joy[2])),
						onscreen_bool(-5,-6,2,1,kbd_bit_tst(kbd_joy[3])),
						onscreen_bool(-8,-2,2,1,kbd_bit_tst(kbd_joy[4])),
						onscreen_bool(-5,-2,2,1,kbd_bit_tst(kbd_joy[5]));
					}
				}
				#ifdef DEBUG
				//onscreen_byte(+1,+1,(video_pos_y>>1)&127,0);
				#endif
				session_status();
			}
			// update session and continue
			//if (!--autorun_t) autorun_next();
			dac_frame(); if (ym3_file) ym3_write(),ym3_flush();
			tape_skipping=audio_queue=0; // reset tape and audio flags
			//if (!tape_fastload) tape_song=0,tape_loud=1; else if (!tape_song) tape_loud=1;
			//else tape_loud=0,--tape_song; // expect song to play for several frames
			tape_output=tape_type<0&&(pio_port_c&32); // keep it output consistent
			if (tape_signal)
			{
				//if ((tape_signal<2||(ula_v2&32))) tape_enabled=0; // stop tape if required
				tape_signal=0,session_dirty=1; // update config
			}
			if (tape&&tape_filetell<tape_filesize&&tape_skipload&&!session_filmfile&&!tape_disabled/*&&tape_loud*/) // `tape_loud` implies `!tape_song`
				session_fast|=+2,audio_disabled|=+2,video_framelimit|=MAIN_FRAMESKIP_MASK+1; // abuse binary logic to reduce activity
			else
				session_fast&=~2,audio_disabled&=~2,video_framelimit&=MAIN_FRAMESKIP_MASK  ; // ditto, to restore normal activity
			session_update();
			//if (!audio_disabled) audio_main(1+ula_clash_z); // preload audio buffer
		}
	}
	// it's over, "acta est fabula"
	z80_close(); cart_remove();
	disc_closeall();
	tape_close(); ym3_close(); if (printer) printer_close();
	return session_byebye(),session_post();
}

BOOTSTRAP

// ============================================ END OF USER INTERFACE //
