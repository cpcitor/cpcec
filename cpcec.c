 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

#define MY_CAPTION "CPCEC"
#define my_caption "cpcec"
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

// The emulation of the Amstrad CPC family is complex because the
// multiple devices are tightly knit together; it's no surprise that
// late models merged most (if not all) devices into a single custom
// ASIC chip to cut costs down without losing any relevant function.

// This file provides CPC-specific features for configuration, Gate
// Array and CRTC, Z80 timings and support, snapshots, options...

#include <stdio.h> // printf()...
#include <stdlib.h> // strtol()...
#include <string.h> // strcpy()...

// Amstrad CPC metrics and constants defined as general types ------- //

#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (64<<4)
#define VIDEO_LENGTH_Y (39<<4)
#ifndef VIDEO_BORDERLESS
#define VIDEO_OFFSET_X (15<<4)
#define VIDEO_OFFSET_Y (17<<2) // ( 4<<4) //
#define VIDEO_PIXELS_X (48<<4)
#define VIDEO_PIXELS_Y (67<<3) // (34<<4) //
#else
#define VIDEO_OFFSET_X (19<<4) // show the default 640x400 screen without the border
#define VIDEO_OFFSET_Y ( 9<<4)
#define VIDEO_PIXELS_X (40<<4)
#define VIDEO_PIXELS_Y (25<<4)
#endif
#define VIDEO_HSYNC_LO (62<<4)
#define VIDEO_HSYNC_HI (66<<4)
#define VIDEO_VSYNC_LO (37<<4)
#define VIDEO_VSYNC_HI (44<<4)
#define VIDEO_RGB2Y(r,g,b) ((r)*3+(g)*9+(b)) // the CPC generates a distinct RGB-to-Y signal!

#if defined(SDL2)||!defined(_WIN32)
unsigned short session_icon32xx16[32*32] = {
	#include "cpcec-a4.h"
	};
#endif

// The CPC 6128 and PLUS keyboard; older models had the cursors (00,02,08,01) and COPY (09) on the right edge.
// +-----------------------------------------------------------------------------------------+
// | ESC| 1! | 2" | 3# | 4$ | 5% | 6& | 7' | 8( | 9) | 0_ | -= | ^~ | CLR| DEL| F7 | F8 | F9 |
// | 42 | 40 | 41 | 39 | 38 | 31 | 30 | 29 | 28 | 21 | 20 | 19 | 18 | 10 | 4F | 0A | 0B | 03 | Amstrad joystick
// +-----------------------------------------------------------------------------------------+      +----+
// | TAB   | Q  | W  | E  | R  | T  | Y  | U  | I  | O  | P  | @| | [{ |      | F4 | F5 | F6 |      | UP |
// | 44    | 43 | 3B | 3A | 32 | 33 | 2B | 2A | 23 | 22 | 1B | 1A | 11 |RETURN| 14 | 0C | 04 |      | 48 |
// +--------------------------------------------------------------------+     +--------------+ +--------------+
// | CAPS   | A  | S  | D  | F  | G  | H  | J  | K  | L  | :* | ;+ | ]} | 12  | F1 | F2 | F3 | | LT | // | RT |
// | 46     | 45 | 3C | 3D | 35 | 34 | 2C | 2D | 25 | 24 | 1D | 1C | 13 |     | 0D | 0E | 05 | | 4A | // | 4B |
// +-----------------------------------------------------------------------------------------+ +--------------+
// | SHIFT   | Z  | X  | C  | V  | B  | N  | M  | ,< | .> | /? | \` | SHIFT   | F0 | UP | F. |      | DN |
// | 15      | 47 | 3F | 3E | 37 | 36 | 2E | 26 | 27 | 1F | 1E | 16 | 15      | 0F | 00 | 07 |      | 49 |
// +-----------------------------------------------------------------------------------------+ +--------------+
// | CONTROL | COPY  | SPACE                                 | ENTER          | LT | DN | RT | | F1 | F2 | F3 |
// | 17      | 09    | 2F                                    | 06             | 08 | 02 | 01 | | 4C | 4D | 4E |
// +-----------------------------------------------------------------------------------------+ +--------------+

#define KBD_JOY_UNIQUE 6 // four sides + two fires
unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1-fire4
	{ 0X48,0X49,0X4A,0X4B,0X4C,0X4D,0X4C,0X4D }; // side bits are hard-wired, but the fire bits can be chosen
#define DEBUG_LONGEST 4 // Z80 opcodes can be up to 4 bytes long
#define MAUS_EMULATION // emulation can examine the mouse
#define MAUS_LIGHTGUNS // lightguns are emulated with the mouse
//#define VIDEO_LO_X_RES // the normal MODE 2 is monochrome, but the PLUS ASIC can scroll in MODE 2 steps :-(
//#define INFLATE_RFC1950 // no file needs inflating RFC1950 data
//#define DEFLATE_RFC1950 // no file needs deflating RFC1950 data
//#define SHA1_CALCULATOR // unused
#define PNG_OUTPUT_MODE 0 // PNG_OUTPUT_MODE implies DEFLATE_RFC1950 and forbids QOI
#define POWER_BOOST1 1 // power_boost default value (enabled)
#define POWER_BOOST0 0
#include "cpcec-rt.h" // emulation framework!

int litegun=0; // 0 = standard joystick, 1 = Trojan Light Phaser, 2 = Gunstick (MHT), 3 = Westphaser (Loriciel)
const unsigned char kbd_map_xlt[]=
{
	// control keys (range 0X81..0XBF)
	KBCODE_F1	,0x81,	KBCODE_F2	,0x82,	KBCODE_F3	,0x83,	KBCODE_F4	,0x84,
	KBCODE_F5	,0x85,	KBCODE_F6	,0x86,	KBCODE_F7	,0x87,	KBCODE_F8	,0x88,
	KBCODE_F9	,0x89,	KBCODE_HOLD	,0x8F,	KBCODE_F11	,0x8B,	KBCODE_F12	,0x8C,
	KBCODE_X_ADD	,0x91,	KBCODE_X_SUB	,0x92,	KBCODE_X_MUL	,0x93,	KBCODE_X_DIV	,0x94,
	#ifdef DEBUG
	KBCODE_PRIOR	,0x95,	KBCODE_NEXT	,0x96,	KBCODE_HOME	,0x97,	KBCODE_END	,0x98,
	#endif
	// actual keys, also in hexadecimal
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
	KBCODE_CHR1_1	,0x19,	KBCODE_CHR2_1	,0x1A,	KBCODE_CHR3_2	,0x1C,	KBCODE_CHR4_5	,0x16,
	KBCODE_CHR1_2	,0x18,	KBCODE_CHR2_2	,0x11,	KBCODE_CHR3_3	,0x13,
	KBCODE_TAB	,0x44,	KBCODE_CAPSLOCK	,0x46,	KBCODE_L_SHIFT	,0x15,	KBCODE_L_CTRL	,0x17,
	KBCODE_ESCAPE	,0x42,	KBCODE_BKSPACE	,0x4F,	KBCODE_ENTER	,0x12,	KBCODE_SPACE	,0x2F,
	KBCODE_X_7	,0x0A,	KBCODE_X_8	,0x0B,	KBCODE_X_9	,0x03,	KBCODE_DELETE	,0x10,
	KBCODE_X_4	,0x14,	KBCODE_X_5	,0x0C,	KBCODE_X_6	,0x04,	KBCODE_INSERT	,0x09,
	KBCODE_X_1	,0x0D,	KBCODE_X_2	,0x0E,	KBCODE_X_3	,0x05,
	KBCODE_X_0	,0x0F,	KBCODE_X_DOT	,0x07,	KBCODE_X_ENTER	,0x06,
	KBCODE_UP	,0x00,	KBCODE_DOWN	,0x02,	KBCODE_LEFT	,0x08,	KBCODE_RIGHT	,0x01,
	// key mirrors
	KBCODE_R_SHIFT	,0x15,	KBCODE_R_CTRL	,0x17,	KBCODE_CHR4_4	,0x16, // KBCODE_CHR4_4 is missing in 104-key layouts!
};

const int video_asic_table[32]= // the 0GRB format used in ASIC PLUS
{
	0X666,0X666,0XF06,0XFF6,0X006,0X0F6,0X606,0X6F6, // "6" ensures that PREHISTORIK 2 PLUS shows well the scores
	0X0F6,0XFF6,0XFF0,0XFFF,0X0F0,0X0FF,0X6F0,0X6FF,
	0X006,0XF06,0XF00,0XF0F,0X000,0X00F,0X600,0X60F,
	0X066,0XF66,0XF60,0XF6F,0X060,0X06F,0X660,0X66F,
};
const VIDEO_UNIT video_table[32+16+16+16]= // colour table, 0xRRGGBB style: the 32 original colours, followed by 16 levels of G, 16 of R and 16 of B
{
	#if 1 // linear RGB
	0X808080,0X808080,0X00FF80,0XFFFF80,
	0X000080,0XFF0080,0X008080,0XFF8080,
	0XFF0080,0XFFFF80,0XFFFF00,0XFFFFFF,
	0XFF0000,0XFF00FF,0XFF8000,0XFF80FF,
	0X000080,0X00FF80,0X00FF00,0X00FFFF,
	0X000000,0X0000FF,0X008000,0X0080FF,
	0X800080,0X80FF80,0X80FF00,0X80FFFF,
	0X800000,0X8000FF,0X808000,0X8080FF,
	// OLD above -/- NEW G/R/B below //
	0X000000,0X001100,0X002200,0X003300,
	0X004400,0X005500,0X006600,0X007700,
	0X008800,0X009900,0X00AA00,0X00BB00,
	0X00CC00,0X00DD00,0X00EE00,0X00FF00,
	0X000000,0X110000,0X220000,0X330000,
	0X440000,0X550000,0X660000,0X770000,
	0X880000,0X990000,0XAA0000,0XBB0000,
	0XCC0000,0XDD0000,0XEE0000,0XFF0000,
	0X000000,0X000011,0X000022,0X000033,
	0X000044,0X000055,0X000066,0X000077,
	0X000088,0X000099,0X0000AA,0X0000BB,
	0X0000CC,0X0000DD,0X0000EE,0X0000FF,
	#else // gamma: 1.6
	0XA5A5A5,0XA5A5A5,0X00FFA5,0XFFFFA5,
	0X0000A5,0XFF00A5,0X00A5A5,0XFFA5A5,
	0XFF00A5,0XFFFFA5,0XFFFF00,0XFFFFFF,
	0XFF0000,0XFF00FF,0XFFA500,0XFFA5FF,
	0X0000A5,0X00FFA5,0X00FF00,0X00FFFF,
	0X000000,0X0000FF,0X00A500,0X00A5FF,
	0XA500A5,0XA5FFA5,0XA5FF00,0XA5FFFF,
	0XA50000,0XA500FF,0XA5A500,0XA5A5FF,
	// OLD above -/- NEW G/R/B below //
	0X000000,0X002F00,0X004800,0X005D00,
	0X006F00,0X008000,0X009000,0X009E00,
	0X00AC00,0X00B900,0X00C600,0X00D200,
	0X00DE00,0X00E900,0X00F400,0X00FF00,
	0X000000,0X2F0000,0X480000,0X5D0000,
	0X6F0000,0X800000,0X900000,0X9E0000,
	0XAC0000,0XB90000,0XC60000,0XD20000,
	0XDE0000,0XE90000,0XF40000,0XFF0000,
	0X000000,0X00002F,0X000048,0X00005D,
	0X00006F,0X000080,0X000090,0X00009E,
	0X0000AC,0X0000B9,0X0000C6,0X0000D2,
	0X0000DE,0X0000E9,0X0000F4,0X0000FF,
	#endif
};
VIDEO_UNIT video_xlat[32]; // static colours only (dynamic ASIC PLUS colours go elsewhere)

// GLOBAL DEFINITIONS =============================================== //

#define TICKS_PER_FRAME ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)>>5)
#define TICKS_PER_SECOND (TICKS_PER_FRAME*VIDEO_PLAYBACK)
// everything in the Amstrad CPC is tuned to a 4 MHz clock,
// using simple binary divisors to adjust the devices' timings,
// mainly defined by the simultaneous operation of the video output
// (32 horizontal thin pixels) and the Z80 behavior (4 cycles)
// (the clock is technically 16 MHz but we mean atomic steps here)
int multi_t=0,multi_u=0; // overclocking shift+bitmask

// HARDWARE DEFINITIONS ============================================= //

#define ram_maxcfg 6 // 0 = 64K, 1 = 128K, 2 = 192K, 3 = 320K, 4 = 576K, 5 = 1088K, 6 = 2112K
BYTE mem_ram[33<<16],mem_rom[33<<14]; // RAM (BASE 64K BANK + 32x 64K BANKS) and ROM (512K ASIC PLUS CARTRIDGE + 16K BDOS)
BYTE *ext_rom=NULL; // external 257x 16K EXTENDED ROMS
BYTE mmu_xtr[257]; // ROM bit masks: nonzero reads from EXTENDED rather than from DEFAULT/CARTRIDGE
#define bdos_rom (&mem_rom[32<<14])
BYTE *mmu_ram[4],*mmu_rom[4]; // memory is divided in 4x 16K R+W areas
#define PEEK(x) mmu_rom[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!

BYTE mmu_bit[4]={0,0,0,0}; // RAM bit masks: nonzero raises a write event
#define plus_enabled (type_id>2) // the PLUS ASIC hardware MUST BE tied to the model!

BYTE type_id=2; // 0=464, 1=664, 2=6128, 3=PLUS
BYTE disc_disabled=0; // disables the disc drive altogether as well as its related logic; +1 = manual, +2 = automatic
BYTE disc_filemode=1; // +1 = read-only by default instead of read-write; +2 = relaxed disc write errors instead of strict
VIDEO_UNIT video_clut[32]; // precalculated colour palette, 16 bitmap inks, 1 border, 15 sprite inks

// Z80 registers: the hardware and the debugger must be allowed to "spy" on them!

HLII z80_af,z80_bc,z80_de,z80_hl; // Accumulator+Flags, BC, DE, HL
HLII z80_af2,z80_bc2,z80_de2,z80_hl2,z80_ix,z80_iy; // AF', BC', DE', HL', IX, IY
HLII z80_pc,z80_sp,z80_iff,z80_ir; // Program Counter, Stack Pointer, Interrupt Flip-Flops, IR pair
BYTE z80_imd,z80_r7; // Interrupt Mode // low 7 bits of R, required by several `IN X,(Y)` operations

// the Dandanator cartridge system can spy on the Z80 and trap its operations

#define Z80_DANDANATOR
BYTE *mem_dandanator=NULL; char dandanator_path[STRMAX]="";
WORD dandanator_trap,dandanator_temp; // Dandanator-Z80 watchdogs
BYTE dandanator_cfg[8]; // PENDING and CURRENT cnfg_0,cnfg_1,zone_0,zone_1 (A0,A1,B,C)
int dandanator_canwrite=0,dandanator_dirty; // R/W status

// above the Gate Array and the CRTC: PLUS ASIC --------------------- //

BYTE plus_gate_lock[]={0000,0x00,0xFF,0x77,0xB3,0x51,0xA8,0xD4,0x62,0x39,0x9C,0x46,0x2B,0x15,0x8A}; // dummy first byte
BYTE plus_gate_counter; // step in the plus lock sequence, starting from 0 (waiting for 0x00) until its length
BYTE plus_gate_enabled; // locked/unlocked state: UNLOCKED if byte after SEQUENCE is $CD, LOCKED otherwise!
BYTE plus_gate_mcr; // RMR2 register: it modifies the behavior of the original MRER (gate_mcr)
WORD plus_dma_regs[3][4]; // loop counter,loop address,pause counter,pause scaler
int plus_dma_index,plus_dma_delay,plus_dma_cache[3]; // DMA channel counters and timings
//BYTE plus_dirtysprite; // tag sprite as "dirty"
BYTE plus_8k_bug; // ASIC IRQ bug flag

// the following block is a hacky way to implement the entire Plus configuration RAM bank:
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
	plus_dma_index=plus_dma_delay=plus_gate_mcr=plus_gate_enabled=plus_gate_counter=0; // default configuration values
	plus_analog[0]=plus_analog[1]=plus_analog[2]=plus_analog[3]=plus_analog[4]=plus_analog[6]=0x3F; // default analog values; WinAPE lets them stay ZERO
	plus_ivr=1; // docs: "Interrupt Vector (Bit 0 set to 1 on reset)"; WinAPE uses 0x51 as the default state
	plus_8k_bug=0; // the ASIC IRQ bug MUST happen once after reset at the very least ("CRTC3" demo)
}

BYTE gate_status; // low 2 bits are the current screen mode; next bits render either border or pure black
int video_threshold=VIDEO_LENGTH_X; // self-adjusting horizontal threshold, to gain speed when possible
int hsync_limit=VIDEO_LENGTH_X,hsync_count=0,hsync_match=0; // LA-7800: self-adjusting HSYNC timings
int vsync_limit=VIDEO_LENGTH_Y,vsync_count=0,vsync_match=0; // LA-7800: self-adjusting VSYNC timings

// 0xBC00-0xBF00: CRTC 6845 ----------------------------------------- //

const BYTE crtc_valid[18]={255,255,255,255,127,31,127,127,63,31,127,31,63,255,63,255,63,255}; // bit masks
BYTE crtc_index,crtc_table[18]; // index and table
BYTE crtc_type=1; // 0 Hitachi, 1 UMC, 2 Motorola, 3 Amstrad+, 4 Amstrad-
int crtc_status,crtc_before; // active status and latest events

// Winape `HDC` and `VDUR` are `video_pos_x` and `video_pos_y`
BYTE crtc_count_r0; // HORIZONTAL CHAR COUNT / Winape `HCC`
BYTE crtc_count_r4; // VERTICAL CHAR COUNT / Winape `VCC`
BYTE crtc_count_r9; // VERTICAL LINE COUNT / Winape `VLC`
BYTE crtc_count_r5; // V_T_A LINE COUNT / Winape `VTAC`
BYTE crtc_count_r3x; // HSYNC CHAR COUNT / Winape `HSC`
BYTE crtc_count_r3y; // VSYNC LINE COUNT / Winape `VSC`
BYTE crtc_limit_r3x,crtc_limit_r3y; // limits of `r3x` and `r3y`
int video_vsync_min,video_vsync_max,crtc_hold=0; // VHOLD modifiers

int crtc_line; // virtual PLUS variable, a shortcut of CRTC registers 4 and 9 used to test PLUS_PRI, PLUS_SSSL and others
#define crtc_line_set() (crtc_line=(crtc_count_r9&7)+(crtc_count_r4&63)*8) // Plus scanline counter

// these flags draw the border instead of the bitmap
#define CRTC_STATUS_H_OFF 4 // i.e. CRTC_STATUS_R1_OK
#define CRTC_STATUS_V_OFF 8 // i.e. CRTC_STATUS_R6_OK
#define CRTC_STATUS_INVIS 16 // different on CRTC1
// these flags draw pure black instead of bitmap and border
#define CRTC_STATUS_HSYNC 32 // i.e. CRTC_STATUS_R2_OK
#define CRTC_STATUS_VSYNC 64 // i.e. CRTC_STATUS_R7_OK
// the following flags are internal rather than visible
#define CRTC_STATUS_REG_8 128 // toggle to shake the screen
#define CRTC_STATUS_V_T_A 256 // i.e. Vertical Total Adjust
#define CRTC_STATUS_R0_OK 512 // i.e. last horizontal character
#define CRTC_STATUS_R4_OK 1024 // i.e. last vertical character
#define CRTC_STATUS_R9_OK 2048 // i.e. last character scanline

// CRTC_STATUS macros that SET and RESet bits and their associated counters
#define CRTC_STATUS_H_OFF_RES (crtc_status&=~CRTC_STATUS_H_OFF,crtc_count_r0=0)
#define CRTC_STATUS_H_OFF_SET (crtc_status|=CRTC_STATUS_H_OFF)
#define CRTC_STATUS_V_OFF_RES (crtc_status&=~CRTC_STATUS_V_OFF,crtc_count_r4=crtc_count_r9=0)
#define CRTC_STATUS_V_OFF_SET (crtc_status|=CRTC_STATUS_V_OFF)
#define CRTC_STATUS_HSYNC_SET (crtc_status|=CRTC_STATUS_HSYNC,crtc_count_r3x=0)
#define CRTC_STATUS_HSYNC_RES (crtc_status&=~CRTC_STATUS_HSYNC)
#define CRTC_STATUS_VSYNC_SET (crtc_status|=CRTC_STATUS_VSYNC,crtc_count_r3y=0)
#define CRTC_STATUS_VSYNC_RES (crtc_status&=~CRTC_STATUS_VSYNC)

void crtc_invis_update(void) // handle invisibility conditions
{
	if (crtc_type!=1?~crtc_table[8]&48:crtc_table[6])
		crtc_status&=~CRTC_STATUS_INVIS;
	else
		crtc_status|=CRTC_STATUS_INVIS;
}
void crtc_syncs_update(void) // horizontal and vertical limits
{
	crtc_limit_r3x=crtc_table[3]&15; if (!crtc_limit_r3x&&crtc_type>=2) crtc_limit_r3x=16; // CRTC0,CRTC1: zero means NO HSYNC; CRTC2,CRTC3,CRTC4: zero means 16 CHARS
	crtc_limit_r3y=(crtc_type<1||crtc_type>2)?crtc_table[3]>>4: // CRTC1,CRTC2: nibble is ignored, height is always 16 lines; CRTC0,CRTC3,CRTC4: zero means 16 LINES
		crtc_table[4]==62&&crtc_table[9]==4&&!crtc_table[5]?15:0; // kludge: 15 (possibly as low as 13 =16-(315-312)) can fix "PHEELONE" on CRTC1 (315 lines tall)
}

#define CRTC_STATUS_T_F(x) ((crtc_status&(x))?'*':'-')
#define crtc_table_select(i) (crtc_index=(i)&31) // i&15 is required by "STILL RISING" (Pulpo Corrosivo part); i&31 is required by "SKEETSHOT" and "THE-ONE"
INLINE void crtc_table_send(BYTE i)
{
	if (crtc_table[crtc_index]!=(i&=crtc_valid[crtc_index]))
		switch (crtc_table[crtc_index]=i,crtc_index)
		{
			case 0:
				if (!(crtc_type|i))
					crtc_table[0]=i=1; // CRTC0: R0 CANNOT BE ZERO!
				if (i==(crtc_table[7]?7:39)) // detect "CHANY DREAM END" and "CHAPELLE SIXTEEN" (but not "ONESCREEN COLONIES"); some of their effects are particularly time-critical
					video_threshold=VIDEO_LENGTH_X*5/8; // hard horizontal threshold
				if (crtc_type<3?crtc_count_r0==i:crtc_count_r0>=i) // "S&KOH" intro needs the first check; "PHX" outro for CRTC3 and CRTC4 needs the second check
					crtc_status|=CRTC_STATUS_R0_OK;
				else
					crtc_status&=~CRTC_STATUS_R0_OK;
				break;
			//case 1:
				//if (crtc_count_r0==i) CRTC_STATUS_H_OFF_SET; // is this real? does any demo use it?
				//break;
			//case 2:
				//if (crtc_count_r0==i) CRTC_STATUS_HSYNC_SET; // is this real? does any demo use it?
				//break;
			case 3:
				crtc_syncs_update(); // split R3 into VSYNC and HSYNC limits
				if (crtc_count_r0==crtc_table[2]) // "CHANY DREAM END part 8" (CRTC1 only) relies on this
					CRTC_STATUS_HSYNC_SET;
				break;
			//case 5:
				//cprintf("%04X:R5 %02X%c%02X   ",z80_pc.w,i,CRTC_STATUS_T_F(CRTC_STATUS_V_T_A),crtc_count_r5); // can this do anything? does any demo use it?
				//break;
			case 7:
				if (crtc_count_r4==i&&crtc_type!=3)
					CRTC_STATUS_VSYNC_SET; // "BYTE 98" (second half) and "PHX" dancing bobs for CRTC4 must trigger a VSYNC, but "CROCO CHANEL 1 part 4" and "PHX" outro for CRTC3 must NOT!
				break;
			case 6:
				if (crtc_count_r4==i)
					CRTC_STATUS_V_OFF_SET; // "VOYAGE 93" hides the bouncing staff with this!
				// no `break`!
			case 8:
				crtc_invis_update(); // mix visibility of R6 and R8 together
				break;
			case 4:
				//printf("%02X%c%02X ",crtc_count_r4,(crtc_status&CRTC_STATUS_R9_OK)?'+':'-',i);
				if (crtc_type&5) // CRTC1, CRTC3 and CRTC4 are straightforward (but see below "THE LIVING DAYLIGHTS" for a chance of overflow on CRTC 1)
				{
					if (crtc_type!=3/*&&!(crtc_status&CRTC_STATUS_R9_OK)*/?crtc_count_r4==i:crtc_count_r4>=i) // the "BG Games" logo of "PINBALL DREAMS" expects ">=" in CRTC3!
						crtc_status|=CRTC_STATUS_R4_OK;
					else
						crtc_status&=~CRTC_STATUS_R4_OK;
				}
				else // CRTC0 and CRTC2 handle R4 and R9 together
				{
					if (crtc_status&CRTC_STATUS_R9_OK)
						; // skipping R9_OK is required by "STILL RISING" (scroller+pong+end), "ONESCREEN COLONIES" (both), "PHX" (first part)...
					else if (crtc_count_r4==i)
						crtc_status|=CRTC_STATUS_R4_OK; // if we check this first, "AMAZING MEGADEMO 2021 Longshot part" fails!
					else
						crtc_status&=~CRTC_STATUS_R4_OK; // TODO: "DEMOIZART part 1" and "OVERFLOW PREVIEW part 2"
				}
				break;
			case 9:
				if (crtc_type&5) // CRTC1, CRTC3 and CRTC4 are straightforward
				{
					if (crtc_type<3?crtc_count_r9==i+(crtc_table[8]&1):crtc_count_r9>=i) // interlaced mode special case: "ECSTASY DEMO part 1" (CRTC1)
						crtc_status|=CRTC_STATUS_R9_OK;
					else
						crtc_status&=~CRTC_STATUS_R9_OK;
				}
				else // CRTC0 and CRTC2 handle R4 and R9 together
				{
					if (crtc_count_r9==i)
						crtc_status|=CRTC_STATUS_R9_OK; // if we check this later, "XMAS 2019 DEMO" fails!
					else if ((crtc_status&CRTC_STATUS_R4_OK)&&crtc_count_r0>1) // "&&crtc_count_r0" fixes "OCTOPUS POCUS" (tunnel)
						; // skipping R4_OK is required by "PRODATRON MEGADEMO" (part 4), "PINBALL DREAMS" (ingame), "ONESCREEN COLONIES" (both)...
					else
						crtc_status&=~CRTC_STATUS_R9_OK; // TODO: "DEMOIZART part 1" and "OVERFLOW PREVIEW part 2"
				}
				break;
		}
}
BYTE crtc_table_recv(void)
	{ return (crtc_index>=12&&crtc_index<18)?crtc_table[crtc_index]:(crtc_type>=3&&crtc_index<8?crtc_table[crtc_index+8]:0); }
INLINE BYTE crtc_table_info(void) { return crtc_count_r4>=crtc_table[6]?32:0; } // +64???

#define crtc_setup()

void crtc_reset(void)
{
	MEMZERO(crtc_table);
	crtc_before=crtc_status=0;//CRTC_STATUS_VSYNC+CRTC_STATUS_HSYNC+CRTC_STATUS_R9_OK+CRTC_STATUS_R4_OK;
	crtc_index=crtc_count_r0=crtc_count_r4=crtc_count_r9=crtc_count_r5=crtc_count_r3x=crtc_count_r3y=0;
	crtc_table[0]=63; crtc_table[3]=0x8E; crtc_table[4]=38; crtc_table[9]=7; // implicit in "GNG11B" (?)
	crtc_syncs_update(),crtc_invis_update();
}

// 0x7F00, 0xDF00: Gate Array --------------------------------------- //

BYTE gate_index,gate_table[17]; // colour table: respectively, Palette Pointer Register and Palette Memory
BYTE gate_mcr; // bit depth + MMU configuration (1/3), also known as MRER (Mode and Rom Enable Register)
BYTE gate_ram; // MMU configuration (2/3), the Memory Mapping Register
BYTE gate_rom; // MMU configuration (3/3)
BYTE ram_depth=1; // RAM configuration: 0 = 64K, 1 = 128K, 2 = 192K, 3 = 320K, 4 = 576K; 5 = 1088K and 6 = 2112K are special
BYTE ram_extra=0; // extended RAM hardware uses one or two extra bits; they're taken from the port address, so some 128K-only softwares will fail
int ram_dirty; // actually used RAM space, in kb
void ram_setcfg(int x) { ram_depth=(x),ram_extra=ram_depth<5?0:ram_depth<6?64:192; } // `ram_extra` is zero unless RAM size goes beyond 576k!
int ram_kbytes(int x) { return ((32<<x)&-64)+64; }
#define ram_getcfg() (ram_depth)
VIDEO_UNIT *video_clut_index,video_clut_value; // slow colour update buffer

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
	int i=gate_ram&(ram_depth?(4<<ram_depth)-1:0),j;
	if (ram_dirty<(j=i?(i>>3)*64+128:64)) // tag memory as dirty?
		session_dirty|=i,ram_dirty=j;
	int k=(i>>3)<<16; i&=7; // k = bank offset, i = paging mode
	// page 0x0000-0x3FFF
	if ((j=mmu_ram_mode[i][0])>=0x10000-0x0000) j+=k;
	mmu_ram[0]=&mem_ram[j];
	mmu_rom[0]=(gate_mcr&4)?mmu_ram[0]:mmu_xtr[0]?&ext_rom[0x0000-0x0000]:&mem_rom[0x0000-0x0000];
	// page 0x4000-0x7FFF
	if ((j=mmu_ram_mode[i][1])>=0x10000-0x4000) j+=k;
	mmu_rom[1]=mmu_ram[1]=&mem_ram[j];
	// page 0x8000-0xBFFF
	if ((j=mmu_ram_mode[i][2])>=0x10000-0x8000) j+=k;
	mmu_rom[2]=mmu_ram[2]=&mem_ram[j];
	// page 0xC000-0xFFFF
	if ((j=mmu_ram_mode[i][3])>=0x10000-0xC000) j+=k;
	mmu_ram[3]=&mem_ram[j];
	mmu_rom[3]=(gate_mcr&8)?mmu_ram[3]:(gate_rom<length(mmu_xtr)-1&&mmu_xtr[gate_rom+1]?&ext_rom[0x4000+(gate_rom<<14)-0xC000]:(gate_rom!=7||disc_disabled)?&mem_rom[0x4000-0xC000]:&bdos_rom[0x0000-0xC000]);
	// extended PLUS ASIC paging?
	if (plus_enabled)
	{
		if (!(gate_mcr&4)) // show low ROM?
		{
			mmu_rom[0]=mmu_xtr[0]?&ext_rom[0x0000-0x0000]:&mem_rom[(plus_gate_mcr&7)<<14];
			switch (plus_gate_mcr&24)
			{
				case 8:
					mmu_rom[1]=&(mmu_rom[0])[0x0000-0x4000]; // show low ROM on page 1
					mmu_rom[0]=mmu_ram[0]; // show RAM on page 0
					break;
				case 16:
					mmu_rom[2]=&(mmu_rom[0])[0x0000-0x8000]; // show low ROM on page 2
					mmu_rom[0]=mmu_ram[0]; // show RAM on page 0
					break;
			}
		}
		if (mmu_bit[1]=!(~plus_gate_mcr&24)) // show PLUS ASIC bank?
			mmu_rom[1]=mmu_ram[1]=&plus_bank[0x0000-0x4000];
		if (!(gate_mcr&8)) // show high ROM?
			mmu_rom[3]=gate_rom<length(mmu_xtr)-1&&mmu_xtr[gate_rom+1]?&ext_rom[0x4000+(gate_rom<<14)-0xC000]:
			gate_rom<128?gate_rom==7?&mem_rom[0xC000-0xC000]:&mem_rom[0x4000-0xC000]:&mem_rom[((gate_rom&31)<<14)-0xC000];
	}
	else
		mmu_bit[1]=0; // hide PLUS ASIC bank
	#ifdef Z80_DANDANATOR // Dandanator is always the last part of the MMU update
	if (mem_dandanator) // emulate the Dandanator (and more exactly its CPC-only memory map) only when a card is loaded
	{
		if (!(dandanator_cfg[5]&32)) // Dandanator memory map?
		{
			if (!(dandanator_cfg[6]&32)) // ZONE_0?
				if (dandanator_cfg[5]&4) // the order is important; checking bit 0 first breaks "THE SWORD OF IANNA" when the first level begins
				{
					mmu_rom[2]=&mem_dandanator[((dandanator_cfg[6]&31)<<14)-0x8000];
					//if ((dandanator_cfg[4]&2)&&(dandanator_cfg[6]&30)&&dandanator_canwrite) mmu_ram[2]=mmu_rom[2],dandanator_dirty=1;
				}
				else if (!(dandanator_cfg[5]&1)) // must check this for "MOJON TWINS ROMSET" to work
				{
					mmu_rom[0]=&mem_dandanator[((dandanator_cfg[6]&31)<<14)-0x0000];
					if ((dandanator_cfg[4]&2)&&(dandanator_cfg[6]&30)&&dandanator_canwrite) // forbid writing on sectors 0 and 1 (!?)
						//cprintf("R/W %02X%02X%02X%02X\n",dandanator_cfg[4],dandanator_cfg[5],dandanator_cfg[6],dandanator_cfg[7]),
						mmu_ram[0]=mmu_rom[0],dandanator_dirty=1; // the emulated EEPROM checks nothing: pure magic!
				}
			if (!(dandanator_cfg[7]&32)) // ZONE_1?
				if (dandanator_cfg[5]&8) // the order is important again: checking bit 1 first breaks "MOJON TWINS ROMSET" and other snapshot packs
				{
					mmu_rom[3]=&mem_dandanator[((dandanator_cfg[7]&31)<<14)-0xC000];
					//if ((dandanator_cfg[4]&2)&&(dandanator_cfg[6]&30)&&dandanator_canwrite) mmu_ram[3]=mmu_rom[3],dandanator_dirty=1;
				}
				else //if (!(dandanator_cfg[5]&2)) // must NOT check this, otherwise "TESORO PERDIDO DE CUAUHTEMOC 64K" stops working
				{
					mmu_rom[1]=&mem_dandanator[((dandanator_cfg[7]&31)<<14)-0x4000];
					//if ((dandanator_cfg[4]&2)&&(dandanator_cfg[6]&30)&&dandanator_canwrite) mmu_ram[1]=mmu_rom[1],dandanator_dirty=1;
				}
		}
		else if (dandanator_cfg[5]&16) // "poor-man" rombox: "CPCSOCCER" overrides the current firmware with its own copy
		{
			if (!(gate_mcr&4)) mmu_rom[0]=&mem_dandanator[0x70000+((dandanator_cfg[4]&24)<<11)-0x0000];
			if (!(gate_mcr&8)) mmu_rom[3]=&mem_dandanator[((dandanator_cfg[7]&31)<<14)-0xC000];
		}
	}
}
void dandanator_update(void)
{
	//cprintf("DAN! %08X: %02X%02X,%02X,%02X\n",z80_pc.w,dandanator_cfg[4],dandanator_cfg[5],dandanator_cfg[6],dandanator_cfg[7]);
	memcpy(&dandanator_cfg[4],&dandanator_cfg[0],4); mmu_update(); // update Dandanator state and MMU
}
#define Z80_DNTR_0XFD(w) (dandanator_trap=dandanator_temp,dandanator_temp=w+4) // notice that `w` is the PC address with the extra 0XFD rather than PC+1!
#define Z80_DNTR_0XFD70(w,b) do{ if (dandanator_trap==w&&!(dandanator_cfg[5]&32)) { dandanator_cfg[2]=b; if (!(dandanator_cfg[1]&64)) dandanator_update(); } }while(0)
#define Z80_DNTR_0XFD71(w,b) do{ if (dandanator_trap==w&&!(dandanator_cfg[5]&32)) { dandanator_cfg[3]=b; if (!(dandanator_cfg[1]&64)) dandanator_update(); } }while(0)
#define Z80_DNTR_0XFD77(w,b) do{ if (dandanator_trap==w&&!(dandanator_cfg[5]&32)) { if (b&128) dandanator_cfg[1]=b; else dandanator_cfg[0]=b; if (!(dandanator_cfg[1]&64)) dandanator_update(); } }while(0)
#define Z80_DNTR_0XC9() do{ if (dandanator_cfg[1]&64) { dandanator_cfg[1]&=~64; if (!(dandanator_cfg[5]&32)) dandanator_update(); } }while(0) // used by game packs, but not by "SWORD OF IANNA"
void dandanator_reset(void)
{
	MEMZERO(dandanator_cfg); dandanator_trap=dandanator_temp=0;
	dandanator_cfg[3]=32; // default values are all ZERO but ZONE_1==32
	dandanator_update();
	#endif
}

BYTE gate_mode0[2][256],gate_mode1[4][256]; // lookup table for byte->pixel conversion and Gate/CRTC exchanges
void gate_setup(void) // setup the Gate Array
{
	for (int i=0;i<256;++i)
	{
		gate_mode0[0][i]=(gate_mode1[0][i]=((i&128)?1:0)+((i&8)?2:0))+((i&32)?4:0)+((i&2)?8:0);
		gate_mode0[1][i]=(gate_mode1[1][i]=((i& 64)?1:0)+((i&4)?2:0))+((i&16)?4:0)+((i&1)?8:0);
		gate_mode1[2][i]=((i& 32)?1:0)+((i&2)?2:0);
		gate_mode1[3][i]=((i& 16)?1:0)+((i&1)?2:0);
	}
}

void video_xlat_clut(void) // precalculate palette following `video_type`; part of it is managed by the PLUS ASIC
{
	if (!plus_enabled)
		for (int i=0;i<17;++i)
			video_clut[i]=video_xlat[gate_table[i]];
	else
		for (int i=0;i<32;++i)
			video_clut[i]=video_xlat_rgb(video_table[32+(plus_palette[i*2+1]&15)]+video_table[48+(plus_palette[i*2+0]>>4)]+video_table[64+(plus_palette[i*2+0]&15)]);
	video_clut_value=*(video_clut_index=video_clut+gate_index);
}
INLINE void gate_table_select(BYTE i) { gate_index=(i&16)?16:(i&15); }
INLINE void gate_table_send(BYTE i)
{
	gate_table[gate_index]=(i&=31);
	video_clut_index=video_clut+gate_index;
	if (!plus_enabled)
	{
		video_clut_value=video_xlat[i];
	}
	else
	{
		int j=video_asic_table[i]; // set both colour and the PLUS ASIC palette
		video_clut_value=video_xlat_rgb(video_table[32+((j>>8)&15)]+video_table[48+((j>>4)&15)]+video_table[64+(j&15)]);
		mputii(&plus_palette[gate_index*2],j);
	}
}

BYTE irq_delay; // 0 = INACTIVE, 1 = LINE 1, 2 = LINE 2, >2 = IRQ!
BYTE irq_timer; // Winape `R52`: rises from 0 to 52, then launches an IRQ!
int z80_irq; // Winape `ICSR`: B7 Raster (Gate Array / PRI), B6 DMA0, B5 DMA1, B4 DMA2; top -- PRI DMA2 DMA1 DMA0 -- bottom
int z80_int; // Z80 INT flag (IFF1 latch + HALT mode + NMI request)
#define z80_nmi_throw (z80_int=-1,z80_irq|=1) // NMI has priority over IRQs but doesn't erase them!
void gate_reset(void) // reset the Gate Array
{
	gate_mcr=gate_ram=gate_rom=gate_index=irq_timer=irq_delay=0;
	ram_dirty=64;
	MEMZERO(gate_table);
	mmu_update();
	video_xlat_clut(); // normal CPC palette is all grey by default, PLUS ASIC paletteis black instead; either way, it's managed from here
}

// 0xF400-0xF700: PIO 8255 ------------------------------------------ //

BYTE pio_port_a,pio_port_b,pio_port_c,pio_control;
BYTE tape_delay=0; // tape motor delay, in frames
#define pio_setup()
void pio_reset(void)
	{ pio_port_a=pio_port_b=pio_port_c=pio_control=tape_delay=0; }

// the CPC printer port is a simple 7-bit data + STROBE signal pair
FILE *printer=NULL; int printer_p=0; BYTE printer_z,printer_t[256];
void printer_flush(void) { fwrite1(printer_t,printer_p,printer),printer_p=0; }
void printer_close(void) { printer_flush(),fclose(printer),printer=NULL; }

// behind the PIO: PSG AY-3-8910 ------------------------------------ //

#define PSG_MAX_VOICE 10922 // 32768/3= 10922
#define PSG_TICK_STEP 8 // 1 MHz /2 /8 = 62500 Hz
#define PSG_KHZ_CLOCK 1000 // compare with the 2000 kHz YM3 standard
#define PSG_MAIN_EXTRABITS 0 // not even the mixer-banging beepers from "STORMBRINGER" and "TERMINUS" improve with >0!
#define PSG_PLAYCITY 2 // the PLAYCITY card contains two chips...
#define PSG_PLAYCITY_XLAT(x) playcity_outputs[(x)] // ...playing at 50% intensity each!
int playcity_clock=0,playcity_hiclock,playcity_loclock; // the PLAYCITY clock is dynamic...
#define playcity_getcfg() (playcity_clock)
void playcity_setcfg(BYTE b) { if (b<16) playcity_clock=b,playcity_hiclock=(b?b*2-1:2)*TICKS_PER_SECOND,playcity_loclock=(b?b:1)*2*AUDIO_PLAYBACK*4; }
#define PSG_PLAYCITY_RESET (playcity_setcfg(0)) // ...and it must be reset!
int playcity_disabled=0,playcity_dirty,playcity_ctc_state[4]={0,0,0,0},playcity_ctc_flags[4]={0,0,0,0},playcity_ctc_count[4]={0,0,0,0},playcity_ctc_limit[4]={0,0,0,0};
int dac_disabled=1; // Digiblaster DAC, disabled by default to avoid trouble with the printer
int playcity_outputs[16]; // to be filled following psg_outputs[]

#include "cpcec-ay.h"

#include "cpcec-ym.h"
void ym3_write(void) { ym3_write_ay(psg_table,&psg_hard_log,PSG_KHZ_CLOCK); }

int dac_delay=0,dac_voice=0,tape_loud=1,tape_song=0; // DAC and tape levels and flags
#define dac_frame() (dac_delay?dac_voice=dac_voice*63/64:++dac_delay) // soften signal as time passes, if required

// behind the PIO: TAPE --------------------------------------------- //

#define tape_enabled (pio_port_c&16)
#define tape_disabled tape_delay // the machine can disable the tape even when we enable it
//#define TAPE_TAP_FORMAT // useless outside Spectrum
//#define TAPE_CAS_FORMAT // useless outside MSX
//#define TAPE_TZX_KANSAS // useless outside MSX AFAIK, are there any non-MSX protections using this method?
//#define FASTTAPE_DUMPER // overkill!
#include "cpcec-k7.h"

// 0xFA7E, 0xFB7E, 0xFB7F: FDC 765 ---------------------------------- //

#define DISC_PARMTR_UNIT (disc_parmtr[1]&1) // CPC hardware is limited to two drives;
#define DISC_PARMTR_UNITHEAD (disc_parmtr[1]&5) // however, it allows two-sided discs.
#define DISC_TIMER_INIT ( 4<<6) // rough approximation: "PhX" requires 1<<7 at least and 8<<7 at most, but "Prehistorik 1" needs more than 1<<7.
#define DISC_TIMER_BYTE ( 2<<6) // rough approximation, too: the HEXAGON-protected "SWIV" needs at least 1<<6 and becomes very slow at 1<<8.
#define DISC_WIRED_MODE 0 // the CPC lacks the End-Of-Operation wire
#define DISC_PER_FRAME (312<<6) // = 1 MHz / 50 Hz ; compare with TICKS_PER_FRAME
#define DISC_R_P_M 300 // revolutions/minute
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

char audio_dirty; int audio_queue=0; // used to clump audio updates together to gain speed

WORD gate_screen; // Gate Array's internal video address within the lowest 64K RAM, see below
int crtc_screen,crtc_raster,crtc_backup,crtc_double; // CRTC's internal video addresses, active and backup
int gate_count_r3x,gate_count_r3y,irq_steps; // Gate Array's horizontal and vertical timers filtering the CRTC's own
VIDEO_UNIT plus_sprite_border,*plus_sprite_target=NULL; // PLUS ASIC sprite screen references
int plus_sprite_offset,plus_sprite_latest,plus_sprite_adjust;
#ifdef DEBUG
#else
const // make the thresholds non-configurable on the official binary!
#endif
int plus_dma_delay1=8,plus_dma_delay0=8; // DMA contention thresholds

void video_main_sprites(void)
{
	int delta=plus_sprite_latest-plus_sprite_offset;
	VIDEO_UNIT plus_backup_pixels[3]; // screen buffer; avoids dirt on the left edge of the screen
	MEMLOAD(plus_backup_pixels,&plus_sprite_target[delta-3]);
	for (int i=15*8;i>=0;i-=8) // render sprites
	{
		//if (i==plus_dirtysprite) continue;
		int zoomy; if (!(zoomy=(plus_sprite_xyz[i+4]&3))) continue;
		int zoomx; if (!(zoomx=(plus_sprite_xyz[i+4]>>2))) continue;
		int spritey=(crtc_line-(plus_sprite_xyz[i+2]+256*plus_sprite_xyz[i+3]))&511; // 9-bit wrap!
		if ((spritey>>=--zoomy)>=16) continue;
		int spritex=(plus_sprite_xyz[i+0]+256*(INT8)plus_sprite_xyz[i+1])+plus_sprite_offset;
		if (plus_sprite_latest>=spritex+(16<<--zoomx)||spritex>=video_pos_x) continue;
		int x=0,xx=16;
		if ((spritex-=plus_sprite_latest)<0)
			x=(0-spritex)>>zoomx,xx-=x,spritex+=x<<zoomx; // clip left edge
		BYTE *s=&plus_sprite_bmp[i*32+spritey*16+x];
		VIDEO_UNIT *t=&plus_sprite_target[spritex+delta];
		if (t+(xx<<zoomx)>video_target)
			xx=((video_target-t-1)>>zoomx)+1; // clip right edge
		switch (zoomx) // `xx` will never be zero!
		{
			case 0:
				do
					if (x=*s++)
						*t=video_clut[16+x];
				while (++t,--xx);
				break;
			case 1:
				do
					if (x=*s++)
						t[0]=t[1]=video_clut[16+x];
				while (t+=2,--xx);
				break;
			case 2:
				do
					if (x=*s++)
						t[0]=t[1]=t[2]=t[3]=video_clut[16+x];
				while (t+=4,--xx);
				break;
		}
	}
	MEMSAVE(&plus_sprite_target[delta-3],plus_backup_pixels); // hide sprite edges :-(
	plus_sprite_latest=video_pos_x; //plus_dirtysprite=-1;
}
void video_main_borders(void)
{
	if ((plus_sscr&128)&&plus_sprite_offset>VIDEO_OFFSET_X-16) // render extra border
		for (int i=0;i<16;++i)
			*plus_sprite_target++=plus_sprite_border;
	video_target-=plus_sprite_adjust; // undo excess pixels
	video_pos_x-=plus_sprite_adjust; // clip right border
	plus_sprite_target=NULL;
	plus_sprite_adjust=0;
}

void video_main(int t) // render video output for `t` clock ticks; t is always nonzero!
{
	do {
		// GATE ARRAY pixel rendering

		if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
		{
			if ((video_pos_x+=16)>VIDEO_OFFSET_X&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X+16)
				switch (gate_status)
				{
					VIDEO_UNIT p; BYTE b;
					case  0: // MODE 0
						VIDEO_NEXT=p=video_clut[gate_mode0[0][b=mem_ram[gate_screen+0]]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode0[1][b]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						*video_clut_index=video_clut_value; // slow update
						VIDEO_NEXT=p=video_clut[gate_mode0[0][b=mem_ram[gate_screen+1]]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode0[1][b]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						break;
					case  1: // MODE 1
						VIDEO_NEXT=p=video_clut[gate_mode1[0][b=mem_ram[gate_screen+0]]];
						VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[1][b]];
						VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[2][b]];
						VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[3][b]];
						VIDEO_NEXT=p;
						*video_clut_index=video_clut_value; // slow update
						VIDEO_NEXT=p=video_clut[gate_mode1[0][b=mem_ram[gate_screen+1]]];
						VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[1][b]];
						VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[2][b]];
						VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[3][b]];
						VIDEO_NEXT=p;
						break;
					case  2: // MODE 2
						VIDEO_NEXT=video_clut[(b=mem_ram[gate_screen+0])>>7];
						VIDEO_NEXT=video_clut[(b>>6)&1];
						VIDEO_NEXT=video_clut[(b>>5)&1];
						VIDEO_NEXT=video_clut[(b>>4)&1];
						VIDEO_NEXT=video_clut[(b>>3)&1];
						VIDEO_NEXT=video_clut[(b>>2)&1];
						VIDEO_NEXT=video_clut[(b>>1)&1];
						VIDEO_NEXT=video_clut[ b&1    ];
						*video_clut_index=video_clut_value; // slow update
						VIDEO_NEXT=video_clut[(b=mem_ram[gate_screen+1])>>7];
						VIDEO_NEXT=video_clut[(b>>6)&1];
						VIDEO_NEXT=video_clut[(b>>5)&1];
						VIDEO_NEXT=video_clut[(b>>4)&1];
						VIDEO_NEXT=video_clut[(b>>3)&1];
						VIDEO_NEXT=video_clut[(b>>2)&1];
						VIDEO_NEXT=video_clut[(b>>1)&1];
						VIDEO_NEXT=video_clut[ b&1    ];
						break;
					case  3: // MODE 3
						VIDEO_NEXT=p=video_clut[gate_mode1[0][b=mem_ram[gate_screen+0]]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[1][b]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						*video_clut_index=video_clut_value; // slow update
						VIDEO_NEXT=p=video_clut[gate_mode1[0][b=mem_ram[gate_screen+1]]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p=video_clut[gate_mode1[1][b]];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						break;
					case  4: case  5: case  6: case  7: case  8: case  9: case 10:
					case 11: case 12: case 13: case 14: case 15: case 16: case 17:
					case 18: case 19: case 20: case 21: case 22: case 23: case 24:
					case 25: case 26: case 27: case 28: case 29: case 30: case 31: // BORDER
						VIDEO_NEXT=p=video_clut[16];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						*video_clut_index=video_clut_value; // slow update
						VIDEO_NEXT=p=video_clut[16];
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						break;
					default: // HBLANK/VBLANK: BLANK BLACK!
						VIDEO_NEXT=p=video_xlat[20]; // BLACK from the colour table
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
						*video_clut_index=video_clut_value; // slow update
				}
			else // drawing, but not now
				video_target+=16,*video_clut_index=video_clut_value; // slow update

			// special pixel rendering cases: start and end of bitmap rasterline

			if (!(crtc_status&(CRTC_STATUS_V_OFF+CRTC_STATUS_VSYNC)) // these events only make sense inside the bitmap!
				&&crtc_table[1]) // ...and if the bitmap size is nonzero!
			{
				if (!crtc_count_r0) // beginning of bitmap -- CRTC_STATUS_H_OFF_RES won't do because of retriggering, f.e. "SYNERGY 2"
				{
					if (plus_enabled)
					{
						if (plus_sprite_target)
						{
							if (video_pos_x>plus_sprite_latest) video_main_sprites(); // draw more sprites, "DELIRIUM" retriggers the horizontal bitmap!
							video_main_borders(); // retrigger border cleanup
						}
						plus_sprite_border=video_clut[16];
						plus_sprite_target=video_target;
						plus_sprite_offset=video_pos_x;
						plus_sprite_adjust=plus_gate_enabled?plus_sscr&15:gate_status&2?0:1; // "IMPERIAL MAHJONG" on PLUS relies on this!
						if (video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X)
						{
							VIDEO_UNIT p=gate_status<32?video_clut[16]:video_xlat[20];
							for (int i=0;i<plus_sprite_adjust;++i)
								VIDEO_NEXT=p; // pad left border
						}
						else
							video_target+=plus_sprite_adjust;
						plus_sprite_latest=video_pos_x+=plus_sprite_adjust;
					}
					else if (!(crtc_type&5)&&video_pos_x>VIDEO_OFFSET_X) // CRTC0 and CRTC2 draw "shadows" on "SYNERGY 2" (5 stripes) and
						video_target[-8]= video_target[-7]= video_target[-6]= video_target[-5]= // "ONESCREEN COLONIES" (brick wall):
						video_target[-4]= video_target[-3]= video_target[-2]= video_target[-1]= video_clut[16]; // actually the border
				}
				else if (crtc_count_r0==crtc_table[1]) // end of bitmap -- CRTC_STATUS_H_OFF_SET will do
					if (plus_sprite_target)
					{
						if (video_pos_x>plus_sprite_latest) video_main_sprites(); // last sprites
						video_main_borders(); // border cleanup
					}
			}
		}
		else // not drawing at all!!
			video_pos_x+=16,video_target+=16,*video_clut_index=video_clut_value; // slow update

		gate_screen=crtc_screen+crtc_raster; hsync_count+=16; // GATE ARRAY offset, LA-7800 counter

		// GATE ARRAY slow reactions to CRTC events

		if (UNLIKELY(crtc_before!=crtc_status)) // speedup: no need to test nonextant events, this is true only 1 out of 6 times (more events should be moved here)
		{
			if (plus_pri==crtc_line&&plus_pri) // the PLUS ASIC handles the programmable raster interrupt (PRI) here:
				if ((~crtc_before&crtc_status&CRTC_STATUS_HSYNC) // the IRQ triggers on CRTC_STATUS_HSYNC_SET (normal)
					||(!crtc_count_r0&&crtc_before&CRTC_STATUS_HSYNC)) // or CRTC_STATUS_H_OFF_RES during HSYNC (early)
					z80_irq|=128;
			if (~crtc_before&crtc_status&CRTC_STATUS_VSYNC) // i.e. CRTC_STATUS_VSYNC_SET
			{
				// the LA-7800 aligns the image in two ways: one is based on measuring the scanline width at VSYNC_SET.
				if ((hsync_limit=(crtc_table[0]+1)*16)>VIDEO_HSYNC_HI) // "S&KOH" needs this exactly at VSYNC_SET!
					hsync_limit=VIDEO_HSYNC_HI;
				else if (hsync_limit<VIDEO_HSYNC_LO)
					hsync_limit=VIDEO_HSYNC_LO;
				if (vsync_count>=video_vsync_min) // the vertical alignment hinges on VSYNC_SET, too.
				{
					if ((vsync_limit=vsync_count)>video_vsync_max)
						vsync_limit=video_vsync_max;
					vsync_count=0;
				}
				gate_count_r3y=26,gate_status|=CRTC_STATUS_VSYNC, // launch secondary VSYNC
				irq_delay=(irq_timer>50&&(crtc_before&crtc_status&CRTC_STATUS_HSYNC))|| // >=50, >50, ==51...!?!? "THE LIVING DAYLIGHTS" needs something!
					(plus_enabled&&crtc_count_r3x+crtc_table[1]-crtc_table[0]>4)?2:1; // >-2 breaks "BLACK SABBATH", >5 breaks "PHX"
				// the first part monitors HSYNC+IRQ clashes in "LOGON DEMO 4" versus "CHANY DREAM 6" (1st half), "PREHISTORIK 2", "ONESCREEN COLONIES" (arkanoid part)...
				// the second part is a kludge (?) for "PHX" (final scroller) versus "BLACK SABBATH" and others, perhaps linked to the early PRI from H_OFF+HSYNC clashes.
			}
			gate_status=(gate_status&(3+CRTC_STATUS_HSYNC+CRTC_STATUS_VSYNC))+
				((crtc_before=crtc_status)&(CRTC_STATUS_H_OFF+CRTC_STATUS_V_OFF+CRTC_STATUS_INVIS));
		}

		// ASIC hardware sprites and DMA

		if (UNLIKELY(!--plus_dma_delay)) // DMA CHANNELS
		{
			int i; while (plus_dma_index<3) // reading phase?
			{
				plus_dma_cache[plus_dma_index]=-1; // no known command yet
				if (plus_dcsr&(1<<plus_dma_index)) // is the channel enabled?
				{
					if (plus_dma_regs[plus_dma_index][2]) // pause?
					{
						if (plus_dma_regs[plus_dma_index][3]) // multiplier?
							--plus_dma_regs[plus_dma_index][3]; // handle multiplier
						else
						{
							plus_dma_regs[plus_dma_index][3]=plus_dmas[plus_dma_index*4+2];
							--plus_dma_regs[plus_dma_index][2]; // handle pause
						}
					}
					if (!plus_dma_regs[plus_dma_index][2]) // no pause?
					{
						i=mgetii(&plus_dmas[plus_dma_index*4])&-2; // get pointer
						plus_dma_cache[plus_dma_index]=mgetii(&mem_ram[i]); // fetch command
						mputii(&plus_dmas[plus_dma_index*4],i+2); // inc pointer!
						plus_dma_delay=1; ++plus_dma_index; break; // channel is done, reading phase is over
					}
				}
				++plus_dma_index; // try next channel
			}
			if (plus_dma_index>=3) while (plus_dma_index<6) // parsing phase?
			{
				if ((i=plus_dma_cache[plus_dma_index-3])>=0) // do we have to do something?
				{
					if (!(i&0x7000)) // 0RDD = LOAD R,DD
					{
						if (i<0x0F00||psg_port_b_lock()) // filter dummy writes
							psg_table_sendto(i>>8,i); // load register
						plus_dma_delay=(0xC0&~pio_port_c)?plus_dma_delay0:plus_dma_delay1;
					}
					else // warning! functions can stack up! "CRTC3" relies on it!
					{
						if (i&0x1000) // 1NNN = PAUSE NNN (0=no pause)
							plus_dma_regs[plus_dma_index-3][2]=i&4095; // reload pause
						if (i&0x2000) // 2NNN = REPEAT NNN (0=no repeat)
							plus_dma_regs[plus_dma_index-3][0]=i&4095, // repeat times
							plus_dma_regs[plus_dma_index-3][1]=mgetii(&plus_dmas[plus_dma_index*4-4*3]); // repeat addr.
						if (i&0x4000) // 4NNN = CONTROL BIT MASK: +01 = LOOP, +10 = INT, +20 = STOP
						{
							if (i& 1)
								if (plus_dma_regs[plus_dma_index-3][0])
									--plus_dma_regs[plus_dma_index-3][0],
									mputii(&plus_dmas[plus_dma_index*4-4*3],plus_dma_regs[plus_dma_index-3][1]);
							if (i&16)
								z80_irq|=8*64>>plus_dma_index; // n*8 == n>>-3
							if (i&32)
								plus_dcsr-=1<<(plus_dma_index-3);
						}
						plus_dma_delay=1;

					}
					++plus_dma_index; break; // channel is done, parsing phase is over
				}
				++plus_dma_index; // try next channel
			}
		}

		// CRTC counters and triggers

		if (UNLIKELY(crtc_status&CRTC_STATUS_R0_OK))
		{
			if (crtc_table[0])
				crtc_status&=~CRTC_STATUS_R0_OK;

			if (crtc_status&CRTC_STATUS_VSYNC)
				if ((crtc_count_r3y=(crtc_count_r3y+1)&15)==crtc_limit_r3y)
				{
					CRTC_STATUS_VSYNC_RES; // stop horizontal sync!
				}

			if (crtc_status&CRTC_STATUS_R9_OK)
			{
				crtc_count_r9=0;
				if (crtc_table[9])
					crtc_status&=~CRTC_STATUS_R9_OK;
				if (crtc_status&CRTC_STATUS_R4_OK)
				{
					if (crtc_table[4]) // "OVERFLOW PREVIEW part 1" needs this condition!
						crtc_status&=~CRTC_STATUS_R4_OK;
					if (crtc_table[4]==crtc_count_r4||(crtc_type&1))
					{
						crtc_status|=CRTC_STATUS_V_T_A;
						if (crtc_table[5]) // "FACEHUGGER MEGADEMO part 3": REG4 $0A, REG5 $02, REG7 $0B; see also "VOYAGE 93 part 3"
							crtc_count_r4=(crtc_count_r4+1)&127;
						crtc_count_r5=0;
					}
					else // "VOYAGE 93 part 3": reset on mismatch! Winape makes this part fail on CRTC1 + CRTC3
						crtc_count_r4=0; // "BYTE98 part 1" moves the top half one pixel down on Winape's CRTC1 + CRTC3
				}
				else
					crtc_count_r4=(crtc_count_r4+1)&127;
			}
			else if (/*(crtc_type|crtc_table[0])&&*/(crtc_count_r9=(crtc_count_r9+1)&31)==crtc_table[9])
				crtc_status|=CRTC_STATUS_R9_OK;

			if (crtc_count_r4==crtc_table[4]) // this is checked on every scanline, not just when C9==R9:
				crtc_status|=CRTC_STATUS_R4_OK; // see "Amazing Megademo 2021" Longshot part on CRTC0!

			if (crtc_status&CRTC_STATUS_V_T_A)
			{
				if (crtc_count_r5==crtc_table[5])
				{
					CRTC_STATUS_V_OFF_RES; // show vertical bitmap!
					//if (crtc_table[5])
						crtc_status&=~CRTC_STATUS_V_T_A; // "FROM SCRATCH" shows that it must ALWAYS happen... at least on CRTC1!
					if (crtc_table[4])
						crtc_status&=~CRTC_STATUS_R4_OK;
					else
						crtc_status|=CRTC_STATUS_R4_OK;
					if (crtc_table[9])
						crtc_status&=~CRTC_STATUS_R9_OK;
					else
						crtc_status|=CRTC_STATUS_R9_OK;
				}
				else
					crtc_count_r5=(crtc_count_r5+1)&31;
			}

			static int crtc_v_off_count;
			if (!crtc_count_r4)
				if (!crtc_count_r9||(crtc_type==1&&(!crtc_table[5]||crtc_table[4]))) // the R5 test fixes "CAMEMBERT MEETING 4" without breaking the title of "FROM SCRATCH"
				{
					crtc_backup=(crtc_table[12]&48)*1024+(crtc_table[12]&3)*512+crtc_table[13]*2;
					crtc_double=(~crtc_table[12]&12)?0:16384;
				}
			if (!crtc_count_r9)
			{
				if (crtc_count_r4==crtc_table[6])
					crtc_v_off_count=(crtc_table[6]+crtc_type)?1:2; // V_OFF counter, see below
				if (crtc_count_r4==crtc_table[7])
					if (crtc_type!=1||(crtc_table[4]|crtc_table[5])) // PDTMD4 (CRTC0), "CHAPEL SIXTEEN" (CRTC1) and "ECOLE BUISSONIERE (staff)" (CRTC1)
						//if (!(crtc_status&CRTC_STATUS_VSYNC)) // does any demo rely on this!?
						{
							CRTC_STATUS_VSYNC_SET; // start vertical sync!
							if ((crtc_table[8]&1)&&crtc_table[7])
								crtc_status^=CRTC_STATUS_REG_8; // "CLEVER & SMART" shakes screen
							else
								crtc_status&=~CRTC_STATUS_REG_8; // "ECSTASY DEMO part 1" doesn't! (why?)
						}
			}
			if (!--crtc_v_off_count) // CRTC0 doesn't instantly hide the screen when R6=0
				CRTC_STATUS_V_OFF_SET; // hide vertical bitmap!
			if (gate_count_r3x<-256)
				gate_count_r3x=1; // "CHANY DREAM END (part 8)" relies on this overflow

			crtc_line_set();
			crtc_screen=crtc_backup;
			crtc_raster=((crtc_count_r9<<11)+(plus_sscr<<7))&0x3800;
			CRTC_STATUS_H_OFF_RES; // show horizontal bitmap!
		}
		else
		{
			if ((crtc_count_r0=(crtc_count_r0+1))==crtc_table[0]) // "CAMEMBERT MEETING" expects &255, not &127
				crtc_status|=CRTC_STATUS_R0_OK;
			if ((crtc_screen+=2)&2048)
				crtc_screen+=crtc_double-2048; // go to next 16K
		}

		if (UNLIKELY(crtc_count_r0==crtc_table[1]))
		{
			// one of the few places where the PLUS ASIC adds a new parameter to the internal logic of the CRTC
			if (plus_enabled?((crtc_count_r9+((plus_sscr&0x70)>>4))&31)==crtc_table[9]:crtc_status&CRTC_STATUS_R9_OK)
			{
				if ((crtc_screen^crtc_backup)&16384)
					crtc_double=-crtc_double; // go to prev 16K
				crtc_backup=crtc_screen; // recalculate new line!
			}
			if (plus_sssl==(BYTE)(crtc_line)&&plus_sssl) // the PLUS ASIC handles split screens with PLUS_SSSL and PLUS_SSSS
				crtc_backup=(plus_ssss[0]&48)*1024+(plus_ssss[0]&3)*512+plus_ssss[1]*2; // "SIMPLY THE BESTS" uses SSSL on a 32K screen, so no `crtc_double=0` here!
			CRTC_STATUS_H_OFF_SET; // hide horizontal bitmap!
		}
		if (UNLIKELY(crtc_count_r0==crtc_table[2])&&!(crtc_status&CRTC_STATUS_HSYNC)) // don't set the HSYNC twice ("S&KOH" main part)
			CRTC_STATUS_HSYNC_SET; // start horizontal sync!
		if (crtc_status&CRTC_STATUS_HSYNC)
		{
			if (crtc_count_r3x>=crtc_limit_r3x)
			{
				// HSYNC_RES cancels the irq_steps countdown and the DMA cleanup
				if (crtc_count_r3x) // the Gate Array is deaf to zero-length HSYNC
					irq_steps=(crtc_type>=3&&plus_sprite_adjust<8)?2:1; // `plus_enabled` isn't enough; this happens on CRTC4 too
				if (plus_dcsr&7) audio_dirty=1; // notice that HSYNC length cannot be zero on PLUS ASIC
				#ifdef PSG_PLAYCITY
				if (playcity_ctc_count[1]>0)
					if ((playcity_ctc_count[1]-=((playcity_ctc_flags[1]&32)?1:16))<=0)
						z80_nmi_throw; // programmed NMI!
				// *!* todo: ... playcity_ctc_count[3] ... z80_irq|=128; *!*
				#endif
				CRTC_STATUS_HSYNC_RES; // stop horizontal sync!
			}
			else if (UNLIKELY(!crtc_count_r3x++))
			{
				// HSYNC_SET calculates the PLUS ASIC horizontal skew and the DMA at once
				plus_dma_index=0;
				hsync_match=hsync_count; // the LA-7800 will need this later; "MADNESS DEMO" may need a filter here...
				if (!plus_enabled)
					plus_dma_delay=0,gate_count_r3x=1-256;
				else if (plus_sprite_adjust<8)
					plus_dma_delay=3,gate_count_r3x=2-256;
				else
					plus_dma_delay=2,gate_count_r3x=1-256;
			}
		}

		// Gate Array and IRQ timers

		if (UNLIKELY(!--gate_count_r3x)) // cancel secondary HSYNC
			gate_status=(gate_mcr&3) // update MODE/bitdepth: "IMPERIAL MAHJONG", "SCROLL FACTORY"...
				+(gate_status&(--gate_count_r3y?~(3+CRTC_STATUS_HSYNC):~(3+CRTC_STATUS_HSYNC+CRTC_STATUS_VSYNC))); // cancel secondary VSYNC on timeout
		else if (UNLIKELY(gate_count_r3x==-256)) // launch secondary HSYNC
			if (crtc_limit_r3x) // "MADNESS DEMO" (CRTC1) disables HSYNC with R3 = $X0
				gate_status|=CRTC_STATUS_HSYNC,gate_count_r3x=crtc_limit_r3x<2?2:crtc_limit_r3x>7?7:crtc_limit_r3x; // length is 2 at least, 7 at most (not 6?)

		if (UNLIKELY(!--irq_steps)) // did the HSYNC end at last?
			if (++irq_timer,irq_delay?++irq_delay>2:irq_timer>=52) // PDTMD0 implies "++irq_timer" even when DELAY takes priority
			{
				if (irq_timer>=32&&!plus_pri)
					z80_irq|=128;
				irq_delay=irq_timer=0;
			}

		// HBLANK and VBLANK handling: the LA-7800 chip

		if (UNLIKELY(hsync_count>=VIDEO_HSYNC_HI)) // HBLANK?
		{
			if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y&&frame_pos_y==video_pos_y)
			{
				if (plus_sprite_target) // just in case they weren't drawn yet!
					if (video_pos_x>plus_sprite_latest) video_main_sprites();
				video_drawscanline();
			}
			plus_sprite_target=NULL;
			if (gate_count_r3y>0?vsync_match>=video_vsync_min:vsync_match>video_vsync_max) // VBLANK?
			{
				vsync_match=0; if (!(video_pos_z&255)&&crtc_type==1) crtc_syncs_update(); // "PHEELONE" watchdog
				// all calculations are ready: feed everything to the video engine!
				video_newscanlines(video_pos_x,(((VIDEO_LENGTH_Y-vsync_limit)/2)&-2)+((crtc_status&CRTC_STATUS_REG_8)?2:0)); // end of frame!
				video_threshold=VIDEO_LENGTH_X/4; // soft horizontal threshold
			}
			vsync_count+=2; vsync_match+=2;
			// "PREHISTORIK 2", "EDGE GRINDER" and the like perform smooth horizontal scrolling with the low nibble of REG3.
			// However, "SCROLL FACTORY" (and to a lesser degree "ONESCREEN COLONIES") expect the REG3 low nibble to do nothing.
			video_nextscanline((hsync_count&15)+(VIDEO_LENGTH_X-hsync_limit)/2+((crtc_table[4]&&crtc_double)?0: // kludge fingerprint: avoid 32K screens
				((crtc_limit_r3x>=2&&crtc_limit_r3x<=6)?(6-crtc_limit_r3x)*8:0))); // scanline event!
			// the LA-7800 aligns the image in two ways: the second one is based on keeping the HSYNC_SET period steady.
			int i=hsync_limit-(hsync_count-hsync_match)*2; if (i>0) // when the image is perfectly stable i==1024
			{
				i=hsync_match-VIDEO_HSYNC_HI; // this is either <0, =0 (perfectly stable image) or >0
				if (i<0)
					hsync_count+=i<-15?5:i<-5?3:1;
				else if (i>0)
					hsync_count-=i>+15?5:i>+5?3:1;
			}
			else if (i>-1024) // LONGSHOT's SHAKER "RV.BAS" needs at least "!=", and ">" breaks "ONESCREEN COLONIES 2" main part, so...
				hsync_count-=5; // the HSYNC is very late already, this will always be >+15!
			hsync_match-=hsync_limit;
			hsync_count-=hsync_limit;
		}
	}
	while (--t);
	if (plus_sprite_target) if (video_pos_x>plus_sprite_latest) video_main_sprites();
}

void audio_main(int t) // render audio output for `t` clock ticks; t is always nonzero!
	{ psg_main(t,(((tape_status^tape_output)&tape_loud)<<12)+dac_voice); } // merge tape signals

// autorun runtime logic -------------------------------------------- //

BYTE snap_done; // avoid accidents with ^F2, see all_reset()
char autorun_path[STRMAX]="",autorun_line[STRMAX]; int autorun_mode=0,autorun_t=0;
BYTE autorun_kbd[16]; // automatic keypresses
#define autorun_kbd_set(k) (autorun_kbd[k>>3]|=1<<(k&7))
#define autorun_kbd_res(k) (autorun_kbd[k>>3]&=~(1<<(k&7)))
int autorun_kbd_bit(int k) // combined autorun+keyboard+joystick bits
{
	if (autorun_mode)
		return autorun_kbd[k];
	if (k==9&&litegun) // catch special case: lightgun models
	{
		k=kbd_bit[9]&128; switch (litegun) // all lightguns do nothing if the trigger is up
		{
			case 1: // TROJAN LIGHT PHASER updates the CRTC light pen registers when FIRE is down
				if (session_maus_z)
				{
					k|=16; // BIT4 = trigger
					int t=crtc_table[12]*256+crtc_table[13] // is [14]:[15] involved in this calculation too???
						+(session_maus_x-VIDEO_PIXELS_X/2+8)/16-crtc_table[2]+65 // an extremely crude approximation,
						+((session_maus_y-VIDEO_PIXELS_Y/2+8)/16-crtc_table[7]+40)*crtc_table[1]; // but it just works!
					crtc_table[17]=t; crtc_table[16]=((t>>8)&3)+(crtc_table[12]&0x30);
				}
				break;
			case 2: // GUNSTICK detects bright pixels when FIRE is down
				if (session_maus_z)
				{
					k|=16; // BIT4 = trigger
					static BYTE g; if (++g) // work around the buggy "TARGET PLUS", that always does 256 reads and must miss at least one
						k|=(video_litegun&0x00C000)?2:0;
				}
				break;
			case 3: // WESTPHASER catches the electron beam even when FIRE is up!
				k|=(session_maus_z?32:0)+(video_pos_y>session_maus_y+VIDEO_OFFSET_Y&&video_pos_x>session_maus_x+VIDEO_OFFSET_X?1:0); // BIT5 = trigger
				break;
		}
		return k;
	}
	return kbd_bit[k]|joy_bit[k];
}
INLINE void autorun_next(void) // handle AUTORUN
{
	switch (autorun_mode)
	{
		case 1: // tape (1/2)
			autorun_kbd_set(0x17); // PRESS CONTROL
			autorun_kbd_set(0x06); // PRESS ENTER
			++autorun_mode; autorun_t=4;
			break;
		case 2: // tape (2/2)
			autorun_kbd_res(0x17); // RELEASE CONTROL
			autorun_kbd_res(0x06); // RELEASE ENTER
			autorun_mode=8; autorun_t=4;
			break;
		case 3: // disc: type 'RUN"filename"' or '|CPM'
			strcpy(&POKE(type_id?0xAC8A:0xACA4),autorun_line); // type hidden line
			autorun_mode=8; autorun_t=4;
			break;
		case 4: // PLUS menu (1/2)
			autorun_kbd_set(0x0D); // PRESS F1
			++autorun_mode; autorun_t=4;
			break;
		case 5: // PLUS menu (2/2)
			autorun_kbd_res(0x0D); // RELEASE F1
			autorun_mode=disc_disabled?1:3; autorun_t=40; // now load the tape or disc proper
			break;
		case 8: // shared (1/2)
			autorun_kbd_set(0x12); // PRESS RETURN
			++autorun_mode; autorun_t=4;
			break;
		case 9: // shared (2/2)
			autorun_kbd_res(0x12); // RELEASE RETURN
			if (plus_enabled&&disc_disabled) // Gremlin games ("Basil the Great Mouse Detective", "Gauntlet", "Skate Crazy"...)
				POKE(0XC2D4)=0XC0; // need this to load on Plus; POKE(0XC374)=0 is redundant, because it's already zero.
			disc_disabled&=1,autorun_mode=0; // end of AUTORUN
			break;
	}
}

// Z80-hardware procedures ------------------------------------------ //

#define Z80_NMI_ACK (z80_irq&=~1) // allow throwing NMIs
// the CPC hands the Z80 a data bus value that is NOT constant on the PLUS ASIC
#define Z80_IRQ_BUS (plus_enabled?(plus_ivr&-8)+((z80_irq&128)?((z80_pc.b.h&0x20)?6:plus_8k_bug):(z80_irq&16)?0:(z80_irq&32)?2:4):255) // 6 = PRI, 0 = DMA2, 2 = DMA1, 4 = DMA0.
// the CPC obeys the Z80 IRQ ACK signal unless the PLUS ASIC IVR bit 0 is off (?)
#define Z80_IRQ_ACK z80_irq_ack() // (complex enough to require its own function)
void z80_irq_ack(void)
{
	if (z80_irq&128) // OLD IRQ + PLUS PRI
		plus_dcsr|=128,z80_irq&=64+32+16,irq_timer&=~32,/*irq_delay=0,*/plus_8k_bug=6; // reset the ASIC IRQ bug
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
int z80_loss; // at least one device (the PLUS ASIC when DMA and PIO clash) can hog the bus!

void z80_sync(int t) // the Z80 asks the hardware/video/audio to catch up
{
	static int r=0; main_t+=t;
	//if (!disc_disabled) // redundant, in most cases disc_main() does nothing
		disc_main(t);
	if (tape_enabled&&tape)
		audio_dirty|=tape_loud,tape_main(t); // echo the tape signal thru sound!
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

void z80_send(WORD p,BYTE b) // the Z80 sends a byte to a hardware port
{
	// Multiple devices can answer to the Z80 request at the same time if the bit patterns match. This is required in cases, some caused by programming bugs, some done on purpose:
	// * 0x7F00 : "Hero Quest" sends GATE ARRAY bytes to 00C0 by mistake! (OUT 00C0,C0 for OUT 7FC0,C0)
	// * 0xDF00 : "The Final Matrix" crack corrupts BC' (0389,038D rather than 7F89,7F8D) before it tries doing OUT 7F00,89 and OUT 7F00,8D!
	// * 0xF600 : "Bombfusion" sends PIO bytes (start tape signal) to 0210 by mistake!
	// * (p&0x100)+0xBC00 : "Knight Rider" sends CRTC bytes to 0088 by mistake! "Camembert Meeting 4" sends CRTC bytes to $0C00! "The Demo" sends CRTC bytes to 0x1D00 and 0x1C00!
	// * 0x00XX : "Overflow Preview part 3" ($521D et al.) switches RAM and ROM by mistake!
	z80_loss=0; if (!(p&0x8000)) // 0x7F00, GATE ARRAY (1/2)
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
							//cprintf("%08X: RESET IRQ!\n",z80_pc.w), // something must go here!?
							z80_irq&=~128,irq_timer=0;
						gate_mcr=b&15; // 0x80-0xBF: MULTICONFIGURATION REGISTER
						if (!(b&4)&&z80_pc.w==5) // warm reset?
							ram_dirty=64,plus_8k_bug=snap_done=0;
					}
				}
			}
			else
				gate_ram=(b&63)+((~p>>2)&ram_extra); // 0xC0-0xFF: SET RAM BANK MODE; 1MB and 2MB extensions add one or two bits and take them from the port address
			mmu_update();
		}
	}
	if (!(p&0x4200)) // 0xBC00-0xBD00, CRTC 6845 // "Night Shift" sends bytes to BFFD by mistake!
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
	if (!(p&0x2000)) // 0xDF00, GATE ARRAY (2/2)
		gate_rom=b,mmu_update(); // SELECT ROM BANK
	if (p>=0XEF00&&p<=0XEFFF)//(!(p&0x1000)) // 0xEF00, PRINTER PORT
	{
		#ifdef PSG_PLAYCITY
		if (!printer&&!dac_disabled)
			audio_dirty=1,dac_delay=0,dac_voice=(((signed char)b)*10922)>>7; // Digiblaster DAC, signed 8-bit audio; "BORDELIK 4.1" used as reference
		else
		#endif
		if (printer&&(b&128)&&!(printer_z&128))
			if (printer_t[printer_p]=b&127,++printer_p>=sizeof(printer_t))
				printer_flush();
		printer_z=b; // avoid duplicate writes (i.e. respect STROBE bit)
	}
	if (!(p&0x0800)) // 0xF400-0xF700, PIO 8255
	{
		if (plus_dma_index>=3&&plus_dma_delay>1) { int z=plus_dma_delay-1; z80_loss+=z; z80_sync(z); } // bus clash! the PLUS DMA hogs the PIO 8255!
		if (!(p&0x0200))
		{
			if (!(p&0x0100)) // 0xF400, PIO PORT A
			{
				pio_port_a=b;
				if (pio_port_c&0x80)
				{
					if (pio_port_c&0x40) // SELECT PSG REGISTER
						psg_table_select(pio_port_a);
					else // WRITE PSG REGISTER
						{ psg_table_send(pio_port_a); if (psg_index<11&&b&&psg_index!=7) tape_song=(tape_disabled||(audio_disabled&1))?0:240; }
				}
				// else ... // *!* should anything happen here?
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
						{ psg_table_send(pio_port_a); if (psg_index<6) tape_song=(tape_disabled||(audio_disabled&1))?0:240; }
				}
				else if (pio_port_c&0x40)
				{
					pio_port_a=~autorun_kbd_bit(pio_port_c&15);
				}
			}
			else // 0xF700, PIO CONTROL
			{
				if (b&128)
				{
					pio_control=b;
					if (!plus_enabled) // CRTC3 has a PIO bug! CRTC0,CRTC1,CRTC2,CRTC4 have a good PIO
						pio_port_a=pio_port_b=pio_port_c=0; // reset all ports!
					else if (b&16)
					{
						pio_port_a=255; // the famous PLUS keyboard bug!
						if (pio_port_c>=0XC0)
							psg_table_select(pio_port_a);
					}
				}
				else
				{
					if (b&1)
					{
						pio_port_c|=(1<<((b>>1)&7)); // SET BIT
						/*if (b==9&&tape_fastload&&((gate_mcr&12)==8)&&(type_id?0X2BD0:0X2A60)==z80_pc.w)
						{
							z80_af.b.l=0X40; // firmware optimisation: skip 1-second delay!
							int k=gate_index; // force palette update, f.e. "SILENT SHADOW"
							for (int i=type_id?0XB7D5:0XB1DA,j=0;j<16;++i,++j)
								gate_table_select(j),gate_table_send(64+(PEEK(i)&63));
							gate_table_select(k);
						}*/ // this optimisation does more harm than good, f.e. NEBULUS.CDT :-(
					}
					else
						pio_port_c&=~(1<<((b>>1)&7)); // RESET BIT
					if ((pio_port_c&0xC0)==0x40)
						pio_port_a=~autorun_kbd_bit(pio_port_c&15); // "SUPER CARS" does this
				}
			}
			tape_output=tape_type<0&&(pio_port_c&32); // tape record signal
		}
	}
	if (!(p&0x0400)) // 0xF800-0xFBFF, miscellaneous
	{
		if (!(p&0x0080)) // 0xFB7F, FDC 765
		{
			if (!disc_disabled)
			{
				if (p&0x100)
					disc_data_send(b); // 0xFB7F: DATA I/O
				else
					disc_motor_set(b&1); // 0xFA7E: MOTOR
			}
		}
		if (p==0xF8FF) // peripheral reset signal
		{
			//disc_reset();
			#ifdef PSG_PLAYCITY
			playcity_reset(); MEMZERO(playcity_ctc_count); session_dirty|=playcity_dirty,playcity_dirty=dac_voice=0;
		}
		if (p==0xF880)
			//cprintf("F880:%02X ",b),
			{ if (b<16) playcity_setcfg(b); } // CTC CHANNEL 0 CONFIG
		else if (!playcity_disabled)
		{
			//#ifdef Z80_NMI_ACK
			if (p==0xF881)
			{
				//cprintf("F881:%02X ",b); // CTC CHANNEL 1 CONFIG
				switch (playcity_ctc_state[1]) // extremely minimal NMI handler emulation
				{
					case 0:
						playcity_ctc_flags[1]=b;
						if (b!=3) // command $03 stops the channel...
							playcity_ctc_count[1]=0; // avoid triggering the counter between steps 0 and 1
						else if (playcity_ctc_count[1]>0&&playcity_ctc_count[1]<16) // ...but pending triggers will still pop up!
							playcity_ctc_count[1]=-1,z80_nmi_throw; // pending NMI!
						playcity_ctc_state[1]=b&4?1:0; // no need to request more bytes if bit 2 is OFF
						break;
					case 1:
						playcity_ctc_limit[1]=playcity_ctc_count[1]=b?b:256; // zero is handled as 256
					default: // no `break`!
						playcity_ctc_state[1]=0;
				}
			}
			//else if (p==0xF882) cprintf("F882:%02X ",b); // *!* todo *!*
			//else if (p==0xF883) cprintf("F883:%02X ",b); // *!* todo *!*
			//#endif
			else if (p==0xF884) dac_voice=0,session_dirty|=1&~playcity_dirty,playcity_dirty|=1,playcity_send(0,b); // YMZ RIGHT CHANNEL WRITE
			else if (p==0xF888) dac_voice=0,session_dirty|=2&~playcity_dirty,playcity_dirty|=2,playcity_send(1,b); // YMZ LEFT CHANNEL WRITE
			else if (p==0xF984) playcity_select(0,b); // YMZ RIGHT CHANNEL SELECT
			else if (p==0xF988) playcity_select(1,b); // YMZ LEFT CHANNEL SELECT
			#endif
		}
	}
}

#if 1 // TAPE_FASTLOAD
BYTE z80_tape_index[1<<16]; // full Z80 16-bit cache, 255 when unused
BYTE z80_tape_fastload[][32] = { // codes that read pulses : <offset, length, data> x N) -------------------------------------------------------------- MAXIMUM WIDTH //
/*  0 */ {  -8,   5,0X79,0XC6,0X02,0X4F,0X38,  +1,   7,0XED,0X78,0XAD,0XE6,0X80,0X20,0XF3 }, // AMSTRAD CPC FIRMWARE
/*  1 */ { -15,   5,0X04,0XC8,0X3E,0XF4,0XDB,  +1,   8,0XE6,0X04,0XEE,0X04,0XC0,0X3E,0XF5,0XDB,  +1,   5,0XA9,0XE6,0X80,0X28,0XEC }, // TOPO
/*  2 */ {  -5,  12,0X04,0XC8,0XD9,0XED,0X78,0XD9,0X1F,0XA9,0XE6,0X40,0X28,0XF4 }, // DINAMIC
/*  3 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   7,0X1F,0XC8,0XA9,0XE6,0X40,0X28,0XF3 }, // ALKATRAZ
/*  4 */ {  -4,   8,0X1C,0XC8,0XED,0X78,0XE6,0X80,0XBA,0XCA,-128, -10 }, // "LA ABADIA DEL CRIMEN"
/*  5 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   6,0XA9,0XE6,0X80,0XD8,0X28,0XF4 }, // MULTILOAD V1
/*  6 */ {  -5,   9,0X01,0X00,0XF5,0XED,0X78,0XCB,0X7F,0X20,0XF7 }, // OPERA V1+V2 1/2
/*  7 */ {  -5,   9,0X01,0X00,0XF5,0XED,0X78,0X2C,0XCB,0X7F,0X28,0XF6 }, // OPERA V1+V2 2/2
/*  8 */ {  -2,   3,0XED,0X78,0XFA,-128,  -5 }, // OPERA V3 1/2, GREMLIN (GAP)
/*  9 */ {  -3,   4,0X2C,0XED,0X78,0XF2,-128,  -6 }, // OPERA V3 2/2
/* 10 */ {  -4,   8,0X24,0XC8,0XED,0X78,0XA9,0XE6,0X80,0XCA,-128, -10 }, // UNILODE
/* 11 */ {  -6,  11,0X24,0XC8,0X06,0XF5,0XED,0X78,0XA9,0XE6,0X80,0X28,0XF5 }, // SPEEDLOCK V1+V2+V3
/* 12 */ {  -3,   5,0X1C,0XED,0X78,0XA9,0XF2,-128,  -7 }, // BLEEPLOAD SINGLE+DOUBLE
/* 13 */ {  -3,   5,0X2C,0XED,0X78,0XA9,0XF2,-128,  -7 }, // BLEEPLOAD MUSICAL
/* 14 */ {  -2,   4,0XED,0X78,0XA9,0XF2,-128,  -6 }, // BLEEPLOAD GAP
/* 15 */ {  -5,  11,0X04,0XC8,0XD9,0XED,0X78,0XD9,0XA9,0XE6,0X80,0X28,0XF5 }, // MIKRO-GEN
/* 16 */ {  -7,  10,0X04,0XC8,0XD9,0X06,0XF5,0XED,0X78,0XD9,0XA9,0XF2,-128, -12 }, // HI-TEC
/* 17 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   7,0X1F,0X1F,0XA9,0XE6,0X20,0X28,0XF3 }, // ZYDROLOAD
/* 18 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   5,0XA9,0XE6,0X80,0X28,0XF5 }, // MINILOAD-CPC
/* 19 */ {  -3,   4,0X0C,0XED,0X78,0XFA,-128,  -6 }, // GREMLIN 1/2
/* 20 */ {  -3,   4,0X0C,0XED,0X78,0XF2,-128,  -6 }, // GREMLIN 2/2
/* 21 */ {  -7,  14,0X04,0XC8,0X37,0X00,0XD9,0XED,0X78,0XD9,0X00,0XA9,0XE6,0X80,0X28,0XF2 }, // BINARY DESIGN, CODEMASTERS
/* 22 */ {  -7,  13,0X04,0XC8,0XD9,0X06,0XF5,0XED,0X78,0XD9,0XA9,0XE6,0X80,0X28,0XF3 }, // CODEMASTERS 1/3
/* 23 */ {  -6,   8,0X14,0XC8,0X06,0XF5,0XED,0X78,0XA9,0XF2,-128, -10 }, // CODEMASTERS 2/3
/* 24 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   9,0X1F,0X00,0X00,0X00,0XA9,0XE6,0X40,0X28,0XF1 }, // CODEMASTERS 3/3
/* 25 */ {  -6,   8,0X24,0XC8,0X06,0XF5,0XED,0X78,0XA9,0XF2,-128, -10 }, // SPEEDLOCK LEVELS
/* 26 */ {  -2,   7,0XED,0X78,0XE6,0X80,0XA9,0X28,0XF9 }, // EH SERVICES 1/2
/* 27 */ {  -9,   6,0XED,0X5F,0XFD,0XBE,0X00,0X30,  +1,   7,0XED,0X78,0XE6,0X80,0XA9,0X28,0XF2 }, // EH SERVICES 2/2
/* 28 */ {  -4,   3,0X3E,0XF5,0XDB,  +1,   5,0X07,0X38,0X02,0X10,0XF7 }, // "PUFFY'S SAGA" 1/2
/* 29 */ {  -4,   3,0X3E,0XF5,0XDB,  +1,   5,0X07,0X30,0X02,0X10,0XF7 }, // "PUFFY'S SAGA" 2/2
/* 30 */ {  -5,  13,0X04,0XC8,0XD9,0XED,0X78,0XD9,0X00,0X00,0XA9,0XE6,0X80,0X28,0XF3 }, // RAINBOW ARTS
/* 31 */ {  -5,  13,0X04,0XC8,0XD9,0XED,0X78,0XD9,0X1F,0X00,0XA9,0XE6,0X40,0X28,0XF3 }, // GREMLIN OLD
/* 32 */ {  -6,  16,0X04,0XC8,0XD9,0X42,0XED,0X78,0XD9,0XF6,0X01,0X1F,0XD0,0XA9,0XE6,0X40,0X28,0XF0 }, // ICON DESIGN
/* 33 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   7,0X07,0XA9,0XE6,0X01,0X00,0X28,0XF3 }, // MULTILOAD V2
/* 34 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   7,0X00,0XA9,0XD8,0XE6,0X80,0X28,0XF3 }, // "SUPERTRUX" (1/2)
/* 35 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +1,   6,0X00,0XA9,0XE6,0X80,0X28,0XF4 }, // "SUPERTRUX" (2/2)
/* 36 */ {  -4,   8,0X14,0XC8,0XED,0X78,0XE6,0X80,0XA9,0XCA,-128, -10 }, // "SPLIT PERSONALITIES"
/* 37 */ {  -6,   5,0X04,0XC8,0X3E,0XF5,0XDB,  +2,   6,0XD8,0XA9,0XE6,0X80,0X28,0XF3 }, // "BIRDIE", "JOYAUX DE BABYLONE"
/* 38 */ {  -9,   5,0X79,0XC6,0X02,0X4F,0XDA,  +2,   6,0XED,0X78,0XAD,0XE6,0X80,0XC2,-128, -15 }, // "KORONIS RIFT" LEVELS
/* 39 */ {  -5,  13,0X04,0XC8,0XD9,0XED,0X78,0XD9,0X1F,0X1F,0XA9,0XE6,0X20,0X28,0XF3 }, // "BOMBFUSION"
/* 40 */ {  -2,   6,0XED,0X78,0XE6,0X80,0XBA,0XC2,-128,  +4,  +0,   2,0X1D,0XC2,-128, -12 }, // "FRANK BRUNO'S BOXING"
/* 41 */ {  -5,  11,0X04,0XC8,0XD9,0XED,0X78,0XD9,0XE6,0X80,0XB9,0X28,0XF5 }, // "SONIC BOOM", "P-47"...
/* 42 */ {  -7,   3,0X0C,0XC5,0X01,  +1,   8,0XF5,0XED,0X78,0XCB,0X17,0XC1,0X30,0XF4 }, // "CARSON CITY" (1/2)
/* 43 */ {  -7,   3,0X0C,0XC5,0X01,  +1,   8,0XF5,0XED,0X78,0XCB,0X17,0XC1,0X38,0XF4 }, // "CARSON CITY" (2/2)
/* 44 */ {  -8,  15,0X04,0XC8,0X00,0XD9,0X06,0XF5,0XED,0X78,0XD9,0X1F,0XA9,0XE6,0X40,0X28,0XF1 }, // "LOTUS ESPRIT TURBO CHALLENGE"
};
BYTE z80_tape_fastfeed[][32] = { // codes that build bytes
/*  0 */ {  -9,   1,0X2A,  +5,   1,0XDC,  +2,   8,0X30,0X0D,0X7C,0X91,0X9F,0XCB,0X12,0XCD,  +2,   3,0X1D,0X20,0XEA }, // AMSTRAD CPC FIRMWARE
/*  1 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   2,0X30,0XF3 }, // TOPO + DINAMIC
/*  2 */ {  -0,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -13 }, // GRANDSLAM
/*  3 */ {  -5,   1,0X06,  +4,   1,0XD2,  +2,   1,0X3E,  +1,   7,0XB8,0XCB,0X15,0X3E,0X00,0X00,0X3E,  +1,   1,0XD2,-128, -21 }, // ALKATRAZ
/*  4 */ { -26,   1,0X3E,  +1,   1,0XCD, +13,   1,0X3E, +12,   2,0X7B,0XFE,  +1,   5,0X3F,0XCB,0X15,0X30,0XDB }, // "LA ABADIA DEL CRIMEN"
/*  5 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2/*,-128, -14*/ }, // TEQUE + ZYDROLOAD
/*  6 */ {  -7,   2,0XC5,0XCD,  +5,   2,0X7D,0XFE,  +1,   7,0X3F,0XCB,0X14,0XAC,0XE6,0X1F,0XCD,  +2,   3,0XC1,0X10,0XEA }, // OPERA
/*  7 */ { -12,   1,0X3E,  +1,   1,0XCD,  +2,   3,0XD0,0XE5,0X3E,  +4,   3,0XE1,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   1,0XD2,-128, -24 }, // SPEEDLOCK V1
/*  8 */ { -13,   1,0X3E,  +1,   1,0XCD,  +2,   4,0XD0,0X00,0X00,0X3E,  +4,   2,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   1,0XD2,-128, -24 }, // SPEEDLOCK V2
/*  9 */ { -11,   1,0X3E,  +1,   1,0XCD,  +2,   2,0XD0,0X3E,  +4,   2,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   1,0XD2,-128, -22 }, // SPEEDLOCK V3
/* 10 */ {  -6,   1,0XCD,-128, +13,  +0,   1,0XCD,-128, +12,  +0,   1,0X3E,  +1,   5,0XBB,0XCB,0X12,0X30,0XF3 }, // BLEEPLOAD DOUBLE 1/2
/* 11 */ {  -6,   1,0XCD,-128, +14,  +0,   1,0XCD,-128, +13,  +0,   1,0X3A,  +2,   5,0XBB,0XCB,0X12,0X30,0XF2 }, // BLEEPLOAD DOUBLE 2/2
/* 12 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   2,0X30,0XF3 }, // RICOCHET
/* 13 */ {  -0,   1,0XD2,  +2,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -16 }, // MIKRO-GEN
/* 14 */ {  -5,   1,0X06,  +4,   1,0XD2,  +2,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X3E,  +1,   1,0XD2,-128, -18 }, // HEXAGON
/* 15 */ {  -9,   1,0X2A,  +5,   1,0XDC,  +2,  10,0X30,0X0A,0X7C,0X91,0X9F,0XCB,0X12,0X1D,0X20,0XED }, // CASSYS
/* 16 */ {  -5,   1,0X3E,  +4,   1,0X30,  +1,   1,0X3E,  +1,   4,0XBA,0XCB,0X13,0X16,  +1,   2,0X30,0XF0 }, // CODEMASTERS
/* 17 */ {  -5,   1,0X3E,  +4,   1,0X30,  +1,   1,0X3E,  +1,   4,0XBC,0XCB,0X15,0X26,  +1,   2,0X30,0XF0 }, // SPEEDLOCK LEVELS
/* 18 */ {  -0,   1,0X30,  +1,   5,0X90,0XCB,0X15,0X30,0XF1 }, // MINILOAD-CPC 1/2
/* 19 */ {  -0,   6,0XD0,0X90,0XCB,0X15,0X30,0XF2 }, // MINILOAD-CPC 2/2
/* 20 */ {  -0,   1,0XFE,  +1,   4,0X3F,0XCB,0X13,0XD2,-128, -11 }, // GREMLIN
/* 21 */ {  -0,   1,0X30,  +1,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   2,0X30,0XF2 }, // MULTILOAD V2
/* 22 */ {  -5,   1,0X3E,  +4,   2,0XD0,0X3E,  +1,   4,0XBA,0XCB,0X13,0X16,  +1,   2,0X30,0XF1 }, // CODEMASTERS 2/3
/* 23 */ {  -0,   3,0X0E,0X01,0X18 }, // UNILODE
};
#ifdef FASTTAPE_DUMPER
BYTE z80_tape_fastdump[][32] = { // codes that fill blocks
/*  0 */ {  -0,  10,0XD0,0XDD,0X77,0X00,0XDD,0X23,0X15,0X1D,0X20,0XF3 }, // AMSTRAD CPC FIRMWARE
/*  1 */ { -29,   8,0X08,0X20,0X05,0XDD,0X75,0X00,0X18,0X0A, +10,   5,0XDD,0X23,0X1B,0X08,0X06, +16,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD2 }, // TOPO
/*  2 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCB }, // DINAMIC
/*  3 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X2B,0X1B, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCB }, // GRANDSLAM
/*  4 */ {  -0,  12,0XDD,0X75,0X00,0XDD,0X2B,0XE1,0X2B,0X7C,0XB5,0XC8,0XE5,0XC3,-128, -17 }, // "LA ABADIA DEL CRIMEN"
/*  5 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +19,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCA }, // TEQUE
/*  6 */ {  -9,   2,0X06,0X08, +22,  10,0XDD,0X74,0X00,0XDD,0X2B,0X1B,0X7B,0XB2,0X20,0XDE }, // OPERA V1
/*  7 */ {  -9,   2,0X06,0X08, +22,  17,0XDD,0X74,0X00,0X7C,0XD9,0X5F,0X16,0X00,0X19,0XD9,0XDD,0X2B,0X1B,0X7B,0XB2,0X20,0XD7 }, // OPERA V2
/*  8 */ { -29,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X26,  +3,   4,0X2E,0X01,0X00,0X3E,  +1,   2,0X18,0X02, +24,   7,0X08,0XAD,0X08,0X7A,0XB3,0X20,0XD0 }, // SPEEDLOCK V1
/*  9 */ { -30,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X26,  +3,   4,0X2E,0X01,0X00,0X3E,  +1,   2,0X18,0X02, +24,   7,0X08,0XAD,0X08,0X7A,0XB3,0X20,0XD0 }, // SPEEDLOCK V1.5
/* 10 */ { -13,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X06,  +1,   2,0X2E,0X01, +14,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE1 }, // CODEMASTERS
/* 11 */ { +11,  11,0XDD,0X75,0X00,0XDD,0X23,0X2E,0X01,0X1B,0X7A,0XB3,0X3E,  +1,   4,0X00,0X00,0X20,0XE2 }, // SPEEDLOCK LEVELS
/* 12 */ {  -5,   1,0X06,  +4,  11,0XD0,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X7A,0XB3,0X20,0XF0 }, // RAINBOW ARTS
/* 13 */ { -16,   4,0XDD,0X75,0X00,0XDD,  +1,   4,0X1B,0X2E,0X01,0X06, +13,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE3 }, // MINILOAD-CPC
/* 14 */ { +11,  10,0X73,0X23,0X1E,0X01,0XD9,0X1B,0X7A,0XB3,0XD9,0X3E,  +1,   2,0X20,0XE5 }, // CODEMASTERS 2/3
/* 15 */ { -13,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X26,  +1,   2,0X2E,0X01, +16,   5,0X00,0X7A,0XB3,0X20,0XE1 }, // RICOCHET
};
#endif

WORD z80_tape_spystack(WORD d) { d+=z80_sp.w; int i=PEEK(d); ++d; return (PEEK(d)<<8)+i; } // must keep everything 16-bit!
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
void z80_tape_fastload_ccitt(int mask8) // the AMSTRAD CPC firmware keeps its own checksum
{
	WORD x=type_id?0xB1EB:0xB8D3,crc16=mgetii(&POKE(x)); // CCITT 16-bit checksum location
	do
		crc16=(mask8^crc16)&0x8000?(crc16<<1)^4129:crc16<<1;
	while ((BYTE)(mask8<<=1)); // notice that `mask8` uses the bottom 8 bits as a limiter
	mputii(&POKE(x),crc16);
}

void z80_tape_trap(void)
{
	int i,j; WORD w; BYTE k; if ((i=z80_tape_index[z80_pc.w])>length(z80_tape_fastload))
	{
		for (i=0;i<length(z80_tape_fastload)&&!fasttape_test(z80_tape_fastload[i],z80_pc.w);++i) {}
		z80_tape_index[z80_pc.w]=i; cprintf("FASTLOAD: %04X=%02d\n",z80_pc.w,(i<length(z80_tape_fastload))?i:-1);
	}
	if (i>=length(z80_tape_fastload)) return; // only known methods can reach here!
	if (!tape_skipping) tape_skipping=-1;
	switch (i)
	{
		case  0: // AMSTRAD CPC FIRMWARE, CASSYS ("WONDER BOY"), HEWSON ("EXOLON", "NEBULUS")
			if (z80_de.b.l==0x08&&FASTTAPE_CAN_FEED()&&(((j=z80_tape_testfeed(z80_tape_spystack(0)))==0&&!(gate_mcr&4))||j==15))
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_tape_testdump(z80_tape_spystack(4))==0)
					while (FASTTAPE_CAN_DUMP()&&(w=z80_sp.w+2,POKE(w)>1)
					{
						k=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--POKE(w),++w,--POKE(w);
						if (j==0) z80_tape_fastload_ccitt((k<<8)+1); // handle all 8 bits in a single step
					}
				#endif
				k=fasttape_feed(~(z80_hl.b.l>>7),16),z80_de.b.h=k>>1,tape_skipping=z80_de.b.l=1,z80_bc.b.l=-(k&1);
				if (j==0) z80_tape_fastload_ccitt((k<<8)+2); // handle 7 bits, leave the last one to the firmware!
			}
			else
			{
				z80_r7+=fasttape_add8(!(z80_hl.b.l>>7),16,&z80_bc.b.l,2)*9;
				if (!(gate_mcr&4)&&z80_tape_spystack(0)<=(type_id?0x2AA0:0x2930))
					fasttape_gotonext(); // if the ROM is expecting the PILOT, throw BYTES and PAUSE away!
			}
			break;
		case  1: // TOPO ("DESPERADO", "MAD MIX GAME", "VIAJE AL CENTRO DE LA TIERRA")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==1)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if (z80_tape_testdump(z80_tape_spystack(0))==1)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>7,25),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>7,25,&z80_bc.b.h,1); // z80_r7+=...*12;
			break;
		case  2: // DINAMIC ("ABU SIMBEL PROFANATION", "GAME OVER", "PHANTIS"), GRANDSLAM ("PACMANIA"), HI-TEC ("CRYSTAL KINGDOM DIZZY")
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==1||j==2||j==5))
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==2||k==3||k==5)
					while (FASTTAPE_CAN_DUMP()&&z80_de2.b.l>1)
						z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),k==3?--z80_ix.w:++z80_ix.w,--z80_de2.w;
				#endif
				k=fasttape_feed(z80_bc2.b.l>>6,16),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc2.b.l>>6,16,&z80_bc2.b.h,1); // z80_r7+=...*10;
			break;
		case  3: // ALKATRAZ ("E-MOTION"), HEXAGON ("ALIEN STORM"), "OLLI & LISA" (j==5)
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==3||j==5||j==14))
				k=fasttape_feed(z80_bc.b.l>>6,17),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>6,17,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case  4: // "LA ABADIA DEL CRIMEN"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==4)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_tape_testdump(z80_tape_spystack(2))==4)
					while (FASTTAPE_CAN_DUMP()&&(w=z80_sp.w+4,POKE(w)>1))
						POKE(z80_ix.w)=fasttape_dump(),--z80_ix.w,--POKE(w);
				#endif
				k=fasttape_feed(z80_de.b.h>>7,13),tape_skipping=z80_hl.b.l=128+(k>>1),z80_de.b.l=-(k&1);
			}
			else
				fasttape_add8(z80_de.b.h>>7,13,&z80_de.b.l,1); // z80_r7+=...*7;
			break;
		case  5: // MULTILOAD V1 ("DEFLEKTOR", "THE FINAL MATRIX", "TIME SCANNER" CODE)
			fasttape_add8(z80_bc.b.l>>7,16,&z80_bc.b.h,1); // z80_r7+=...*8;
			break;
		case  6: // OPERA V1+V2 1/2 (V1: "GOODY"; V2: "ULISES")
			z80_r7|=128; fasttape_add8(1,12,&z80_r7,6);
			break;
		case  7: // OPERA V1+V2 2/2
			if (w=z80_sp.w+3,PEEK(w)==0x08&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==6)
			{
				#ifdef FASTTAPE_DUMPER
				if ((k=z80_tape_testdump(z80_tape_spystack(0)))==6||k==7)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl2.w+=POKE(z80_ix.w)=fasttape_dump(),--z80_ix.w,--z80_de.w; // k==6 doesn't touch Z80_HL2
				#endif
				k=fasttape_feed(0,13),POKE(w)=1,z80_hl.b.h=tape_skipping=(k>>1),z80_hl.b.l=k&1?-2:0; // -2 avoids the later INC L!
			}
			else
				fasttape_add8(0,13,&z80_hl.b.l,1); // z80_r7+=...*7;
			break;
		case  8: // OPERA V3 1/2 ("MUNDIAL DE FUTBOL")
			z80_r7|=128; fasttape_add8(1,7,&z80_r7,3); // "MARMELADE" uses R as counter!
			break;
		case  9: // OPERA V3 2/2
			if (w=z80_sp.w+3,PEEK(w)==0x08&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==6)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_tape_testdump(z80_tape_spystack(0))==7)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl2.w+=POKE(z80_ix.w)=fasttape_dump(),--z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(0,8),POKE(w)=1,z80_hl.b.h=tape_skipping=(k>>1),z80_hl.b.l=-(k&1);
			}
			else
				fasttape_add8(0,8,&z80_hl.b.l,1); // z80_r7+=...*4;
			break;
		case 10: // UNILODE ("TRIVIAL PURSUIT")
			if ((z80_af.b.l&1)&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==23) // we must force a RET in this loader :-(
				k=fasttape_feed(z80_bc.b.l>>7,13),tape_skipping=z80_bc.b.l=128+(k>>1),z80_hl.b.h=-(k&1),k=z80_tape_spystack(0)+2,z80_pc.w=z80_tape_spystack(0)+2,z80_sp.w+=2;
			else
				fasttape_add8(z80_bc.b.l>>7,13,&z80_hl.b.h,1); // z80_r7+=...*7;
			break;
		case 11: // SPEEDLOCK (V1+V1.5+V2+V3: "DONKEY KONG", "ARKANOID", "WIZBALL", "THE ADDAMS FAMILY"), RICOCHET ("RICK DANGEROUS 2")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==7||j==8||j==9||j==12))
			{
				#ifdef FASTTAPE_DUMPER
				if ((k=z80_tape_testdump(z80_tape_spystack(0)))==8||k==9||k==15)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_af2.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w; // some Ricochet versions ("OCTOPLEX") don't touch Z80_AF2
				#endif
				k=fasttape_feed(z80_bc.b.l>>7,15);
				if (j==7) // SPEEDLOCK V1 uses the stack
					w=z80_sp.w+2,POKE(w)=tape_skipping=128+(k>>1),++w,POKE(w)=-(k&1);
				else // all other versions use registers
					tape_skipping=z80_hl.b.l=128+(k>>1),z80_hl.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>7,15,&z80_hl.b.h,1); // z80_r7+=...*8;
			break;
		case 12: // BLEEPLOAD (SINGLE: "CHIMERA"; DOUBLE: "SENTINEL", "THRUST (V2)")
			if (z80_de.b.h==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(2)))==10||j==11))
				k=fasttape_feed(z80_bc.b.l>>7,9),tape_skipping=z80_de.b.h=128+(k>>1),z80_de.b.l=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>7,9,&z80_de.b.l,1); // z80_r7+=...*5;
			break;
		case 13: // BLEEPLOAD MUSICAL
			fasttape_add8(z80_bc.b.l>>7,9,&z80_hl.b.l,1); // z80_r7+=...*5;
			break;
		case 14: // BLEEPLOAD GAP
			fasttape_skip(z80_bc.b.l>>7,8); // z80_r7+=...*4;
			break;
		case 41: // "SONIC BOOM", "P-47"...
		case 15: // MIKRO-GEN ("FROST BYTE")
		case 16: // HI-TEC ("INTERCHANGE")
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==13||j==5))
				k=fasttape_feed(z80_bc2.b.l>>7,15),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			else
				fasttape_add8(z80_bc2.b.l>>7,15,&z80_bc2.b.h,1);
			break;
		case 17: // ZYDROLOAD ("HOSTAGES")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==5)
				k=fasttape_feed(z80_bc.b.l>>5,16),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>5,16,&z80_bc.b.h,1);
			break;
		case 18: // MINILOAD-CPC
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==18||j==19))
				k=fasttape_feed(z80_bc.b.l>>7,14),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>7,14,&z80_bc.b.h,1); // z80_r7+=...*7;
			break;
		case 19: // GREMLIN 1/2 ("BASIL THE GREAT MOUSE DETECTIVE", "SAMURAI TRILOGY")
			fasttape_add8(1,8,&z80_bc.b.l,1); // z80_r7+=...*4;
			break;
		case 20: // GREMLIN 2/2
			if (z80_de.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==20)
				k=fasttape_feed(0,8),tape_skipping=z80_de.b.l=128+(k>>1),z80_bc.b.l=-(k&1);
			else
				fasttape_add8(0,8,&z80_bc.b.l,1); // z80_r7+=...*4;
			break;
		case 21: // BINARY DESIGN ("DEFCOM"), CODEMASTERS ("MAGICLAND DIZZY")
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==5)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if (z80_tape_testdump(z80_tape_spystack(0))==5)
					while (FASTTAPE_CAN_DUMP()&&z80_de2.b.l>1)
						z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de2.w;
				#endif
				k=fasttape_feed(z80_bc2.b.l>>7,18),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc2.b.l>>7,18,&z80_bc2.b.h,1);
			break;
		case 22: // CODEMASTERS 1/3 ("SUPER SEYMOUR SAVES THE PLANET")
		case 30: // RAINBOW ARTS ("ROCK'N ROLL")
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==5)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_tape_testdump(z80_tape_spystack(0))==10) // CODEMASTERS
					while (FASTTAPE_CAN_DUMP()&&z80_de2.b.l>1)
						z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de2.w;
				else if (z80_tape_testdump(z80_tape_spystack(2))==12) // RAINBOW ARTS
					while (FASTTAPE_CAN_DUMP()&&z80_de2.b.l>1)
						POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de2.w;
				#endif
				k=fasttape_feed(z80_bc2.b.l>>7,17),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc2.b.l>>7,17,&z80_bc2.b.h,1);
			break;
		case 23: // CODEMASTERS 2/3 ("TREASURE ISLAND DIZZY"), OCEAN LEVELS ("THE VINDICATOR")
			if (z80_de.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==16||j==22))
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_tape_testdump(z80_tape_spystack(0))==14)
					while (FASTTAPE_CAN_DUMP()&&z80_de2.b.l>1)
						POKE(z80_hl.w)=fasttape_dump(),++z80_hl.w,--z80_de2.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>7,13),tape_skipping=z80_de.b.l=128+(k>>1),z80_de.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>7,13,&z80_de.b.h,1);
			break;
		case 24: // CODEMASTERS 3/3 ("KWIK SNAX")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==5)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if (z80_tape_testdump(z80_tape_spystack(0))==5)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>6,18),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>6,18,&z80_bc.b.h,1);
			break;
		case 25: // SPEEDLOCK LEVELS ("RAINBOW ISLANDS")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==17)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_tape_testdump(z80_tape_spystack(0))==11)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>7,13),tape_skipping=z80_hl.b.l=128+(k>>1),z80_hl.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>7,13,&z80_hl.b.h,1);
			break;
		case 26: // EH SERVICES 1/2 ("ONE", "BLUES BROTHERS")
			z80_r7|=128; fasttape_add8(z80_bc.b.l>>7,10,&z80_r7,5);
			break;
		case 27: // EH SERVICES 2/2
			z80_r7+=10,tape_main(20); // no fasttape_skip :-(
			break;
		case 28: // "PUFFY'S SAGA" 1/2
		case 29: // "PUFFY'S SAGA" 2/2
			j=(WORD)(z80_pc.w+1); // it self-modifies :-(
			fasttape_sub8(PEEK(j)==0x30,12,&z80_bc.b.h,1);
			break;
		case 31: // GREMLIN OLD ("WAY OF THE TIGER" V1)
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==2||j==5))
				k=fasttape_feed(z80_bc2.b.l>>6,17),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			else
				fasttape_add8(z80_bc2.b.l>>6,17,&z80_bc2.b.h,1);
			break;
		case 32: // ICON DESIGN ("RECKLESS RUFUS", "NETHER EARTH")
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==1||j==5))
				k=fasttape_feed(z80_bc2.b.l>>6,21),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			else
				fasttape_add8(z80_bc2.b.l>>6,21,&z80_bc2.b.h,1);
			break;
		case 33: // MULTILOAD V2 ("TIME SCANNER" LEVELS, "SUPER CARS")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==21)
				k=fasttape_feed(z80_bc.b.l,16),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l,16,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case 34: // "SUPERTRUX" (1/2)
		case 37: // "BIRDIE" (btw, it requires 3223 pilots, not 3224: it cannot be BLOCK $10!), "JOYAUX DE BABYLONE"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==5)
				k=fasttape_feed(z80_bc.b.l>>7,17),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>7,17,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case 35: // "SUPERTRUX" (2/2)
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==5)
				k=fasttape_feed(z80_bc.b.l>>7,15),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>7,15,&z80_bc.b.h,1); // z80_r7+=...*8;
			break;
		case 36: // "SPLIT PERSONALITIES"
			fasttape_add8(z80_bc.b.l>>7,13,&z80_de.b.h,1); // z80_r7+=...*7;
			break;
		case 38: // "KORONIS RIFT" LEVELS (main loader is case #0)
			fasttape_add8(!(z80_hl.b.l>>7),17,&z80_bc.b.l,2); // z80_r7+=...*9;
			break;
		case 39: // "BOMBFUSION"
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==5)
				k=fasttape_feed(z80_bc2.b.l>>5,16),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			else
				fasttape_add8(z80_bc2.b.l>>5,17,&z80_bc2.b.h,1);
			break;
		case 40: // "FRANK BRUNO'S BOXING"
			fasttape_sub8(z80_de.b.h>>7,14,&z80_de.b.l,1);
			break;
		case 42: // "CARSON CITY" (1/2)
		case 43: // "CARSON CITY" (2/2)
			fasttape_add8(i&1,20,&(POKE(z80_sp.w)),1);
			break;
		case 44: // "LOTUS ESPRIT TURBO CHALLENGE"
			if (z80_hl2.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==1)
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==...)
					while (FASTTAPE_CAN_DUMP()&&z80_de2.b.l>1)
						z80_hl2.b.h^=POKE(z80_ix.w)=fasttape_dump(),k==3?--z80_ix.w:++z80_ix.w,--z80_de2.w;
				#endif
				k=fasttape_feed(z80_bc2.b.l>>6,19),tape_skipping=z80_hl2.b.l=128+(k>>1),z80_bc2.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc2.b.l>>6,19,&z80_bc2.b.h,1); // z80_r7+=...*8;
			break;
	}
}
#endif

BYTE z80_recv(WORD p) // the Z80 receives a byte from a hardware port
{
	BYTE b=255; z80_loss=0; // as in z80_send, multiple devices can answer to the Z80 request at the same time if the bit patterns match; hence the use of "b&=" from the second device onward.
	if (!(p&0x8000)) // 0x7F00, GATE ARRAY (1/2)
		cprintf("%08X: RECV $%04X!\n",z80_pc.w,p); // do any titles do this!?
	if ((p&0x4200)==0x0200) // 0xBE00-0xBF00, CRTC 6845
	{
		if (!(p&0x100)) // 0xBE00: depends on the CRTC type!
		{
			if (crtc_type>=3)
				b=crtc_table_recv(); // CRTC3,CRTC4: READ CRTC REGISTER
			else if (crtc_type==1)
				b=crtc_table_info(); // CRTC1: READ CRTC INFORMATION STATUS
			//else if (crtc_type==0)
				//b=0; // CRTC0: ALL RESET! // "ECOLE BUISSONIERE" disagrees!
			//else b=255; // CRTC2: ALL SET!
		}
		else // 0xBF00: depends on the CRTC type too!
		{
			if (crtc_type==1&&crtc_index==31&&ram_depth>1) // high impedance bogus register! used "THE-ONE" by AsT, identified as unreliable by Madram et Roudoudou
				b=(crtc_status&CRTC_STATUS_HSYNC)?255:127;
			else if (crtc_type>0&&crtc_type<3)
				b=0; // CRTC1,CRTC2: ALL RESET!
			else
				b=crtc_table_recv(); // CRTC0,CRTC3,CRTC4: READ CRTC REGISTER
		}
	}
	if (!(p&0x0800)) // 0xF400-0xF700, PIO 8255
	{
		if (!(p&0x0200))
		{
			if (!(p&0x0100)) // 0xF400, PIO PORT A
			{
				if ((pio_control&0x10)||plus_enabled) // "TIRE AU FLAN" expects this to detect PLUS! "GX8K" accidentally reads port A (or C!) when it should read port B :-(
					b&=pio_port_a=(psg_index==14&&!psg_port_a_lock())?~autorun_kbd_bit(pio_port_c&15):pio_control&0x10?psg_table_recv():pio_port_a; // READ PSG REGISTER
				else
					b&=pio_port_a; // just look at the port
			}
			else // 0xF500, PIO PORT B
			{
				if ((pio_control&2)||plus_enabled) // PLUS ASIC CRTC3 has a PIO bug!
				{
					//static int zzz=0; if (z80_pc.w<0XBB00) if (mmu_rom[0]==mem_ram) { if (zzz!=z80_pc.w) zzz=z80_pc.w,cprintf("$F5:%04X  ",z80_pc.w); else cputchar('.'); }
					#if 1 // TAPE_FASTLOAD
					if (tape)//&&!z80_iff.b.l) // at least one tape loader enables interrupts: "INVASION OF THE ZOMBIE MONSTERS"
						if (tape_fastload&&tape_loud) z80_tape_trap(); // handle tape analysis upon status
					#endif
					b&=(crtc_status&CRTC_STATUS_VSYNC? // gate_status&CRTC_STATUS_VSYNC ???
						(crtc_type!=2||crtc_table[2]+crtc_limit_r3x!=crtc_table[0]+1): // CRTC2 VSYNC fails if HSYNC sets and H_OFF resets at once!
						((crtc_table[8]&2)&&irq_timer<3&&video_pos_y>=VIDEO_LENGTH_Y*5/12&&video_pos_y<VIDEO_LENGTH_Y*7/12)) // interlaced VSYNC
						+(printer?0:64)+(tape_status>tape_delay?0x9E:0x1E); // TAPE SIGNAL (0x00/0x80 -- "LOGON DEMO 2" expects 0x00 w/o a tape)
						// *!* todo: AMSTRAD MODEL (0x1E) *!*
				}
				else
					b&=pio_port_b; // CRTC0,CRTC1,CRTC2,CRTC4 have a good PIO
			}
		}
		else
		{
			if (!(p&0x0100)) // 0xF600, PIO PORT C
				b&=(pio_control&1)?15|(pio_port_c&~15):pio_port_c; // *!* todo: else...? *!*
		}
	}
	if (!(p&0x2000)) // 0xDF00, GATE ARRAY (2/2)
		cprintf("%08X: RECV $%04X!\n",z80_pc.w,p); // do any titles do this!?
	if (!(p&0x0400)) // 0xFB00, FDC 765
	{
		if ((p&0x0380)==0x0300) // 0xFB7F: DATA I/O // 0xFB7E: STATUS
			if (!disc_disabled)
				b&=p&1?disc_data_recv():disc_data_info(); // 0xF87E+n
		#ifdef PSG_PLAYCITY
		else if (!playcity_disabled) // is this real? can the Playcity be read?
		{
			if (p==0xF884) b=playcity_recv(0); // YMZ RIGHT CHANNEL READ
			else if (p==0xF888) b=playcity_recv(1); // YMZ LEFT CHANNEL READ
		}
		#endif
	}
	return p==0xFEFE?0xCE:b; // emulator ID styled after the old CPCE
}

void z80_trap(WORD p,BYTE b) // catch Z80 write operations
{
	switch (p>>8)
	{
		// PLUS ASIC: the range 0x4000-0x6C0F behaves like a hardware address set
		case 0x40: case 0x41: case 0x42: case 0x43:
		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4A: case 0x4B:
		case 0x4C: case 0x4D: case 0x4E: case 0x4F:
			// sprite pixels: plus_sprite_bmp
			plus_bank[p-0x4000]=b&15;
			//plus_dirtysprite=(p>>8)&15;
			break;
		case 0x60: // sprite coordinates: plus_sprite_xyz
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
						p&=-4; // clip last bits (EERIE FOREST!)
						memset(&plus_bank[p-0x4000],b&15,4);
						break;
				}
			break;
		case 0x64: // colour palette: plus_palette
			if (p<0x6440)
			{
				if (p&1)
					b&=15; // a nibble, not a byte
				plus_bank[p-0x4000]=b;
				p&=64-2; // select ink
				video_clut_index=video_clut+p/2; // keep ASIC and Gate Array from clashing
				video_clut_value=video_xlat_rgb(video_table[32+plus_palette[p+1]]+video_table[48+(plus_palette[p]>>4)]+video_table[64+(plus_palette[p]&15)]);
				if (plus_sprite_adjust<8) *video_clut_index=video_clut_value; // fast update
			}
			break;
		case 0x68: // scanline events: plus_pri, plus_sssl, plus_ssss (x2), plus_sscr, plus_ivr
			if (p<0x6806&&plus_bank[p-0x4000]!=b)
			{
				if (p==0x6800) // plus_pri, PROGRAMMABLE RASTER INTERRUPT
				{
					if (crtc_line==b&&crtc_status&CRTC_STATUS_HSYNC) // "FIRE N FORGET 2" needs this!
						z80_irq|=128; // SET!
					else if (b) // "EERIE FOREST" needs this!
						z80_irq&=64+32+16; // RESET!
				}
				else if (p==0x6804) // plus_sscr, SOFT SCROLL CONTROL REGISTER
				{
					crtc_raster=((crtc_count_r9<<11)+(b<<7))&0x3800; // the impact is immediate
				}
				plus_bank[p-0x4000]=b;
			}
			break;
		case 0x6C: // DMA channel parameters: plus_dmas (x15), plus_dcsr
			if (p<0x6C10)
			{
				if (p==0x6C0F) // plus_dcsr, DMA CONTROL/STATUS REGISTER
				{
					z80_irq&=~(b&0x70);
					if (!(b&1)) z80_irq&=~16;
					if (!(b&2)) z80_irq&=~32;
					if (!(b&4)) z80_irq&=~64;
					b=((plus_dcsr&128)+(b&7))|z80_irq;
				}
				plus_bank[p-0x4000]=b;
			}
			break;
	}
}

// CPU: ZILOG Z80 MICROPROCESSOR ==================================== //

const BYTE z80_delays[]= // precalc'd coarse timings
{
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x000 -
	1,3,2,2,1,1,2,1,1,3,2,2,1,1,2,1, // base: 1/2
	3,3,2,2,1,1,2,1,3,3,2,2,1,1,2,1, // 0x10-0x1F
	2,3,4,2,1,1,2,1,2,3,5,2,1,1,2,1, // 0x20-0x2F
	2,3,4,2,3,3,3,1,2,3,4,2,1,1,2,1, // 0x30-0x3F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x40-0x4F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x50-0x5F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x60-0x6F
	2,2,2,2,2,2,1,2,1,1,1,1,1,1,2,1, // 0x70-0x7F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x80-0x8F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0x90-0x9F
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0xA0-0xAF
	1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1, // 0xB0-0xBF
	2,3,3,3,3,3,2,4,2,3,3,1,3,5,2,4, // 0xC0-0xCF
	2,3,3,2,3,3,2,4,2,1,3,2,3,1,2,4, // 0xD0-0xDF // "Zap'T'Balls Adv.Edition" menu/hiscores and "Onescreen Colonies 2" title show OUT (NN),A as 3+IORQ rather than 2+IORQ+1
	2,3,3,5,3,3,2,4,2,1,3,1,3,1,2,4, // 0xE0-0xEF
	2,3,3,1,3,3,2,4,2,2,3,1,3,1,2,4, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x100 -
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // base: 2/2
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x10-0x1F
	1,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0, // 0x20-0x2F
	1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0, // 0x30-0x3F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x40-0x4F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x50-0x5F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x60-0x6F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x70-0x7F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x80-0x8F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x90-0x9F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xA0-0xAF
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xB0-0xBF
	2,0,0,0,2,1,0,0,2,0,0,0,2,0,0,0, // 0xC0-0xCF
	2,0,0,1,2,1,0,0,2,0,0,1,2,0,0,0, // 0xD0-0xDF
	2,0,0,1,2,1,0,0,2,0,0,0,2,0,0,0, // 0xE0-0xEF
	2,0,0,0,2,1,0,0,2,0,0,0,2,0,0,0, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x200 -
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // EDxx: 1/2
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x10-0x1F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x20-0x2F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x30-0x3F
	3,2,3,4,1,3,1,2,3,2,3,5,1,3,1,2, // 0x40-0x4F
	3,2,3,4,1,3,1,2,3,2,3,5,1,3,1,2, // 0x50-0x5F
	3,2,3,4,1,3,1,4,3,2,3,5,1,3,1,4, // 0x60-0x6F
	3,2,3,4,1,3,1,1,3,2,3,5,1,3,1,1, // 0x70-0x7F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x80-0x8F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x90-0x9F
	3,3,4,4,1,1,1,1,3,3,4,4,1,1,1,1, // 0xA0-0xAF // "KKB First Demo" (that also uses OUT ($NN),A) and "Prehistorik 2" show OUTI as 0XED+4+IORQ rather than 0XED+3+IORQ+1
	3,3,4,4,1,1,1,1,3,3,4,4,1,1,1,1, // 0xB0-0xBF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xC0-0xCF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xD0-0xDF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xE0-0xEF
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0xF0-0xFF
	//1 2 3 4 5 6 7 8 9 A B C D E F	//// +0x300 -
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // EDxx: 2/2
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x10-0x1F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x20-0x2F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x30-0x3F
	0,1,0,1,0,0,0,0,0,1,0,0,0,0,0,0, // 0x40-0x4F
	0,1,0,1,0,0,0,0,0,1,0,0,0,0,0,0, // 0x50-0x5F
	0,1,0,1,0,0,0,0,0,1,0,0,0,0,0,0, // 0x60-0x6F
	0,1,0,1,0,0,0,0,0,1,0,0,0,0,0,0, // 0x70-0x7F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x80-0x8F
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x90-0x9F
	1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0, // 0xA0-0xAF
	2,2,1,1,0,0,0,0,2,2,1,1,0,0,0,0, // 0xB0-0xBF
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
	0,3,4,2,1,1,2,0,0,3,5,2,1,1,2,0, // 0x20-0x2F
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
	2,3,0,5,0,3,0,0,2,1,0,0,0,0,0,0, // 0xE0-0xEF
	2,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0, // 0xF0-0xFF
};

// input/output
#define Z80_SYNC() ( _t_-=z80_t, z80_sync(z80_t), z80_t=0 )
#define Z80_SYNC_IO ( _t_-=z80_t, z80_sync(z80_t) ) // see Z80_STRIDE_IO for the missing "z80_t=0"
#define Z80_PRAE_RECV(w) Z80_SYNC_IO
#define Z80_RECV z80_recv
#define Z80_POST_RECV(w) (_t_-=z80_loss)
#define Z80_PRAE_SEND(w) do{ if (!(w&0x0900)) audio_dirty=1; Z80_SYNC_IO; }while(0)
#define Z80_SEND z80_send
#define Z80_POST_SEND(w) (_t_-=z80_loss)
// fine timings
#define Z80_LOCAL
#define Z80_MREQ(t,w)
#define Z80_MREQ_1X(t,w)
#define Z80_MREQ_NEXT(t)
#define Z80_MREQ_1X_NEXT(t)
#define Z80_WAIT(t)
#define Z80_WAIT_IR1X(t)
#define Z80_DUMB_M1(w) ((void)0) // 4-T dumb PEEK
#define Z80_DUMB Z80_DUMB_M1 // 3-T dumb PEEK
#define Z80_NEXT_M1 PEEK // 4-T PC FETCH
#define Z80_NEXT PEEK // 3-T PC FETCH
#define Z80_PEEK PEEK // trappable single read
#define Z80_PEEK0 Z80_PEEK // untrappable single read, use with care
#define Z80_PEEK1WZ Z80_PEEK // 1st twin read from LD rr,($hhll)
#define Z80_PEEK2WZ Z80_PEEK // 2nd twin read
#define Z80_PEEK1SP Z80_PEEK // 1st twin read from POP rr
#define Z80_PEEK2SP Z80_PEEK // 2nd twin read
#define Z80_PEEK1EX Z80_PEEK // 1st twin read from EX rr,(SP)
#define Z80_PEEK2EX Z80_PEEK // 2nd twin read
#define Z80_PRAE_NEXTXY PEEK // special DD/FD PEEK (1/2)
#define Z80_POST_NEXTXY // special DD/FD PEEK (2/2)
#define Z80_POKE(w,b) do{ BYTE z80_aux=w>>14; if (mmu_bit[z80_aux]) Z80_SYNC_IO, z80_t=0, z80_trap(w,b); else mmu_ram[z80_aux][w]=(b); }while(0) // trappable single write
#define Z80_PEEKPOKE Z80_POKE // a POKE that follows a same-address PEEK, f.e. INC (HL)
#define Z80_POKE0(w,b) (POKE(w)=(b)) // untrappable single write, use with care
#define Z80_POKE1WZ Z80_POKE // 1st twin write from LD ($hhll),rr; see SPLIT.CPR
#define Z80_POKE2WZ Z80_POKE // 2nd twin write
#define Z80_POKE1SP Z80_POKE // 1st twin write from PUSH rr
#define Z80_POKE2SP Z80_POKE // 2nd twin write
#define Z80_POKE1EX Z80_PEEKPOKE // 1st twin write from EX rr,(SP)
#define Z80_POKE2EX Z80_POKE // 2nd twin write
// coarse timings
#define Z80_STRIDE(o) (z80_t+=z80_delays[o])
#define Z80_STRIDE_ZZ(o) (z80_t+=z80_delays[o]+z80_ack) // the slow/fast ACK quirk! cfr. "Z80 quirks"
#define Z80_STRIDE_IO(o) (z80_t=z80_delays[o]) // "z80_t=XXX" makes "z80_t=0" redundant in Z80_SYNC_IO
#define Z80_LIST_PEEKXY(o) z80_delays[o+0x600] // defined, this makes the Z80 DD/FD boolean list redundant
#define Z80_SLEEP(t) z80_t+=(t) // `t` must be nonzero!
#define Z80_HALT_STRIDE 1 // i.e. optimal HALT step, handled in a single go
// Z80 quirks
BYTE z80_xcf=0,z80_ack=0; // the XCF and ACK quirk flags
#define Z80_QUIRK(x) do{ if ((x)&1) z80_xcf=2; if ((x)&2) z80_ack=0; }while(0) // XCF (+1) and ACK (+2) quirks
#define Z80_QUIRK_M1 (z80_xcf>>=1,z80_ack=1) // refresh XCF and ACK quirks; the order is critical both here and above (!)
#define Z80_XCF_BUG (z80_xcf?0:z80_af.b.l) // replicate the XCF quirk -- seen on Spectrum, and on CPC too AFAIK
#define Z80_0XED71 (0) // whether OUT (C) sends 0 (NMOS) or 255 (CMOS)
#define Z80_0XED5X (1) // whether LD A,I/R checks the IRQ in-opcode

#define bios_magick() (debug_point[0XC154]=debug_point[0XC14D]=DEBUG_MAGICK)
void z80_magick(void) // virtual magick!
{
	//cprintf("MAGICK:%08X ",z80_pc.w);
	/**/ if (z80_pc.w==(type_id?0XC154:0XC14D))
	{
		if (mmu_rom[3]==&mem_rom[0X4000-0XC000]&&equalsmm(&mem_rom[z80_pc.w+0x4000-0xC000],0XEDB0)&&gate_ram<power_boost&&z80_hl.w+1==z80_de.w&&z80_bc.w>1) // power-up boost
			memset(&mem_ram[z80_de.w],0,z80_bc.w),z80_hl.w+=z80_bc.w-1,z80_de.w+=z80_bc.w-1,z80_bc.w=1; // nuke memory and fill registers
	}
}

#define DEBUG_HERE
#define DEBUG_INFOX 20 // panel width
BYTE debug_pook(int q,WORD w) { return q?PEEK(w):POKE(w); } // show either READING or WRITING space
void debug_info(int q)
{
	if (!(q&1)) // 1/2
	{
		strcpy (DEBUG_INFOZ(0),"GATE:");
		//sprintf(DEBUG_INFOZ(1),"%02X: ",gate_index);
		byte2hexa(DEBUG_INFOZ(1)+4,&gate_table[0],8);
		byte2hexa(DEBUG_INFOZ(2)+4,&gate_table[8],8);
		sprintf(DEBUG_INFOZ(3)+4,"%02X %1XBPP %02X:%02X:%02X",gate_table[16],(gate_status&1)?2:(gate_status&2)?1:4,gate_mcr,gate_ram,gate_rom);
		debug_hilight2(DEBUG_INFOZ(1+gate_index/8)+4+(gate_index&7)*2);
		sprintf(DEBUG_INFOZ(4),"CRTC:      (%03dx%03d)",hsync_limit/2,vsync_limit/2);
		//sprintf(DEBUG_INFOZ(5),"%02X: ",crtc_index);
		byte2hexa(DEBUG_INFOZ(5)+4,&crtc_table[0],8);
		byte2hexa(DEBUG_INFOZ(6)+4,&crtc_table[8],8);
		debug_hilight2(DEBUG_INFOZ(5+crtc_index/8)+4+(crtc_index&7)*2);
		sprintf(DEBUG_INFOZ(7)+4,"%02X:%02X:%02X:%02X %04X",crtc_count_r4,irq_timer,(BYTE)((video_pos_x-VIDEO_OFFSET_X)/16),crtc_count_r0,(WORD)(crtc_screen+crtc_raster));
		sprintf(DEBUG_INFOZ(8)+4,"%02X:%02X:%02X:%02X %04X",crtc_count_r9,crtc_count_r3y,crtc_count_r5,crtc_count_r3x,(WORD)((video_pos_y-VIDEO_OFFSET_Y)/2+3));
		if (crtc_status&CRTC_STATUS_R4_OK) debug_hilight2(DEBUG_INFOZ(7)+ 4);
		if (irq_delay>0) debug_hilight2(DEBUG_INFOZ(7)+ 7);
		if (crtc_status&CRTC_STATUS_R0_OK) debug_hilight2(DEBUG_INFOZ(7)+13);
		if (crtc_status&CRTC_STATUS_R9_OK) debug_hilight2(DEBUG_INFOZ(8)+ 4);
		if (crtc_status&CRTC_STATUS_VSYNC) debug_hilight2(DEBUG_INFOZ(8)+ 7);
		if (crtc_status&CRTC_STATUS_HSYNC) debug_hilight2(DEBUG_INFOZ(8)+13);
		strcpy (DEBUG_INFOZ(9),"PSG:");
		//sprintf(DEBUG_INFOZ(10),"%02X: ",psg_index);
		byte2hexa(DEBUG_INFOZ(10)+4,&psg_table[0],8);
		byte2hexa(DEBUG_INFOZ(11)+4,&psg_table[8],8);
		debug_hilight2(DEBUG_INFOZ(10+(psg_index&15)/8)+4+(psg_index&7)*2);
		sprintf(DEBUG_INFOZ(12)+4,"PIO: %02X:%02X:%02X:%02X",pio_port_a,pio_port_b,pio_port_c,pio_control);
		#ifdef Z80_DANDANATOR
		sprintf(DEBUG_INFOZ(13),"DANDANATOR%c",mem_dandanator?'*':'-');
		sprintf(DEBUG_INFOZ(14)+4,"%02X:%02X:%02X:%02X %c%c%c%c",dandanator_cfg[4],dandanator_cfg[5],dandanator_cfg[6],dandanator_cfg[7]
			,mmu_rom[0]>=mem_dandanator&&mmu_rom[0]<mem_dandanator+0X80000?'*':'-'
			,mmu_rom[1]>=mem_dandanator&&mmu_rom[1]<mem_dandanator+0X80000?'*':'-'
			,mmu_rom[2]>=mem_dandanator&&mmu_rom[2]<mem_dandanator+0X80000?'*':'-'
			,mmu_rom[3]>=mem_dandanator&&mmu_rom[3]<mem_dandanator+0X80000?'*':'-'
		);
		#endif
	}
	else // 2/2
	{
		sprintf(DEBUG_INFOZ(0),"PLUS: M:%02X D:%02X I:%02X",plus_gate_mcr,plus_dcsr,z80_irq);
		sprintf(DEBUG_INFOZ(1),"    SSSL:%02X SCAN:%03X",plus_sssl,crtc_line);
		sprintf(DEBUG_INFOZ(2),"    SSSS:%04X PRI:%02X",mgetmm(plus_ssss),plus_pri);
		sprintf(DEBUG_INFOZ(3),"    SSCR:%02X IVR:%02X %c",plus_sscr,plus_ivr,plus_gate_enabled?'*':plus_gate_counter?'@'+plus_gate_counter:'-');
		for (q=0;q<3;++q)
			sprintf(DEBUG_INFOZ(4+q),"DMA%c%c %04X:%03X.%02X/%02X",
					'0'+q,(plus_dcsr&(1<<q))?'*':'-',
					mgetii(&plus_dmas[q*4+0]), // DMA address
					plus_dma_regs[q][2], // pause
					plus_dma_regs[q][3], // multiplier
					plus_dmas[q*4+2]); // pause length
		for (q=0;q<8;++q)
		{
			int q0=plus_sprite_xyz[q*16+ 4]&15,q1=plus_sprite_xyz[q*16+12]&15;
			sprintf(DEBUG_INFOZ(7+q),"%03X,%03X:%c  %03X,%03X:%c"
				,(mgetii(&plus_sprite_xyz[q*16+ 0]))&0xFFF // posx
				,(mgetii(&plus_sprite_xyz[q*16+ 2]))&0xFFF // posy
				,hexa1[q0]+((q0&3)&&(q0&12)?128:0) // hilight if visible
				,(mgetii(&plus_sprite_xyz[q*16+ 8]))&0xFFF // posx
				,(mgetii(&plus_sprite_xyz[q*16+10]))&0xFFF // posy
				,hexa1[q1]+((q1&3)&&(q1&12)?128:0)); // ditto
		}
	}
}
int grafx_mask(void) { return 0XFFFF; }
BYTE grafx_peek(int w) { return debug_pook(debug_mode,w); }
void grafx_poke(int w,BYTE b) { debug_poke(w,b); }
int grafx_size(int i) { return i*4; }
int grafx_show(VIDEO_UNIT *t,int g,int n,int w,int o)
{
	BYTE z=-(o&1); g-=4; do
	{
		w&=0XFFFF; BYTE b=grafx_peek(w)^z; // readable/writeable (up to the user)
		if (gate_mcr&1) // MODE 1
		{
			*t++=video_clut[gate_mode1[0][b]];
			*t++=video_clut[gate_mode1[1][b]];
			*t++=video_clut[gate_mode1[2][b]];
			*t++=video_clut[gate_mode1[3][b]];
		}
		else if (gate_mcr&2) // MODE 2, dithered
		{
			VIDEO_UNIT p[4]={video_clut[0],VIDEO_FILTER_7525(video_clut[0],video_clut[1]),VIDEO_FILTER_7525(video_clut[1],video_clut[0]),video_clut[1]};
			*t++=p[(b>>6)&3];
			*t++=p[(b>>4)&3];
			*t++=p[(b>>2)&3];
			*t++=p[ b    &3];
		}
		else // MODE 0
		{
			VIDEO_UNIT p;
			*t++=p=video_clut[gate_mode0[0][b]]; *t++=p;
			*t++=p=video_clut[gate_mode0[1][b]]; *t++=p;
		}
	}
	while (++w,t+=g,--n); return w&grafx_mask();
}
void grafx_info(VIDEO_UNIT *t,int g,int o) // draw the palette and the PLUS sprites
{
	t-=16*8; if (plus_enabled)
	{
		for (int y=0;y<2*12;++y)
			for (int x=0;x<16*8;++x)
				t[y*g+x]=video_clut[(x/8)+(y/12)*16];
		for (int y=0;y<2*16;++y)
			for (int x=0;x<8*16;++x)
				t[(y+2*12)*g+x]=video_clut[16+plus_sprite_bmp[(x%16)+((y&16)/2+x/16)*256+(y%16)*16]];
	}
	else
		for (int y=0;y<1*12;++y)
			for (int x=0;x<16*8;++x)
				t[y*g+x]=video_clut[(x/8)+(y/12)*16];
}
#include "cpcec-z8.h"
#undef DEBUG_HERE

// EMULATION CONTROL ================================================ //

char txt_error_any_load[]="Cannot open file!";
char txt_error_bios[]="Cannot load firmware!";

// emulation setup and reset operations ----------------------------- //

void all_setup(void) // setup everything!
{
	//video_table_reset(); // const palette
	bios_magick(); // MAGICK follows DEBUG!
	crtc_setup();
	plus_setup();
	gate_setup();
	tape_setup();
	disc_setup();
	pio_setup();
	psg_setup();
	z80_setup();
}
void all_reset(void) // reset everything!
{
	const int k=16; // the extended CPC RAM reveals a pattern that isn't erased by the firmware
	for (int i=0;i<sizeof(mem_ram);i+=k) memset(&mem_ram[i],0,k),i+=k,memset(&mem_ram[i],-1,k);
	//MEMZERO(mem_ram);
	MEMZERO(autorun_kbd);
	crtc_reset();
	plus_reset();
	gate_reset();
	tape_reset();
	disc_reset();
	pio_reset();
	psg_reset();
	#ifdef PSG_PLAYCITY
	MEMZERO(playcity_ctc_count); session_dirty|=playcity_dirty,playcity_dirty=dac_voice=0;
	#endif
	z80_reset();
	z80_sp.w=0xC000; // implicit in "No Exit" PLUS!
	z80_imd=1; // implicit in "Pro Tennis Tour" PLUS!
	debug_reset();
	disc_disabled&=1,z80_irq=snap_done=autorun_mode=autorun_t=0; // avoid accidents!
	MEMBYTE(z80_tape_index,-1); // TAPE_FASTLOAD, avoid false positives!
}

// firmware/cartridge ROM file handling operations ------------------ //

BYTE old_type_id=99; // the last loaded BIOS type
char bios_system[][13]={"cpc464.rom","cpc664.rom","cpc6128.rom","cpcplus.rom"};
char bios_path[STRMAX]="";

int bios_wrong_dword(DWORD t) // catch fingerprints that belong to other file types; DWORD is MMMM-styled
{
	#if 0 // overkill
	return t==0x4D56202D|| // "MV - CPC" (floppy disc image) and "MV - SNA" (CPC Snapshot)
		t==0x45585445|| // "EXTENDED" (advanced floppy disc image)
		t==0x5A585461|| // "ZXTape!" (advanced tape image)
		t==0x436F6D70|| // "Compressed Wave File" (advanced audio file)
		(t>=0X41420000&&t<=0X4142FFFF)|| // MSX cartridge "ABxx"
	#else // simpler!
	return (t>=0X41000000&&t<=0X5AFFFFFF)|| // reject all ASCII upper case letters!
	#endif
		(t>=0x13000000&&t<=0x13000003); // Spectrum BASIC-BINARY header (simple tape image)
}

void bios_ignore_amsdos(FILE *f) // just in case some ROM files still include AMSDOS headers (i.e. the first 128 bytes)
{
	BYTE bios_amsdos[128]; fread1(bios_amsdos,128,f);
	WORD checksum=0;
	for (int i=0;i<67;++i) checksum+=bios_amsdos[i];
	if ((BYTE)checksum==bios_amsdos[67]&&(checksum>>8)==bios_amsdos[68]&&bios_amsdos[18]==2)
		cprintf("ROM with AMSDOS header, checksum %04X.\n",checksum); // it's AMSDOS, ignore
	else
		fseek(f,0,SEEK_SET); // not AMSDOS, restore
}
int bios_load(char *s) // load a cartridge file or a firmware ROM file. 0 OK, !0 ERROR
{
	//if (globbing("*.des",s,1)) return 1; // Dandanator [sub]cartridge!
	if (globbing("*.ini",s,1)&&(ext_rom||(ext_rom=malloc(257<<14)))) // profile?
	{
		unsigned char *ss,t[STRMAX],*tt,u[STRMAX],*uu;
		strcpy(u,s); if (uu=strrchr(u,PATHCHAR)) ++uu; else uu=u; // make ROM files relative to INI path
		MEMZERO(mmu_xtr);
		FILE *f,*ff;
		if (f=puff_fopen(s,"r"))
		{
			while (fgets(t,STRMAX,f))
			{
				ss=tt=UTF8_BOM(t);
				while (*ss&&*ss<=' ') ++ss; // trim left
				while ((*tt=*ss)>=' ') ++tt,++ss; // trim right
				*tt=0; if (*t>' '&&(tt=strchr(t,'='))&&tt[1]) // "name[blank]=[blank]value"?
				{
					BYTE *rr=tt; while (*++rr==' ') {} strcpy(uu,rr); // trim right, copy
					while (*--tt==' ') {} tt[1]=0; // trim left, keep the name
					if (!strcasecmp("lowest",t))
					{
						if (ff=puff_fopen(u,"rb"))
							bios_ignore_amsdos(ff),fread1(&ext_rom[0],0x4000,ff),puff_fclose(ff),mmu_xtr[0]=1;
					}
					else if (globbing("high*",t,1))
					{
						int i=strtol(&t[4],NULL,16);
						if (i>=0&&i<length(mmu_xtr)-1)
							if (ff=puff_fopen(u,"rb"))
								bios_ignore_amsdos(ff),fread1(&ext_rom[0x4000+(i<<14)],0x4000,ff),puff_fclose(ff),mmu_xtr[i+1]=1;
					}
					else if (!strcasecmp("type",t))
					{
						type_id=(*uu)&3;
						bios_load(strcat(strcpy(session_substr,session_path),bios_system[type_id])); // not a big fan of recursive calls...
					}
					else if (!strcasecmp("crtc",t))
					{
						if ((crtc_type=(*uu)&7)>4)
							crtc_type=4;
					}
					else if (!strcasecmp("bank",t))
					{
						if ((ram_depth=(*uu-'0')&15)>ram_maxcfg)
							ram_depth=ram_maxcfg;
						ram_setcfg(ram_depth);
					}
					else if (!strcasecmp("fddc",t))
						disc_disabled=(disc_disabled&2)+(1&~*uu);
				}
			}
			puff_fclose(f);
		}
	}
	else // binary file!
	{
		FILE *f=puff_fopen(s,"rb");
		if (!f) return 1;
		int i=0,j=0x63623030,k,l; // first cartridge block ID "cb00"
		k=fgetmmmm(f); l=fgetiiii(f);
		if (k==0x52494646&&l>0&&fgetmmmm(f)==0x414D5321) // cartridge file? ("RIFF" + "AMS!")
			while ((k=fgetmmmm(f))&&(l=fgetiiii(f))>=0) // l<0 = feof
			{
				//cprintf(" [%08X:%08X]",k,l);
				if (k==j&&l==0X4000&&i+l<=(1<<19)) // sanity check: ROM ID, length, offset
				{
					i+=fread1(&mem_rom[i],l,f);
					if (((++j)&15)==10)
						j+=256-10; // "cb09" is followed by "cb10" (decimal format)
				}
				else if (l)
					fseek(f,l+(l&1),SEEK_CUR); // RIFF even-padding
			}
		else if (!bios_wrong_dword(k)) // reject different formats
		{
			fseek(f,0,SEEK_END); i=ftell(f); // filesize can tell apart improperly sized files
			if (i<0x4000||i>1<<19||i&0x3F7F||fseek(f,i-0x4000+0x0601,SEEK_SET)||fgetmmmm(f)==0xD3FE37C9) // Spectrum TAPE LOAD fingerprint
				return puff_fclose(f),1; // reject!
			fseek(f,0,SEEK_SET);
			bios_ignore_amsdos(f); // hence the 0x3F7F rather than 0x3FFF -- we need to consider the 128-byte header
			i=fread1(mem_rom,1<<19,f);
		}
		else
			return puff_fclose(f),1;
		puff_fclose(f);
		MEMZERO(mmu_xtr);
		if (i==(1<<15)&&equalsmmmm(&mem_rom[0],0X01897FED)&&(type_id=mem_rom[0x4002])<3) // fingerprints + revision
		{
			if (crtc_type==3) crtc_type=0; // adjust hardware
			plus_reset(); // PLUS ASIC can be safely reset
			cprintf("ROM firmware v%d.\n",1+type_id);
		}
		else // Amstrad PLUS cartridge
		{
			type_id=crtc_type=3; // enforce hardware
			j=1<<14; // smallest size is 16K
			while (j<i) j<<=1;
			if (j>i) memset(&mem_rom[i],-1,j-i); // pad memory
			while (j<(1<<19)) // mirror low ROM blocks upwards
				memcpy(&mem_rom[j],mem_rom,j),j<<=1;
			cprintf("PLUS cartridge %dK.\n",i>>10);
		}
	}
	if (!memcmp(&mem_rom[0xCE07],"\x1B\xBB\x30\xF0\xFE\x81\x28\x0C\xEE\x82",10))
		mem_rom[0xCE07]=0x09,mem_rom[0xCE0C]=0x31,mem_rom[0xCE10]=0x32; // PLUS HACK: boot menu accepts '1' and '2'!
	if (session_substr!=s) STRCOPY(bios_path,s);
	return old_type_id=type_id,mmu_update(),0;
}
int bios_reload(void) // ditto, but from the current type_id
	{ return old_type_id==type_id?0:type_id>=length(bios_system)?1:bios_load(strcat(strcpy(session_substr,session_path),bios_system[type_id])); }

int amsdos_load(char *s) // load AMSDOS ROM. `s` path; 0 OK, !0 ERROR
{
	*bdos_rom=0; // tag ROM as empty
	FILE *f=fopen(strcat(strcpy(session_substr,session_path),s),"rb"); if (!f) return 1; // fail!
	int i=fread1(bdos_rom,1<<14,f); i+=fread1(bdos_rom,1<<14,f);
	return fclose(f),i!=(1<<14); //||!equalsmmmm(&bdos_rom[0x3C],0XC366C6C3); // AMSDOS fingerprint
}

#ifdef Z80_DANDANATOR
int dandanator_load(char *s) // inserts Dandanator cartridge, performs several tests, removes cartridge if they fail
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1; // fail!
	fread1(session_scratch,0X4000,f); fseek(f,0,SEEK_END);
	int i=ftell(f); puff_fclose(f);
	if (i<0X4000||i>(8<<16)||(i&0x3FFF)||bios_wrong_dword(mgetmmmm(session_scratch))
		/*||equalsmmmm(session_scratch,0X52494646)*/||(equalsmmmm(&session_scratch[0X2C],0XED49D9FB))) // + "RIFF" + CPC Firmware
		return 1; // ensure that the filesize is right and that the header doesn't belong to other filetypes!
	for (i=0X4000-4;--i>=0;) if (equalsmmmm(&session_scratch[i],0xFDFDFD77)) break; // ..77 or 71?
	if (i<0) return 1; // the CPC Dandanator is strong: cartridges must include at least one FDFDFD77 string at the beginning! :-)
	dandanator_remove(); if (dandanator_insert(s)) return 1;
	STRCOPY(dandanator_path,s); return 0;
}
#else
#define dandanator_remove() (0)
#endif

// snapshot file handling operations -------------------------------- //

char snap_pattern[]="*.sna",snap_magic8[]="MV - SNA";
char snap_path[STRMAX]="",snap_extended=1; // compress memory dumps and save extra blocks

void snap_save_rle5h(int k,int l,BYTE **t) // flush a compression code; `l` can be zero!
{
	if (l>(k==0xE5?1:2)) *(*t)++=0xE5,*(*t)++=l,*(*t)++=k;
	else while (--l>=0) if ((*(*t)++=k)==0xE5) *(*t)++=0;
}
int snap_save_rle5(BYTE *t,int o,BYTE *s,int i) // compress a block (see below)
{
	BYTE *u=t,*v=t+o-3; int k=o=0; // `k` is irrelevant as long as `o` is zero
	for (;i>0/*&&t<v*/;++s,--i) // `t` will always be bigger than `i`*3/2
		if (k==*s&&o<255) ++o; else snap_save_rle5h(k,o,&t),k=*s,o=1;
	if (t<v) { snap_save_rle5h(k,o,&t); return t-u; } // flush any remainders
	return -1; // the compressed output is too long, reject it!
}
#define SNAP_SAVE_Z80W(x,r) (header[x]=r.b.l,header[x+1]=r.b.h)
int snap_save(char *s) // save a snapshot. `s` path, NULL to resave; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"wb");
	if (!f) return 1;
	BYTE header[256];
	MEMZERO(header);
	strcpy(header,snap_magic8);
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
	header[0x41]=gate_ram; // the top two bits are zero unless RAM is extended beyond 576k
	header[0x42]=crtc_index;
	MEMSAVE(&header[0x43],crtc_table);
	header[0x55]=gate_rom;
	header[0x56]=pio_port_a;
	header[0x57]=pio_port_b;
	header[0x58]=pio_port_c;
	header[0x59]=pio_control|128;
	header[0x5A]=psg_index; // mistake in http://cpctech.cpc-live.com/docs/snapshot.html : INDEX can be invalid, it's the emulator's duty to behave accordingly
	MEMSAVE(&header[0x5B],psg_table);
	#ifdef PSG_PLAYCITY
	header[0x5B+14]=playcity_disabled?0:(240+playcity_getcfg()); // Playcity kludge: register 14 isn't readable on CPC, keyboard overrides it!
	#endif
	header[0x6B]=ram_dirty; header[0x6C]=ram_dirty>>8; // in V3, this field is zero if MEM0..MEM8 chunks are used. Avoid them to stay compatible.
	// V2 data
	header[0x6D]=type_id<3?type_id:ram_depth?4:5; // ACE uses different values for PLUS machines and complains about mismatching firmware IDs :-/
	strcpy(&header[0xE0],session_caption);
	//if (snap_extended) // V3 data
	{
		header[0x9C]=!!disc_motor;
		MEMSAVE(&header[0x9D],disc_track);
		//header[0xA1]=printer_data;
		/*header[0xA2]=video_pos_y>>1; header[0xA3]=video_pos_y>>9;*/ // quite unreliable, emulators often ignore it!
		header[0xA4]=crtc_type;
		header[0xA9]=crtc_count_r0;
		header[0xAB]=crtc_count_r4;
		header[0xAC]=crtc_count_r9;
		header[0xAD]=crtc_count_r5;
		header[0xAE]=crtc_count_r3x;
		header[0xAF]=crtc_count_r3y;

		/* straight from Richard Wilson's http://www.cpcwiki.eu/forum/emulators/javacpc-desktop-available-as-beta!/msg9427/#msg9427
		CRTC_FLAG_VSYNC_ACTIVE    = $01;
		CRTC_FLAG_HSYNC_ACTIVE    = $02;
		CRTC_FLAG_HDISP_INACTIVE  = $04;
		CRTC_FLAG_VDISP_INACTIVE  = $08;
		CRTC_FLAG_HTOT_REACHED    = $10;
		CRTC_FLAG_VTOT_REACHED    = $20;
		CRTC_FLAG_MAXIMUM_RASTER_COUNT_REACHED = $40;
		CRTC_FLAG_REG8_BLANKING   = $80;
		CRTC_FLAG_VTA_ACTIVE     = $100;
		CRTC_FLAG_WINAPE_ICSR   = $8000; */
		header[0xB0]=(crtc_status&CRTC_STATUS_VSYNC?1:0)+(crtc_status&CRTC_STATUS_HSYNC?2:0)
			+(crtc_status&CRTC_STATUS_H_OFF?4:0)+(crtc_status&CRTC_STATUS_V_OFF?8:0)
			+(crtc_status&CRTC_STATUS_R0_OK?16:0)+(crtc_status&CRTC_STATUS_R4_OK?32:0)
			+(crtc_status&CRTC_STATUS_R9_OK?64:0)+(crtc_status&CRTC_STATUS_REG_8?128:0);
		header[0xB1]=(crtc_status&CRTC_STATUS_V_T_A?1:0); // =$100>>8
		// other emulators seem not to read these bits at all despite the format details :-(

		header[0xB2]=irq_delay;
		header[0xB3]=irq_timer;
		header[0xB4]=!!z80_irq; // reduce all IRQ combinations to TRUE or FALSE
		header[0xB5]=z80_irq; // compare plus_dcsr with ICSR
		header[0xB6]=!(header[0xB7]=plus_enabled);
		header[0x10]=3;
	}
	//else header[0x10]=2;
	if (snap_extended) // V3 compression
	{
		header[0x6B]=header[0x6C]=0; fwrite1(header,256,f);
		for (i=0;i<ram_dirty;i+=64)
		{
			int j=snap_save_rle5(session_scratch,1<<16,&mem_ram[i<<10],1<<16),k=i>>6;
			if (k>9) k+=7;
			fputmmmm(k+0x4D454D30,f); // "MEM0", "MEM1"... "MEM9", "MEMA"... "MEMW" (2112K)
			if (j>=0)
				fputiiii(j,f),fwrite1(session_scratch,j,f);
			else // failed to compress, store!
				kputiiii(1<<16,f),fwrite1(&mem_ram[i<<16],1<<16,f);
		}
	}
	else // no compression
		{ fwrite1(header,256,f); fwrite1(mem_ram,ram_dirty<<10,f); }
	if (plus_enabled) // PLUS ASIC?
	{
		kputmmmm(0x4350432B,f); // "CPC+"
		kputiiii(0x08F8,f);
		nibble2byte(&session_scratch[0X000],plus_sprite_bmp,16*16*8); // 0x4000-0x4FFF nibbles => 0x000-0x7FF byte
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

	#ifdef Z80_DANDANATOR
	if (mem_dandanator)
	{
		kputmmmm(0x444E5452,f); // DANDANATOR flags "DNTR"
		kputiiii(sizeof(dandanator_cfg),f);
		fwrite1(dandanator_cfg,sizeof(dandanator_cfg),f);
	}
	#endif
	#if 0 // 1 // experimental
	{
		int j=0; for (i=0;i<length(debug_point);++i)
			if (debug_point[i]&16)
				++j;
		if (j) // save the currently defined breakpoints, if any
		{
			kputmmmm(0x42524B53,f); // breakpoint table "BRKS"
			fputiiii(j*5,f);
			for (i=0;j&&i<length(debug_point);++i)
				if (debug_point[i]&16) // stick to breakpoints without flags
					fputiiii(i,f),fputc(0,f),--j;
		}
	}
	#endif
	STRCOPY(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

void snap_load_plus(FILE *f,int i)
{
	BYTE *s=session_scratch,*t=plus_sprite_bmp;
	fread1(s,i,f),memset(&session_scratch[i],0,0X0900-i);
	byte2nibble(t,s,16*16*8); // 0x000-0x7FF bytes => 0x4000-0x4FFF nibbles
	//MEMNCPY(plus_sprite_xyz,&session_scratch[0x800],128); // 0x800-0x87F => 0x6000-0x607F
	for (i=0;i<16*8;i+=8) // sanitize sprite values
	{
		plus_sprite_xyz[i+0]=session_scratch[0x800+i+0];
		char b=session_scratch[0x800+i+1]&3; plus_sprite_xyz[i+1]=b==3?-1:b; // POSX ranges from -256 to 767
		plus_sprite_xyz[i+2]=session_scratch[0x800+i+2];
		plus_sprite_xyz[i+3]=session_scratch[0x800+i+3]&1?-1:0; // POSY ranges from -256 to 256
		plus_sprite_xyz[i+4]=session_scratch[0x800+i+4]&15;
		plus_sprite_xyz[i+5]=plus_sprite_xyz[i+6]=plus_sprite_xyz[i+7]=0; // can these be anything but zero?
	}
	//MEMNCPY(plus_palette,&session_scratch[0x880],64); // 0x880-0x8BF => 0x6400-0x643F
	for (i=0;i<32*2;i+=2) // sanitize colour values
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
	if ((plus_gate_mcr=session_scratch[0x8C6])&&!(plus_gate_mcr>=0xA0&&plus_gate_mcr<0xC0)) // Winape uses 0x8C6!?
		plus_gate_mcr=session_scratch[0x8F5],cprintf("ACE:%02X:%02X!? ",session_scratch[0x8C6],plus_gate_mcr); // ACE uses 0x8F5!?
	plus_gate_mcr&=31;
	plus_gate_enabled=!!session_scratch[0x8F6];
	plus_gate_counter=session_scratch[0x8F7];
}
#define SNAP_LOAD_Z80W(x,r) (r.b.l=header[x],r.b.h=header[x+1])
int snap_load(char *s) // load a snapshot. `s` path, NULL to reload; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb");
	if (!f) return 1;
	BYTE header[256];
	if ((fread1(header,256,f)!=256)||(memcmp(snap_magic8,header,8)))
		return puff_fclose(f),1;
	int dumpsize=mgetii(&header[0x6B]);
	if (dumpsize>(sizeof(mem_ram)>>10)||dumpsize&15)
		return puff_fclose(f),1; // improper memory length!
	if (dumpsize&&dumpsize<64)
		dumpsize=64; // avoid bugs with very old snapshots!
	// V1 data
	int i;
	SNAP_LOAD_Z80W(0x11,z80_af);
	SNAP_LOAD_Z80W(0x13,z80_bc);
	SNAP_LOAD_Z80W(0x15,z80_de);
	SNAP_LOAD_Z80W(0x17,z80_hl);
	SNAP_LOAD_Z80W(0x19,z80_ir);
	SNAP_LOAD_Z80W(0x1B,z80_iff);
	z80_iff.w&=257; // sanity!
	SNAP_LOAD_Z80W(0x1D,z80_ix);
	SNAP_LOAD_Z80W(0x1F,z80_iy);
	SNAP_LOAD_Z80W(0x21,z80_sp);
	SNAP_LOAD_Z80W(0x23,z80_pc);
	z80_imd=header[0x25]&3;
	SNAP_LOAD_Z80W(0x26,z80_af2);
	SNAP_LOAD_Z80W(0x28,z80_bc2);
	SNAP_LOAD_Z80W(0x2A,z80_de2);
	SNAP_LOAD_Z80W(0x2C,z80_hl2);
	z80_int=z80_irq=0; // avoid nasty surprises!
	for (i=0;i<17;++i)
		gate_table[i]=header[0x2F+i]&31;
	gate_table_select(header[0x2E]);
	gate_mcr=header[0x40]&15;
	gate_ram=header[0x41]; // some emulators store trash in the top two bits; this means trouble beyond 576k
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
	#ifdef PSG_PLAYCITY
	if (psg_table[14]>=240)
		playcity_dirty=playcity_disabled=0,playcity_setcfg(psg_table[14]-240); // Playcity kludge, see snap_save()
	else
		playcity_reset();
	#endif
	if (header[0x10]>1) // V2?
	{
		if ((type_id=header[0x6D])>3) type_id=3; // merge all PLUS types together
		cprintf("SNAv2 ROM type v%d.\n",1+type_id);
	}
	if (header[0x10]>2) // V3?
	{
		disc_motor=header[0x9C];
		MEMLOAD(disc_track,&header[0x9D]);
		//printer_data=header[0xA1];
		/*if ((i=(mgetii(&header[0xA2])*2))>=0&&i<VIDEO_VSYNC_HI) // quite unreliable, ignore it if invalid!
		{
			video_target+=(i-video_pos_y)*VIDEO_LENGTH_X;
			video_pos_y=i;
		}*/
		crtc_type=header[0xA4]&7; // cprintf("SNAv3 CRTC type %d.\n",header[0xA4]); // unreliable!?
		crtc_count_r0=header[0xA9];
		crtc_count_r4=header[0xAB];
		crtc_count_r9=header[0xAC];
		crtc_count_r5=header[0xAD];
		crtc_count_r3x=header[0xAE];
		crtc_count_r3y=header[0xAF];
		crtc_status=(header[0xB0]&1?CRTC_STATUS_VSYNC:0)+(header[0xB0]&2?CRTC_STATUS_HSYNC:0)
			+(crtc_count_r0>=crtc_table[0]?CRTC_STATUS_R0_OK:0) // ">=" or "=="?
			+(crtc_count_r4>=crtc_table[4]?CRTC_STATUS_R4_OK:0) // the former is more compatible
			+(crtc_count_r9>=crtc_table[9]?CRTC_STATUS_R9_OK:0) // without a trustworthy CRTC_STATUS
			+(crtc_count_r0>=crtc_table[1]?CRTC_STATUS_H_OFF:0)
			+(crtc_count_r4>=crtc_table[6]?CRTC_STATUS_V_OFF:0)
			+(header[0xB1]&1?CRTC_STATUS_V_T_A:0); // other emulators don't write all the bits :-(
		if (crtc_status&CRTC_STATUS_VSYNC)
			gate_count_r3y=26-crtc_count_r3y,irq_delay=header[0xB2]; // WINAPE sometimes stored unwanted values here
		else
			gate_count_r3y=irq_delay=0;
		//gate_status=(crtc_status&~3)+(gate_mcr&3); // imprecise, but it will do
		irq_timer=header[0xB3];
		z80_irq=header[0xB5]&0xF0; // keep just the top 4 bits
	}
	// handle memory dump (compressed or not) and PLUS ASIC from V3+, if required; careful, [6C][6B] isn't reliable!
	int j=dumpsize?fread1(mem_ram,dumpsize<<10,f):0,q=0;
	i=0x4D454D30+(j>>16); if (i>0x4D454D39) i+=7; // "MEM0", "MEM1"... // "MEM9", "MEMA"...
	for (int k,l;;)
	{
		k=fgetmmmm(f),l=fgetiiii(f); // chunk + bytes
		if (l<=0) // feof or corrupted?
			break;
		if (k==i)
		{
			++i; if (i==0x4D454D3A) i=0x4D454D41; // "MEM9", "MEMA"...
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
						for (++k,m=fgetc(f);n--;)
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
		else if (k==0x4350432B&&l<0X0900) // PLUS ASIC "CPC+"
			q|=1,type_id=3,snap_load_plus(f,l);
		#ifdef Z80_DANDANATOR
		else if (k==0x444E5452&&l>=sizeof(dandanator_cfg)) // DANDANATOR flags "DNTR"
		{
			q|=2; fread1(dandanator_cfg,sizeof(dandanator_cfg),f);
			if (!mem_dandanator&&*dandanator_path)
				if (dandanator_load(dandanator_path)) // insert last known Dandanator file
					dandanator_reset(); // nuke these variables on failure :-(
			fseek(f,l-sizeof(dandanator_cfg),SEEK_CUR);
		}
		#endif
		#if 0 // 1 // experimental
		else if (k==0x42524B53&&!(l%5)) // breakpoint table "BRKS"
		{
			for (i=0;i<length(debug_point);++i) debug_point[i]&=64; // respect virtual magick!
			for (;l>0;l-=5)
			{
				k=fgetiiii(f); if (!fgetc(f)&&k>=0&&k<length(debug_point))
					debug_point[k]|=16; // stick to breakpoints without flags
			}
		}
		#endif
		else
			fseek(f,l,SEEK_CUR); // skip unknown block
	}
	if (!(q&1)) // missing PLUS ASIC block?
	{
		plus_reset(); // reset PLUS hardware if the snapshot is OLD style!
		for (i=0;i<17;++i) // avoid accidents in old snapshots recorded as PLUS but otherwise sticking to old hardware
			mputii(&plus_palette[i*2],video_asic_table[gate_table[i]]);
	}
	#ifdef Z80_DANDANATOR
	if (!(q&2)) // missing DANDANATOR block?
		dandanator_remove();
	#endif
	// we adjust the REAL RAM SIZE if the snapshot is bigger than the current setup!
	dumpsize=j>>10;
	cprintf("SNAv1 RAM size %dK.\n",dumpsize);
	if (dumpsize>(ram_dirty=64))
	{
		q=0; // look for lowest RAM limit that fits
		while ((ram_dirty=ram_kbytes(++q))<dumpsize&&q<ram_maxcfg) {}
		if (ram_depth<q) ram_setcfg(q); // expand RAM limit if required
	}
	bios_reload();
	video_xlat_clut(); // sync both Old and Plus palettes
	crtc_syncs_update(),crtc_invis_update();
	crtc_line_set();
	disc_track_update();
	psg_all_update();
	mmu_update();
	if (!plus_enabled) z80_irq&=128; // reset unused bits recorded by some emus

	debug_reset();
	MEMBYTE(z80_tape_index,-1); // TAPE_FASTLOAD, avoid false positives!
	STRCOPY(snap_path,s);
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
	autorun_mode=0; if (!s||!*s) return -1; // cancel any autoloading yet; reject invalid paths
	if (snap_load(s))
	{
		#ifdef Z80_DANDANATOR
		if (dandanator_load(s))
		#endif
			if (bios_load(s))
			{
				if (tape_open(s))
				{
					if (disc_open(s,0,!(disc_filemode&1))) // use same setting as DEFAULT READ-ONLY
						return 1; // everything failed!
					if (q)
					{
						// disc autorun is more complex than tape because we must find out the right file
						int i,j,k;
						if ((j=any_load_catalog(0,0xC1))!=0x800) // VENDOR format instead of DATA?
							if ((j=any_load_catalog(2,0x41))!=0x800) // VENDOR format instead of DATA?
								j=any_load_catalog(1,0x01); // IBM format instead of VENDOR or DATA?
						BYTE bestfile[STRMAX]; bestfile[8]='.'; // filenames follow the "8.3" style
						int bestscore=bestfile[0]=0;
						cprintf("AUTORUN: ");
						if (j==0x800) // we got a seemingly valid catalogue
							for (i=0;i<0x800;i+=32) // scan the catalogue
								// check whether the entry is valid at all: it belongs to USER 0, it's got a valid entry length, WORD is ZERO and SIZE is NONZERO and VALID
								if (!disc_buffer[i]&&!disc_buffer[i+12]&&!disc_buffer[i+14]&&disc_buffer[i+15]>0&&disc_buffer[i+15]<=128&&disc_buffer[i+16])
								{
									int h=disc_buffer[i+10]&128,c; // HIDDEN flag!
									for (k=1,j=1;j<12;++j)
										k&=(c=(disc_buffer[i+j]&=127))>=32&&c!=34; // remove bit 7 (used to tag the file as READ ONLY, HIDDEN, etc) and accept all printable characters but quotes
									if (k&&disc_buffer[i+1]>32) // all chars are valid; measure how good it is as a candidate
									{
										k=33-disc_buffer[i+15]/4; // in most cases, loaders are the shortest files on the disc
										if (!h)
											k+=48; // visible files are better candidates
										if (!memcmp(&disc_buffer[i+1],"DISC    ",8)||!memcmp(&disc_buffer[i+1],"DISK    ",8))
											k+=128; // RUN"DISC and RUN"DISK are standard!!
										else if (disc_buffer[i+1]=='-')
											k+=40; // very popular shortcut in French prods!
										if (disc_buffer[i+9]==' ')
											k+=32; // together with BAS, a typical launch file
										else if (equalsmmmm(&disc_buffer[i+9],0X42415300)) // "BAS\000"
											k+=30; // ditto
										else if (equalsmmmm(&disc_buffer[i+9],0X42494E00)) // "BIN\000"
											k+=28; // still standard, but the worst case
										cprintf("`%s`:%d ",&disc_buffer[i+1],k);
										if (bestscore<k)
										{
											bestscore=k;
											MEMNCPY(bestfile,&disc_buffer[i+1],8); // name
											strcpy(&bestfile[9],&disc_buffer[i+9]); // extension
										}
									}
								}
						if (bestscore) // load and run a file
							sprintf(autorun_line,"RUN\"%s",bestfile);
						else // no known files, run boot sector
							strcpy(autorun_line,"|CPM");
						cprintf("%s\n",autorun_line);
						disc_disabled=0,tape_close(); // open disc? enable disc, close tapes!
					}
				}
				else if (q)
					disc_close(0),disc_close(1); // open tape? close discs!
				if (q) // autorun for tape and disc
				{
					dandanator_remove(),old_type_id=old_type_id>2?-1:old_type_id,all_reset(),bios_reload(); // force firmware reload if required
					if (tape) disc_disabled|=2; // disable disc to avoid accidents
					autorun_mode=type_id<3?disc_disabled?1:3:4; autorun_t=55; // the PLUS menu must be handled separately
				}
			}
			else dandanator_remove(),all_reset(); // load bios? reset!
		#ifdef Z80_DANDANATOR
		else all_reset(); // dandanator? reset!
		#endif
	}
	if (q) STRCOPY(autorun_path,s);
	return 0;
}

// auxiliary user interface operations ------------------------------ //

BYTE key2joy_flag=0; // alternate joystick buttons
char txt_error_snap_save[]="Cannot save snapshot!";
char file_pattern[]="*.cdp;*.cdt;*.cpr;*.crt;*.csw;*.dsk;*.ini;*.des;*.rom;*.sna;*.wav"; // from A to Z

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
	"0x0701 Flip disc sides in A:\n"
	"0x0700 Remove disc from A:\tCtrl-F7\n"
	"0xC700 Insert disc into B:..\tShift-F7\n"
	"0xC701 Create disc in B:..\n"
	"0x4701 Flip disc sides in B:\n"
	"0x4700 Remove disc from B:\tCtrl-Shift-F7\n"
	"=\n"
	"0x8800 Insert tape..\tF8\n"
	"0xC800 Record tape..\tShift-F8\n"
	"0x8801 Browse tape..\n"
	"0x0800 Remove tape\tCtrl-F8\n"
	//"0x4800 Play tape\tCtrl-Shift-F8\n"
	"0x4801 Flip tape polarity\n"
	#ifdef Z80_DANDANATOR
	"=\n"
	"0xC500 Insert Dandanator..\tShift-F5\n"
	"0x4500 Remove Dandanator\tCtrl-Shift-F5\n"
	"0x0510 Writeable Dandanator\n"
	#endif
	"=\n"
	"0x0080 E_xit\n"
	"Edit\n"
	"0x8500 Select firmware..\tF5\n"
	"0x0500 Reset emulation\tCtrl-F5\n"
	"0x8F00 Pause\tPause\n"
	"0x8900 Debug\tF9\n"
	//"0x9000 NMI\n"
	"=\n"
	"0x8511 64K RAM\n"
	"0x8512 128K RAM\n"
	"0x8513 192K RAM\n"
	"0x8514 320K RAM\n"
	"0x8515 576K RAM\n"
	"0x8516 1088K RAM\n"
	"0x8517 2112K RAM\n"
	//"=\n"
	"0x8501 CRTC type 0\n"
	"0x8502 CRTC type 1\n"
	"0x8503 CRTC type 2\n"
	"0x8504 CRTC type 3\n"
	"0x8505 CRTC type 4\n"
	"0x8508 Standard V-hold\n"
	"=\n"
	"0x8521 Regular joystick\n" // = No lightgun
	"0x8522 Trojan Light Phaser\n"
	"0x8523 Gunstick (MHT)\n"
	"0x8524 Westphaser (Loriciel)\n"
	#ifdef PSG_PLAYCITY
	"0x8518 PlayCity audio\n"
	"0x8519 Digiblaster audio\n"
	#endif
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
	"0x4400 Flip joystick buttons\tCtrl-Shift-F4\n"
	"=\n"
	"0x851F Strict snapshots\n"
	"0x852F Printer output..\n"
	"0x8510 Enable disc drives\n"
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
	session_menucheck(0x0701,disc_flip[0]);
	session_menucheck(0x4701,disc_flip[1]);
	session_menucheck(0x8800,tape_type>=0&&tape);
	session_menucheck(0xC800,tape_type<0&&tape);
	session_menucheck(0x4801,tape_polarity);
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	session_menucheck(0x0400,session_key2joy);
	session_menucheck(0x4400,key2joy_flag);
	session_menuradio(0x0601+multi_t,0x0601,0x0604);
	session_menucheck(0x0605,power_boost-POWER_BOOST0);
	session_menuradio(0x8501+crtc_type,0x8501,0x8505);
	session_menucheck(0x8508,!crtc_hold);
	session_menucheck(0x8590,!(disc_filemode&2));
	session_menucheck(0x8591,disc_filemode&1);
	session_menucheck(0x8510,!(disc_disabled&1));
	#ifdef Z80_DANDANATOR
	session_menucheck(0xC500,!!mem_dandanator);
	session_menucheck(0x0510,dandanator_canwrite);
	#endif
	#ifdef PSG_PLAYCITY
	session_menucheck(0x8518,!playcity_disabled);
	session_menucheck(0x8519,!dac_disabled);
	#endif
	session_menuradio(0x8521+litegun,0x8521,0x8524);
	session_menuradio(0x8511+ram_depth,0x8511,0x8517);
	session_menucheck(0x851F,!snap_extended);
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
	video_vsync_min=crtc_hold?VIDEO_VSYNC_LO*2-VIDEO_LENGTH_Y:VIDEO_VSYNC_LO;
	video_vsync_max=crtc_hold?VIDEO_VSYNC_HI*2-VIDEO_LENGTH_Y:VIDEO_VSYNC_HI;
	kbd_joy[4]=kbd_joy[6]=0x4C+key2joy_flag;
	kbd_joy[5]=kbd_joy[7]=0x4D-key2joy_flag;
	#if AUDIO_CHANNELS > 1
	session_menuradio(0xC401+audio_mixmode,0xC401,0xC404);
	for (int i=0;i<3;++i)
		psg_stereo[i][0]=256+audio_stereos[audio_mixmode][i],psg_stereo[i][1]=256-audio_stereos[audio_mixmode][i]; // built-in stereo is L:M:R
	#ifdef PSG_PLAYCITY
	if (!playcity_disabled&&playcity_dirty)
	{
		if (playcity_dirty<3) // "ALCON 2020: SLAP FIGHT" uses just one Playcity chip, as stereo as the PSG!
		{
			for (int i=0;i<16;++i) playcity_outputs[i]=psg_outputs[i]; // 100% single chip
			for (int i=0;i<3;++i)
				playcity_stereo[0][i][0]=playcity_stereo[1][i][0]=psg_stereo[i][0], // as long as just one chip (whichever) is on,
				playcity_stereo[0][i][1]=playcity_stereo[1][i][1]=psg_stereo[i][1]; // it doesn't matter how the other is config'd
		}
		else
		{
			for (int i=0;i<16;++i) playcity_outputs[i]=psg_outputs[i]>>1; // 50% double chip
			for (int i=0;i<3;++i)
				playcity_stereo[0][i][0]=psg_stereo[2][0],playcity_stereo[0][i][1]=psg_stereo[2][1], // PlayCity chip 0 is RIGHT
				playcity_stereo[1][i][0]=psg_stereo[0][0],playcity_stereo[1][i][1]=psg_stereo[0][1]; // PlayCity chip 1 is LEFT
			psg_stereo[0][0]=psg_stereo[2][0]=psg_stereo[1][0]; psg_stereo[0][1]=psg_stereo[2][1]=psg_stereo[1][1]; // AY chip is MIDDLE
		}
	}
	#endif
	#endif
	video_resetscanline(),debug_dirty=1; sprintf(session_info,"%d:%dK %s%c %d.0MHz"//" | disc %s | tape %s | %s"
		,ram_dirty,ram_kbytes(ram_depth),plus_enabled?"ASIC":"CRTC",'0'+crtc_type,4<<multi_t);
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
				#ifdef Z80_DANDANATOR
				"\t(shift: load Dntr..)" "\t"
				"\t(shift: eject Dntr)"
				"\n"
				#endif
				"F6\tToggle realtime" MESSAGEBOX_WIDETAB
				"^F6\tNext CPU speed"
				"\n"
				"\t(shift: ..CRTC)" MESSAGEBOX_WIDETAB
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
				"\t(shift: record..)"
				//"\t\t(shift: play/stop)"
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
				"Amstrad CPC emulator written by Cesar Nicolas-Gonzalez\n"
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
			if (session_shift)
				key2joy_flag=!key2joy_flag; // FLIP JOYSTICK BUTTONS
			else
				session_key2joy=!session_key2joy; // TOGGLE JOYSTICK
			break;
		case 0x0401: // REDEFINE VIRTUAL JOYSTICK
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
		case 0x8501: // CRTC0
		case 0x8502: // CRTC1
		case 0x8503: // CRTC2
		//case 0x8504: // CRTC3
		case 0x8505: // CRTC4
			if (plus_enabled) {} // CRTC TYPE 3
			else //if (k!=0x8504)
			{
				crtc_type=k-0x8501; // CRTC TYPES 0,1,2,4
				crtc_syncs_update(),crtc_invis_update();
			}
			break;
		case 0x8508: // STRICT V-HOLD
			crtc_hold=!crtc_hold; crtc_syncs_update(),crtc_invis_update();
			break;
		case 0x8590: // STRICT DISC WRITES
			disc_filemode^=2;
			break;
		case 0x8591: // DEFAULT READ-ONLY
			disc_filemode^=1;
			break;
		case 0x8510: // DISC DRIVE
			disc_disabled^=1; // disabling the disc drive while the firmware is on will freeze the machine!
			break;
		case 0x8511: // 64K RAM
		case 0x8512: // 128K RAM
		case 0x8513: // 192K RAM
		case 0x8514: // 320K RAM
		case 0x8515: // 576K RAM
		case 0x8516: // 1088K RAM
		case 0x8517: // 2112K RAM
			ram_setcfg(k-0x8511); mmu_update();
			break;
		#ifdef PSG_PLAYCITY
		case 0x8518: // PLAYCITY
			if (playcity_disabled=!playcity_disabled)
				playcity_reset();
			break;
		case 0x8519: // DIGIBLASTER
			if (dac_disabled=!dac_disabled)
				dac_voice=0;
			break;
		#endif
		case 0x8521: // JOYSTICK, NO LIGHTGUN
		case 0x8522: // TROJAN LIGHT PHASER
		case 0x8523: // GUNSTICK (MHT)
		case 0x8524: // WESTPHASER (LORICIEL)
			litegun=k-0x8521;
			break;
		case 0x851F: // STRICT SNA DATA
			snap_extended=!snap_extended;
			break;
		case 0x8500: // F5: LOAD FIRMWARE.. // INSERT DANDANATOR..
		#ifdef Z80_DANDANATOR
			if (session_shift)
			{
				if (s=puff_session_getfile(dandanator_path,"*.des;*.rom","Insert Dandanator card"))
				{
					if (dandanator_load(s)) // error? warn!
						session_message(txt_error_any_load,txt_error);
					else
						all_reset(); // setup and reset
				}
			}
			else
		#endif
			if (s=puff_session_getfile(bios_path,"*.crt;*.cpr;*.ini;*.rom","Load firmware"))
			{
				if (bios_load(s)) // error? warn and undo!
					session_message(txt_error_bios,txt_error),bios_reload(); // reload valid firmware, if required
				else
					all_reset(); // setup and reset
			}
			break;
		case 0x0500: // ^F5: RESET EMULATION // REMOVE DANDANATOR
			#ifdef Z80_DANDANATOR
			if (session_shift)
				{ if (mem_dandanator) dandanator_remove(); }
			//else // redundant!
			#endif
			all_reset();
			break;
		#ifdef Z80_DANDANATOR
		case 0x0510:
			dandanator_canwrite=!dandanator_canwrite;
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
			//case 0xC600: // +SHIFT: NEXT CRTC TYPE
			if (plus_enabled) {} // CRTC TYPE 3
			else
			{
				if (++crtc_type==3) crtc_type=4;
				else if (crtc_type>4) crtc_type=0;
				crtc_syncs_update(),crtc_invis_update();
			}
			break;
		case 0x8601: // REALTIME x1
		case 0x8602: // REALTIME x2
		case 0x8603: // REALTIME x3
		case 0x8604: // REALTIME x4
			session_rhythm=k-0x8600-1; session_fast&=~1;
			break;
		case 0x0600: // ^F6: TOGGLE TURBO Z80
			multi_u=(1<<(multi_t=(multi_t+(session_shift?-1:1))&3))-1;
			// update hardware?
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
			else if (s=puff_session_getfile(tape_path,"*.cdp;*.cdt;*.csw;*.wav","Insert tape"))
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
		/*case 0x9000: // NMI
			Z80_NMI_SET;
			break;*/
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
			video_main_xlat(),video_xlat_clut();
			break;
		case 0x8B00: // F11: PALETTE
			video_type=(video_type+(session_shift?4:1))%5;
			video_main_xlat(),video_xlat_clut();
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
		#ifdef DEBUG
		case 0x9500: // PRIOR
			++plus_dma_delay1;
			break;
		case 0x9600: // NEXT
			--plus_dma_delay1;
			break;
		case 0x9700: // HOME
			++plus_dma_delay0;
			break;
		case 0x9800: // END
			--plus_dma_delay0;
			break;
		#endif
	}
}

void session_configreadmore(char *s) // parse a pre-processed configuration line: `session_parmtr` keeps the value name, `s` points to its value
{
	int i; char *t=UTF8_BOM(session_parmtr); if (!s||!*s||!*t) {} // ignore if empty or internal!
	else if (i=eval_hex(*s)&31,!strcasecmp(t,"type")) { if (i<length(bios_system)) type_id=i; }
	else if (!strcasecmp(t,"crtc")) { if (i<5) crtc_type=i; }
	else if (!strcasecmp(t,"bank")) { if (i<=ram_maxcfg) ram_setcfg(i); }
	else if (!strcasecmp(t,"unit")) disc_filemode=i&3,disc_disabled=(i>>2)&1;
	else if (!strcasecmp(t,"misc")) snap_extended=i&1,key2joy_flag=(i>>1)&1;
	#ifdef PSG_PLAYCITY
	else if (!strcasecmp(t,"play")) playcity_disabled=~i&1,dac_disabled=(~i>>1)&1;
	#endif
	else if (!strcasecmp(t,"file")) strcpy(autorun_path,s);
	else if (!strcasecmp(t,"snap")) strcpy(snap_path,s);
	else if (!strcasecmp(t,"tape")) strcpy(tape_path,s);
	else if (!strcasecmp(t,"disc")) strcpy(disc_path,s);
	else if (!strcasecmp(t,"bios")) strcpy(bios_path,s);
	#ifdef Z80_DANDANATOR
	else if (!strcasecmp(t,"cart")) strcpy(dandanator_path,s);
	#endif
	else if (!strcasecmp(t,"vjoy")) { if (!hexa2byte(session_parmtr,s,KBD_JOY_UNIQUE)) usbkey2native(kbd_k2j,session_parmtr,KBD_JOY_UNIQUE); }
	else if (!strcasecmp(t,"palette")) { if (i<5) video_type=i; }
	else if (!strcasecmp(t,"casette")) tape_rewind=i&1,tape_skipload=(i>>1)&1,tape_fastload=(i>>2)&1;
	else if (!strcasecmp(t,"debug")) debug_configread(strtol(s,NULL,10));
}
void session_configwritemore(FILE *f) // update the configuration file `f` with emulator-specific names and values
{
	native2usbkey(kbd_k2j,KBD_JOY_UNIQUE); fprintf(f,"type %d\ncrtc %d\nbank %d\nunit %d\nmisc %d\n"
		#ifdef PSG_PLAYCITY
		"play %d\n"
		#endif
		"file %s\nsnap %s\ntape %s\ndisc %s\nbios %s\n"
		#ifdef Z80_DANDANATOR
		"cart %s\n"
		#endif
		"vjoy %s\npalette %d\ncasette %d\ndebug %d\n",
		type_id,crtc_type,ram_depth,(disc_disabled&1)*4+disc_filemode,(key2joy_flag&1)*2+snap_extended,
		#ifdef PSG_PLAYCITY
		(playcity_disabled?0:1)+(dac_disabled?0:2),
		#endif
		autorun_path,snap_path,tape_path,disc_path,bios_path,
		#ifdef Z80_DANDANATOR
		dandanator_path,
		#endif
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
						crtc_type=(BYTE)(argv[i][j++]-'0');
						if (crtc_type<0||crtc_type>4)
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
						if (type_id<0||type_id>length(bios_system))
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
			"  -gN\tset CRTC type (0..4)\n"
			"  -j\tenable joystick keys\n"
			"  -J\tdisable joystick\n"
			"  -k0\t64K RAM\n"//"-K"
			"  -k1\t128K RAM\n"
			"  -k2\t192K RAM\n"
			"  -k3\t320K RAM\n"
			"  -k4\t576K RAM\n"
			"  -k5\t1088K RAM\n"
			"  -k6\t2112K RAM\n"
			"  -m0\t464 firmware\n"
			"  -m1\t664 firmware\n"
			"  -m2\t6128 firmware\n"
			"  -m3\tPLUS firmware\n"
			"  -o/O\tenable/disable onscreen status\n"
			"  -p/P\tenable/disable Playcity audio\n"
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
	amsdos_load("cpcados.rom"); //if (!*bdos_rom&&!disc_disabled) disc_disabled=1,mmu_update(); // can't enable the disc drive without its ROM!
	if (k) all_reset(); // reset machine again if required
	char *s=session_create(session_menudata); if (s)
		return sprintf(session_scratch,"Cannot create session: %s!",s),printferror(session_scratch),1;
	session_kbdreset();
	session_kbdsetup(kbd_map_xlt,length(kbd_map_xlt)/2);
	video_target=&video_frame[video_pos_y*VIDEO_LENGTH_X+video_pos_x]; audio_target=audio_frame;
	video_main_xlat(),video_xlat_clut(); session_resize();
	// it begins, "alea jacta est!"
	for (audio_disabled=!session_audio;!session_listen();)
	{
		while (!session_signal)
			z80_main( // clump Z80 instructions together to gain speed...
				UNLIKELY(video_pos_x<video_threshold)?0: // VRAM threshold ("Chapelle Sixteen") and IRQ events
				((VIDEO_LENGTH_X+15-video_pos_x)>>4)<<multi_t); // ...without missing any IRQ and CRTC deadlines!
		if (session_signal&SESSION_SIGNAL_FRAME) // end of frame?
		{
			if (audio_required)
			{
				if (audio_pos_z<AUDIO_LENGTH_Z) audio_main(TICKS_PER_FRAME); // fill sound buffer to the brim!
				#ifdef PSG_PLAYCITY
				if (!playcity_disabled)
					playcity_main(audio_frame,AUDIO_LENGTH_Z);
				#endif
				audio_playframe();
			}
			if (video_required&&onscreen_flag)
			{
				if (disc_disabled)
					onscreen_text(+1,-3,"--\t--",0);
				else // disc drive is busy?
				{
					if (k=disc_phase&2) onscreen_char(+3,-3,disc_phase&1?128+'R':128+'W');
					onscreen_byte(+1,-3,disc_track[0],k&&((disc_parmtr[1]&3)==0));
					onscreen_byte(+4,-3,disc_track[1],k&&((disc_parmtr[1]&3)==1));
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
					if (autorun_mode) // big letter "A"
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
						if (video_threshold>VIDEO_LENGTH_X/4)
							onscreen_bool(-6,-6,1,1,0);
					}
				}
				#ifdef DEBUG
				onscreen_byte(+1,+1,plus_dma_delay1,0);
				onscreen_byte(+4,+1,plus_dma_delay0,0);
				#endif
				session_status();
			}
			// update session and continue
			if (!--autorun_t) autorun_next();
			dac_frame(); if (ym3_file) ym3_write(),ym3_flush();
			tape_skipping=audio_queue=0; // reset tape and audio flags
			if (tape_enabled)
				{ if (tape_delay>0) --tape_delay; } // handle tape delays
			else if (tape_delay<3) // the tape is temporarily "deaf":
				++tape_delay; // OPERA SOFT tapes need this delay! [3..]
			if (!tape_fastload) tape_song=0,tape_loud=1; else if (!tape_song) tape_loud=1;
			else tape_loud=0,--tape_song; // expect song to play for several frames
			tape_output=tape_type<0&&(pio_port_c&32); // keep tape output consistent
			if (tape_signal)
			{
				//if (tape_signal<2||!ram_depth) pio_port_c&=~16; // stop tape if required
				tape_signal=0,session_dirty=1; // update config
			}
			if (tape&&tape_filetell<tape_filesize&&tape_skipload&&!session_filmfile&&!tape_disabled&&tape_loud) // `tape_loud` implies `!tape_song`
				session_fast|=+2,audio_disabled|=+2,video_framelimit|=MAIN_FRAMESKIP_MASK+1; // abuse binary logic to reduce activity
			else
				session_fast&=~2,audio_disabled&=~2,video_framelimit&=MAIN_FRAMESKIP_MASK  ; // ditto, to restore normal activity
			session_update();
			//if (!audio_disabled) audio_main(1+(video_pos_x>>4)); // preload audio buffer
		}
	}
	// it's over, "acta est fabula"
	z80_close(); if (ext_rom) free(ext_rom);
	disc_closeall();
	tape_close(); ym3_close(); if (printer) printer_close();
	return session_byebye(),session_post();
}

BOOTSTRAP

// ============================================ END OF USER INTERFACE //
