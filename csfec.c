 //  ####    ####   ######  ######   ####  ------------------------- //
//  ##  ##  ##  ##  ##      ##      ##  ##  CSFEC, small Commodore 64 //
//  ##      ##      ##      ##      ##      (SixtyFour) emulator made //
//  ##       ####   ####    ####    ##      on top of CPCEC's modules //
//  ##          ##  ##      ##      ##      by Cesar Nicolas-Gonzalez //
//  ##  ##  ##  ##  ##      ##      ##  ##  since 2022-01-12 till now //
 //  ####    ####   ##      ######   ####  ------------------------- //

#define MY_CAPTION "CSFEC"
#define my_caption "csfec"
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

// In a similar spirit to ZXSEC, the goal is to adequately emulate the
// Commodore 64 computer, reusing as much of CPCEC as possible despite
// MOS' own hardware: CPU 6510, VIC-II 6569 (video), SID 6581 (audio),
// CIA 6526 (I/O+timers)... plus the chips in the C1541 disc drive.

// This file provides C64-specific features for configuration, VIC-II,
// PAL and CIA logic, 6510 timings and support, snapshots, options...

#include <stdio.h> // printf()...
#include <stdlib.h> // strtol()...
#include <string.h> // strcpy()...

// Commodore 64 PAL metrics and constants defined as general types -- //

#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (63<<4)
#define VIDEO_LENGTH_Y (39<<4)
#define VIDEO_OFFSET_X (13<<4)
#define VIDEO_OFFSET_Y (11<<2) // the best balance for "Delta" (score panel on top) and "Megaphoenix" (on bottom)
#define VIDEO_PIXELS_X (48<<4)
#define VIDEO_PIXELS_Y (67<<3)
//#define VIDEO_OFFSET_X (17<<4) // show the default 640x400 screen without the border
//#define VIDEO_OFFSET_Y (25<<2)
//#define VIDEO_PIXELS_X (40<<4)
//#define VIDEO_PIXELS_Y (25<<4)
#define VIDEO_RGB2Y(r,g,b) ((r)*3+(g)*6+(b)) // generic RGB-to-Y expression

#if defined(SDL2)||!defined(_WIN32)
unsigned short session_icon32xx16[32*32] = {
	#include "csfec-a4.h"
	};
#endif

// The Commodore 64 keyboard ( http://unusedino.de/ec64/technical/aay/c64/keybmatr.htm )
// Bear in mind that the keyboard codes are written in OCTAL rather than HEXADECIMAL!
// +-------------------------------------------------------------------------------+ +----+
// | <- | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 0  | +  | -  | £  |HOME|DEL | | F1 |
// | 71 | 70 | 73 | 10 | 13 | 20 | 23 | 30 | 33 | 40 | 43 | 50 | 53 | 60 | 63 | 00 | | 04 |
// +-------------------------------------------------------------------------------+ +----+
// | CTRL | Q  | W  | E  | R  | T  | Y  | U  | I  | O  | P  | @  | *  | ^  |RESTORE| | F3 |
// | 72   | 76 | 11 | 16 | 21 | 26 | 31 | 36 | 41 | 46 | 51 | 56 | 61 | 66 |RESTORE| | 05 |
// +-------------------------------------------------------------------------------+ +----+
// |R/S |CAPS| A  | S  | D  | F  | G  | H  | J  | K  | L  | :  | ;  | =  | RETURN  | | F5 |
// | 77 |LOCK| 12 | 15 | 22 | 25 | 32 | 35 | 42 | 45 | 52 | 55 | 62 | 65 |   01    | | 06 |
// +----+----------------------------------------------------------------+---------+ +----+
// | C= |LSHIFT| Z  | X  | C  | V  | B  | N  | M  | ,  | .  | /  |RSHIFT |C.DN|C.RT| | F7 |
// | 75 | 17   | 14 | 27 | 24 | 37 | 34 | 47 | 44 | 57 | 54 | 67 | 64    | 07 | 02 | | 03 |
// +--------------------------------------------------------+----------------------+ +----+
// SHIFT+HOME=CLR |                  SPACE                  | SHIFT+C.DN=C.UP  "CAPS LOCK":
// SHIFT+DEL=INST |                   74                    | SHIFT+C.RT=C.LT    SHIFT+C=
//                +-----------------------------------------+

#define KBD_JOY_UNIQUE 5 // four sides + one fire
unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1-fire4
	{ 0110,0111,0112,0113,0114,0114,0114,0114 }; // always constant, all joystick bits are hard-wired
//#define MAUS_EMULATION // ignore!
//#define MAUS_LIGHTGUNS // ignore!
#define VIDEO_LO_X_RES // no "half" (hi-res) pixels are ever drawn
#define RUNLENGTH_OBSOLETE // snap_load/snap_save (oldest!!)
#define RUNLENGTH_ENCODING // snap_load/snap_save (old!)
#define LEMPELZIV_ENCODING // snap_load/snap_save
#define POWER_BOOST1 7 // power_boost default value (enabled)
#define POWER_BOOST0 8
#define DEBUG_LONGEST 3 // MOS 65XX opcodes can be up to 3 bytes long
#include "cpcec-rt.h" // emulation framework!

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
	// key mirrors
	KBCODE_CHR4_4	,0071, // "<" = ARROW LEFT; KBCODE_CHR4_4 105-K!
};

VIDEO_UNIT video_table[16]; // colour table, 0xRRGGBB style: the 16 original colours
VIDEO_UNIT video_xlat[16]; // all colours are static in the VIC-II

char palette_path[STRMAX]="";
int video_table_load(char *s) // based on VICE's palette files (either RR GG BB or RR GG BB X) for the 16 inks
{
	FILE *f=puff_fopen(s,"r"); if (!f) return -1;
	unsigned char t[STRMAX],n=0; VIDEO_UNIT p[16];
	while (fgets(t,STRMAX,f)&&n<=16) if (*t>'#') // skip "# comment" and others
		{ unsigned int r,g,b; if (sscanf(UTF8_BOM(t),"%X%X%X",&r,&g,&b)!=3) n=16; else if (n<16) p[n]=((r&255)<<16)+((g&255)<<8)+(b&255); ++n; }
	puff_fclose(f); if (n!=16) return -1;
	STRCOPY(palette_path,s); for (n=0;n<16;++n) video_table[n]=p[n]; return 0;
}
void video_table_reset(void)
{
	VIDEO_UNIT const p[]={
		// Community-Colors v1.2a -- p1x3l.net
		0X000000,0XFFFFFF,0XAF2A2A,0X62D8CC,0XB03FB6,0X4AC64A,0X3739C4,0XE4ED4E,
		0XB6591C,0X683808,0XEA746C,0X4D4D4D,0X848484,0XA6FA9E,0X707CE6,0XB6B6B6,
		// Pepto's Colodore v1 -- colodore.com
		//0X000000,0XFFFFFF,0X813338,0X75CEC8,0X8E3C97,0X56AC4D,0X2E2C9B,0XEDF171,
		//0X8E5029,0X553800,0XC46C71,0X4A4A4A,0X7B7B7B,0XA9FF9F,0X706DEB,0XB2B2B2,
		// VICE 2.4 -- vice-emu.sourceforge.io
		//0X000000,0XFFFFFF,0X924A40,0X84C5CC,0X9351B6,0X72B14B,0X483AAA,0XD5DF7C,
		//0X99692D,0X675200,0XC18178,0X606060,0X8A8A8A,0XB3EC91,0X867ADE,0XB3B3B3,
	};
	for (int n=0;n<16;++n) video_table[n]=p[n];
}

// GLOBAL DEFINITIONS =============================================== //

#define TICKS_PER_FRAME ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)>>5)
#define TICKS_PER_SECOND (TICKS_PER_FRAME*VIDEO_PLAYBACK)
int multi_t=0,multi_u=0; // overclocking shift+bitmask

// HARDWARE DEFINITIONS ============================================= //

BYTE mem_ram[9<<16],mem_rom[5<<12],mem_i_o[1<<12]; // RAM (64K C64 + 512K REU/GEORAM), ROM (8K KERNAL, 8K BASIC, 4K CHARGEN) and I/O (1K VIC-II, 1K SID, 1K VRAM, 1K CIA+EXTRAS)
#define ext_ram (&mem_ram[1<<16]) // RAM beyond the base 64K, see below
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

BYTE disc_disabled=0; // disables the disc drive altogether as well as its related logic; +1 = manual, +2 = automatic
BYTE disc_filemode=1; // +1 = read-only by default instead of read-write; +2 = relaxed disc write errors instead of strict
VIDEO_UNIT video_clut[32]; // precalculated 16-colour palette + $D020 border [16] + $D021.. background 0-3 [17..20] + $D025.. sprite multicolour 0-1 [21,22] + sprite #0-7 colour [23..30] + DEBUG

/*char audio_dirty;*/ int audio_queue=0; // used to clump audio updates together to gain speed

// the MOS 6510 and its MMU ----------------------------------------- //

HLII m6510_pc; BYTE m6510_a,m6510_x,m6510_y,m6510_s,m6510_p; int m6510_irq,m6510_int; // the MOS 6510 registers; we must be able to spy them!
BYTE mmu_cfg[2]; // the first two bytes in the MOS 6510 address map; distinct to the first two bytes of RAM!
BYTE mmu_inp,mmu_out; // the full I/O values defined by the MOS 6510's first two bytes
BYTE mmu_mcr=~0; // the current memory map configuration; used to reduce polling. cfr. mmu_recalc() to force an update of the memory map
BYTE tape_enabled=0; // manual tape playback, i.e. the PLAY button
#define tape_disabled ((mmu_cfg[1]&32)>=tape_enabled) // the machine can disable the tape even when we enable it
BYTE tape_browsing=0; // the signal toggles when the tape deck rewinds or fast-forwards

void mmu_setup(void)
{
	for (int i=0;i<256;++i) mmu_rom[i]=mmu_ram[i]=mem_ram; // by default, everything is RAM...
	MEMZERO(mmu_bit); mmu_bit[0]=1+2; // ...and only ZEROPAGE triggers events (non-dumb R/W)
}

// the REU and GeoRAM memory extensions are handled through the MMU, so their logic goes here

BYTE ram_depth=0; // REU memory extension
int ram_kbyte[]={0,64,128,256,512}; // REU/GEORAM size in kb
int ram_cap=0,ram_dirty=0; // up to ((8<<16)-1); see `ext_ram`
HLII reu_word; int reu_addr,reu_size=0; // REU C64-extra offsets+length
BYTE reu_table[32]; // REU config.registers
BYTE georam_yes=0,georam_block=0,georam_page=0; // GEORAM configuration
#define reu_ram ext_ram
#define reu_cap ram_cap
#define reu_dirty ram_dirty
#define reu_empty 0XFF // the REU can request bytes from empty space
#define reu_max 0XFFFFF // i.e. (16<<16)-1, 512k RAM + 512k empty space!
#define reu_inc_65xx (!(reu_table[10]&128))
#define reu_inc_addr (!(reu_table[10]& 64))
#define reu_fail() (reu_table[0]|=32)
#define reu_mode (reu_table[1]&3)
#define reu_last() do{ reu_table[0]|=64; if (!(reu_table[1]&32)) reu_table[2]=reu_word.b.l,reu_table[3]=reu_word.b.h,reu_table[4]=reu_addr,reu_table[5]=reu_addr>>8,reu_table[6]=reu_addr>>16,reu_table[7]=1,reu_table[8]=0; } while(0)
void reu_kick(void) // start a REU operation according to the current registers
{
	if (reu_table[1]&128)
	{
		reu_word.b.l=reu_table[2],reu_word.b.h=reu_table[3];
		reu_addr=(reu_table[4]+(reu_table[5]<<8)+((reu_table[6]&15)<<16)); // slight mod to catch >512K invasions
		if (!(reu_size=reu_table[7]+(reu_table[8]<<8))) reu_size=1<<16;
		//char t[4][5]={"SAVE","LOAD","SWAP","TEST"};
		//cprintf("REU-%c:%s-%04X ",ram_depth+'0',t[reu_mode],reu_size-1);
		//cprintf("REU: %s offsets $%04X:$%06X length $%05X:$%06X\n",t[reu_mode],reu_word.w,reu_addr,reu_size,ram_cap);
	}
}
#define reu_reset() (MEMZERO(reu_table),georam_block=georam_page=ram_dirty=0,reu_table[1]=16) // notice that REU + GEORAM are reset at once

// cartridges use their own memory maps (EasyFlash is the preferred type) although always through the MMU

BYTE *cart=NULL,cart_boot=0,cart_mode,cart_bank; INT8 cart_type=-1; WORD m6510_start; // general cartridge settings, including MOS 6510 RESET override
BYTE cart_poke[64]; // some cartridges embed RAM chips; this is what the first WORD in the CHIP header means.
BYTE cart_easy[256]; // Easyflash RAM page: avoid weird accidents by keeping it separate from other devices!

// generally speaking, cartridges can operate in four ways, as defined by GAME (+1) and EXROM (+2); notice that docs often talk about /GAME and /EXROM, their negative states!
// cart_mode = 0 : the cartridge is invisible, regardless of I/O status
// cart_mode = 1 : Ultimax mode: a 8k page (ROML) is always visible on $8000-$9FFF and another one (ROMH) on $E000-$FFFF, effectively overriding the KERNAL bank!
// cart_mode = 2 : 8K mode: if BASIC is visible at $A000-$BFFF, then ROML occupies $8000-$9FFF
// cart_mode = 3 : 16K mode: if BASIC is visible at $A000-$BFFF, then ROML occupies $8000-$9FFF, as in 8K mode; besides, if KERNAL is visible at $E000-$FFFF, then ROMH occupies $A000-$BFFF
// some cartridges are always stuck in these modes; others can change modes and even make themselves fully invisible until the next system reset.
// For example, type 32 (Easyflash) stores the bank value (0-63) in $DE00 and GAME (bit 0) and EXROM (bit 1) in $DE02.

char cart_path[STRMAX]="";

void cart_remove(void) // remove a cartridge; resetting the system must be done by the caller
{
	cart_boot=0; /*cart_reset();*/ cart_type=-1; if (cart) { free(cart),cart=NULL; }
}
int cart_insert(char *s) // insert a cartridge or "inject" a program file thru a subset of the autorun handling
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1;
	BYTE header[32]=""; fread1(header,32,f);
	if (!strcmp(header,"C64 CARTRIDGE   ")) // notice the trailing spaces and the implied final ZERO!
	{
		fseek(f,mgetmmmm(&header[16]),SEEK_SET); // skip unused bytes!
		if (!cart) cart=malloc(1<<20); // 1 MEGABYTE!
		if (!cart||header[20]!=1) return puff_fclose(f),1; // something went wrong!
		cart_type=header[23]&127; // no types go above 32 ...AFAIK!
		cprintf("CRT: ");
		int i; while (fgetmmmm(f)==0X43484950&&(i=fgetmmmm(f))>16&&i<=16400) // "CHIP" header
		{
			int q=fgetmm(f); int p=fgetmm(f); cart_poke[p&63]=q&3; q=fgetmm(f); // first WORD: bank type: 0 ROM, 1 RAM, 2 FLASH
			if (i-fgetmm(f)!=16) { cprintf("%08X? ",i); break; } // the header+body size must match the chunk size!!
			cprintf("%04X:%04X ",p,q); // perhaps we should check whether `p` and `q` are valid as well...
			fread1(&cart[(p&63)*0X4000+(q&0X2000)],i-16,f);
		}
		cprintf("END!\n");
		cart_boot=cart_type==32?1:(~header[24]&1)*2+(~header[25]&1); /*cart_reset();*/ // boot mode according to header, f.e. 01 00 = ULTIMAX, 00 01 = 8K
	}
	else if (equalsii(header,0x0801)&&(header[2]+header[3])) // PRG magic numbers?
		cart_remove(); // a program file needs BASIC, and thus cannot coexist with a cartridge!
	else
		return puff_fclose(f),1; // wrong magic number!
	STRCOPY(cart_path,s);
	return puff_fclose(f),0;
}

#define cart_reset() (cart_bank=0,m6510_start=(cart_mode=cart_boot)==1?mgetii(&cart[0X3FFC]):mgetii(&mem_rom[0X1FFC]))

// PORT $0001 input mask: 0XFF -0X17 (MMU + TAPE INP) -0X20 (TAPE MOTOR) =0XC8!
#define MMU_CFG_GET(w) ((w)?(mmu_cfg[0]&mmu_cfg[1])+(~mmu_cfg[0]&((mmu_out|mmu_inp)&0XDF)):mmu_cfg[0])
BYTE mmu_cfg_get(WORD w) { return MMU_CFG_GET(w); }
#define MMU_CFG_SET(w,b) do{ mmu_cfg[(w)]=(b); mmu_update(); }while(0)
#define mmu_cfg_set MMU_CFG_SET

#define mmu_recalc() (mmu_mcr=~0,mmu_update()) // this overrides the MMU cache; use this with care!

void mmu_update(void) // set the address offsets, as well as the tables for m6510_recv(WORD) and m6510_send(WORD,BYTE)
{
	int i; if (cart_mode==1) i=5; // ULTIMAX mode always forces BASIC and KERNAL out and I/O in!
	else mmu_out=(~mmu_cfg[0]&mmu_out)+(i=mmu_cfg[0]&mmu_cfg[1]); // other modes rely on MMU CFG
	// we build the MMU status cache: low 3 bits are taken from the 6510 I/O ports, top 2 bits are the cartridge
	if (mmu_mcr!=(i=(~mmu_cfg[0]&7)+(i&7)+(cart_mode<<6))) // i.e. EG000ZZZ, E = EXROM, G = GAME, Z = 6510 I/O MMU
	{
		BYTE *recv8=mem_ram,*recva=mem_ram,*recve=mem_ram,*recvd=mem_ram,*sendd=mem_ram; // all RAM by default
		switch ((mmu_mcr=i)&7) // notice the "incremental" design and the lack of `break` here!
		{
			case 7: recva=&mem_rom[0X2000-0XA000]; // BASIC
			case 6: recve=&mem_rom[0X0000-0XE000]; // KERNAL
			case 5: recvd=sendd=&mem_i_o[-0XD000]; // I/O
			case 4: break;
			case 3: recva=&mem_rom[0X2000-0XA000]; // BASIC
			case 2: recve=&mem_rom[0X0000-0XE000]; // KERNAL
			case 1: recvd=&mem_rom[0X4000-0XD000]; // CHARGEN
			case 0: break;
		}
		/*if (cart)*/ switch (mmu_mcr&192) // now we check the cartridge mode; again, the design is incremental
		{
			case 192: // 16K mode (both low)
				if ((mmu_mcr&2)) recva=&cart[((cart_bank&63)<<14)+0X2000-0XA000]; // ROMH
			case 128: // 8K mode (/EXROM low)
				if (!(~mmu_mcr&3)) recv8=&cart[((cart_bank&63)<<14)+0X0000-0X8000]; // ROML
				break;
			case  64: // ULTIMAX mode (/GAME low)
				recv8=&cart[((cart_bank&63)<<14)+0X0000-0X8000]; // ROML
				recve=&cart[((cart_bank&63)<<14)+0X2000-0XE000]; // ROMH
			case   0: // invisible!
				break;
		}
		for (i=0;i<16;++i)
			mmu_rom[i+0X80]=mmu_rom[i+0X90]=recv8,
			mmu_rom[i+0XA0]=mmu_rom[i+0XB0]=recva,
			mmu_rom[i+0XE0]=mmu_rom[i+0XF0]=recve,
			mmu_rom[i+0XD0]=recvd,mmu_ram[i+0XD0]=sendd;
		i=sendd!=mem_ram?15:0; // tag the page I/O only if possible
		memset(&mmu_bit[0XD0],i  ,4); // $D000-$D3FF, VIC-II: all operations, cfr. "FUNNY RASTERS"
		memset(&mmu_bit[0XD4],i&3,4); // $D400-$D7FF, SID #1: non-dumb R/W
		memset(&mmu_bit[0XD8],i&2,4); // $D800-$DBFF, COLORS: non-dumb W
		memset(&mmu_bit[0XDC],i&7,2); // $DC00-$DDFF, CIA1+2: non-dumb R/W and dumb R, cfr. "4KRAWALL"
		//memset(&mmu_bit[0XDE],i&3,0X02); // $DE00-$DFFF, extras: non-dumb R/W
		//cprintf("CRT:%02d,M:%d,B:%02X\n",cart_type,mmu_mcr>>6,cart_bank);
	}
	// the logic of pages $DE00, $DF00 and $FF00 is simpler, the footprint is less severe
	/**/ if ((mmu_bit[0XDE]=mmu_bit[0XD4])&&cart_type>=0) // show cartridge in range $DE00-$DFFF?
		switch (cart_type)
		{
			case 32: // Easyflash
				mmu_bit[0XDF]=0,mmu_ram[0XDF]=mmu_rom[0XDF]=&cart_easy[0-0XDF00]; // $DF00-$DFFF: Easyflash RAM space
				break;
			default:
				mmu_bit[0XDF]=3; // non-dumb R/W
		}
	else if ((mmu_bit[0XDF]=mmu_bit[0XD4])&&ram_cap&&georam_yes) // show GEORAM in range $DE00-$DEFF?
	{
		ram_dirty|=255+(i=(((georam_block<<6)+(georam_page&63))<<8)&ram_cap); // `i` is the first byte, but `ram_dirty` must be the last byte
		mmu_bit[0XDE]=0,mmu_ram[0XDE]=mmu_rom[0XDE]=&ext_ram[i-0XDE00]; // notice that `mmu_bit[0XDE]` was set to 3 beforehand, instead of 0
		//mmu_bit[0XDF]=3; // $DF00-$DFFF: non-dumb R/W
	}
	else // merge pages $DE00 and $DF00 with current I/O configuration, be it I/O ports or just the RAM at $D000-$DFFF
		mmu_ram[0XDE]=mmu_ram[0XDF]=mmu_ram[0XD4],mmu_rom[0XDE]=mmu_rom[0XDF]=mmu_rom[0XD4];
	mmu_bit[0XFF]=(ram_cap&&!georam_yes&&(reu_table[1]&(128+16))==128)?2:0; // $FF00, REU: non-dumb W (the $FF00 "trigger")
}

void mmu_reset(void)
	{ cart_reset(); mmu_cfg[0]=0X2F; mmu_cfg[1]=mmu_mcr=0XFF; mmu_out=0; mmu_inp=16+7; mmu_recalc(); }

// the VIC-II video chip ============================================ //

BYTE *vicii_memory,*vicii_attrib,*vicii_bitmap; // pointers to the video data blocks: the 16K page, the attribute map and the pixel table
BYTE vicii_mode,vicii_lastmode; // current video mode: 0..7 video mode proper, +8 LEFT/RIGHT BORDER, +16 = BACKGROUND COLOUR, +32 TOP/BOTTOM BORDER
int vicii_pos_y,vicii_len_y,vicii_irq_y; // coordinates and dimensions of the current frame (PAL-312 or NTSC-262)
int vicii_pos_x,vicii_len_x,vicii_irq_x; // coordinates and dimensions of the current scanline and its sprite fetch point
BYTE vicii_ready; // bit 4 of $D011, the DEN bit; it must stick in several moments of the frame
BYTE vicii_frame; // enabled when a new frame must begin (not exactly the same as vicii_pos_y>=vicii_len_y)
BYTE vicii_crunch; // sprite crunch bitmask
BYTE vicii_badline; // are we in a badline?
BYTE vicii_takeover; // is the VIC-II going to fetch the sprites, a badline...? 0 = idle, 1 = busy, >1 special!
BYTE vicii_dmadelay; // advanced horizontal scroll caused by DMA delay
BYTE vicii_nosprites=0,vicii_noimpacts=0; // cheating!!
unsigned int vicii_sprite_k[8]; // the current 24 bits of each sprite's scanline
BYTE vicii_sprite_y[8]; // the low 6 bits of each sprite's counter
BYTE vicii_copy_border[256],vicii_copy_border_l,vicii_copy_border_r; // backups of border colours and states

void video_xlat_clut(void) // precalculate palette following `video_type`; part of it is managed by the VIC-II
{
	for (int i=0;i<16;++i) video_clut[i]=video_xlat[i]; // VIC-II built-in palette
	for (int i=0;i<15;++i) video_clut[16+i]=video_xlat[VICII_TABLE[32+i]&15]; // VIC-II dynamic palette
}
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
	memset(vicii_sprite_y,63,sizeof(vicii_sprite_y)); MEMZERO(vicii_sprite_k); MEMZERO(vicii_copy_border);
	memset(VICII_TABLE,vicii_mode=vicii_pos_x=vicii_pos_y=vicii_irq_y=vicii_frame=0,64);
	vicii_setmaps(); vicii_setmode(); video_xlat_clut();
}

// the CIA #1+#2 gate arrays ======================================== //

BYTE key2joy_flag=0; // alternate joystick ports
BYTE kbd_bits[10]; // combined autorun+keyboard+joystick bits

int cia_count_a[2],cia_count_b[2]; // the CIA event counters: the active value range is 0..65535
int cia_minor_a[2],cia_minor_b[2],cia_major_b[2]; // MINOR = clock tick step, MAJOR = cia_count_a[] underflow
int cia_event_a[2],cia_event_b[2]; // sending a byte to ports 14 and 15 triggers events; >=256 means a new byte
BYTE cia_state_a[2],cia_state_b[2]; // state bits: +8 LOAD, +4 WAIT, +2 MOVE, +1 STOP
int cia_serials[2]; char cia_serialz[2]; // serial bit transfer countdowns and values

#define cia_port_14(i,b) (cia_minor_a[i]=~b&32)
#define cia_port_15(i,b) ((cia_minor_b[i]=!(b&96)),(cia_major_b[i]=(b&96)==64))

unsigned int cia_hhmmssd[2]; // the CIA time-of-day registers
BYTE cia_port_13[2],cia_port_11[2]; // flags: CIA interrupt status + locked time-of-day status
char vicii_n_cia=0; // the counter of frames between decimals of second (5 PAL, 6 NTSC)
char cia_nouveau=1; // the original CIA 6526 had a bug that the later CIA 6526A fixed out: polling port 13 halted the countdown for an instant (?)
char vic_nouveau=1; // the original CIA 6526 - VIC-II 6569 bridge relied on discrete glue logic instead of a custom IC devised for the VIC-II 8565

int cias_alarmclock(int i) // check the alarm; return ZERO if no interruption must trigger, NONZERO otherwise
{
	if (equalsiiii(&CIA_TABLE_0[i*(CIA_TABLE_1-CIA_TABLE_0)+8],cia_hhmmssd[i]))
		if (((cia_port_13[i]|=4)&CIA_TABLE_0[i*(CIA_TABLE_1-CIA_TABLE_0)+13]&4)&&cia_port_13[i]<128)
			return cia_port_13[i]+=128;
	return 0;
}
int cias_24hours(int i) // tick the CIA time-of-day clock; return ZERO if no alarm happens, NONZERO otherwise
{
	DWORD x=cia_hhmmssd[i]; if (x&0X40000000) return 0;
	if ((++x&15)>=10) // tenths of second
		if (!(~(x+= 256 - 10 )&0X00000A00)) // seconds
			if (!(~(x+=0X00000600)&0X00006000)) // tens of seconds
				if (!(~(x+=0X0000A000)&0X000A0000)) // minutes
					if (!(~(x+=0X00060000)&0X00600000)) // tens of minutes
					{
						if (!(~(x+=0X00A00000)&0X13000000)) // hours
							x-=0X92000000;
						else if (!(~(x&0X0A000000))) // tens of hours
							x+=0X06000000;
					}
	return cia_hhmmssd[i]=x,cias_alarmclock(i);
}

// signals between the C64 main CPU and the C1541 controller:
// on the C1541 side ($1800)
// - bit 7: ATN IN
// - bit 6: 0 in drives 8 and 9, 1 in drives 10 and 11
// - bit 5: 0 in drives 8 and 10, 1 in drives 9 and 11
// - bit 4: ATN ACK OUT
// - bit 3: CLOCK OUT
// - bit 2: CLOCK IN
// - bit 1: DATA OUT
// - bit 0: DATA IN
// on the C64 side ($DD00)
// - bit 7: DATA IN
// - bit 6: CLOCK IN
// - bit 5: DATA OUT
// - bit 4: CLOCK OUT
// - bit 3: ATN OUT
BYTE c1541_peeks_c64(void) // the C1541 requests a byte from the C64
{
	return (((CIA_TABLE_1[0]>>7)&  1)+
		((CIA_TABLE_1[0]>>4)&  4)+
		((CIA_TABLE_1[0]<<3)&128))^133; // negative!?
}
void c1541_pokes_c64(BYTE b) // the C1541 sends a byte to the C64
{
	if (b&8)
	{
		if ((CIA_TABLE_1[13]|=16)&CIA_TABLE_1[14]&63)
			m6510_irq|=+128; // C1541 $XXXX raises an IRQ on the C64!
	}
}

void cia_reset(void)
{
	cia_hhmmssd[0]=cia_hhmmssd[1]=0X01000000; // = 1:00:00.0 AM
	memset(CIA_TABLE_0,cia_port_13[0]=0,16);
	memset(CIA_TABLE_1,cia_port_13[1]=0,16);
	CIA_TABLE_0[11]=CIA_TABLE_1[11]=vicii_n_cia=1;
	cia_state_a[0]=cia_state_a[1]=cia_state_b[0]=cia_state_b[1]=+0+0+0+1;
	cia_minor_a[0]=cia_minor_a[1]=
	cia_minor_b[0]=cia_minor_b[1]=
	cia_major_b[0]=cia_major_b[1]=
	cia_port_11[0]=cia_port_11[1]=
	cia_serials[0]=cia_serials[1]=
	cia_serialz[0]=cia_serialz[1]=0;
	CIA_TABLE_0[4]=CIA_TABLE_0[5]=CIA_TABLE_0[6]=CIA_TABLE_0[7]=
	CIA_TABLE_1[4]=CIA_TABLE_1[5]=CIA_TABLE_1[6]=CIA_TABLE_1[7]=255;
	CIA_TABLE_0[0]=CIA_TABLE_0[1]=CIA_TABLE_1[0]=CIA_TABLE_1[1]=255; // *?*
	cia_event_a[0]=cia_event_a[1]=cia_event_b[0]=cia_event_b[1]=256; // *!*
	cia_count_a[0]=cia_count_a[1]=cia_count_b[0]=cia_count_b[1]=65535;
}

// behind the CIA: tape I/O handling -------------------------------- //

char tape_path[STRMAX]="",tap_magic[]="C64-TAPE-RAW",t64_magic1[]="C64 tape image",t64_magic2[]="C64S tape ";
FILE *tape=NULL; int tape_filetell=0,tape_filesize=0,tape_filebase; // file handle, offset and length

BYTE tape_buffer[8<<8],tape_old_type; int tape_length,tape_offset; // tape buffer/directory (must be >1k!)
int tape_seek(int i) // moves file cursor to `i`, cancels the cache if required
{
	if (i<0) i=0; else if (i>tape_filesize) i=tape_filesize; // sanitize
	int j=i-tape_filetell+tape_offset; if (j>=0&&j<tape_length) // within cache?
		return tape_offset=j,tape_filetell=i;
	return fseek(tape,i,SEEK_SET),tape_offset=tape_length=0,tape_filetell=i;
}
void tape_skip(int i) { if (i) tape_seek(tape_filetell+i); } // quick!
int tape_getc(void) // returns the next byte in the file, or -1 on EOF
{
	if (tape_offset>=tape_length)
		if ((tape_offset=0)>=(tape_length=fread1(tape_buffer,sizeof(tape_buffer),tape)))
			return -1; // EOF
	return ++tape_filetell,tape_buffer[tape_offset++];
}
int tape_getcc(void) // returns an Intel-style WORD; see tape_getc()
	{ int i; if ((i=tape_getc())>=0) i|=tape_getc()<<8; return i; }
int tape_getccc(void) // returns an Intel-style 24-bit; see tape_getc()
	{ int i; if ((i=tape_getc())>=0) if ((i|=tape_getc()<<8)>=0) i|=tape_getc()<<16; return i; }
int tape_getcccc(void) // returns an Intel-style DWORD; mind the sign in 32bits!
	{ int i; if ((i=tape_getc())>=0) if ((i|=tape_getc()<<8)>=0) if ((i|=tape_getc()<<16)>=0) i|=tape_getc()<<24; return i; }

void tape_flush(void) // flushes the tape buffer onto the tape file
	{ fwrite1(tape_buffer,sizeof(tape_buffer),tape),tape_offset=0; }
void tape_putc(int i) // sends one byte to the tape buffer
	{ tape_buffer[tape_offset]=i; if (++tape_offset>=sizeof(tape_buffer)) tape_flush(); }

// tape_skipload disables realtime, raises frameskip, etc. during tape operation; tape_fastload hijacks tape loaders
char tape_thru_cia=0,tape_rewind=0,tape_skipload=1,tape_fastload=0; int tape_skipping=0; // tape options and status
char tape_signal,tape_status=0,tape_output,tape_type=0; // tape status
int tape_wave,tape_waverate,tape_waveskip,tape_wavesign; // WAVE status
int tape_t; // duration of the current tape signal in microseconds
BYTE m6510_t64ok=8; // T64-specific variable: MMU configuration trap (7 enabled, 8 disabled)
BYTE tape_polarity=0; // signal polarity, only relevant in WAVE files
#define FASTTAPE_STABLE // overkill!

int tape_close(void) // closes tape and cleans up
{
	if (tape)
	{
		if (tape_type<0) tape_flush(); //fseek(tape,16,SEEK_SET),fputiiii(tape_filesize-tape_filebase,tape); // saving a tape? store the real tape length
		puff_fclose(tape),tape=NULL;
	}
	tape_filesize=tape_filetell=tape_waveskip=0; m6510_t64ok=8;
	return tape_thru_cia=tape_type=tape_t=tape_output=tape_status=0;
}

int tape_open(char *s) // inserts a tape from path `s`; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"rb")))
		return 1; // cannot open file!
	MEMZERO(tape_buffer); tape_length=fread1(tape_buffer,sizeof(tape_buffer),tape);
	if (equalsmmmm(tape_buffer,0X52494646)&&equalsmmmm(&tape_buffer[8],0X57415645)&&equalsmmmm(&tape_buffer[12],0X666D7420)) // = "RIFF....WAVEfmt "
	{
		tape_filetell=tape_offset=16; int i,j=tape_getcccc(); tape_getcccc();
		tape_filesize=mgetiiii(&tape_buffer[4])+8;
		if (!(tape_waverate=tape_getcccc())) return tape_close(),1; // wrong WAVE header!
		tape_waveskip=(tape_getcccc()/tape_waverate)-1; tape_wavesign=(tape_getcccc()&0X100000)?1:0;
		tape_seek(20+j); j=-1; while ((i=tape_getcccc())>0&&(j=tape_getcccc())>=0&&i!=0x61746164) tape_skip(j); // skip RIFF blocks
		if (j<=0) return tape_close(),1; // broken WAVE datas!
		tape_type=+0; tape_filebase=tape_filetell; tape_thru_cia=1; tape_wave=0;
	}
	else if (!memcmp(tape_buffer,"Compressed Square Wave\032\001",24)) // CSW v1 header
	{
		fseek(tape,0,SEEK_END); tape_filesize=ftell(tape); fseek(tape,tape_length,SEEK_SET);
		tape_filetell=tape_filebase=tape_offset=32; tape_type=+1; tape_waverate=(mgetiiii(&tape_buffer[24])>>8)-65536;
		tape_thru_cia=1; tape_wavesign=~tape_buffer[28]&1; // first signal (tape polarity) will toggle on the first read!
		tape_wave=0;
	}
	else if (!memcmp(tape_buffer,tap_magic,12)) // C64 TAP header
	{
		tape_old_type=!tape_buffer[12]; // in the old versions a zero isn't a prefix ("HAWKEYE") but a signal-less 256-tick duration
		//tape_filesize=tape_getcccc()+tape_filetell; // this field is unreliable!!!
		fseek(tape,0,SEEK_END); tape_filesize=ftell(tape); fseek(tape,tape_length,SEEK_SET);
		tape_thru_cia=1; tape_filetell=tape_filebase=tape_offset=20; tape_type=+2;
	}
	else if (tape_length>=64+tape_buffer[34]*32&&!memcmp(tape_buffer,t64_magic1,14)&&tape_buffer[33]==1&&tape_buffer[34]>=tape_buffer[36]&&tape_buffer[36])
	{
		tape_filetell=0; tape_filesize=tape_buffer[36]; // the buffer won't be used as such, so we can rewire it to our purposes
		tape_type=+3; m6510_t64ok=7;
	}
	else if (tape_length>=64&&!memcmp(tape_buffer,t64_magic2,10)&&tape_buffer[36]<2) // obsolete T64 archives made with faulty tools
	{
		fseek(tape,0,SEEK_END); int i=ftell(tape)+mgetii(&tape_buffer[66])-mgetiiii(&tape_buffer[72]); // the actual size...
		mputii(&tape_buffer[68],i); //  ...is the archive size plus the loading address minus the offset within the archive
		tape_filetell=0; tape_filesize=1; // there's just one file in these archives, we don't need to guess anything else
		tape_type=+3; m6510_t64ok=7;
	}
	else return tape_close(),1; // wrong file type!
	STRCOPY(tape_path,s);
	return /*tape_output=tape_status=*/0;
}

int tape_create(char *s) // creates a tape on path `s`; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"wb")))
		return 1; // cannot create file!
	fwrite1(tap_magic,12,tape); kputiiii(1,tape); kputiiii(0,tape); // the size field is irrelevant
	STRCOPY(tape_path,s);
	tape_filesize=tape_filetell=tape_offset=0;
	return tape_type=-1,tape_t=/*tape_output=tape_status=*/0;
}
void tape_dump(void) // send the latest sample to the recording tape
{
	if (tape_t>=256*8)
		tape_putc(0),tape_putc(tape_t),tape_putc(tape_t>>8),tape_putc(tape_t>>16),tape_t=0;
	else
		tape_putc(tape_t>>3),tape_t&=7;
}

int tape_catalog(char *t,int x) // fills the buffer `t` of size `x` with the tape contents as an ASCIZ list; returns <0 ERROR, >=0 CURRENT BLOCK
{
	if (!tape) return -1; // no tape!
	char *u=t,q=0; x=0; // this won't fill up... will it? :-/
	switch (tape_type)
	{
		case +0: // WAV
			q=tape_waveskip;
		case +1: // CSW
		case +2: // TAP
			for (int n=0,m;n<25;++n) // we can afford ignoring `x` because this list is always short
			{
				m=((tape_filesize-tape_filebase)*n/25)&~q;
				u+=1+sprintf(u,"%010d -- %02d%%",m+=tape_filebase,n*4);
				if (m<=tape_filetell) x=n;
			}
			return *u=0,x;
		case +3: // T64
			for (int n=0;n<tape_filesize;++n) // ditto, the contents of a T64 file are limited
			{
				char s[17],m,k; for (m=0;m<16;++m)
					k=tape_buffer[80+n*32+m]&127,s[m]=k<32?'?':k;
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
	tape_browsing=99; // the C64 "notices" rewinding and fast-forwarding
	switch (tape_type)
	{
		case +1: // CSW
			tape_status=i&1;
		case +0: // WAV
		case +2: // TAP
			return tape_seek(i),tape_t=0;
		case +3: // T64
			return tape_filetell=i,0;
	}
	return -1; // unknown type!
}

void tape_eofmet(void) // the tape is over, shall we close or rewind?
{
	if (tape_type==1) tape_status=0; // CSW requires some caution
	if (tape_signal||(tape_signal=-1,!tape_rewind)) tape_close();
	else tape_seek(tape_filebase); // rewind to beginning of tape
}
int tape_main(int i) // simulate tape activity for `t` ticks; returns nonzero if a new signal was detected
{
	char q=0; switch (tape_type)
	{
		case +0: // WAV
			tape_wave-=tape_waverate*i; while (tape_wave<=0)
			{
				tape_wave+=1000000;
				tape_skip(tape_waveskip); if ((i=tape_getc())<0) { tape_eofmet(); return 1; }
				if ((i=(i>128)?1:0)!=tape_status) if ((tape_status=i)^tape_wavesign==tape_polarity) q=1;
			}
			return q;
		case +1: // CSW
			tape_wave-=tape_waverate*i; while (tape_wave<=0)
				if (tape_wave+=1000000,--tape_t<=0)
				{
					if ((i=tape_getc())>0||(i=tape_getcccc())>0) tape_t+=i;
					else { tape_eofmet(); return 1; } // EOF or ZERO!
					if ((tape_status^=1)^tape_wavesign==tape_polarity) q=1;
				}
			return q;
		case +2: // TAP
			tape_t-=i; while (tape_t<=0)
			{
				q=1; if ((i=tape_getc())<0) { tape_eofmet(); return 1; }
				else if (i>0) tape_t+=i<<3; // nonzero = normal
				else if (!tape_old_type) tape_t+=tape_getccc();
				else { do tape_t+=256<<3; while (!(i=tape_getc())); tape_t+=i<<3; }
			}
			return q;
	}
	return 0; // T64, REC...
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
	while (i<16) t[i++]=' '; // pad the filename with spaces!
	return t[i]=0,tape_t64load(t);
}

// behind the CIA: disc I/O handling -------------------------------- //

char disc_path[STRMAX]="",*disc_ram[4]={NULL,NULL,NULL,NULL},disc_canwrite[4]={0,0,0,0};
FILE *disc[4]={NULL,NULL,NULL,NULL}; BYTE disc_motor[4]={0,0,0,0},disc_track[4]={0,0,0,0},disc_sector[4]={0,0,0,0};

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
	STRCOPY(disc_path,s); return 0;
}

#define DISC_R_P_M 300 // revolutions/minute
WORD disc_trackoffset[]= // offset of each track, in 256-byte sectors
{
	0, // 1st track is 1, not 0!
	  0, 21, 42, 63, 84,105,126, //  1.. 7
	562,580,147,168,189,210,231, //  8..14
	252,273,294,315,336,357,376, // 15..21
	395,414,433,452,471,490,508, // 22..28
	526,544,598,615,632,649,666, // 29..35
	683 // file ends where #36 would begin
}; // this implicitly includes the length of each track in sectors: length[x]=offset[x+1]-offset[x]
char disc_hex2gcr[16]={ 10,11,18,19,14,15,22,23, 9,25,26,27,13,29,30,21 }; // soft-hard translation
INT8 disc_gcr2hex[32]={ -1,-1,-1,-1,-1,-1,-1,-1, // all words below 9 are invalid!
	-1, 8, 0, 1,-1,12, 4, 5, -1,-1, 2, 3,-1,15, 6, 7, -1, 9,10,11,-1,13,14,-1,
}; // hard-soft translation; 0..15 is the valid range, everything else is an invalid 5-bit word!

#define DISC_SPEED_DIV 1000000 // using 1 MHz (roughly the 6502 clock) as the disc motion reference
int disc_gears,disc_speed; // the disc moves at variable speeds, resulting in different "gear" steps
int disc_gcr_header,disc_gcr_offset,disc_gcr_length; // delay before first byte, buffer offset, number of bytes
BYTE disc_gcr_buffer[512]; // GCR buffer; byte buffers are never longer than 384 bytes and can safely overlap:
BYTE *disc_ptr[4]={NULL,NULL,NULL,NULL};
#define disc_source (&disc_gcr_buffer[128]) // bit expansion writes bytes faster than it reads them :-(
#define disc_target (disc_gcr_buffer) // bit contraction writes bytes more slowly :-)
int disc_byte2gcr(BYTE *t,BYTE *s,int n) // encode `n` bytes of string `s` into the GCR bitstream `t`
{
	for (;n>0;t+=5,s+=4,n-=4) // AAAAAaaa.aaBBBBBb.bbbbCCCC.CcccccDD.DDDddddd!
	{
		BYTE x[8]={
			disc_hex2gcr[s[0]>>4],disc_hex2gcr[s[0]&15],
			disc_hex2gcr[s[1]>>4],disc_hex2gcr[s[1]&15],
			disc_hex2gcr[s[2]>>4],disc_hex2gcr[s[2]&15],
			disc_hex2gcr[s[3]>>4],disc_hex2gcr[s[3]&15]};
		t[0]=(x[0]<<3)          +(x[1]>>2); // AAAAABBB
		t[1]=(x[1]<<6)+(x[2]<<1)+(x[3]>>4); // ........:BBCCCCCD
		t[2]=(x[3]<<4)          +(x[4]>>1); // ........:........:DDDDEEEE
		t[3]=(x[4]<<7)+(x[5]<<2)+(x[6]>>3); // ........:........:........:EFFFFFGG
		t[4]=(x[6]<<5)          +(x[7]>>0); // ........:........:........:........:GGGHHHHH
	}
	return n; // 0 OK, !0 ERROR
}
int disc_seek_sector(int d) // prepare the currently active sector at drive `d`; 0 OK, !0 ERROR
{
	if (!disc[d]||!disc_motor[d]) return -1; // no disc!
	int i=disc_track[d]; if (i<1||i>=length(disc_trackoffset)-1) return -1; // bad track!
	if (disc_sector[d]>=disc_trackoffset[i+1]-disc_trackoffset[i]) return -1; // bad sector!
	disc_ptr[d]=&disc_ram[d][(disc_trackoffset[i]+disc_sector[d])<<8];
	return 0;
}
int disc_head_sector(int d) // generate the currently active sector's header
{
	if (disc_seek_sector(d)) return -1; // error!
	disc_source[0]=0X08; // HEAD ID
	disc_source[1]=(disc_source[2]=disc_sector[d])^(disc_source[3]=disc_track[d])^(disc_source[4]=disc_ram[d][0X165A2])^(disc_source[5]=disc_ram[d][0X165A3]); // EOR8 + SECTOR+TRACK+ID1+ID2
	disc_source[6]=disc_source[7]=0X0F; // HEAD END
	return disc_byte2gcr(disc_gcr_buffer,disc_source,8);
}
int disc_load_sector(int d) // read the currently active sector's contents
{
	if (disc_seek_sector(d)) return -1; // error!
	disc_source[0]=0X07; // DATA ID
	disc_source[257]=0; for (int i=0;i<256;++i) disc_source[257]^=disc_source[1+i]=disc_ptr[d][i]; // EOR8
	disc_source[258]=disc_source[259]=0; // DATA END
	memset(&disc_source[260],0,100); // FILLER, ranging from 4 to 12 between sectors within the track, and up to 100 after the last sector
	return disc_byte2gcr(disc_gcr_buffer,disc_source,360);
}
int disc_gcr2byte(BYTE *t,BYTE *s,int n) // decode `n` bytes of GCR bitstream `s` into the string `t`
{
	BYTE z=0,x[8]; for (;n>0;t+=4,s+=5,n-=5) // AAAAaaaaBB.BBbbbbCCCC.ccccDDDDdd.ddEEEEeeee!
	{
		z|=x[0]=disc_gcr2hex[            s[0]>>3     ]; // AAAAA___
		z|=x[1]=disc_gcr2hex[((s[0]<<2)+(s[1]>>6))&31]; // _____BBB:BB______
		z|=x[2]=disc_gcr2hex[           (s[1]>>1) &31]; // ........:__CCCCC_
		z|=x[3]=disc_gcr2hex[((s[1]<<4)+(s[2]>>4))&31]; // ........:_______D:DDDD____
		z|=x[4]=disc_gcr2hex[((s[2]<<1)+(s[3]>>7))&31]; // ........:........:____EEEE:E_______
		z|=x[5]=disc_gcr2hex[           (s[3]>>2) &31]; // ........:........:........:_FFFFF__
		z|=x[6]=disc_gcr2hex[((s[3]<<3)+(s[4]>>5))&31]; // ........:........:........:______GG:GGG_____
		z|=x[7]=disc_gcr2hex[            s[4]     &31]; // ........:........:........:........:___HHHHH
		t[0]=(x[0]<<4)+x[1];
		t[1]=(x[2]<<4)+x[3];
		t[2]=(x[4]<<4)+x[5];
		t[3]=(x[6]<<4)+x[7];
	}
	return n||z>=16; // 0 OK, !0 ERROR
}
int disc_save_sector(int d) // save the currently active sector's contents
{
	if (!disc_canwrite[d]) return -1; // read only!
	if (disc_seek_sector(d)) return -1; // error!
	disc_gcr2byte(disc_gcr_buffer,disc_target,360);
	memcpy(disc_ptr[d],disc_target+1,256);
	if (disc_canwrite[d]>0) disc_canwrite[d]=-disc_canwrite[d]; // tag disc as modified
	return 0;
}

// signals between the disc drive and the C1541 controller:
// bit 7: 0 = SYNC found, 1 = waiting for SYNC
// bit 6: bit rate (hi)
// bit 5: bit rate (lo)
// bit 4: write protect (1 = on)
// bit 3: drive light (1 = on)
// bit 2: drive motor (1 = on)
// bit 1: motor step (hi)
// bit 0: motor step (lo) : if ((new-old)&3)==1 go to next track else if ((new-old)&3)==3 go to prev track
BYTE c1541_peeks_disc(void)
{
	return 128+16;
}
void c1541_pokes_disc(BYTE b)
{
	;
}

// C1541: MOS 6502 MICROPROCESSOR =================================== //

BYTE c1541_mem[1<<16]; // 48K RAM + 16K ROM
#define c1541_rom &c1541_mem[48<<10]
#define VIA_TABLE_0 (&c1541_mem[0X1800])
#define VIA_TABLE_1 (&c1541_mem[0X1C00])

HLII m6502_pc; BYTE m6502_a,m6502_x,m6502_y,m6502_s,m6502_p; int m6502_irq,m6502_int; // the MOS 6502 registers
int m6502_t=0; // used to keep both clocks in time!

void m6502_sync(int t) // runs the C1541 hardware for `t` microseconds
{
	int i; m6502_t+=t;
	if (VIA_TABLE_0[14]&64)
	{
		if ((i=mgetii(&VIA_TABLE_0[4])-t)<0)
		{
			if ((VIA_TABLE_0[13]|=64)&VIA_TABLE_0[14]&64) m6502_irq|=1;
			i+=mgetii(&VIA_TABLE_0[6])+1;
		}
		mputii(&VIA_TABLE_0[4],i);
	}
	if (VIA_TABLE_1[14]&64)
	{
		if ((i=mgetii(&VIA_TABLE_1[4])-t)<0)
		{
			if ((VIA_TABLE_1[13]|=64)&VIA_TABLE_1[14]&64) m6502_irq|=1;
			i+=mgetii(&VIA_TABLE_1[6])+1;
		}
		mputii(&VIA_TABLE_1[4],i);
	}
	if (VIA_TABLE_0[15]==0X55) // *!* DUMMY, TODO
		disc_head_sector(0);
	if (VIA_TABLE_1[15]==0XAA) // *!* DUMMY, TODO
		disc_load_sector(0);
}

BYTE c64_peeks_c1541(void) // the C64 requests a byte from the C1541
{
	return ((VIA_TABLE_0[0]<<5)&128)+((VIA_TABLE_0[0]<<6)&64);
}
void c64_pokes_c1541(BYTE b) // the C64 sends a byte to the C1541
{
	if (b&8)
	{
		if ((VIA_TABLE_0[13]|=12)&VIA_TABLE_0[14]&2)
			m6502_irq|=1; // C64 $ED33 raises an IRQ on the C1541!
	}
}

BYTE m6502_recv(WORD w) // receive a byte from the C1541 address `w`
{
	if (w<0X1C00)
	{
		if (w<0X1800)
			return w>>8; // $1000-$17FF: unused!
		else
			switch (w&=15) // $1800: VIA #1
			{
				case  0: // DATA PORT B
					return (VIA_TABLE_0[0]&VIA_TABLE_0[2])+(c1541_peeks_c64()&~VIA_TABLE_0[2]);
				case  1: // DATA PORT A
				case 15: // DATA PORT A*
					return 255;
				case  4: // TIMER 1 COUNT LO-BYTE
					VIA_TABLE_0[13]&=~64; // reset Timer 1 IRQ!
					return VIA_TABLE_0[4];
				case  8: // *!* TIMER 2 LO-BYTE
					VIA_TABLE_0[13]&=~32; // reset Timer 2 IRQ!
					return VIA_TABLE_0[8];
				case 13: // INTERRUPT FLAG
					// bit 7: 1 = an interrupt happened
					// bit 6: timer 1 (used by the DOS)
					// bit 5: timer 2
					// bit 4: CB1
					// bit 3: CB2
					// bit 2: shift register
					// bit 1: CA1 (used by hard.extens.)
					// bit 0: CA2
					return VIA_TABLE_0[13]|((VIA_TABLE_0[13]&VIA_TABLE_0[14])?128:0);
				case 14: // INTERRUPT MASK
					return VIA_TABLE_0[14]|128;
				default:
				//case  2: // DATA DIRECTION B
				//case  3: // DATA DIRECTION A
				//case  5: // TIMER 1 COUNT HI-BYTE
				//case  6: // TIMER 1 LATCH LO-BYTE
				//case  7: // TIMER 1 LATCH HI-BYTE
				//case  9: // *!* TIMER 2 HI-BYTE
				//case 10: // SHIFT REGISTER
				//case 11: // AUXILIARY CONTROL
				//case 12: // PERIPHERAL CONTROL
					return VIA_TABLE_0[w];
			}
	}
	else
	{
		if (w<0X2000)
			switch (w&=15) // $1C00: VIA #2
			{
				case  0: // DATA PORT B
					return (VIA_TABLE_1[0]&VIA_TABLE_1[2])+(c1541_peeks_disc()&~VIA_TABLE_1[2]);
				case  1: // DATA PORT A
				case 15: // DATA PORT A*
					return !disc_gcr_header&&(disc_gcr_offset<disc_gcr_length)?disc_gcr_buffer[disc_gcr_offset++]:255;
				case  4: // TIMER 1 COUNT LO-BYTE
					VIA_TABLE_1[13]&=~64; // reset Timer 1 IRQ!
					return VIA_TABLE_1[4];
				case  8: // *!* TIMER 2 LO-BYTE
					VIA_TABLE_1[13]&=~32; // reset Timer 2 IRQ!
					return VIA_TABLE_1[4];
				case 13: // INTERRUPT FLAG
					if (!(VIA_TABLE_0[13]&VIA_TABLE_0[14]&127)) { m6502_irq=0; } // IRQ RES!
					return VIA_TABLE_1[13]|((VIA_TABLE_1[13]&VIA_TABLE_1[14])?128:0);
				case 14: // INTERRUPT MASK
					return VIA_TABLE_1[14]|128;
				default:
				//case  2: // DATA DIRECTION B
				//case  3: // DATA DIRECTION A
				//case  5: // TIMER 1 COUNT HI-BYTE
				//case  6: // TIMER 1 LATCH LO-BYTE
				//case  7: // TIMER 1 LATCH HI-BYTE
				//case  9: // *!* TIMER 2 HI-BYTE
				//case 10: // SHIFT REGISTER
				//case 11: // AUXILIARY CONTROL
				//case 12: // PERIPHERAL CONTROL
					return VIA_TABLE_1[w];
			}
		else
			return w>>8; // $2000-$BFFF: unused!
	}
}
void m6502_send(WORD w,BYTE b) // send the byte `b` to the C1541 address `w`
{
	if (w<0X1C00)
	{
		if (w<0X1800)
			; // $1000-$17FF: unused!
		else
			switch (w&=15) // $1800: VIA #1
			{
				case  0: // DATA PORT B
					c1541_pokes_c64(b); // update the C1541-C64 status!
					VIA_TABLE_0[0]=b; break;
				case  1: // DATA PORT A
				case 15: // DATA PORT A*
					VIA_TABLE_0[1]=b; break;
				case  2: // DATA DIRECTION B
					// *!* update the C1541-C64 status?
					VIA_TABLE_0[2]=b; break;
				case  4: // TIMER 1 COUNT LO-BYTE
				case  6: // TIMER 1 LATCH LO-BYTE
					VIA_TABLE_0[6]=b; break;
				case  5: // TIMER 1 COUNT HI-BYTE
					VIA_TABLE_0[4]=VIA_TABLE_0[6];
					VIA_TABLE_0[5]=VIA_TABLE_0[7];
					VIA_TABLE_0[13]&=~64; break;
				case  9: // TIMER 2 HI-BYTE
					VIA_TABLE_0[8]=VIA_TABLE_0[10];
					VIA_TABLE_0[9]=VIA_TABLE_0[11];
					VIA_TABLE_0[13]&=~32; break;
				case 13: // INTERRUPT FLAG
					VIA_TABLE_0[13]&=~b;
					if (!(VIA_TABLE_0[13]&VIA_TABLE_0[14]&127)&&!(VIA_TABLE_1[13]&VIA_TABLE_1[14]&127)) { m6502_irq=0; } // IRQ RES!
					break;
				case 14: // INTERRUPT MASK
					if (b&128) VIA_TABLE_0[14]|=b&127;
					else VIA_TABLE_0[14]&=~b;
					break;
				default:
				//case  3: // DATA DIRECTION A
				//case  7: // TIMER 1 LATCH HI-BYTE
				//case  8: // TIMER 2 LO-BYTE
				//case 10: // SHIFT REGISTER
				//case 11: // AUXILIARY CONTROL
				//case 12: // PERIPHERAL CONTROL
					VIA_TABLE_0[w]=b; break;
			}
	}
	else
	{
		if (w<0X2000)
			switch (w&=15) // $1C00: VIA #2
			{
				case  0: // DATA PORT B
					c1541_pokes_disc(b); // update the C1541-DISC status!
					VIA_TABLE_1[0]=b; break;
				case  1: // DATA PORT A
				case 15: // DATA PORT A*
					if (!disc_gcr_header&&(disc_gcr_offset<disc_gcr_length)) disc_gcr_buffer[disc_gcr_offset++]=b;
					VIA_TABLE_1[1]=b; break;
				case  2: // DATA DIRECTION B
					// *!* update the C1541-DISC status?
					VIA_TABLE_1[2]=b; break;
				case  4: // TIMER 1 COUNT LO-BYTE
				case  6: // TIMER 1 LATCH LO-BYTE
					VIA_TABLE_1[6]=b; break;
				case  5: // TIMER 1 COUNT HI-BYTE
					VIA_TABLE_1[4]=VIA_TABLE_1[6];
					VIA_TABLE_1[5]=VIA_TABLE_1[7];
					VIA_TABLE_1[13]&=~64; break;
				case  9: // TIMER 2 HI-BYTE
					VIA_TABLE_1[8]=VIA_TABLE_1[10];
					VIA_TABLE_1[9]=VIA_TABLE_1[11];
					VIA_TABLE_1[13]&=~32; break;
				case 13: // INTERRUPT FLAG
					VIA_TABLE_1[13]&=~b;
					if (!(VIA_TABLE_0[13]&VIA_TABLE_0[14]&127)&&!(VIA_TABLE_1[13]&VIA_TABLE_1[14]&127)) { m6502_irq=0; } // IRQ RES!
					break;
				case 14: // INTERRUPT MASK
					if (b&128) VIA_TABLE_1[14]|=b&127;
					else VIA_TABLE_1[14]&=~b;
					break;
				default:
				//case  3: // DATA DIRECTION A
				//case  7: // TIMER 1 LATCH HI-BYTE
				//case  8: // TIMER 2 LO-BYTE
				//case 10: // SHIFT REGISTER
				//case 11: // AUXILIARY CONTROL
				//case 12: // PERIPHERAL CONTROL
					VIA_TABLE_1[w]=b; break;
			}
		else
			; // $2000-$BFFF: unused!
	}
}

// the MOS 6502 memory operations: PAGE, PEEK, POKE and others
#define M65XX_LOCAL
#define M65XX_PAGE(x) ((void)0)
// 0000-07FF: RAM (2K)
// 0800-0FFF: mirror of 0000-07FF
// 1000-17FF: unused
// 1800-1BFF: VIA #1
// 1C00-1FFF: VIA #2
// 2000-BFFF: unused
// C000-FFFF: ROM (16K)

#define M65XX_SAFEPEEK(x) c1541_mem[x]
#define M65XX_PEEK(x) (x>=0XC000?c1541_mem[x]:x<0X1000?c1541_mem[x&0X07FF]:m6502_recv(x))
#define M65XX_POKE(x,o) do{ if (x<0X2000) { if (x<0X1000) c1541_mem[x&0X07FF]=o; else m6502_send(x,o); } }while(0)
#define M65XX_PEEKZERO(x) (c1541_mem[x])
#define M65XX_POKEZERO(x,o) (c1541_mem[x]=o)
#define M65XX_PULL(x) (c1541_mem[256+(x)])
#define M65XX_PUSH(x,o) (c1541_mem[256+(x)]=o)
// the C1541's MOS 6502 "dumb" operations do nothing relevant, they only consume clock ticks
#define M65XX_DUMBPAGE(x) ((void)0)
#define M65XX_DUMBPEEK(x) ((void)0)
#define M65XX_DUMBPOKE(x,o) ((void)0)
// the delays between instruction steps are always 1 T
#define M65XX_XEE (0XEE)
#define M65XX_XEF (0XEF)
#define M65XX_XVS 0 // the C1541 drive will actually toggle the OVERFLOW pin... when I get to write it in :-(
#define M65XX_WAIT (++m65xx_t)
#define M65XX_WHAT (++m65xx_t)
#define M65XX_TICK (++m65xx_t)
#define M65XX_TOCK (M65XX_INT=M65XX_IRQ)

void c1541_reset(void); // a JAM in the 6502 can reset the C1541!
#define M65XX_START (mgetii(&c1541_mem[0XFFFC]))
#define M65XX_RESET m6502_reset
#define M65XX_CRASH c1541_reset
#define M65XX_MAIN m6502_main
#define M65XX_SYNC m6502_sync
#define M65XX_IRQ_ACK ((void)0)
#define M65XX_IRQ m6502_irq
#define M65XX_INT m6502_int
#define M65XX_PC m6502_pc
#define M65XX_P m6502_p
#define M65XX_A m6502_a
#define M65XX_X m6502_x
#define M65XX_Y m6502_y
#define M65XX_S m6502_s

#include "cpcec-m6.h"

void c1541_reset(void)
{
	m6502_reset();
	memset(VIA_TABLE_0,0,16); // VIA #1
	memset(VIA_TABLE_1,0,16); // VIA #2
	m6502_irq=disc_gcr_header=disc_gcr_offset=disc_gcr_length=0;
}

// CPU-HARDWARE-VIDEO-AUDIO INTERFACE =============================== //

#define SID_TICK_STEP 16
#define SID_MAIN_EXTRABITS 0
#define SID_MAX_VOICE 10922 // 11585 // 13376 // the filter can be noisy with 16383 =32767 /2 channels
#define SID_MUTE_TIME main_t
#ifdef DEBUG
BYTE sid_quiet[3]={0,0,0}; // optional muting of channels
#endif
int sid_extras=0; // no extra chips by default
#include "cpcec-m8.h"

#include "cpcec-ym.h"
int ym3_write_aux(int i)
	{ int j=16,k=SID_MAX_VOICE; for (;;) if (!--j||(i>=(k=(k*181)>>8))) return j; } // rough approximation
void ym3_write(void)
{
	int i,n=511,x=0X38; // we must adjust YM values to a 2 MHz clock approx.
	i=SID_TABLE_0[ 0]+SID_TABLE_0[ 1]*256; if (sid_tone_shape[0][0]&16) i=0,x|=1; else if (sid_tone_shape[0][0]&8) n&=(i>>5)&31,i=0,x&=~ 8;
	i=i>512?(512*4096+i/2)/i:0; // convert channel 1 wavelength on the fly
	ym3_tmp[ym3_count++]=i; ym3_tmp[ym3_count++]=i>>8;
	i=SID_TABLE_0[ 7]+SID_TABLE_0[ 8]*256; if (sid_tone_shape[0][1]&16) i=0,x|=2; else if (sid_tone_shape[0][1]&8) n&=(i>>5)&31,i=0,x&=~16;
	i=i>512?(512*4096+i/2)/i:0; // ditto, channel 2 wavelength
	ym3_tmp[ym3_count++]=i; ym3_tmp[ym3_count++]=i>>8;
	i=SID_TABLE_0[14]+SID_TABLE_0[15]*256; if (sid_tone_shape[0][2]&16) i=0,x|=4; else if (sid_tone_shape[0][2]&8) n&=(i>>5)&31,i=0,x&=~32;
	i=i>512?(512*4096+i/2)/i:0; // ditto, channel 3 wavelength
	ym3_tmp[ym3_count++]=i; ym3_tmp[ym3_count++]=i>>8;
	ym3_tmp[ym3_count++]=n; // noise wavelength
	ym3_tmp[ym3_count++]=x+(sid_tone_shape[0][0]&16?1:0)+(sid_tone_shape[0][1]&16?2:0)+(sid_tone_shape[0][2]&16?4:0); // mixer
	ym3_tmp[ym3_count++]=ym3_write_aux(sid_tone_power[0][0]); // convert channel 1 amplitude on the fly
	ym3_tmp[ym3_count++]=ym3_write_aux(sid_tone_power[0][1]); // ditto, channel 2 amplitude
	ym3_tmp[ym3_count++]=ym3_write_aux(sid_tone_power[0][2]); // ditto, channel 3 amplitude
	ym3_tmp[ym3_count++]=0; ym3_tmp[ym3_count++]=0; // hard envelope wavelength
	ym3_tmp[ym3_count++]=0; // hard envelope type
	//psg_hard_log=0xFF; // 0xFF means the hard envelope doesn't change
}

int /*tape_loud=1,*/tape_song=0;
//void audio_main(int t) { sid_main(t/*,((tape_status^tape_output)&tape_loud)<<12*/); }
#define audio_main sid_main // (the SID chips are the only audio generators on the C64)

void audio_sync(void) // force audio output on demand, to avoid generating old samples with new data!
{ if (/*audio_dirty&&*/audio_required&&audio_queue) audio_main(audio_queue),/*audio_dirty=*/audio_queue=0; }

// generic ASCII printer logic
FILE *printer=NULL; int printer_p=0; BYTE printer_z,printer_t[256];
void printer_flush(void) { fwrite1(printer_t,printer_p,printer),printer_p=0; }
void printer_close(void) { printer_flush(),fclose(printer),printer=NULL; }
#define printer_send8(b) do{ if (printer_t[printer_p]=(b),++printer_p>=sizeof(printer_t)) printer_flush(); }while(0)

WORD vicii_hits[512]; // 96+320+96 pixels (although we only show 32+320+32); +1..+128 means impact with sprite #0..#7, +256 means impact with a pixel in the scenery
WORD vicii_copy_hits; // the old value may stick for a short while (DL/Discrete Logic versus IC/Integrated Circuit)
int vicii_cursor=0,vicii_backup=0; BYTE vicii_eighth=0; // 0..1023, 0..1023 and 0..7 respectively while drawing the bitmap
BYTE vicii_cache1[40],vicii_cache2[40]; // the 40 ATTRIB/COLOUR pairs that are read once every badline; the COLOUR part is required by demos such as "REUTASTIC"
BYTE vicii_horizon=0,vicii_port_22=0; // bits 2-0 and 3 of register 22
BYTE vicii_sprite_x; // the double-height flip-flop table
BYTE vicii_sprite_z; // toggling the sprite height can delay sprite reloading
BYTE vicii_cow=0; // "fetchez la vache!"
int vicii_sprite_min=-24,vicii_sprite_max=320+8;

void vicii_blit_sprite(VIDEO_UNIT *t,char i,unsigned int k) // draw a sprite on screen; `t` is the video buffer (NULL if only the collision matters), `i` is the sprite ID and `k` is its current 24-bit data
{
	vicii_sprite_k[i]=0; // don't draw the same sprite twice!
	BYTE j=1<<i; int x=VICII_TABLE[0+i*2];
	if (VICII_TABLE[16]&j) if ((x+=256)>=384) { if ((x-=vicii_len_x*8)>=0) return; } // PAL wraps at $01F8!
	if ((x-=24)>=vicii_sprite_max) return; // too far to the right? (cfr. the bouncing circle of letters of "4KRAWALL")
	WORD *h=&vicii_hits[96+x]; char a; // collision bit pointer and buffers
	if (t) // draw sprite and update collision bits
	{
		t+=x*2; if (UNLIKELY(VICII_TABLE[29]&j)) // double?
		{
			if (x+24<=vicii_sprite_min) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=48-4,t+=96-8;
				VIDEO_UNIT p,pp[4]={0,video_clut[21],video_clut[23+i],video_clut[22]};
				if (VICII_TABLE[27]&j) // back?
					do if (a=k&3)
					{
						p=pp[a];
						if (!h[0]) t[0]=t[1]=p;
						if (!h[1]) t[2]=t[3]=p;
						if (!h[2]) t[4]=t[5]=p;
						if (!h[3]) t[6]=t[7]=p;
						x|=h[0]|=j;
						x|=h[1]|=j;
						x|=h[2]|=j;
						x|=h[3]|=j;
					}
					while (h-=4,t-=8,k>>=2);
				else // fore!
					do if (a=k&3)
					{
						p=pp[a];
						if (!(BYTE)h[0]) t[0]=t[1]=p;
						if (!(BYTE)h[1]) t[2]=t[3]=p;
						if (!(BYTE)h[2]) t[4]=t[5]=p;
						if (!(BYTE)h[3]) t[6]=t[7]=p;
						x|=h[0]|=j;
						x|=h[1]|=j;
						x|=h[2]|=j;
						x|=h[3]|=j;
					}
					while (h-=4,t-=8,k>>=2);
			}
			else // HI-RES
			{
				h+=48-2,t+=96-4;
				VIDEO_UNIT p=video_clut[23+i];
				if (VICII_TABLE[27]&j) // back?
					do if (k&1)
					{
						if (!h[0]) t[0]=t[1]=p;
						if (!h[1]) t[2]=t[3]=p;
						x|=h[0]|=j;
						x|=h[1]|=j;
					}
					while (h-=2,t-=4,k>>=1);
				else // fore!
					do if (k&1)
					{
						if (!(BYTE)h[0]) t[0]=t[1]=p;
						if (!(BYTE)h[1]) t[2]=t[3]=p;
						x|=h[0]|=j;
						x|=h[1]|=j;
					}
					while (h-=2,t-=4,k>>=1);
			}
		}
		else // single!
		{
			if (x<=vicii_sprite_min) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=24-2,t+=48-4;
				VIDEO_UNIT p,pp[4]={0,video_clut[21],video_clut[23+i],video_clut[22]};
				if (VICII_TABLE[27]&j) // back?
					do if (a=k&3)
					{
						p=pp[a];
						if (!h[0]) t[0]=t[1]=p;
						if (!h[1]) t[2]=t[3]=p;
						x|=h[0]|=j;
						x|=h[1]|=j;
					}
					while (h-=2,t-=4,k>>=2);
				else // fore!
					do if (a=k&3)
					{
						p=pp[a];
						if (!(BYTE)h[0]) t[0]=t[1]=p;
						if (!(BYTE)h[1]) t[2]=t[3]=p;
						x|=h[0]|=j;
						x|=h[1]|=j;
					}
					while (h-=2,t-=4,k>>=2);
			}
			else // HI-RES
			{
				h+=24-1,t+=48-2;
				VIDEO_UNIT p=video_clut[23+i];
				if (VICII_TABLE[27]&j) // back?
					do if (k&1)
					{
						if (!h[0]) t[0]=t[1]=p;
						x|=h[0]|=j;
					}
					while (h-=1,t-=2,k>>=1);
				else // fore!
					do if (k&1)
					{
						if (!(BYTE)h[0]) t[0]=t[1]=p;
						x|=h[0]|=j;
					}
					while (h-=1,t-=2,k>>=1);
			}
		}
	}
	else // don't draw, just plot the collision bits: the back-fore bit is useless here
	{
		if (UNLIKELY(VICII_TABLE[29]&j)) // double?
		{
			if (x+24<=vicii_sprite_min) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=48-4;
				do if (k&3)
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
				do if (k&1)
				{
					x|=h[0]|=j;
					x|=h[1]|=j;
				}
				while (h-=2,k>>=1);
			}
		}
		else // single!
		{
			if (x<=vicii_sprite_min) return; // too far to the left!
			x=0; if (VICII_TABLE[28]&j) // LO-RES?
			{
				h+=24-2;
				do if (k&3)
				{
					x|=h[0]|=j;
					x|=h[1]|=j;
				}
				while (h-=2,k>>=2);
			}
			else // HI-RES
			{
				h+=24-1;
				do if (k&1)
				{
					x|=h[0]|=j;
				}
				while (h-=1,k>>=1);
			}
		}
	}
	if (!vicii_noimpacts) // no sprite hits? the intro of DKONGJR suggests that these interrupts cannot "retrigger"!
	{
		if (x&256) { { if (!VICII_TABLE[31]) VICII_TABLE[25]|=2; } VICII_TABLE[31]|=j; } // set flags if it hits the background...
		if ((x&=255)&&x!=j) { { if (!VICII_TABLE[30]) VICII_TABLE[25]|=4; } VICII_TABLE[30]|=x; } // or a sprite (but not itself!)
	}
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
						BYTE b=vicii_bitmap[vicii_cache1[x]*8+vicii_eighth];
						VIDEO_UNIT p0=video_clut[vicii_cache2[x]];
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
						BYTE b=vicii_bitmap[vicii_cache1[x]*8+vicii_eighth],c=vicii_cache2[x];
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
						BYTE a=vicii_cache1[x],b=vicii_bitmap[(y++&1023)*8+vicii_eighth];
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
						BYTE a=vicii_cache1[x],b=vicii_bitmap[(y++&1023)*8+vicii_eighth];
						VIDEO_UNIT q[4]={video_clut[17],video_clut[a>>4],video_clut[a&15],video_clut[vicii_cache2[x]]};
						*t++=p=q[ b>>6   ]; *t++=p; *t++=p; *t++=p;
						*t++=p=q[(b>>4)&3]; *t++=p; *t++=p; *t++=p;
						*t++=p=q[(b>>2)&3]; *t++=p; *t++=p; *t++=p;
						*t++=p=q[(b   )&3]; *t++=p; *t++=p; *t++=p;
					}
					break;
				case 4: // HI-RES EXTENDED
					for (BYTE x=0;x<40;++x)
					{
						BYTE a=vicii_cache1[x],b=vicii_bitmap[(a&63)*8+vicii_eighth];
						VIDEO_UNIT q0=video_clut[(a>>6)+17],q1=video_clut[vicii_cache2[x]];
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
		}
		else
		{
			#if 1
			memset(vicii_hits,0,sizeof(vicii_hits));
			#else
			memset(vicii_hits,0,sizeof(vicii_hits)/4);
			memset(&vicii_hits[length(vicii_hits)*3/4],0,sizeof(vicii_hits)/4);
			#endif
			WORD *h=&vicii_hits[(512-320)/2+vicii_horizon];
			switch (vicii_mode) // render the background pixels and the collision bitmap in a single go!
			{
				case 0: // HI-RES CHARACTER
					for (BYTE x=0;x<40;++x,h+=8)
					{
						BYTE b=vicii_bitmap[vicii_cache1[x]*8+vicii_eighth];
						VIDEO_UNIT p0=video_clut[vicii_cache2[x]];
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
						BYTE b=vicii_bitmap[vicii_cache1[x]*8+vicii_eighth],c=vicii_cache2[x];
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
						BYTE a=vicii_cache1[x],b=vicii_bitmap[(y++&1023)*8+vicii_eighth];
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
						BYTE a=vicii_cache1[x],b=vicii_bitmap[(y++&1023)*8+vicii_eighth];
						VIDEO_UNIT q[4]={video_clut[17],video_clut[a>>4],video_clut[a&15],video_clut[vicii_cache2[x]]};
						h[0]=h[1]=(b&128)<<1; *t++=p=q[ b>>6   ]; *t++=p; *t++=p; *t++=p;
						h[2]=h[3]=(b& 32)<<3; *t++=p=q[(b>>4)&3]; *t++=p; *t++=p; *t++=p;
						h[4]=h[5]=(b&  8)<<5; *t++=p=q[(b>>2)&3]; *t++=p; *t++=p; *t++=p;
						h[6]=h[7]=(b&  2)<<7; *t++=p=q[(b   )&3]; *t++=p; *t++=p; *t++=p;
					}
					break;
				case 4: // HI-RES EXTENDED
					for (BYTE x=0;x<40;++x,h+=8)
					{
						BYTE a=vicii_cache1[x],b=vicii_bitmap[(a&63)*8+vicii_eighth];
						VIDEO_UNIT q0=video_clut[(a>>6)+17],q1=video_clut[vicii_cache2[x]];
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
			vicii_sprite_min=vicii_copy_border_l?-24:-24-(VIDEO_PIXELS_X-640)/4;
			vicii_sprite_max=vicii_copy_border_l?320:320+(VIDEO_PIXELS_X-640)/4;
			t=u+(VIDEO_PIXELS_X/2)-320; { int k; for (BYTE i=0;i<8;++i) if (k=vicii_sprite_k[i]) vicii_blit_sprite(t,i,k); }
		}
	}
	else if ((vicii_nosprites|vicii_noimpacts)<vicii_cow) // wrong scanline or frame, do the collission bitmap and nothing else
	{
		#if 1
		memset(vicii_hits,0,sizeof(vicii_hits));
		#else
		memset(vicii_hits,0,sizeof(vicii_hits)/4);
		memset(&vicii_hits[length(vicii_hits)*3/4],0,sizeof(vicii_hits)/4);
		#endif
		WORD *h=&vicii_hits[(512-320)/2+vicii_horizon],y=vicii_cursor;
		switch (vicii_mode) // render the background's collision bitmap in a single go!
		{
			case 0: // HI-RES CHARACTER
				for (BYTE x=0;x<40;++x,h+=8)
				{
					BYTE b=vicii_bitmap[vicii_cache1[x]*8+vicii_eighth];
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
					BYTE b=vicii_bitmap[vicii_cache1[x]*8+vicii_eighth];
					if (vicii_cache2[x]>=8) // LO-RES?
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
					BYTE b=vicii_bitmap[(vicii_cache1[x]&63)*8+vicii_eighth];
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
		vicii_sprite_min=vicii_copy_border_l?-24:-24-32;
		vicii_sprite_max=vicii_copy_border_l?320:320+32;
		{ int k; for (BYTE i=0;i<8;++i) if (k=vicii_sprite_k[i]) vicii_blit_sprite(NULL,i,k); }
	}
}
void vicii_draw_border(void) // render a single line of the border, again relying on extant datas
{
	if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
	{
		VIDEO_UNIT p,*t=video_target-video_pos_x+VIDEO_OFFSET_X;
		if (UNLIKELY(vicii_lastmode&32)) // VIC-II off or drawing the top/bottom border?
			for (int i=VIDEO_OFFSET_X>>4;p=video_clut[vicii_copy_border[i]],i<((VIDEO_OFFSET_X+VIDEO_PIXELS_X)>>4);++i)
				for (char x=16;x;--x) *t++=p;
		else // render the borders
		{
			int i,j,k; if (vicii_port_22&8)
				k=j=(VIDEO_PIXELS_X-640)/2;
			else
				k=4+(j=(VIDEO_PIXELS_X-640)/2+14); // left border 7 px, right border 9 px
			if (vicii_copy_border_l)
			{
				VIDEO_UNIT *u=t; i=VIDEO_OFFSET_X>>4;
				while (j>16)
				{
					p=video_clut[vicii_copy_border[i]];
					for (int n=0;n<16;++n) *u++=p;
					++i; j-=16;
				}
				for (p=video_clut[vicii_copy_border[i]];j--;) *u++=p;
			}
			if (vicii_copy_border_r)
			{
				t+=VIDEO_PIXELS_X-1; i=((VIDEO_OFFSET_X+VIDEO_PIXELS_X)>>4)-1;
				while (k>16)
				{
					p=video_clut[vicii_copy_border[i]];
					for (int n=0;n<16;++n) *t--=p;
					--i; k-=16;
				}
				for (p=video_clut[vicii_copy_border[i]];k--;) *t--=p;
			}
		}
	}
}

//void video_main(int t) // the VIC-II updates the video on its own!

// CPU: MOS 6510 MICROPROCESSOR ===================================== //

// interrupt queue operations; notice that the 6510 checks m6510_int first and m6510_irq later
#define M6510_VIC_SET (m6510_irq|=+  1)
#define M6510_CIA_SET (m6510_irq|=+  2)
#define M6510_NMI_SET (m6510_irq|=+128)
#define M6510_VIC_RES (m6510_irq&=~  1)
#define M6510_CIA_RES (m6510_irq&=~  2)
#define M6510_NMI_RES (m6510_irq&=~128)

// notice that nonzero "tape_thru_cia" and "tape_type<0" imply a valid tape
#define M6510_62500HZ_H do{ if (!tape_disabled) { if (LIKELY(tape_thru_cia)) m6510_62500hz_playtape(); else if (tape_type<0) m6510_62500hz_savetape(); } \
	else if (!disc_disabled) m6510_62500hz_disc(); }while(0) // in theory the disc should coexist with the tape, i.e. no "else"; in practice, however...
#if 0 // update twice / every 8 T
#define M6510_62500HZ_T 8
#define M6510_62500HZ_L M6510_62500HZ_H
#else // update once / every 16 T
#define M6510_62500HZ_T 16
#define M6510_62500HZ_L ((void)0)
#endif
void m6510_62500hz_playtape(void) // update TAPE-to-CIA logic
{
	if (tape_main(M6510_62500HZ_T))
		if (cia_port_13[0]|=16,(CIA_TABLE_0[13]&16)&&cia_port_13[0]<128)
			{ cia_port_13[0]+=128; M6510_CIA_SET; }
}
void m6510_62500hz_savetape(void) // update CIA-to-TAPE logic
	{ tape_t+=M6510_62500HZ_T; char o=tape_output; if ((tape_output=mmu_cfg[1]&8)>o) tape_dump(); }
void m6510_62500hz_disc(void) // update the C1541 disc drive
	{ int t=M6510_62500HZ_T-m6502_t; m6502_t-=M6510_62500HZ_T; if (t>0) m6502_main(t); }
#if 0
void m6510_62500hz_cia0(void) // update the CIA #1 serial shift register (not sure if ever used for timing in any real-world title)
{
	if (cia_serialz[0])
		if (--cia_serialz[0],cia_serials[0]=16,cia_port_13[0]|=8,(CIA_TABLE_0[13]&8)&&cia_port_13[0]<128)
			{ cia_port_13[0]+=128; M6510_CIA_SET; }
}
#else
#define m6510_62500hz_cia0() ((void)0) // the original tape of "Arkanoid" polls (without running) the shift register after loading.
#endif
void m6510_62500hz_cia1(void) // update the CIA #2 serial shift register, famously used by Allan Shortt's "Athena" and "Mario Bros"
{
	if (cia_serialz[1])
		if (--cia_serialz[1],cia_serials[1]=16,cia_port_13[1]|=8,(CIA_TABLE_1[13]&8)&&cia_port_13[1]<128)
			{ cia_port_13[1]+=128; M6510_NMI_SET; }
}

#define M6510_LOAD_SPRITE(i) do{ z=vicii_attrib[1016+i]*64+vicii_sprite_y[i]; vicii_sprite_k[i]=vicii_memory[z+2]+(vicii_memory[z+1]<<8)+(vicii_memory[z]<<16); }while(0)

int m6510_tick(int q) // handles the essential events that happen on each tick, according to the threshold `q`; returns the resulting timing (1 if no delays happen, >1 otherwise)
{
	for (int t=0;;)
	{
		vicii_copy_border[(BYTE)(video_pos_x>>4)]=VICII_TABLE[32];
		video_pos_x+=16,video_target+=16; // just for debugging purposes, we actually draw the scanline elsewhere
		if (UNLIKELY(++vicii_pos_x>=vicii_irq_x)) // update the VIC-II clock
			vicii_pos_x=-8; // the "logical" ticker wraps before the "physical" one; this makes NTSC and PAL effectively equivalent, as their differences lay after the canvas and before the sprites
		switch (vicii_pos_x)
		{
			int z; // a single temporary variable will do
			case -8: // equivalent to 55 on PAL, and to 56 or 57 on NTSC depending on the horizontal timing (64 or 65 T)
				vicii_cow=0; for (BYTE i=8,j=128;i--;j>>=1)
				{
					vicii_sprite_k[i]=0; // wipe scanline bits
					vicii_sprite_x^=VICII_TABLE[23]&j; // toggle height step
					if ((vicii_pos_y&255)==VICII_TABLE[1+i*2]) // "GOTHIK" ($9703) and "SPRITE INVADERS" want sprites not to reload!
						if ((VICII_TABLE[21]&j)&&vicii_sprite_y[i]>=63) // enabled sprite, disabled DMA?
						{
							vicii_sprite_x=(VICII_TABLE[23]&j)+(vicii_sprite_x&~j); // set height!
							if (vicii_pos_y<256||!(vicii_sprite_z&j)) vicii_sprite_y[i]=0; // reload if allowed!
						}
					if (vicii_sprite_y[i]<63)
						vicii_cow|=j; // tag sprite for fetching!
				}
				vicii_sprite_z=0;
				vicii_takeover=vicii_cow&  1;
				// no `break`!
			case +8: case 24: case 40:
				M6510_62500HZ_L; // evenly distributed (hi-res)
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
				vicii_draw_border();
				break;
			case +0:
				if (vicii_cow& 16) vicii_takeover=1;
				if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y&&frame_pos_y==video_pos_y) video_drawscanline(); // scanline is complete!
				video_nextscanline(0); // scanline event!
				audio_queue+=vicii_len_x>>multi_t; // update audio; even the sharpest samples eat more than one scanline per signal
				M6510_62500HZ_H; // evenly distributed
				break;
			case +1:
				if (vicii_cow&  4) { M6510_LOAD_SPRITE(2); if (!(vicii_cow& 24)) vicii_takeover=0; }
				if (++vicii_pos_y<vicii_len_y)
				{
					if (vicii_pos_y==vicii_irq_y) // the RASTER IRQ doesn't happen here on line 0!
						VICII_TABLE[25]|=1;
					if ((VICII_TABLE[25]&VICII_TABLE[26]&7)&&VICII_TABLE[25]<128) // kludge: check both RASTER (+1) and COLLISION (+2+4) here
						{ VICII_TABLE[25]+=128; M6510_VIC_SET; }
					if (vicii_pos_y==48)
						vicii_ready=VICII_TABLE[17]&16; // DEN bit: allow badlines (and enable bitmap)
					if (vicii_badline=vicii_pos_y>=48&&vicii_pos_y<248&&vicii_ready&&!((vicii_pos_y-VICII_TABLE[17])&7))
						vicii_mode&=~16; // we're in a badline, thus we must NOT display the background!
				}
				else
					vicii_frame=--vicii_pos_y; // line 311 must last one extra tick, but the CPU must never see "312" here!
				break;
			case +2:
				if (vicii_frame) // frame flyback?
				{
					//if (video_pos_y>=VIDEO_LENGTH_Y)
					//{
						#define VICII_POS_Y_ADD (-1) // the physical VSYNC when this line ends, hence this value
						video_newscanlines(video_pos_x,VICII_POS_Y_ADD*2); // end of frame!
					//}
					// handle decimals, seconds, minutes, hours and alarm in a single block;
					// bit 30 keeps the clock from updating (f.e. while setting the time)
					if (--vicii_n_cia<=0)
					{
						vicii_n_cia=vicii_len_y<288?6:5; // i.e. NTSC versus PAL
						if (!cia_port_11[0]) if (cias_24hours(0)) { M6510_CIA_SET; }
						if (!cia_port_11[1]) if (cias_24hours(1)) { M6510_NMI_SET; }
					}
					// reset VIC-II in-frame counters
					vicii_pos_y=vicii_backup=vicii_frame=0;
					if (!vicii_irq_y) // the RASTER IRQ happens 1 T later on line 0!
						if (((VICII_TABLE[25]|=1)&VICII_TABLE[26]&7)&&VICII_TABLE[25]<128)
							{ VICII_TABLE[25]+=128; M6510_VIC_SET; }
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
					if (vicii_sprite_y[i]<63) // sprite DMA going on?
						if (vicii_crunch&j) // sprite crunch?
						{
							//int k=vicii_sprite_y[i]; // cfr. https://www.linusakesson.net/scene/lunatico/misc.php :
							//vicii_sprite_y[i]=(((k+3)|k)&0X15)+(((k+3)&k)&0X2A); // "Massively Interleaved Sprite Crunch"
							const char k[64]= // precalc'd!
							{1, 5, 5, 7, 5, 5, 5, 7, 9,13,13,15,13,21,21,23,17,21,21,23,21,21,21,23,25,29,29,31,29,21,21,23,
							33,37,37,39,37,37,37,39,41,45,45,47,45,53,53,55,49,53,53,55,53,53,53,55,57,61,61,63,61,21,21,23};
							vicii_sprite_y[i]=k[vicii_sprite_y[i]];
						}
						else if (!(vicii_sprite_x&j)) // next line?
							vicii_sprite_y[i]=(vicii_sprite_y[i]+3)&63;
				vicii_crunch=0; vicii_copy_border_l=vicii_mode&8;
				vicii_mode&=~8; // here the left border ends and the bitmap begins, even if the 38-char mode hides the first char
				if (VICII_TABLE[17]&8)
					{ if (vicii_pos_y==51&&vicii_ready) vicii_mode&=~32; else if (vicii_pos_y==251) vicii_mode|=+32; }
				else
					{ if (vicii_pos_y==55&&vicii_ready) vicii_mode&=~32; else if (vicii_pos_y==247) vicii_mode|=+32; }
				// no `break`!
			case 48:
				M6510_62500HZ_H; // evenly distributed
				break;
			case 18: // the VIC-II 6569/8565 barrier is 18/19
				vicii_horizon=VICII_TABLE[22]&7; // the canvas needs this right now!
				vicii_port_22=VICII_TABLE[22]; // the borders will need this later
				vicii_copy_hits=VICII_TABLE[31]*256+VICII_TABLE[30]; // custom IC
				if (vicii_badline) // the final part of "ONEDER-OXYRON" implies that this must happen after 16. 17 is enough, but is it right? 18 perhaps?
				{
					//vicii_mode&=~16; // *!* redundant!?
					z=vicii_cursor; for (BYTE x=0;x<40;++x) vicii_cache1[x]=vicii_attrib[z&=1023],vicii_cache2[x]=VICII_COLOR[z++];
					if (vicii_takeover>1) vicii_cache1[0]=vicii_cache1[1]=vicii_cache1[2]=255; // the "light grey" bug, see below
				}
				break;
			case 18+11: // empirical middle point: 18+10 is the minimum value where "DISCONNECT" works, but 18+13 breaks "BOX CHECK TEST"
				if (!vic_nouveau)
					{ if ((vicii_lastmode=vicii_mode)&32) {} else vicii_draw_canvas(); if (!(vicii_mode&16)) vicii_cursor+=40; }
				break;
			case 18+14: // empirical middle point: the cylinder of "REWIND-TEMPEST" needs 18+11..18+18, but "BOX CHECK TEST" breaks at 18+17
				if (vic_nouveau)
					{ if ((vicii_lastmode=vicii_mode)&32) {} else vicii_draw_canvas(); if (!(vicii_mode&16)) vicii_cursor+=40; }
			//case 32:
				M6510_62500HZ_H; // evenly distributed
				break;
		}
		sid_mute_tick(); // the SID LFSR! neither sooner or later!
		// handle pending CIA events before updating the counters;
		// remember the state bits: +8 LOAD, +4 WAIT, +2 MOVE, +1 STOP
		#if 0 // better on my desktop than on my laptop: relative states
		#define CIA_DO_STATE(xxx,yyy,zzz,aaa) \
			case +8+4+2+0: if (xxx==1) goto aaa; case +8+0+0+1: case +8+0+2+0: yyy-=+8; xxx=mgetii(&zzz); break; \
			case +0+4+2+0: yyy=+0+0+2+0; break; case +0+0+2+1: yyy=+0+0+0+1; case +0+0+2+0:
		#else // better on my laptop than on my desktop: absolute states
		#define CIA_DO_STATE(xxx,yyy,zzz,aaa) \
			case +0+4+2+0: yyy=+0+0+2+0; break; \
			case +8+0+0+1: yyy=+0+0+0+1; xxx=mgetii(&zzz); break; \
			case +8+0+2+0: yyy=+0+0+2+0; xxx=mgetii(&zzz); break; \
			case +8+4+2+0: if (UNLIKELY(xxx==1)) goto aaa; yyy=+0+4+2+0; xxx=mgetii(&zzz); break; \
			case +0+0+2+1: yyy=+0+0+0+1; case +0+0+2+0:
		#endif
		// `goto` is a terrible reserved word, but it simplifies the work here :-(
		#define CIA_DO_CNT_A(xxx,yyy,zzz,aaa,bbb) \
			if (cia_minor_a[zzz]&&UNLIKELY(--cia_count_a[zzz]<=0)) \
				{ if (LIKELY(cia_state_a[zzz]!=+0+0+0+1)) { \
					if (!(--cia_serials[zzz])) bbb; \
					aaa: cia_count_a[zzz]=mgetii(&yyy[4]); \
					if (((cia_port_13[zzz]|=1)&yyy[13]&1)&&cia_port_13[zzz]<128) \
						{ cia_port_13[zzz]+=128; xxx; } \
					cia_state_a[zzz]=(yyy[14]&8)?yyy[14]&=~1,cia_event_a[zzz]&=~1,+8+0+0+1:+8+0+2+0; \
				} cia_carry=cia_major_b[zzz]; }
		#define CIA_DO_CNT_B(xxx,yyy,zzz,aaa) \
			if ((cia_minor_b[zzz]|cia_carry)&&UNLIKELY(--cia_count_b[zzz]<=0)) \
				{ if (LIKELY(cia_state_b[zzz]!=+0+0+0+1)) { \
					aaa: cia_count_b[zzz]=mgetii(&yyy[6]); \
					if (((cia_port_13[zzz]|=2)&yyy[13]&2)&&cia_port_13[zzz]<128) \
						{ cia_port_13[zzz]+=128; xxx; } \
					cia_state_b[zzz]=(yyy[15]&8)?yyy[15]&=~1,cia_event_b[zzz]&=~1,+8+0+0+1:+8+0+2+0; \
				} }
		// the macros thus cover both halves of the CIA logic: the state machine and the control listener
		#define CIA_DO_EVENT(xxx,yyy,zzz) \
			if (UNLIKELY(xxx)) switch (yyy) { \
				case +0+0+0+1: case +8+0+0+1: \
					if (xxx&1) yyy=xxx&16?+8+4+2+0:+0+4+2+0; \
					else if (xxx&16) yyy=+8+0+0+1; \
					zzz=xxx&~16; xxx=0; break; \
				case +0+0+2+0: \
					if (xxx&1) { if (xxx&16) yyy=8+4+2+0; } \
					else yyy=xxx&16?+8+0+0+1:+0+0+2+1; \
					zzz=xxx&~16; xxx=0; break; \
				case +8+0+2+0: case +0+4+2+0: \
					if (xxx&1) { \
						if (xxx&8) --xxx,yyy=+0+0+0+1; \
						else if (xxx&16) yyy=+8+4+2+0; \
					} else yyy=+0+0+0+1; \
					zzz=xxx&~16; xxx=0; }
		int cia_carry=0; switch (cia_state_a[0])
		{
			CIA_DO_STATE(cia_count_a[0],cia_state_a[0],CIA_TABLE_0[4],cia_goto0_a);
			CIA_DO_CNT_A(M6510_CIA_SET,CIA_TABLE_0,0,cia_goto0_a,m6510_62500hz_cia0());
		}
		CIA_DO_EVENT(cia_event_a[0],cia_state_a[0],CIA_TABLE_0[14]);
		switch (cia_state_b[0])
		{
			CIA_DO_STATE(cia_count_b[0],cia_state_b[0],CIA_TABLE_0[6],cia_goto0_b);
			CIA_DO_CNT_B(M6510_CIA_SET,CIA_TABLE_0,0,cia_goto0_b);
		}
		CIA_DO_EVENT(cia_event_b[0],cia_state_b[0],CIA_TABLE_0[15]);
		cia_carry=0; switch (cia_state_a[1])
		{
			CIA_DO_STATE(cia_count_a[1],cia_state_a[1],CIA_TABLE_1[4],cia_goto1_a);
			CIA_DO_CNT_A(M6510_NMI_SET,CIA_TABLE_1,1,cia_goto1_a,m6510_62500hz_cia1());
		}
		CIA_DO_EVENT(cia_event_a[1],cia_state_a[1],CIA_TABLE_1[14]);
		switch (cia_state_b[1])
		{
			CIA_DO_STATE(cia_count_b[1],cia_state_b[1],CIA_TABLE_1[6],cia_goto1_b);
			CIA_DO_CNT_B(M6510_NMI_SET,CIA_TABLE_1,1,cia_goto1_b);
		}
		CIA_DO_EVENT(cia_event_b[1],cia_state_b[1],CIA_TABLE_1[15]);
		// freeze the CPU if required, return otherwise
		if (++t,vicii_takeover<=q) return main_t+=t,t;
	}
}

/*void m6510_sync(int t) // handles the hardware events that can gather multiple clock ticks, set here as `t`
{
	static int r=0; //main_t+=t;
	t=(r+=t)>>multi_t; r&=multi_u; // calculate base value of `t` and keep remainder
	if (t>0)
	{
		... // devices with their own clocks
	}
	... // devices sharing clock with the M6510 even when it's sped-up
}*/

// the C64 special I/O addresses are divided in several spaces:
// 0000-0001 : memory configuration
// 0002-00FF : a window to RAM (PAGEZERO)
// D000-D3FF : video configuration; mask is 003F
// D400-D7FF : audio configuration; mask is 001F
// D800-DBFF : color configuration
// DC00-DCFF : CIA #1 configuration; mask is 000F
// DD00-DDFF : CIA #2 configuration; mask is 000F
// DE00-DFFF : hardware expansions
// remember: 0100-CFFF + E000-FFFF get filtered out in advance!

BYTE m6510_recv(WORD w) // receive a byte from the I/O address `w`
{
	if (w<0XD800)
		if (w<0XD000)
			if (w<0X0002) // memory configuration, $0000-$0001
				return MMU_CFG_GET(w);
			else // a window to RAM (PAGEZERO), $0002-$00FF
				return mem_ram[w];
		else
			if (w<0XD400) // video configuration, $D000-$D03F
				switch (w&=63)
				{
					case 17: return (VICII_TABLE[w]&127)+((vicii_pos_y>>1)&128); // $D011: CONTROL REGISTER 1
					case 18: return vicii_pos_y; // $D012: RASTER COUNTER (BITS 0-7; BIT 8 IS IN $D011)
					case 22: return VICII_TABLE[22]|0XC0; // $D016: CONTROL REGISTER 2
					case 24: return VICII_TABLE[24]|0X01; // $D018: MEMORY CONTROL REGISTER
					case 25: // $D019: INTERRUPT REQUEST REGISTER
						return VICII_TABLE[25]|0X70;
					case 30: return w=VICII_TABLE[30],VICII_TABLE[30]=0,w; // $D01E: SPRITES-SPRITES COLLISIONS
					case 31: return w=VICII_TABLE[31],VICII_TABLE[31]=0,w; // $D01F: SPRITES-SCENERY COLLISIONS
					case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39: case 40:
					case 41: case 42: case 43: case 44: case 45: case 46: // $D020-$D02E: COLOURS
					case 26: // $D01A: INTERRUPT REQUEST MASK // with 0X70, "HVSC 20 YEARS" hangs on launch!
						return VICII_TABLE[w]|0XF0;
					case 47: // ???
					case 48: // "PENTAGRAM": this is ALWAYS $FF on a C64, but not on a C128!
					case 49: // ???
					case 50: case 51: case 52: case 53: case 54: // ???
					case 55: case 56: case 57: case 58: case 59: // ???
					case 60: // "GUNFRIGHT": this is ALWAYS $FF on a C64, but not on a SUPER CPU!
					case 61: case 62: case 63: return 255; // unused, always 255!
					default: return VICII_TABLE[w];
				}
			else // audio configuration, $D400-$D41F
			{
				if (w>=0XD420&&sid_extras) // beyond the range $D400-$D41F and with SID extensions?
				{
					if (sid_extras==1||sid_extras==3)
					{
						if (w<0XD440)
							return (w==0XD420+27)?sid_mute_peek27(1):(w==0XD420+28)?sid_mute_peek28(1):SID_TABLE[1][w-0XD420];
						if (w<0XD460&&sid_extras==3)
							return (w==0XD440+27)?sid_mute_peek27(2):(w==0XD440+28)?sid_mute_peek28(2):SID_TABLE[2][w-0XD440];
					}
					return 0XFF; // nothing here!
				}
				// the base chip, without extensions, spans the whole $D400-$D7FF range
				switch (w&=31)
				{
					case 25: case 26: return 255; // GAME PADDLES
					case 27: return sid_mute_peek27(0); // "BOX CHECK TEST" examines this port; see below
					case 28: return sid_mute_peek28(0);
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
						w=CIA_TABLE_0[0]|(BYTE)(~CIA_TABLE_0[2]);
						b=CIA_TABLE_0[1]|~CIA_TABLE_0[3];
						if (key2joy_flag) b&=~kbd_bits[9];
						for (int j=1;j<256;j<<=1)
							if (!(b&j)) w&=~(((kbd_bits[0]&j)?1:0)+((kbd_bits[1]&j)?2:0)+((kbd_bits[2]&j)?4:0)+((kbd_bits[3]&j)?8:0)
								+((kbd_bits[4]&j)?16:0)+((kbd_bits[5]&j)?32:0)+((kbd_bits[6]&j)?64:0)+((kbd_bits[7]&j)?128:0));
						if (!key2joy_flag) w&=~kbd_bits[9];
						return w;
					case  1: // keyboard bitmap
						w=CIA_TABLE_0[1]|(BYTE)(~CIA_TABLE_0[3]);
						b=CIA_TABLE_0[0]|~CIA_TABLE_0[2];
						if (!key2joy_flag) b&=~kbd_bits[9];
						for (int i=0;i<8;++i)
							if (!(b&(1<<i)))
								w&=~kbd_bits[i];
						if (key2joy_flag) w&=~kbd_bits[9];
						return w;
					case  4: return cia_count_a[0];
					case  5: return cia_count_a[0]>>8;
					case  6: return cia_count_b[0]; // "BUBBLE BOBBLE" expects 255 here because TIMER B is never used!
					case  7: return cia_count_b[0]>>8;
					case  8: return CIA_TABLE_0[15]>=128?CIA_TABLE_0[ 8]:(cia_port_11[0]=0,cia_hhmmssd[0]    );
					case  9: return CIA_TABLE_0[15]>=128?CIA_TABLE_0[ 9]:(                 cia_hhmmssd[0]>> 8);
					case 10: return CIA_TABLE_0[15]>=128?CIA_TABLE_0[10]:(                 cia_hhmmssd[0]>>16);
					case 11: return CIA_TABLE_0[15]>=128?CIA_TABLE_0[11]:(cia_port_11[0]=1,cia_hhmmssd[0]>>24);
					case 13: // acknowledge CIA #1 IRQ!
						M6510_CIA_RES;
						w=cia_port_13[0]; cia_port_13[0]=0;
						// this satisfies "BOX CHECK TEST" but there are more differences (PC64 CIA IRQ test)
						if (!cia_nouveau&&cia_state_a[0]==0+0+2+0&&CIA_TABLE_0[5]*256+CIA_TABLE_0[4]==cia_count_a[0]+1) ++cia_count_a[0];
						#if 0 //#ifndef FASTTAPE_STABLE // limited in scope (for example Mastertronic tapes) and very unreliable :-/
						// tape analysis?
						if (!(CIA_TABLE_0[13]&16)&&tape_fastload&&!tape_disabled&&tape&&!(w&m6510_a)) // can A be anything but $10 here?
						{
							WORD www=m6510_pc.w-3; BYTE *zzz=&PEEK(www);
							if (zzz[0]==0X2C&&zzz[3]==0XF0&&zzz[4]==0XFB&&!(cia_event_a[0]|cia_event_a[1]|cia_event_b[0]|cia_event_b[1]))
							{
								int x=tape_t;
								if (x>cia_count_a[0]) x=cia_count_a[0];
								if (x>cia_count_a[1]) x=cia_count_a[1];
								if (x>cia_count_b[0]) x=cia_count_b[0];
								if (x>cia_count_b[1]) x=cia_count_b[1];
								if (--x>1)
								{
									tape_t-=tape_skipping=--x;
									if (cia_state_a[0]==+0+0+2+0&&cia_minor_a[0]) cia_count_a[0]-=x;
									if (cia_state_a[1]==+0+0+2+0&&cia_minor_a[1]) cia_count_a[1]-=x;
									if (cia_state_b[0]==+0+0+2+0&&cia_minor_b[0]) cia_count_b[0]-=x;
									if (cia_state_b[1]==+0+0+2+0&&cia_minor_b[1]) cia_count_b[1]-=x;
								}
							}
						}
						#endif
						return w;
					case 14: return CIA_TABLE_0[14]&~16;
					case 15: return CIA_TABLE_0[15]&~16;
					case 12: // *!* SHIFT REGISTER ???
					default: return CIA_TABLE_0[w];
				}
		else
			if (w<0XDE00) // CIA #2 configuration, $DD00-$DD0F
				switch (w&=15)
				{
					case  0: return ((CIA_TABLE_1[0]|~CIA_TABLE_1[2])&0X3F)|(disc_disabled?192:(c64_peeks_c1541()&192)); // 192 = no disc at all, 00 = drive is powering up, 128 = drive is ready...
					case  1: return CIA_TABLE_1[1]|~CIA_TABLE_1[3];
					case  4: return cia_count_a[1];
					case  5: return cia_count_a[1]>>8;
					case  6: return cia_count_b[1];
					case  7: return cia_count_b[1]>>8;
					case  8: return CIA_TABLE_1[15]>=128?CIA_TABLE_1[ 8]:(cia_port_11[1]=0,cia_hhmmssd[1]    ); // "DRUID 2" (loader) sets the clock and freezes it, $089F: LDA $DD0F; AND #$7F; STA $DD0F; LDX #$01; STX $DD0B; STX $DD09; STX $DD08; LDA $DD0B
					case  9: return CIA_TABLE_1[15]>=128?CIA_TABLE_1[ 9]:(                 cia_hhmmssd[1]>> 8); // "DRUID 2" (game) expects the clock to stay frozen when a new game begins, $842F: LDA $DD0B; LDX $DD09; DEX; BNE $8400 (CRASH!)
					case 10: return CIA_TABLE_1[15]>=128?CIA_TABLE_1[10]:(                 cia_hhmmssd[1]>>16); // (the 1-second "Get ready!!" pause in "ISLAND OF DR.DESTRUCTO" shows that registers 10 and 9 shouldn't freeze anything, though)
					case 11: return CIA_TABLE_1[15]>=128?CIA_TABLE_1[11]:(cia_port_11[1]=1,cia_hhmmssd[1]>>24);
					case 13: // acknowledge CIA #2 NMI!
						// cfr. "ALL ROADS LEAD TO UIT", but mind the clash with "BARRYMCGUIGAN.TAP.ZIP" if we use >=-1
						M6510_NMI_RES;
						w=cia_port_13[1]; cia_port_13[1]=0;
						// this satisfies "BOX CHECK TEST" but there are more differences (PC64 CIA NMI test)
						if (!cia_nouveau&&cia_state_a[1]==0+0+2+0&&CIA_TABLE_1[4]+CIA_TABLE_1[5]*256-1==cia_count_a[1]) ++cia_count_a[1];
						return w;
					case 14: return CIA_TABLE_1[14]&~16;
					case 15: return CIA_TABLE_1[15]&~16;
					case 12: // *!* SHIFT REGISTER ???
					default: return CIA_TABLE_1[w];
				}
			else // hardware expansions
			{
				/**/ if (cart_type>=0) // cartridge? (not sure if REU can be compatible with this...)
					switch (cart_type)
					{
						case 17: // Dinamic's "Satan" and the like
							if (w>=0XDE00&&w<=0XDEFF) { cart_bank=w&15; mmu_recalc(); }
							break;
					}
				else if (w>=0XDF00&&ram_cap&&!georam_yes)
					{ BYTE b=reu_table[w&=31]; if (!w) reu_table[0]&=~32; return b; } // reset FAULT on read!
				else if (sid_extras==2||sid_extras==4) // within the range $DE00-$DFFF and with SID extensions?
				{
					if (w>=0XDE00&&w<0XDE20)
						return (w==0XDE00+27)?sid_mute_peek27(1):(w==0XDE00+28)?sid_mute_peek28(1):SID_TABLE[1][w-0XDE00];
					if (w>=0XDF00&&w<0XDF20&&sid_extras==4)
						return (w==0XDF00+27)?sid_mute_peek27(2):(w==0XDF00+28)?sid_mute_peek28(2):SID_TABLE[2][w-0XDF00];
				}
				// pseudorandom: "BARRY MCGUIGAN'S BOXING" uses this as a copy protection! it must not return 0XFF always!
				return vicii_attrib[((vicii_pos_y<<3)+vicii_pos_x)&1023]; // approximation to the actual value, VIC-II's last fetch
			}
}
void m6510_send(WORD w,BYTE b) // send the byte `b` to the I/O address `w`
{
	if (w<0XD800)
		if (w<0XD000)
			if (w<0X0002) // memory configuration, $0000-$0001
				MMU_CFG_SET(w,b);
			else // a window to RAM (PAGEZERO), $0002-$00FF
				mem_ram[w]=b;
		else
			if (w<0XD400) // video configuration, $D000-$D02E
				switch (w&=63)
				{
					case 17: // $D011: CONTROL REGISTER 1
						if (vicii_pos_y>=48&&vicii_pos_y<248&&vicii_ready&&((b-VICII_TABLE[17])&7))
						{
							// we must satisfy many different and seemingly contradictory cases:
							// - "CREATURES" ($23B1, Y=$032) scrolls the screen;
							// - "REWIND-TEMPEST" ($85D1, Y=$03B) scrolls the screen;
							// - "SMR-PAF" ($0A38, Y=$030) scrolls the screen;
							// - "DAWNFALL" ($B25A, Y=$034,$03C,$044...) reloads the attributes;
							// - "MS PACMAN" ($09E2..., Y=$02F...) reloads the attributes;
							// - "FUNNY RASTERS" ($C15F, Y=$03A,$03B...) freezes the cursor;
							// - "ONEDER-OXYRON" ($3403, every 4 Y...) reloads the attributes;
							// ...and all we know is that the VIC-II dominant range is X=12..55
							// one mystery: the high score scroll of "BMX KIDZ" is glitchy, why?
							if (vicii_badline=!((b-vicii_pos_y)&7))
							{
								if (vicii_pos_x>=12/*&&vicii_pos_x<55*/)
								{
									vicii_takeover=vicii_pos_x==14?2:1; // "MS PACMAN" and other FLI pictures force an attribute reload without the 3-T pause
									if ((vicii_dmadelay=55-1-vicii_pos_x)>40||!(vicii_mode&16)) vicii_dmadelay=0; // "CREATURES" scrolls with delays of 0..40
								}
								else if (vicii_pos_x>=-3&&vicii_pos_x<-0&&vicii_eighth==7&&vicii_backup==vicii_cursor) // line crunch?
									vicii_backup+=40; // "ALIEN 3" must not pass: vicii_backup!=vicii_cursor
							}
							else if (vicii_pos_x>=12) // "UKIYO-SAMAR" ($4577) kills the badline... and the bus takeover! (x==12 may be enough to do this)
								vicii_takeover=0; // also on VICE: the logo "jumps" exactly once, when the scroller reads "[...smal]l contrib[ution...]"
						}
						if (b&16)
						{
							if (vicii_pos_y==48)
								vicii_ready=1; // we can enable the display if we're still on line $30
							else if (vicii_pos_y>48&&vicii_pos_y<248) // '&&!vicii_ready' is WRONG: "SUPER STOCK CAR" (menu)
								if (VICII_TABLE[17]!=b) // "Paradroid" behaves weirdly without this sanity check!
									if (!vicii_takeover) // avoid top-line glitch in "MAYHEM IN MONSTERLAND" (cfr. "CREATURES")
										vicii_mode&=~32; // remove border but DON'T enable display: "SWIRL BY DNP"
						}
						// the remainder of the consequences of modifying this register are handled in the VIC-II ticker, not here
						// no `break`! The IRQ high bit is handled by the next register
					case 18: // $D012: RASTER POSITION
						VICII_TABLE[w]=b;
						if (w==17) vicii_setmode(),vicii_setmaps();
						w=(VICII_TABLE[17]&128)*2+VICII_TABLE[18];
						if (vicii_irq_y!=w)
							if (vicii_pos_y==(vicii_irq_y=w))
								if (((VICII_TABLE[25]|=1)&VICII_TABLE[26]&1)&&VICII_TABLE[25]<128)
									{ VICII_TABLE[25]+=128; M6510_VIC_SET; }
						break;
					case 22: // $D016: CONTROL REGISTER 2
						VICII_TABLE[22]=b&31; vicii_setmode();
						break;
					case 23: // $D017: SPRITE Y EXPANSION
						if (vicii_pos_x==15) vicii_crunch=~b&VICII_TABLE[23]; // catch sprite crunch: "HERAKLION", "MERSWINY"...
						vicii_sprite_z|=b; // cfr. the bottom border of "FOR YOUR SPRITES ONLY" and the scroller of "HVSC 10 YEARS"
						vicii_sprite_x&=VICII_TABLE[23]=b; // cfr. "20 YEARS OXYRON": changing the Y-scaling has immediate effects!
						break;
					case 24: // $D018: MEMORY CONTROL REGISTER
						VICII_TABLE[24]=b|1; vicii_setmaps();
						break;
					case 25: // $D019: INTERRUPT REQUEST REGISTER
						if ((VICII_TABLE[25]&=(~b&15))&VICII_TABLE[26]&15)
							{ if (VICII_TABLE[25]<128) { VICII_TABLE[25]+=128; M6510_VIC_SET; } } // throw pending VIC-II IRQ
						else
							{ VICII_TABLE[25]&=127; M6510_VIC_RES; } // acknowledge VIC-II IRQ
						break;
					case 26: // $D01A: INTERRUPT REQUEST MASK
						if (VICII_TABLE[25]&(VICII_TABLE[26]=b&15))
							{ if (VICII_TABLE[25]<128) { VICII_TABLE[25]+=128; M6510_VIC_SET; } } // throw pending VIC-II IRQ
						else
							{ VICII_TABLE[25]&=127; M6510_VIC_RES; } // acknowledge VIC-II IRQ
						break;
					case 19: case 20: break; // $D013 and $D014 are READ ONLY!?
					case 30: case 31: break; // $D01E and $D01F are READ ONLY!!
					case 32: case 33: case 34: case 35:
					case 36: case 37: case 38: case 39:
					case 40: case 41: case 42: case 43:
					case 44: case 45: case 46: // COLOR TABLE
						video_clut[16+w-32]=video_xlat[VICII_TABLE[w]=b&=15];
						break;
					default:
						VICII_TABLE[w]=b;
				}
			else // audio configuration, $D400-$D41F
			{
				if (w>=0XD420&&(sid_extras==1||sid_extras==3)) // beyond the range $D400-$D41F and with SID extensions?
				{
					if (w<0XD440)
					{
						if ((w-=0XD420)==18) sid_mute_poke18(1,b); // f.e. "ENCHANTED FOREST"...
						//else if (w>=19&&w<=20) sid_mute_dumb18(1);
						audio_sync(),SID_TABLE[1][w]=b,sid_reg_update(1,w); return;
					}
					/*else*/ if (w<0XD460&&sid_extras==3)
					{
						if ((w-=0XD440)==18) sid_mute_poke18(2,b); // ... aka "EF BY SAMAR"
						//else if (w>=19&&w<=20) sid_mute_dumb18(2);
						audio_sync(),SID_TABLE[2][w]=b,sid_reg_update(2,w); return;
					}
					//return; // stop here!
				}
				// the base chip, without extensions, spans the whole $D400-$D7FF range
				if ((w&=31)==18) sid_mute_poke18(0,b); // cfr. "BOX CHECK TEST"
				//else if (w>=19&&w<=20) sid_mute_dumb18(0);
				audio_sync(),SID_TABLE_0[w]=b,sid_reg_update(0,w); if (w<21&&b) tape_song=(tape_disabled||(audio_disabled&1))?0:240;
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
							cia_count_a[0]=CIA_TABLE_0[4]+b*256;
						break;
					case  7: // reload TIMER B
						CIA_TABLE_0[7]=b; if (!(CIA_TABLE_0[15]&1)) // *!* set TIMER LOAD flag?
							cia_count_b[0]=CIA_TABLE_0[6]+b*256;
						break;
					case  8: // decimals
						if (CIA_TABLE_0[15]>=128) CIA_TABLE_0[ 8]=b&15;
							else cia_port_11[0]=0,cia_hhmmssd[0]=(cia_hhmmssd[0]&0XFFFFFF00)+ (b& 15);
						if (cias_alarmclock(0)) { M6510_CIA_SET; } // "HAMMERFIST" needs this to run (cfr. $A7D0)
						break;
					case  9: // seconds
						if (CIA_TABLE_0[15]>=128) CIA_TABLE_0[ 9]=b&63;
							else /*cia_port_11[0]=1,*/cia_hhmmssd[0]=(cia_hhmmssd[0]&0XFFFF00FF)+((b& 63)<< 8);
						break;
					case 10: // minutes
						if (CIA_TABLE_0[15]>=128) CIA_TABLE_0[10]=b&63;
							else /*cia_port_11[0]=1,*/cia_hhmmssd[0]=(cia_hhmmssd[0]&0XFF00FFFF)+((b& 63)<<16);
						break;
					case 11: // hours + AM/PM bit
						if (CIA_TABLE_0[15]>=128) CIA_TABLE_0[11]=b&143;
							else cia_port_11[0]=1,cia_hhmmssd[0]=(cia_hhmmssd[0]&0X00FFFFFF)+((b&143)<<24);
						break;
					case 12: // *!* SHIFT REGISTER ???
						//cprintf("%04X:CIA1.0C=%02X ",m6510_pc.w,b); // cfr. event m6510_62500hz_cia0()
						CIA_TABLE_0[12]=b;
						if (cia_serialz[0]) cia_serialz[0]=2; // second byte
							else cia_serials[0]=15,cia_serialz[0]=1; // cfr. same port from CIA #2!
						break;
					case 13: // CIA #1 int.mask
						if (b&128) CIA_TABLE_0[13]|=b&31;
							else CIA_TABLE_0[13]&=~(b&31);
						if ((CIA_TABLE_0[13]&cia_port_13[0]&31)&&cia_port_13[0]<128) // throw pending CIA #1 IRQ?
							{ cia_port_13[0]+=128; M6510_CIA_SET; }
						break;
					case 14: // CIA #1 TIMER A control
						// related to m6510_62500hz_cia0() // the original "Arkanoid" tape needs this!
						if ((b&9)==9&&cia_serialz[0]&&(cia_state_a[0]&1)) if (!--cia_serials[0]) --cia_serialz[0],cia_serials[0]=16,cia_port_13[0]|=8;
						cia_event_a[0]=b+256;
						cia_port_14(0,b);
						break;
					case 15: // CIA #1 TIMER B control
						cia_event_b[0]=b+256;
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
						if (!disc_disabled) c64_pokes_c1541(b&0X38);
						if (!vic_nouveau) // *!* kludge for glue logic: unlike IC (Integrated Circuit), DL (Discrete Logic) delays collisions on page switch
							if ((CIA_TABLE_1[0]^b)&3) VICII_TABLE[31]=vicii_copy_hits>>8,VICII_TABLE[30]=vicii_copy_hits;
						// no `break`! (btw, it's more probable that IC performs vicii_setmaps() here, while DL does it once per scanline)
					case  2:
						CIA_TABLE_1[w]=b; vicii_setmaps();
						break;
					case  5: // reload TIMER A
						CIA_TABLE_1[5]=b; if (!(CIA_TABLE_1[14]&1)) // *!* set TIMER LOAD flag?
							cia_count_a[1]=CIA_TABLE_1[4]+b*256;
						break;
					case  7: // reload TIMER B
						CIA_TABLE_1[7]=b; if (!(CIA_TABLE_1[15]&1)) // *!* set TIMER LOAD flag?
							cia_count_b[1]=CIA_TABLE_1[6]+b*256;
						break;
					case  8: // decimals
						if (CIA_TABLE_1[15]>=128) CIA_TABLE_1[ 8]=b&15;
							else cia_port_11[1]=0,cia_hhmmssd[1]=(cia_hhmmssd[1]&0XFFFFFF00)+ (b& 15);
						if (cias_alarmclock(1)) { M6510_NMI_SET; }
						break;
					case  9: // seconds
						if (CIA_TABLE_1[15]>=128) CIA_TABLE_1[ 9]=b&63;
							else /*cia_port_11[1]=1,*/cia_hhmmssd[1]=(cia_hhmmssd[1]&0XFFFF00FF)+((b& 63)<< 8);
						break;
					case 10: // minutes
						if (CIA_TABLE_1[15]>=128) CIA_TABLE_1[10]=b&63;
							else /*cia_port_11[1]=1,*/cia_hhmmssd[1]=(cia_hhmmssd[1]&0XFF00FFFF)+((b& 63)<<16);
						break;
					case 11: // hours + AM/PM bit
						if (CIA_TABLE_1[15]>=128) CIA_TABLE_1[11]=b&143;
							else cia_port_11[1]=1,cia_hhmmssd[1]=(cia_hhmmssd[1]&0X00FFFFFF)+((b&143)<<24);
						break;
					case 12: // *!* SHIFT REGISTER ???
						//cprintf("%04X:CIA2.0C=%02X ",m6510_pc.w,b); // cfr. event m6510_62500hz_cia1()
						CIA_TABLE_1[12]=b;
						if (cia_serialz[1]) cia_serialz[1]=2; // second byte always does 16 loops
							else cia_serials[1]=15,cia_serialz[1]=1; // "Athena" requires 12..15, "Mario Bros" needs 15!
						break;
					case 13: // CIA #2 int.mask
						if (b&128) CIA_TABLE_1[13]|=b&31;
							else CIA_TABLE_1[13]&=~(b&31);
						if ((CIA_TABLE_1[13]&cia_port_13[1]&31)&&cia_port_13[1]<128) // throw pending CIA #2 NMI!
							{ cia_port_13[1]+=128; M6510_NMI_SET; }
						break;
					case 14: // CIA #2 TIMER A control
						// related to m6510_62500hz_cia1() // "Athena" and "Mario Bros" never do this!
						//if ((b&9)==9&&cia_serialz[1]&&(cia_state_a[0]&1)) if (!--cia_serials[1]) --cia_serialz[1],cia_serials[1]=16,cia_port_13[1]|=8;
						cia_event_a[1]=b+256;
						cia_port_14(1,b);
						break;
					case 15: // CIA #2 TIMER B control
						cia_event_b[1]=b+256;
						cia_port_15(1,b);
						break;
					default:
						CIA_TABLE_1[w]=b;
				}
			else // hardware expansions
			{
				/**/ if (w>=0XE000) // REU remote! (this only happens on page $FF00-$FFFF)
					{ POKE(w)=b; if (w==0XFF00&&ram_cap&&!georam_yes&&!(reu_table[1]&16)) reu_kick(); }
				else if (cart_type>=0) // cartridge? (not sure if REU can be compatible with this...)
					switch (cart_type)
					{
						case  5: // Ocean's "Robocop 2"
							if (w==0XDE00) { cart_bank=b&63; mmu_recalc(); }
							break;
						case 15: // System 3's "C64 Game System"
							if (w>=0XDE00&&w<=0XDEFF) { cart_bank=w&63; mmu_recalc(); }
							break;
						case 19: // "Alien 8" and the like
							if (w==0XDE00) { if (b&128) cart_bank=cart_mode=0; else cart_bank=b&63; mmu_recalc(); }
							break;
						case 32: // EasyFlash
							if (w==0XDE00) { cart_bank=b&63; if (cart_mode&3) mmu_recalc(); } else if (w==0XDE02) { cart_mode=b&3; mmu_update(); }
							break;
					}
				else if (ram_cap) // GEORAM or REU?
				{
					if (w>=0XDF00)
					{
						if (georam_yes)
							{ if (w==0XDFFF) { georam_block=b; mmu_update(); } else if (w==0XDFFE) { georam_page=b&63; mmu_update(); } }
						else
							{ if (w&=31) { reu_table[w]=b; if (w==1&&b>=128) { if (b&16) reu_kick(); else mmu_update(); } } }
					}
					else
						; // if (georam_yes) { POKE(w)=b; } // useless, mmu_bit[0XDE] is 0 in GEORAM mode, reads and writes are trivial
				}
				else if (sid_extras==2||sid_extras==4)
				{
					if (w>=0XDE00&&w<0XDE20)
					{
						if ((w-=0XDE00)==18) sid_mute_poke18(1,b);
						//else if (w>=19&&w<=20) sid_mute_dumb18(1);
						audio_sync(),SID_TABLE[1][w]=b,sid_reg_update(1,w);
					}
					else if (w>=0XDF00&&w<0XDF20&&sid_extras==4)
					{
						if ((w-=0XDF00)==18) sid_mute_poke18(2,b);
						//else if (w>=19&&w<=20) sid_mute_dumb18(2);
						audio_sync(),SID_TABLE[2][w]=b,sid_reg_update(2,w);
					}
					else
						; // nothing else!
				}
				else
					; // nothing else!
			}
}

// the MOS 6510 memory operations PAGE (setup MMU), PEEK and POKE, plus ZEROPAGE and others
#define M65XX_LOCAL BYTE m6510_aux1,m6510_aux2
#define M65XX_PAGE(x) (m6510_aux2=mmu_bit[m6510_aux1=x])
#define M65XX_SAFEPEEK(x) mmu_rom[m6510_aux1][x]
#define M65XX_PEEK(x) ((m6510_aux2&1)?m6510_recv(x):mmu_rom[m6510_aux1][x])
#define M65XX_POKE(x,o) do{ if (m6510_aux2&2) m6510_send(x,o); else mmu_ram[m6510_aux1][x]=o; }while(0) // not `mem_ram[x]=o`!
#define M65XX_PEEKZERO(x) ((x<0X0002)?mmu_cfg_get(x):mem_ram[x]) // ZEROPAGE is simpler, we only need to filter the first two bytes
#define M65XX_POKEZERO(x,o) do{ if (x<0X0002) mmu_cfg_set(x,o); else mem_ram[x]=o; }while(0) // ditto
#define M65XX_PULL(x) (mem_ram[256+(x)]) // stack operations are simpler, they're always located on the same RAM area
#define M65XX_PUSH(x,o) (mem_ram[256+(x)]=o) // ditto
// the MOS 6510 "dumb" operations sometimes have an impact on the hardware of the C64
#define M65XX_DUMBPAGE(x) (m6510_aux2=mmu_bit[m6510_aux1=x])
#define M65XX_DUMBPEEK(x) ((void)((m6510_aux2&4)&&m6510_recv(x)))
#define M65XX_DUMBPOKE(x,o) do{ if (m6510_aux2&8) m6510_send(x,o); }while(0)
// When the VIC-II announces that it needs the MOS 6510 to wait, it allows up to three writing operations at the beginning;
// this is equivalent to writing operations being non-blocking because the 6510 never does more than three writes in a row.
#define M65XX_WAIT (m65xx_t+=m6510_tick(0)) // and the comparison is against the highest acceptable value of the takeover status
#define M65XX_WHAT (m65xx_t+=m6510_what=m6510_tick(0)) // like M65XX_WAIT but we write down the delay; see M65XX_SHW below
#define M65XX_TICK (m65xx_t+=m6510_tick(2)) // we use 2 because `vicii_takeover` can be 0 (idle), 1 (busy normal) and 2 (busy rushed)
#define M65XX_TOCK (M65XX_INT=M65XX_IRQ)

void all_reset(void); // a JAM in the 6510 can reset the C64!
#define M65XX_START m6510_start
#define M65XX_RESET m6510_reset
#define M65XX_CRASH all_reset
#define M65XX_MAIN m6510_main
#define M65XX_SYNC(x) ((void)0)
#define M65XX_IRQ_ACK ((void)0)
#define M65XX_NMI_ACK (m6510_irq&=~128)
#define M65XX_IRQ m6510_irq
#define M65XX_INT m6510_int
#define M65XX_PC m6510_pc
#define M65XX_P m6510_p
#define M65XX_A m6510_a
#define M65XX_X m6510_x
#define M65XX_Y m6510_y
#define M65XX_S m6510_s

#define M65XX_XEE (0XEE)
#define M65XX_XEF ((tape&&m6510_t64ok!=7)?0XEF:0XEE) // catch special case: the Mastertronic tape loader :-/
#define M65XX_XVS 0 // the OVERFLOW pin doesn't exist on the MOS 6510, unlike the C1541's MOS 6502
#define M65XX_HLT (vicii_takeover) // this is how the VIC-II can override the MOS 6510
#define M65XX_SHW (m6510_what==1) // "FELLAS-EXT" checks this to detect when READY turns true!
int m6510_what; // it doesn't need to stick, but putting this in M65XX_LOCAL hurts the performance :-(
//#define M65XX_TRAP_0X20 if (UNLIKELY(M65XX_PC.w==0XF53A&&mmu_mcr==m6510_t64ok)) { t64_loadfile(); M65XX_X=mem_ram[0X00AC],M65XX_Y=mem_ram[0X00AD],M65XX_PC.w=0XF8D1; } // load T64 file
#define M65XX_REU

#define bios_magick() (debug_point[0XF539]=debug_point[0XFD6E]=DEBUG_MAGICK)
void m6510_magick(void) // virtual magick!
{
	//cprintf("MAGICK:%08X ",m6510_pc.w);
	/**/ if (m6510_pc.w==0XF539) { if (mmu_mcr==m6510_t64ok&&mem_rom[0X1539]==0X20) { t64_loadfile(); M65XX_X=mem_ram[0X00AC],M65XX_Y=mem_ram[0X00AD],M65XX_PC.w=0XF8D0; } } // load T64 file
	else if (m6510_pc.w==0XFD6E) { if (mem_ram[0XC2]==0X04&&mmu_mcr==power_boost&&mem_rom[0X1D6E]==0XB1) mem_ram[0XC2]=0X9F; } // power-up boost (memory test)
}
#define M65XX_MAGICK() m6510_magick()

#define DEBUG_HERE
#define DEBUG_INFOX 20 // panel width
unsigned int debug_info_binary(BYTE b) // creates a fake integer with the binary display of `b`
	{ return ((b&128)<<21)+((b&64)<<18)+((b&32)<<15)+((b&16)<<12)+((b&8)<<9)+((b&4)<<6)+((b&2)<<3)+(b&1); }
void debug_info_sid(int x,int y)
{
	sprintf(DEBUG_INFOZ(y+2),"/%02X",sid_mute_byte27[x]);
	sprintf(DEBUG_INFOZ(y+3),"%02X/",sid_mute_byte28[x]);
	sprintf(DEBUG_INFOZ(y),"$%04X:",(WORD)((SID_TABLE[x]-VICII_TABLE)+0XD000+21));
	byte2hexa(DEBUG_INFOZ(y)+6,&SID_TABLE[x][21],7);
	for (int q=0;q<3;++q)
		sprintf(DEBUG_INFOZ(y+q+1)+4,"%c:",q+(sid_tone_stage[x][q]<2?'A':'a')),
		byte2hexa(DEBUG_INFOZ(y+q+1)+6,&SID_TABLE[x][q*7],7);
}
BYTE debug_peek_c1541(WORD p) { return c1541_mem[p]; }
BYTE debug_pook(int q,WORD w) { return q?PEEK(w):POKE(w); } // show either READING or WRITING space
void debug_info_cias(char *t,BYTE b,BYTE *s)
	{ sprintf(t,"%c%c%c",b&8?'L':'-',b&4?'W':'-',b&2?'*':b&1?'-':'?'); byte2hexa(t+4,s,8); }
void debug_info(int q)
{
	strcpy (DEBUG_INFOZ( 0),"I-O:"); if (cart)
		sprintf(DEBUG_INFOZ( 0)+8,"CART %02d:%1X,%02X",cart_type,cart_mode&3,cart_bank);
	else if (ram_cap)
	{
		if (georam_yes) sprintf(DEBUG_INFOZ( 0)+8,"GEORAM:%02X.%02X",georam_block,georam_page);
		else sprintf(DEBUG_INFOZ( 0)+6,"REU:%04X,%05X",reu_word.w,reu_size&0XFFFFF);
	}
	sprintf(DEBUG_INFOZ( 1)+4,"[%02X:%02X] %08X",mmu_cfg[0],mmu_cfg[1],debug_info_binary(mmu_out));
	if (!(q&1)) // 1/2
	{
		sprintf(DEBUG_INFOZ( 2),"CIA #1: %02X %04X:%04X",cia_port_13[0],cia_count_a[0]&0XFFFF,cia_count_b[0]&0XFFFF);
		if (cia_port_13[0] &128) debug_hilight2(DEBUG_INFOZ( 2)+4+4);
		debug_info_cias(DEBUG_INFOZ( 3),cia_state_a[0],&CIA_TABLE_0[0]);
		debug_info_cias(DEBUG_INFOZ( 4),cia_state_b[0],&CIA_TABLE_0[8]);
		debug_info_cias(DEBUG_INFOZ( 6),cia_state_a[1],&CIA_TABLE_1[0]);
		debug_info_cias(DEBUG_INFOZ( 7),cia_state_b[1],&CIA_TABLE_1[8]);
		sprintf(DEBUG_INFOZ( 5),"CIA #2: %02X %04X:%04X",cia_port_13[1],cia_count_a[1]&0XFFFF,cia_count_b[1]&0XFFFF);
		if (cia_port_13[1] &128) debug_hilight2(DEBUG_INFOZ( 5)+4+4);
		sprintf(DEBUG_INFOZ( 8),"TOD %c%07X%c%07X",cia_port_11[0]?'*':'-',(cia_hhmmssd[0]>>4)+(cia_hhmmssd[0]&15),cia_port_11[1]?'*':'-',(cia_hhmmssd[1]>>4)+(cia_hhmmssd[1]&15)); // time-of-day
		sprintf(DEBUG_INFOZ( 9),"VIC-II: %02X %02X%c%03X %02X",VICII_TABLE[25],(video_pos_x>>4)&255,vicii_badline?'*':'-',vicii_pos_y&511,vicii_mode);
		if (VICII_TABLE[25]&128) debug_hilight2(DEBUG_INFOZ( 9)+4+4);
		for (q=0;q<4;++q)
			byte2hexa(DEBUG_INFOZ(10+q)+4,&VICII_TABLE[q*8],8);
		byte2hexa(DEBUG_INFOZ(14)+4,vicii_sprite_y,8); // the 6-bit sprite offset counters
		sprintf(DEBUG_INFOZ( 9)+21,"ATTR:%04X",(int)(vicii_attrib-mem_ram)); // 64-bit warning
		sprintf(DEBUG_INFOZ(10)+21,"BASE:+%03X",vicii_backup&1023);
		sprintf(DEBUG_INFOZ(11)+21,"Y:%c  +%03X",48+(vicii_eighth&7),vicii_cursor&1023);
		sprintf(DEBUG_INFOZ(12)+21,"IEC1:%02X %c",cia_serials[0]&255,cia_serialz[0]+48);
		sprintf(DEBUG_INFOZ(13)+21,"IEC2:%02X %c",cia_serials[1]&255,cia_serialz[1]+48);
	}
	else // 2/2
	{
		sid_mute(); sprintf(DEBUG_INFOZ(2),"SID x%d:",sid_chips); debug_info_sid(0,3);
		//" %+04d%+04d%+04d",sid_filter_lll[0],sid_filter_hhh[0],sid_filter_lll[0]+sid_filter_hhh[0]
		if (sid_extras<=2&&!disc_disabled)
		{
			int y=sid_extras?11:7;
			sprintf(DEBUG_INFOZ(y),"6502: A=%02X X=%02X Y=%02X",m6502_a,m6502_x,m6502_y);
			WORD p=m6502_pc.w,pp; char t[32];
			while (++y<15)
			{ pp=debug_dasm_any(t,p,debug_peek_c1541); sprintf(DEBUG_INFOZ(y)+2,"%04X: %s",p,t); if (p==m6502_pc.w) debug_hilight(DEBUG_INFOZ(y)+2,4); p=pp; }
		}
		if (sid_extras>0)
		{
			debug_info_sid(1,7);
			if (sid_extras>2)
				debug_info_sid(2,11);
		}
	}
}
int grafx_mask(void) { return 0XFFFF; }
BYTE grafx_peek(int w) { return mem_ram[(WORD)w]; }
void grafx_poke(int w,BYTE b) { mem_ram[(WORD)w]=b; }
VIDEO_UNIT grafx_show2b[4]={0,0X0080FF,0XFF8000,0XFFFFFF};
int grafx_size(int i) { return i*8; }
int grafx_show(VIDEO_UNIT *t,int g,int n,int w,int o)
{
	BYTE z=-(o&1); g-=8; do
	{
		BYTE b=grafx_peek(w)^z; // base RAM only
		if (o&2)
		{
			VIDEO_UNIT p;
			*t++=p=grafx_show2b[ b>>6   ]; *t++=p;
			*t++=p=grafx_show2b[(b>>4)&3]; *t++=p;
			*t++=p=grafx_show2b[(b>>2)&3]; *t++=p;
			*t++=p=grafx_show2b[ b    &3]; *t++=p;
		}
		else
		{
			const VIDEO_UNIT p0=0,p1=0XFFFFFF;
			*t++=b&128?p1:p0; *t++=b& 64?p1:p0;
			*t++=b& 32?p1:p0; *t++=b& 16?p1:p0;
			*t++=b&  8?p1:p0; *t++=b&  4?p1:p0;
			*t++=b&  2?p1:p0; *t++=b&  1?p1:p0;
		}
	}
	while (++w,t+=g,--n); return w&grafx_mask();
}
void grafx_info(VIDEO_UNIT *t,int g,int o) // draw the palette and the sprites
{
	BYTE z=-(o&1); t-=16*6; for (int y=0;y<2*12;++y)
		for (int x=0;x<16*6;++x)
			t[y*g+x]=video_clut[(x/6)+(y/12)*16];
	for (int y=0,i=0;y<2;++y)
		for (int x=0;x<4;++x,++i)
		{
			VIDEO_UNIT *tt=&t[g*(2*12+y*21)+x*24];
			for (int yy=0,ii=vicii_attrib[1016+i]*64;yy<21;++yy,tt+=g)
				if (o&2)
					for (int xx=0;xx<3;++xx,++ii)
					{
						BYTE b=vicii_memory[ii]^z;
						for (int zz=0;zz<8;zz+=2)
							tt[xx*8+zz]=tt[xx*8+zz+1]=grafx_show2b[(b>>(6-zz))&3];
					}
				else
					for (int xx=0;xx<3;++xx,++ii)
					{
						BYTE b=vicii_memory[ii]^z;
						for (int zz=0;zz<8;++zz)
							tt[xx*8+zz]=(b<<zz)&128?0XFFFFFF:0;//video_clut[23+i]:video_clut[17];
					}
		}
}
#include "cpcec-m6.h"
#undef DEBUG_HERE

// autorun runtime logic -------------------------------------------- //

BYTE snap_done; // avoid accidents with ^F2, see all_reset()
char autorun_path[STRMAX]=""; int autorun_mode=0,autorun_t=0;
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

INLINE void autorun_next(void) // handle AUTORUN
{
	switch (autorun_mode)
	{
		case 1: // TYPE "RUN", PRESS RETURN...
			if (autorun_type("\022\025\016",3)) break; // retry!
			// no `break`!
		case 2: // INJECT FILE...
			if (autorun_type(NULL,0)) break; // retry!
			int i=2; mem_ram[0X0801]=mem_ram[0X0802]=0;
			FILE *f=puff_fopen(cart_path,"rb"); if (f) { fgetii(f),i=fread1(&mem_ram[0X0801],0XFFFA-0X0801,f); puff_fclose(f); }
			i+=0X0801; // end of prog, rather than prog size
			mputii(&mem_ram[0X00AC],i); mputii(&mem_ram[0X00AE],i); // poke end-of-file KERNAL pokes
			mputii(&mem_ram[0X002D],i); mputii(&mem_ram[0X002F],i); mputii(&mem_ram[0X0031],i); // end-of-file BASIC pokes
			autorun_mode=9;
			break;
		case 3: // TYPE "LOAD", PRESS RETURN...
			if (!autorun_type("\014\017\001\004",4)) session_dirty|=(tape_enabled=m6510_t64ok!=7),autorun_mode=4; // `tape_enabled` is reset by all_reset(), so we must set it here
			break;
		case 4: // RELEASE RETURN...
			autorun_kbd_res(0001);
			autorun_t=3; autorun_mode=5;
			break;
		case 5: // TYPE "RUN", PRESS RETURN...
			if (!(m6510_pc.w>=0XE000&&autorun_type("\022\025\016",3))) autorun_t=autorun_mode=9; // we must NOT retry if we're already running code!!
			break;
		case 8: // TYPE "LOAD"*",8,1", PRESS RETURN...
			if (!autorun_type("\014\017\001\004\042\052\042" ",8,1",11)) autorun_mode=9; // ditto!
			break;
		case 9: // ...RELEASE RETURN!
			autorun_kbd_res(0001);
			disc_disabled&=1,autorun_mode=0; // end of AUTORUN
			break;
	}
}

#ifdef DEBUG
char psid_rsid,psid_info[6]; WORD psid_play,psid_last,psid_song,psid_boot,psid_poke; int psid_bits; // PSID playback is a special case of autorun
char psid_text[99];
#define PSID_STOP (psid_last=0)
#define PSID_TRUE (psid_last>0)
#else
#define PSID_STOP ((void)0)
#define PSID_TRUE 0
#endif

// EMULATION CONTROL ================================================ //

char txt_error_any_load[]="Cannot open file!";
char txt_error_bios[]="Cannot load firmware!";

// emulation setup and reset operations ----------------------------- //

void all_setup(void) // setup everything!
{
	video_table_reset(); // user palette
	bios_magick(); // MAGICK follows DEBUG!
	memset(&mem_ram[0X10000],0,sizeof(mem_ram)-0X10000); // REU/GEORAM must be ZERO!
	mmu_setup(),m65xx_setup(); // 6510+6502!
	sid_setup();
}
void all_reset(void) // reset everything!
{
	const int k=64; // the C64 RAM is a little special about the power-up memory state
	for (int i=0;i<0X10000;i+=k) memset(&mem_ram[i],0,k),i+=k,memset(&mem_ram[i],-1,k);
	memset(&mem_ram[0X10000],0,sizeof(mem_ram)-0X10000); // extra memory is filled with $00 AFAIK
	//MEMZERO(mem_ram);
	MEMZERO(autorun_kbd); MEMZERO(kbd_bits);
	mmu_reset(),m6510_reset(),cia_reset();
	vicii_reset(),sid_all_reset();
	m6510_int=m6510_irq=0; debug_reset();
	c1541_reset(); PSID_STOP;
	disc_disabled&=1,tape_enabled=snap_done=autorun_mode=autorun_t=0; //MEMBYTE(m6510_tape_index,-1); // avoid accidents!
	//if (!memcmp(&mem_ram[0X8004],"\303\302\31580",5)) mem_ram[0X8004]=0330; // disable "CBM80" trap! // we already wiped the RAM above
}

// firmware ROM file handling operations ---------------------------- //

char bios_path[STRMAX]="";
char old_bios_id=-1,bios_id=0;

int bios_load(char *s) // loads the CBM64 firmware; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1; // fail!
	int i=fread1(mem_rom,20<<10,f); i+=fread1(mem_rom,20<<10,f);
	old_bios_id=99; // temporarily tag BIOS as wrong -- it will be made right later
	if (puff_fclose(f),i!=(20<<10)) return 1; // check filesize; are there any fingerprints?
	for (i=0;i<0XD0;++i) if (mem_rom[0X4008+i]^mem_rom[0X4408+i]^mem_rom[0X4808+i]^mem_rom[0X4C08+i]) return 1; // CHAR ROM fail!
	if (session_substr!=s) STRCOPY(bios_path,s);
	old_bios_id=bios_id;
	return 0;
}

int bios_reload(void) // loads the default CBM64 firmware
	{ return old_bios_id==bios_id?0:bios_load(strcat(strcpy(session_substr,session_path),"c64en.rom")); }

int bdos_load(char *s) // loads the C1541 firmware; 0 OK, !0 ERROR
{
	*c1541_rom=0; // tag ROM as empty
	FILE *f=fopen(strcat(strcpy(session_substr,session_path),s),"rb"); if (!f) return 1; // fail!
	int i=fread1(c1541_rom,16<<10,f); i+=fread1(c1541_rom,16<<10,f);
	return fclose(f),i-(16<<10); // check filesize; are there any fingerprints?
}

// snapshot file handling operations -------------------------------- //

char snap_pattern[]="*.s64",snap_obsolete[]="C64-SNAPSHOT v1\032",snap_magic16[]="C64 SNAPSHOT V1\032";
char snap_path[STRMAX]="",snap_extended=1; // compress memory dumps

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
int snap_x2bin(BYTE *t,int o,BYTE *s,int i,int q) // just to keep compatibility with snap_obsolete...
{
	return q<0?rle2bin(t,o,s,i):
	#ifdef LEMPELZIV_ENCODING
	q?lzf2bin(t,o,s,i):
	#endif
	rlf2bin(t,o,s,i);
}
int snap_loadchunk(BYTE *t,int l,FILE *f,int i,int q) // load an optionally compressed block; 0 OK, !0 ERROR
	{ return (i<l?snap_x2bin(t,l,session_scratch,fread1(session_scratch,i,f),q):fread1(t,l,f))-l; }

int snap_save(char *s) // saves snapshot file `s`; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"wb"); if (!f) return 1;
	BYTE header[256]; MEMZERO(header); strcpy(header,snap_magic16);
	#ifdef LEMPELZIV_ENCODING
	if (snap_extended) header[14]='2';
	#endif
	int i,j,k; for (i=0;i<512;++i) session_scratch[i]=(VICII_COLOR[i*2]<<4)+(VICII_COLOR[1+i*2]&15);
	if ((j=snap_extended?snap_bin2x(&session_scratch[512],512,session_scratch,512):0)>0) mputii(&header[0x10],j); else j=0;
	if ((k=snap_extended?snap_bin2x(&session_scratch[512*2],1<<16,mem_ram,1<<16):0)>0) mputii(&header[0x12],k); else k=0;
	// CPU #1
	mputii(&header[0x20],m6510_pc.w);
	header[0x22]=m6510_p;
	header[0x23]=m6510_a;
	header[0x24]=m6510_x;
	header[0x25]=m6510_y;
	header[0x26]=m6510_s;
	header[0x27]=m6510_int/*&(128+2+1)*/;
	header[0x30]=mmu_cfg[0];
	header[0x31]=mmu_cfg[1];
	//header[0x32]=mmu_out; // this value is normally identical to mmu_cfg[1], but not always, cfr. CPUPORT.PRG
	// CIA #1
	memcpy(&header[0x40],CIA_TABLE_0,0X10);
	mputii(&header[0x54],cia_count_a[0]); header[0x5E]=cia_event_a[0]^CIA_TABLE_0[14];
	mputii(&header[0x56],cia_count_b[0]); header[0x5F]=cia_event_b[0]^CIA_TABLE_0[15];
	header[0x5D]=cia_port_13[0]; mputiiii(&header[0x58],cia_hhmmssd[0]|(cia_port_11[0]<<30));
	/*if (cia_serialz[0])*/ header[0x5C]=(cia_serials[0]<<2)+cia_serialz[0];
	// CIA #2
	memcpy(&header[0x60],CIA_TABLE_1,0X10);
	mputii(&header[0x74],cia_count_a[1]); header[0x7E]=cia_event_a[1]^CIA_TABLE_1[14];
	mputii(&header[0x76],cia_count_b[1]); header[0x7F]=cia_event_b[1]^CIA_TABLE_1[15];
	header[0x7D]=cia_port_13[1]; mputiiii(&header[0x78],cia_hhmmssd[1]|(cia_port_11[1]<<30));
	/*if (cia_serialz[1])*/ header[0x7C]=(cia_serials[1]<<2)+cia_serialz[1];
	// SID #1
	memcpy(&header[0x80],SID_TABLE_0,0X20-4);
	mputii(&header[0x9C],sid_extras>0?0XD000+(SID_TABLE[1]-VICII_TABLE):0);
	mputii(&header[0x9E],sid_extras>2?0XD000+(SID_TABLE[2]-VICII_TABLE):0);
	mputiiii(&header[0xA8],sid_randomize); // the global SID LFSR must stick between sessions
	// VIC-II
	memcpy(&header[0xB0],VICII_TABLE,0X30); if ((i=vicii_pos_x)<0) i+=vicii_len_x;
	header[0xE0]=i>>multi_t;
	header[0xE1]=6-vicii_n_cia; mputii(&header[0xE2],vicii_pos_y);
	// CARTRIDGE (if any)
	if (cart) header[0XF0]=cart_mode,header[0XF1]=cart_bank;
	fwrite1(header,256,f);
	// COLOR TABLE + 64K RAM
	j?fwrite1(&session_scratch[512],j,f):fwrite1(session_scratch,512,f);
	k?fwrite1(&session_scratch[512*2],k,f):fwrite1(mem_ram,1<<16,f);
	// REU and other extras
	if (ram_cap&&ram_dirty)
	{
		if (georam_yes)
			kputmmmm(0X47454F30,f), // "GEO0", GeoRAM header
			kputiiii(2,f),
			fputc(georam_block,f),
			fputc(georam_page,f);
		else
			kputmmmm(0X52455530,f), // "REU0", REU header
			kputiiii(12+2+3+3,f),
			fwrite1(reu_table,12,f),
			fputii(reu_word.w,f),
			fputii(reu_addr,f), fputc(reu_addr>>16,f),
			fputii(reu_size,f), fputc(reu_size>>16,f);
		for (i=1;i<(ram_dirty>>16)+2;++i)
		{
			if (georam_yes)
				fputmmmm(0X47454F30+i,f); // "GEO1".."GEO8", GeoRAM 64K page
			else
				fputmmmm(0X52455530+i,f); // "REU1".."REU8", REU 64K page
			snap_savechunk(&mem_ram[i<<16],1<<16,f);
		}
	}
	if (cart_type==32) // EasyFlash?
		kputmmmm(0X45415359,f), // "EASY", EasyFlash RAM page
		snap_savechunk(cart_easy,256,f); // the mode and bank are already in the main header
	// ... future blocks will go here ...
	STRCOPY(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

int snap_load(char *s) // loads snapshot file `s`; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1;
	BYTE header[256]; int i,q;
	#ifdef LEMPELZIV_ENCODING
	if ((fread1(header,256,f)!=256)||((q=memcmp(header,snap_obsolete,16))&&(memcmp(header,snap_magic16,14)||(header[14]-='1')>1))) return puff_fclose(f),1;
	q=q?header[14]:-1; // i.e. -1 = obsolete, 0 = run-length, +1 = lempel-ziv
	#else
	if ((fread1(header,256,f)!=256)||((q=memcmp(header,snap_obsolete,16))&&memcmp(header,snap_magic16,16))) return puff_fclose(f),1;
	q=q?0:-1; // -1 = obsolete, 0 = run-length
	#endif
	// CPU #1
	m6510_pc.w=mgetii(&header[0x20]);
	m6510_p=header[0x22];
	m6510_a=header[0x23];
	m6510_x=header[0x24];
	m6510_y=header[0x25];
	m6510_s=header[0x26];
	m6510_int=header[0x27]&(128+2+1); // avoid accidents; see `m6510_irq` below!
	mmu_cfg[0]=header[0x30];
	mmu_cfg[1]=header[0x31];
	//if (!(mmu_out=header[0x32])) mmu_out=0XFF; // *!* temporary bugfix
	// CIA #1
	memcpy(CIA_TABLE_0,&header[0x40],0X10);
	cia_count_a[0]=mgetii(&header[0x54]); if (cia_event_a[0]=header[0x5E]^CIA_TABLE_0[14]) cia_event_a[0]+=256;
	cia_count_b[0]=mgetii(&header[0x56]); if (cia_event_b[0]=header[0x5F]^CIA_TABLE_0[15]) cia_event_b[0]+=256;
	cia_port_13[0]=header[0x5D]; cia_hhmmssd[0]=mgetiiii(&header[0x58]);
	cia_port_14(0,CIA_TABLE_0[14]); cia_state_a[0]=(CIA_TABLE_0[14]&1)?+0+0+2+0:+0+0+0+1;
	cia_port_15(0,CIA_TABLE_0[15]); cia_state_b[0]=(CIA_TABLE_0[15]&1)?+0+0+2+0:+0+0+0+1;
	/*if (header[0x5C])*/ cia_serials[0]=((INT8)header[0x5C])>>2,cia_serialz[0]=header[0x5C]&3;
	//CIA_TABLE_0[8]&=15; CIA_TABLE_0[9]&=63; CIA_TABLE_0[10]&=63; CIA_TABLE_0[11]&=143; // normalize
	cia_port_11[0]=(cia_hhmmssd[0]>>30)&1; cia_hhmmssd[0]&=0X8F3F3F0F; // bit 30 is the "locked" flag
	// CIA #2
	memcpy(CIA_TABLE_1,&header[0x60],0X10);
	cia_count_a[1]=mgetii(&header[0x74]); if (cia_event_a[1]=header[0x7E]^CIA_TABLE_1[14]) cia_event_a[1]+=256;
	cia_count_b[1]=mgetii(&header[0x76]); if (cia_event_b[1]=header[0x7F]^CIA_TABLE_1[15]) cia_event_b[1]+=256;
	cia_port_13[1]=header[0x7D]; cia_hhmmssd[1]=mgetiiii(&header[0x78]);
	cia_port_14(1,CIA_TABLE_1[14]); cia_state_a[1]=(CIA_TABLE_1[14]&1)?+0+0+2+0:+0+0+0+1;
	cia_port_15(1,CIA_TABLE_1[15]); cia_state_b[1]=(CIA_TABLE_1[15]&1)?+0+0+2+0:+0+0+0+1;
	/*if (header[0x7C])*/ cia_serials[1]=((INT8)header[0x7C])>>2,cia_serialz[1]=header[0x7C]&3;
	//CIA_TABLE_1[8]&=15; CIA_TABLE_1[9]&=63; CIA_TABLE_1[10]&=63; CIA_TABLE_1[11]&=143; // normalize
	cia_port_11[1]=(cia_hhmmssd[1]>>30)&1; cia_hhmmssd[1]&=0X8F3F3F0F; // bit 30 is the "locked" flag
	// SID #1
	memcpy(SID_TABLE_0,&header[0x80],0X1C);
	sid_randomize=((i=mgetiiii(&header[0xA8]))&0XFFFFFF)?i:1; // old versions wrote ZERO; that kills the SID LFSR!
	if (equalsii(&header[0x9C],0XD420))
		sid_extras=equalsii(&header[0x9E],0XD440)?3:1;
	else if (equalsii(&header[0x9C],0XDE00))
		sid_extras=equalsii(&header[0x9E],0XDF00)?4:2;
	else
		sid_extras=0;
	if (sid_extras<3) { sid_reset(2); if (sid_extras<1) sid_reset(1); }
	sid_all_update();
	// VIC-II
	memcpy(VICII_TABLE,&header[0xB0],0X30);
	if ((vicii_pos_x=(header[0xE0]<<multi_t))>=vicii_irq_x) vicii_pos_x-=vicii_len_x;
	frame_pos_y-=video_pos_y,video_target-=video_pos_y*VIDEO_LENGTH_X; // adjust (1/2)
	video_pos_y=(video_pos_y&1)+((vicii_pos_y=mgetii(&header[0xE2]))+VICII_POS_Y_ADD)*2;
	frame_pos_y+=video_pos_y,video_target+=video_pos_y*VIDEO_LENGTH_X; // adjust (2/2)
	vicii_n_cia=(6-header[0xE1])&7; vicii_irq_y=(VICII_TABLE[17]&128)*2+VICII_TABLE[18];
	vicii_badline=vicii_pos_y>=48&&vicii_pos_y<248&&!((VICII_TABLE[17]-vicii_pos_y)&7);
	vicii_setmaps(),vicii_setmode();
	vicii_sprite_x&=VICII_TABLE[23]; // avoid surprises!
	video_xlat_clut();
	// CARTRIDGE (if any)
	cart_mode=header[0XF0],cart_bank=header[0XF1]; // see below
	// COLOR TABLE + 64K RAM
	int j=mgetii(&header[0x10]),k=mgetii(&header[0x12]); // `j` and `k` are zero when the blocks aren't compressed
	if (q<0) k=j,j=0; // snap_obsolete stored the RAM0 flag where the VRAM flag is now 'cause it didn't pack the VRAM
	(j>0&&j<512)?snap_x2bin(mem_ram,512,session_scratch,fread1(session_scratch,j,f),q):fread1(mem_ram,512,f);
	for (i=0;i<512;++i) VICII_COLOR[i*2]=mem_ram[i]>>4,VICII_COLOR[1+i*2]=mem_ram[i]&15;
	k?snap_x2bin(mem_ram,1<<16,session_scratch,fread1(session_scratch,k,f),q):fread1(mem_ram,1<<16,f);
	// C1541, REU and other extras
	ram_dirty=0; while ((k=fgetmmmm(f),i=fgetiiii(f))>=0)
	{
		/**/ if (k==0X31353431) // "1541", the C1541 status
		{
			disc_disabled=0;
			// CPU #2
			m6502_pc.w=fgetii(f);
			m6502_p=fgetc(f);
			m6502_a=fgetc(f);
			m6502_x=fgetc(f);
			m6502_y=fgetc(f);
			m6502_s=fgetc(f);
			m6502_irq=fgetc(f)&1;
			fread1(VIA_TABLE_0,16,f); // VIA #1
			fread1(VIA_TABLE_1,16,f); // VIA #2
			fread1(disc_track,4,f); // current tracks
			i-=8+16+16+4; // keep leftovers, if any; fseek() will skip them
		}
		else if (k==0X52455530) // "REU0", REU config
		{
			georam_yes=2+0;
			fread1(reu_table,12,f);
			reu_word.w=fgetii(f);
			reu_addr=fgetii(f); reu_addr+=fgetc(f)<<16;
			reu_size=fgetii(f); reu_size+=fgetc(f)<<16;
			i-=12+2+3+3; // keep leftovers
		}
		else if (k==0X47454F30) // "GEO0", GeoRAM config
		{
			georam_yes=2+1;
			georam_block=fgetc(f);
			georam_page=fgetc(f)&63;
			i-=2; // keep leftovers
		}
		else if (georam_yes>=2&&i<=65536&&((k>=0X52455531&&k<=0X52455538)||(k>=0X47454F31&&k<=0X47454F38))) // "REU1".."REU8", REU 64K pages + "GEO1".."GEO8", GeoRAM 64K pages
		{
			i=snap_loadchunk(&mem_ram[(k&=15)<<16],65536,f,i,q);
			if (ram_dirty<(k<<=16)) ram_dirty=k; // remember highest page
		}
		else if (k==0X45415359&&i<=256) // "EASY", EasyFlash RAM page
		{
			i=snap_loadchunk(cart_easy,256,f,i,q),
			cart_mode|=+128; // temporary bit, see below
		}
		// ... future blocks will go here ...
		else cprintf("SNAP %08X:%08X?\n",k,i); // unknown type:size
		{ if (i<0) return puff_fclose(f),1; } fseek(f,i,SEEK_CUR); // abort on error!
	}
	if (georam_yes>=2) // adjust variables after loading REU data
	{
		georam_yes&=1; ram_cap=1<<(ram_depth+15);
		while (ram_cap<ram_dirty) ram_cap<<=1,++ram_depth;
		--ram_cap; // binary mask, so it's all 1s
		if (ram_dirty>ram_cap) ram_dirty=ram_cap; else if (ram_dirty) --ram_dirty;
	}
	PSID_STOP;
	if (!cart&&cart_mode) // insert last known cartridge file if active
		if (cart_insert(cart_path)||!cart) cart_reset(); // nuke these variables on failure :-(
	cart_mode&=~128; // remove temporary bit
	m6510_irq=((cia_port_13[0]&128)>>6)+((VICII_TABLE[25]&128)>>7); // see `m6510_int` above; we skip `(cia_port_13[1]&128)+` because it's volatile :-(
	mmu_recalc(); // CPU #1 ports $00 and $01 handle the tape together with CIA #1
	//if ((VICII_TABLE[25]&VICII_TABLE[26]&15)||(cia_port_13[0]&CIA_TABLE_0[13]&31)) m6510_i_t=-2; // pending IRQ workaround?

	debug_reset();
	//MEMBYTE(m6510_tape_index,-1); // clean tape trap cache to avoid false positives
	STRCOPY(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

// "autorun" file and logic operations ------------------------------ //

#ifdef DEBUG
void psid_init(void)
{
	m6510_send(0,0X2F); m6510_send(1,0X35); // map I/O as visible
	VICII_TABLE[17]=0X1B; VICII_TABLE[18]=vicii_irq_y=128;
	VICII_TABLE[25]=VICII_TABLE[26]=0; // wipe all pending VIC-II interrupts
	CIA_TABLE_0[4]=cia_count_a[0]=(63*312)-1; CIA_TABLE_0[5]=cia_count_a[0]>>8;
	CIA_TABLE_0[6]=cia_count_b[0]=0XFFFF; CIA_TABLE_0[7]=cia_count_b[0]>>8;
	CIA_TABLE_0[13]=cia_port_13[0]=0; m6510_send(0XDC0E,0); // wipe all pending CIA #1 interrupts
	if (psid_rsid) // the song sets everything up on its own
		;
	else if (psid_bits&(1<<((psid_song-1)&31))) // CIA?
	{
		CIA_TABLE_0[13]=1; m6510_send(0XDC0E,1);
		mem_ram[psid_poke+0]=0XAD; // "LDA $DC0D"
		mem_ram[psid_poke+1]=0X0D;
		mem_ram[psid_poke+2]=0XDC;
	}
	else // VIC-II!
	{
		VICII_TABLE[26]=1;
		mem_ram[psid_poke+0]=0X8D; // "STA $D019"
		mem_ram[psid_poke+1]=0X19;
		mem_ram[psid_poke+2]=0XD0;
	}
	m6510_int=m6510_irq=0; m6510_p=32+16+4; // wipe interrupt states
	m6510_pc.w=psid_boot; m6510_s=0XFF; m6510_a=psid_song-1;
}

int psid_load(char *s) // load a PSID file and build a minimal framework around it
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1;
	int i=fgetmmmm(f),j,k; j=fgetmm(f); k=fgetmm(f);
	if ((i!=0X50534944&&i!=0X52534944)||!j||k>=256) return puff_fclose(f),1; // wrong magic numbers!
	all_reset();
	mputmmmm(psid_info,i); mputii(&psid_info[4],'0'+j);
	psid_rsid=i==0X52534944;
	i=fgetmm(f); // load address
	j=fgetmm(f); // init address
	psid_play=fgetmm(f);
	psid_last=fgetmm(f); if (psid_last>256) psid_last=0; // tag it as invalid!
	psid_song=fgetmm(f); if (psid_song<1) psid_song=1; if (psid_song>psid_last) psid_song=psid_last;
	psid_bits=fgetmmmm(f);
	MEMZERO(psid_text); fread1(&psid_text[ 0],32,f),fread1(&psid_text[33],32,f),fread1(&psid_text[66],32,f); // ensure all strings end in zero!
	fseek(f,k,SEEK_SET);
	if (!i) i=fgetii(f); // if the load address was zero then we get it from the PRG-like data
	if (!j) j=i; // if the init address was zero then it's the same as the actual load address
	// setup a tiny MOS 6510 stub
	psid_boot=0X0100; // we're hiding in the depth of the stack :-/
	BYTE psid_stub[28]={
		// +0: JSR j
		0X20,j,j>>8,
		// +3: LDA #$34/#$35: STA $01
		0XA9,0X34+psid_rsid,0X85,0X01,
		// +7: CLI: JMP psid_boot+8
		0X58,0X4C,+8,psid_boot>>8,
		// +11: LDA #$35: STA $01
		0XA9,0X35,0X85,0X01,
		// +15: LDA $#01: (dummy) LDA $0000
		0XA9,0X01,0XAD,0,0,
		// +20: JSR psid_play
		0X20,psid_play,psid_play>>8,
		// +23: LDA #$34: STA $01
		0XA9,0X34,0X85,0X01,
		// +27: RTI
		0X40,
	};
	psid_poke=psid_boot+17;
	mputii(&mem_ram[0XFFFE],psid_boot+11); // IRQ
	mputii(&mem_ram[0XFFFA],psid_boot+27); // NMI
	memcpy(&mem_ram[psid_boot=0X0100],psid_stub,sizeof(psid_stub));
	psid_init();
	// load the PSID code proper
	j=fread1(&mem_ram[i],0XFFFA-i,f);
	return puff_fclose(f),!(i>0&&j>0&&psid_last>0);
}
#endif

int any_load(char *s,int q) // load a file regardless of format. `s` path, `q` autorun; 0 OK, !0 ERROR
{
	autorun_mode=0; // cancel any autoloading yet
	if (!video_table_load(s)) video_main_xlat(),video_xlat_clut(); else
	#ifdef DEBUG
	if (!psid_load(s)) disc_disabled|=2; else // SID files ALWAYS take over: disable drive!
	#endif
	if (snap_load(s))
	{
		if (tape_open(s))
		{
			if (disc_open(s,0,0))
			{
				if (cart_insert(s))
				{
					if (bios_load(s))
						return bios_reload(),1; // everything failed!
					else
						all_reset(); // cleanup!
				}
				else
					{ all_reset(); if (!cart) disc_disabled|=2,autorun_t=9,autorun_mode=q?1:2; } // disable disc drive, PRG files are standalone
			}
			else
				{ if (q) cart_remove(),all_reset(),disc_disabled=0,autorun_t=9,autorun_mode=8; } // enable disc drive
		}
		else
			{ if (q) cart_remove(),all_reset(),disc_disabled|=2,autorun_t=9,autorun_mode=3; } // disable disc drive
	}
	if (q) STRCOPY(autorun_path,s);
	return 0;
}

// auxiliary user interface operations ------------------------------ //

char txt_error_snap_save[]="Cannot save snapshot!";
char file_pattern[]="*.csw;*.d64;*.crt;*.prg;*.rom;*.s64;"
#ifdef DEBUG
	"*.sid;"
#endif
	";*.t64;*.tap;*.vpl;*.wav"; // from A to Z!

char session_menudata[]=
	"File\n"
	"0x8300 Open any file..\tF3\n"
	"0xC300 Load snapshot..\tShift-F3\n"
	"0x0300 Load last snapshot\tCtrl-F3\n"
	"0x8200 Save snapshot..\tF2\n"
	"0x0200 Save last snapshot\tCtrl-F2\n"
	"=\n"
	"0x8700 Insert disc in 8:..\tF7\n"
	"0x8701 Create disc in 8:..\n"
	"0x0700 Remove disc from 8:\tCtrl-F7\n"
	#if 0 // *!* TODO
	"0xC700 Insert disc in 9:..\tShift-F7\n"
	"0xC701 Create disc in 9:..\n"
	"0x4700 Remove disc from 9:\tCtrl-Shift-F7\n"
	#endif
	"=\n"
	"0x8800 Insert tape..\tF8\n"
	"0xC800 Record tape..\tShift-F8\n"
	"0x8801 Browse tape..\n"
	"0x0800 Remove tape\tCtrl-F8\n"
	"0x4800 Play tape\tCtrl-Shift-F8\n"
	"0x4801 Flip tape polarity\n"
	"=\n"
	//"0XC500 Load programme..\tShift-F5\n"
	"0xC500 Insert cart/prog..\tShift-F5\n"
	"0x4500 Remove cartridge\tCtrl-Shift-F5\n"
	"=\n"
	"0x0080 E_xit\n"
	"Edit\n"
	"0x8500 Select firmware..\tF5\n"
	"0x0500 Reset emulation\tCtrl-F5\n"
	"0x8F00 Pause\tPause\n"
	"0x8900 Debug\tF9\n"
	//"0x8910 NMI\n"
	"=\n"
	"0x8511 64K RAM\n"
	"0x8512 128K RAM\n"
	"0x8513 192K RAM\n"
	"0x8514 320K RAM\n"
	"0x8515 576K RAM\n"
	"0x8517 GeoRAM mode\n"
	//"=\n"
	"0x8502 Early glue logic\n"
	"0x8501 Early CIA 6526\n"
	"0x8503 Early SID 6581\n"
	"0x8504 Play SID samples\n"
	"0x8505 Disable SID filters\n"
	"0x8508 SID " I18N_MULTIPLY "1\n"
	"0x8509 SID " I18N_MULTIPLY "2 #:$D420\n"
	"0x850A SID " I18N_MULTIPLY "2 #:$DE00\n"
	"0x850B SID " I18N_MULTIPLY "3 $D420:#:$D440\n"
	"0x850C SID " I18N_MULTIPLY "3 $DE00:#:$DF00\n"
	"=\n"
	"0x8B08 Custom VIC-II palette..\n"
	"0x8B18 Reset VIC-II palette\n"
	"0x8506 Disable sprites\n"
	"0x8507 Cancel sprite collisions\n"
	"Settings\n"
	"0x8601 1" I18N_MULTIPLY " real speed\n"
	"0x8602 2" I18N_MULTIPLY " real speed\n"
	"0x8603 3" I18N_MULTIPLY " real speed\n"
	"0x8604 4" I18N_MULTIPLY " real speed\n"
	"0x8600 Run at full throttle\tF6\n"
	//"0x0600 Raise CPU speed\tCtrl-F6\n"
	//"0x4600 Lower CPU speed\tCtrl-Shift-F6\n"
	"0x0601 1" I18N_MULTIPLY " CPU clock\n"
	"0x0602 2" I18N_MULTIPLY " CPU clock\n"
	"0x0603 4" I18N_MULTIPLY " CPU clock\n"
	"0x0604 8" I18N_MULTIPLY " CPU clock\n"
	"=\n"
	"0x0400 Virtual joystick\tCtrl-F4\n"
	"0x0401 Redefine virtual joystick\n"
	"0x4400 Flip joystick ports\tCtrl-Shift-F4\n"
	#ifdef DEBUG
	"0x850D Mute SID voice A\n"
	"0x850E Mute SID voice B\n"
	"0x850F Mute SID voice C\n"
	#endif
	"=\n"
	"0x851F Strict snapshots\n"
	//"0x852F Printer output..\n" // *!* TODO
	"0x8510 Enable disc drive\tShift-F6\n"
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
	"0x8906 Frame blending\n"
	#ifdef VIDEO_FILTER_BLUR0
	"0x8909 Narrow X-blending\n"
	#endif
	#ifndef RGB2LINEAR
	"0x8908 Gamma correction\n"
	#endif
	"=\n"
	"0x9100 Raise frameskip\tNum.+\n"
	"0x9200 Lower frameskip\tNum.-\n"
	"0x9300 Full frameskip\tNum.*\n"
	"0x9400 No frameskip\tNum./\n"
	"=\n"
	"0x8C00 Save screenshot\tF12\n"
	"0x8C04 Output QOI format\n"
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
	session_menucheck(0x8C04,session_qoi3_flag);
	session_menucheck(0xCC00,!!session_filmfile);
	session_menucheck(0x0C00,!!session_wavefile);
	session_menucheck(0x4C00,!!ym3_file);
	session_menucheck(0x8700,!!disc[0]);
	session_menucheck(0xC700,!!disc[1]);
	session_menucheck(0x8800,tape_type>=0&&tape);
	session_menucheck(0xC800,tape_type<0&&tape);
	session_menucheck(0x4800,tape_enabled);
	session_menucheck(0x4801,tape_polarity);
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	session_menucheck(0x0400,session_key2joy);
	session_menucheck(0x4400,key2joy_flag);
	session_menuradio(0x0601+multi_t,0x0601,0x0604);
	session_menucheck(0x0605,power_boost-POWER_BOOST0);
	session_menucheck(0x8501,!cia_nouveau);
	session_menucheck(0x8502,!vic_nouveau);
	session_menucheck(0x8503,!sid_nouveau);
	session_menucheck(0x8504,sid_samples);
	session_menucheck(0x8505,!sid_filters);
	session_menucheck(0x8506,vicii_nosprites);
	session_menucheck(0x8507,vicii_noimpacts);
	session_menuradio(0x8508+sid_extras,0X8508,0X850C);
	#ifdef DEBUG
	session_menucheck(0x850D,sid_quiet[0]);
	session_menucheck(0x850E,sid_quiet[1]);
	session_menucheck(0x850F,sid_quiet[2]);
	#endif
	session_menucheck(0xC500,cart_type>=0);
	session_menucheck(0x8590,!(disc_filemode&2));
	session_menucheck(0x8591,disc_filemode&1);
	session_menucheck(0x8510,!(disc_disabled&1));
	session_menuradio(0x8511+ram_depth,0x8511,0x8515);
	session_menucheck(0x8517,georam_yes);
	session_menucheck(0x851F,!(snap_extended));
	//session_menucheck(0x852F,!!printer); // *!* TODO
	session_menucheck(0x8901,onscreen_flag);
	session_menucheck(0x8902,video_filter&VIDEO_FILTER_MASK_Y);
	session_menucheck(0x8903,video_filter&VIDEO_FILTER_MASK_X);
	session_menucheck(0x8904,video_filter&VIDEO_FILTER_MASK_Z);
	session_menucheck(0x8905,video_lineblend);
	session_menucheck(0x8906,video_pageblend);
	session_menucheck(0x8907,video_microwave);
	#ifndef RGB2LINEAR
	session_menucheck(0x8908,video_gammaflag);
	#endif
	#ifdef VIDEO_FILTER_BLUR0
	session_menucheck(0x8909,video_fineblend);
	#endif
	session_menuradio(0x8A10+(session_fullblit?0:1+session_zoomblit),0X8A10,0X8A15);
	session_menucheck(0x8A00,!session_softblit);
	session_menuradio(0x8A01+session_softplay,0x8A01,0x8A04);
	session_menuradio(0x8B01+video_type,0x8B01,0x8B05);
	session_menuradio(0x0B01+video_scanline,0x0B01,0x0B04);
	reu_table[0]&=64+32; // +128 INTERRUPT PENDING (unused), +64 END OF BLOCK, +32 FAULT
	ram_cap=ram_depth?(ram_kbyte[ram_depth]<<10)-1:0; // nonzero depth enables REU
	if (ram_dirty>ram_cap) ram_dirty=ram_cap; // avoid hot-switch accidents!
	//if (ram_depth>2) reu_table[0]+=16; // +256K REU mode, but only on old models!
	vicii_len_y=312; // PAL/NTSC vertical configuration
	vicii_irq_x=(vicii_len_x=63<<multi_t)-8; // PAL/NTSC horizontal; cfr. "logical" VS "physical" tickers
	if (sid_extras&1)
		SID_TABLE[1]=&mem_i_o[0X420],SID_TABLE[2]=&mem_i_o[0X440];
	else
		SID_TABLE[1]=&mem_i_o[0XE00],SID_TABLE[2]=&mem_i_o[0XF00];
	sid_chips=sid_extras<1?1:sid_extras<3?2:3;
	#if AUDIO_CHANNELS > 1
	session_menuradio(0xC401+audio_mixmode,0xC401,0xC404);
	switch (sid_extras)
	{
		case 1: case 2:
			for (int i=0;i<3;++i)
				sid_stereo[0][i][0]=(256+audio_stereos[audio_mixmode][0])/2,sid_stereo[0][i][1]=(256-audio_stereos[audio_mixmode][0])/2, // 1st chip is LEFT
				sid_stereo[1][i][0]=(256+audio_stereos[audio_mixmode][2])/2,sid_stereo[1][i][1]=(256-audio_stereos[audio_mixmode][2])/2; // 2nd chip is RIGHT
			break;
		case 3: case 4:
			for (int i=0;i<3;++i)
				sid_stereo[0][i][0]=(256+audio_stereos[audio_mixmode][1]+1)/3,sid_stereo[0][i][1]=(256-audio_stereos[audio_mixmode][1]+1)/3, // 1st chip is M
				sid_stereo[1][i][0]=(256+audio_stereos[audio_mixmode][0]+1)/3,sid_stereo[1][i][1]=(256-audio_stereos[audio_mixmode][0]+1)/3, // 2nd chip is L
				sid_stereo[2][i][0]=(256+audio_stereos[audio_mixmode][2]+1)/3,sid_stereo[2][i][1]=(256-audio_stereos[audio_mixmode][2]+1)/3; // 3rd chip is R
			break;
		default: // some songs (f.e. "CATWALK") could overflow with stereo separation at 100%; it shows that the SID is mono because it has one filter, not two :-/
			sid_stereo[0][0][0]=256+audio_stereos[audio_mixmode][0],sid_stereo[0][0][1]=256-audio_stereos[audio_mixmode][0]; // A:B:C order is LEFT:RIGHT:MIDDLE
			sid_stereo[0][1][0]=256+audio_stereos[audio_mixmode][2],sid_stereo[0][1][1]=256-audio_stereos[audio_mixmode][2]; // why? because C can be sampled:
			sid_stereo[0][2][0]=256+audio_stereos[audio_mixmode][1],sid_stereo[0][2][1]=256-audio_stereos[audio_mixmode][1]; // samples will always be MIDDLE!
			#ifdef DEBUG
			for (int i=0;i<3;++i) if (sid_quiet[i]) sid_stereo[0][i][0]=sid_stereo[0][i][1]=0;
			#endif
	}
	#endif
	video_resetscanline(),debug_dirty=1; sprintf(session_info,"%d:%dK %s %sx%c %d.0MHz"//" | disc %s | tape %s | %s"
		,ram_dirty?65+(ram_dirty>>10):64,64+ram_kbyte[ram_depth],georam_yes?"GEO":"REU"
		,sid_nouveau?"8580":"6581",'0'+sid_chips,1<<multi_t);
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
				"F7\tInsert disc into 8:..\t"
				"^F7\tEject disc from 8:"
				"\n"
				/*
				"\t(shift: ..into 9:)\t"
				"\t(shift: ..from 9:)"
				"\n"
				*/
				"F8\tInsert tape.." MESSAGEBOX_WIDETAB
				"^F8\tRemove tape"
				"\n"
				"\t(shift: record..)\t"
				"\t(shift: play/stop)"
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
				"*Return\tMaximize/restore" "\t"
				"\n"
				"*Up\tIncrease zoom" MESSAGEBOX_WIDETAB
				"*Down\tDecrease zoom" "\t"
				// "\n" "^Key =\tControl+Key" MESSAGEBOX_WIDETAB "*Key =\tAlt+Key\n"
			,"Help");
			break;
		case 0x0100: // ^F1: ABOUT..
			session_aboutme(
				"Commodore 64 emulator written by Cesar Nicolas-Gonzalez\n"
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
				{ if (++audio_mixmode>=length(audio_stereos)) audio_mixmode=0; }
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
				audio_mixmode=k-0x8401;
			else
			#endif
			audio_filter=k-0x8401;
			break;
		case 0x0400: // ^F4: TOGGLE JOYSTICK
			if (session_shift)
				key2joy_flag=!key2joy_flag; // FLIP JOYSTICK PORTS
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
				if ((t[4]=session_scan("FIRE"))>=0)
					for (int i=0;i<KBD_JOY_UNIQUE;++i) kbd_k2j[i]=t[i];
			}
			break;
		case 0x8501: // CIA 6526/6526A SWITCH
			cia_nouveau=!cia_nouveau;
			break;
		case 0x8502: // C64 DISCRETE/C64C CUSTOM IC SWITCH
			vic_nouveau=!vic_nouveau;
			break;
		case 0x8503: // SID 8580/6581 SWITCH
			sid_nouveau=!sid_nouveau; sid_shape_setup(); //sid_all_update();
			break;
		case 0x8504: // DISABLE SID SAMPLES
			sid_samples=!sid_samples; sid_all_update();
			break;
		case 0x8505: // DISABLE SID FILTERS
			sid_filters=!sid_filters; sid_all_update();
			break;
		case 0x8506: // DISABLE VIC-II SPRITES
			vicii_nosprites=!vicii_nosprites; VICII_TABLE[31]=VICII_TABLE[30]=0;
			break;
		case 0x8507: // DISABLE SPRITE COLLISIONS
			vicii_noimpacts=!vicii_noimpacts; VICII_TABLE[31]=VICII_TABLE[30]=0;
			break;
		case 0x8508: // SID X1
		case 0x8509: // SID X2 (base:$D420)
		case 0x850A: // SID X2 (base:$DE00)
		case 0x850B: // SID X3 ($D420:base:$D440)
		case 0x850C: // SID X3 ($DE00:base:$DF00)
			if (sid_extras!=k-0X8508) { sid_extras=k-0X8508; sid_all_update(); }
			break;
		#ifdef DEBUG
		case 0x850D: // SID VOICE A OFF
		case 0x850E: // SID VOICE B OFF
		case 0x850F: // SID VOICE C OFF
			sid_quiet[k-0x850D]=!sid_quiet[k-0x850D];
			break;
		#endif
		case 0x8590: // STRICT DISC WRITES
			disc_filemode^=2;
			break;
		case 0x8591: // DEFAULT READ-ONLY
			disc_filemode^=1;
			break;
		case 0x8511: // 64K
		case 0x8512: // 128K
		case 0x8513: // 192K
		case 0x8514: // 320K
		case 0x8515: // 576K
			ram_depth=k-0x8511; mmu_recalc();
			break;
		case 0x8517: // GEORAM/REU
			georam_yes=!georam_yes;
			break;
		case 0x851F: // STRICT SNA DATA
			snap_extended=!snap_extended;
			break;
		case 0x8500: // F5: LOAD FIRMWARE.. // INSERT CARTRIDGE..
			if (session_shift)
			{
				if (s=puff_session_getfile(cart_path,"*.crt;*.prg","Insert cart/prog"))
					if (cart_insert(s)) // error? warn and undo!
						session_message("Cannot insert cartridge!",txt_error);
					else
						all_reset(),autorun_mode=cart?0:(autorun_t=9,2); // reset machine!
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
				{ cart_remove(); }
			//else // redundant!
			all_reset();
			break;
		/* *!* TODO
		case 0x852F: // PRINTER
			if (printer) printer_close(); else
				if (s=session_newfile(NULL,"*.txt","Printer output"))
					if (printer_p=0,!(printer=fopen(s,"wb")))
						session_message("Cannot record printer!",txt_error);
			break;
		*/
		case 0x8600: // F6: TOGGLE REALTIME
			if (!session_shift)
				{ session_fast^=1; break; }
			// +SHIFT: no `break`!
		case 0x8510: // DISC DRIVE
			if (!(disc_disabled^=1)) c1541_reset(); // disabling the disc drive = powering it off, enabling it = powering it on!
			break;
		case 0x8B08: // LOAD VIC-II PALETTE..
			if (s=puff_session_getfile(palette_path,"*.vpl","Custom VIC-II palette"))
			{
				if (video_table_load(s))
					session_message("Cannot load VIC-II palette!",txt_error);
				else
					video_main_xlat(),video_xlat_clut();
			}
			break;
		case 0x8B18: // RESET VIC-II PALETTE
			video_table_reset(); video_main_xlat(),video_xlat_clut();
			break;
		case 0x8601: // REALTIME x1
		case 0x8602: // REALTIME x2
		case 0x8603: // REALTIME x3
		case 0x8604: // REALTIME x4
			session_rhythm=k-0x8600-1; session_fast&=~1;
			break;
		case 0x0600: // ^F6: TOGGLE TURBO 6510
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
				if (s=puff_session_newfile(disc_path,"*.d64",session_shift?"Create disc in B:":"Create disc in A:"))
				{
					if (disc_create(s))
						session_message("Cannot create disc!",txt_error);
					else
						disc_open(s,session_shift,1);
				}
			break;
		case 0x8700: // F7: INSERT DISC..
			if (!disc_disabled)
				if (s=puff_session_getfilereadonly(disc_path,"*.d64",session_shift?"Insert disc into B:":"Insert disc into A:",disc_filemode&1))
					if (disc_open(s,session_shift,!session_filedialog_get_readonly()))
						session_message("Cannot open disc!",txt_error);
			break;
		case 0x0700: // ^F7: EJECT DISC
			disc_close(session_shift);
			break;
		case 0x8800: // F8: INSERT OR RECORD TAPE..
			if (session_shift)
			{
				if (s=puff_session_newfile(tape_path,"*.tap","Record tape"))
					if (tape_create(s))
						session_message("Cannot create tape!",txt_error);
			}
			else if (s=puff_session_getfile(tape_path,"*.csw;*.t64;*.tap;*.wav","Insert tape"))
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
		case 0x0800: // ^F8: REMOVE TAPE, PLAY TAPE
			if (session_shift)
				tape_enabled=!tape_enabled&&tape&&m6510_t64ok!=7;
			else
				tape_enabled=0,tape_close();
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
			video_filter^=VIDEO_FILTER_MASK_Y;
			break;
		case 0x8903: // X-MASKING
			video_filter^=VIDEO_FILTER_MASK_X;
			break;
		case 0x8904: // X-BLENDING
			video_filter^=VIDEO_FILTER_MASK_Z;
			break;
		case 0x8905: // Y-BLENDING
			video_lineblend^=1;
			break;
		case 0x8906: // FRAME BLENDING
			video_pageblend^=1;
			break;
		case 0x8907: // MICROWAVES
			video_microwave^=1;
			break;
		#ifndef RGB2LINEAR
		case 0x8908: // GAMMA BLENDING/CORRECTION
			video_gammaflag^=1; video_recalc();
			break;
		#endif
		#ifdef VIDEO_FILTER_BLUR0
		case 0x8909: // FINE/COARSE X-BLENDING
			video_fineblend^=1; break;
		#endif
		case 0x9000: // NMI (RESTORE key)
			m6510_irq|=+128;
			break;
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
			video_scanline=k-0x0B01;
			break;
		case 0x0B00: // ^F11: SCANLINES
			if (session_shift)
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
		case 0X8C04: // OUTPUT AS QOI
			session_qoi3_flag^=1;
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
		case 0x9500: break; // PRIOR
		case 0x9600: break; // NEXT
		//case 0x9700: break; // HOME
		//case 0x9800: break; // END
		#endif
	}
}

void session_configreadmore(char *s) // parse a pre-processed configuration line: `session_parmtr` keeps the value name, `s` points to its value
{
	int i; char *t=UTF8_BOM(session_parmtr); if (!s||!*s||!*t) {} // ignore if empty or internal!
	else if (!strcasecmp(t,"type")) cia_nouveau=*s&1,vic_nouveau=(*s>>1)&1;
	else if (!strcasecmp(t,"sid1")) sid_filters=~*s&1,sid_samples=(~*s>>1)&1;
	else if (!strcasecmp(t,"bank")) georam_yes=*s&1,ram_depth=(*s>>1)&7,ram_depth=ram_depth>length(ram_kbyte)?0:ram_depth;
	else if (!strcasecmp(t,"unit")) disc_filemode=*s&3,disc_disabled=(*s>>2)&1;
	else if (!strcasecmp(t,"misc")) snap_extended=*s&1,key2joy_flag=(*s>>1)&1;
	else if (!strcasecmp(t,"sids")) sid_nouveau=*s&1,sid_extras=(*s>>1)&7,sid_extras=sid_extras>4?0:sid_extras;
	else if (!strcasecmp(t,"file")) strcpy(autorun_path,s);
	else if (!strcasecmp(t,"snap")) strcpy(snap_path,s);
	else if (!strcasecmp(t,"tape")) strcpy(tape_path,s);
	else if (!strcasecmp(t,"disc")) strcpy(disc_path,s);
	else if (!strcasecmp(t,"bios")) strcpy(bios_path,s);
	else if (!strcasecmp(t,"cart")) strcpy(cart_path,s);
	else if (!strcasecmp(t,"rgbs")) strcpy(palette_path,s);
	else if (!strcasecmp(t,"vjoy")) { if (!hexa2byte(session_parmtr,s,KBD_JOY_UNIQUE)) usbkey2native(kbd_k2j,session_parmtr,KBD_JOY_UNIQUE); }
	else if (!strcasecmp(t,"palette")) { if ((i=*s&7)<5) video_type=i; }
	else if (!strcasecmp(t,"casette")) tape_rewind=*s&1,tape_skipload=(*s>>1)&1,tape_fastload=(*s>>2)&1;
	else if (!strcasecmp(t,"debug")) debug_configread(strtol(s,NULL,10));
}
void session_configwritemore(FILE *f) // update the configuration file `f` with emulator-specific names and values
{
	native2usbkey(kbd_k2j,kbd_k2j,KBD_JOY_UNIQUE); byte2hexa0(session_parmtr,kbd_k2j,KBD_JOY_UNIQUE);
	fprintf(f,"type %d\nsid1 %d\nbank %d\nunit %d\nmisc %d\nsids %d\n"
		"file %s\nsnap %s\ntape %s\ndisc %s\nbios %s\ncart %s\nrgbs %s\n"
		"vjoy %s\npalette %d\ncasette %d\ndebug %d\n",
		cia_nouveau+vic_nouveau*2,(sid_filters?0:1)+(sid_samples?0:2),(ram_depth<<1)+(georam_yes&1),(disc_disabled&1)*4+disc_filemode,(key2joy_flag&1)*2+snap_extended,sid_extras*2+sid_nouveau,
		autorun_path,snap_path,tape_path,disc_path,bios_path,cart_path,palette_path,
		session_parmtr,video_type,tape_rewind+tape_skipload*2+tape_fastload*4,debug_configwrite());
}

// START OF USER INTERFACE ========================================== //

int main(int argc,char *argv[])
{
	session_prae(argv[0]); all_setup(); all_reset();
	int i=0,j,k=m6510_pc.w=0; while (++i<argc) // see later about m6510_pc
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
						georam_yes=1;
						break;
					case 'G':
						georam_yes=0;
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
						if (ram_depth<0||ram_depth>=length(ram_kbyte))
							i=argc; // help!
						break;
					case 'K':
						ram_depth=0;
						break;
					/*
					case 'm':
						k=1; flag_m=(BYTE)(argv[i][j++]-'0');
						if (flag_m<0||flag_m>9)
							i=argc; // help!
						break;
					*/
					case 'o':
						onscreen_flag=1;
						break;
					case 'O':
						onscreen_flag=0;
						break;
					case 'p':
						sid_nouveau=1;
						break;
					case 'P':
						sid_nouveau=0;
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
		else if (any_load(argv[i],1))
			i=argc; // help!
	if (i>argc)
		return
			printfusage("usage: " my_caption " [option..] [file..]\n"
			"  -cN\tscanline type (0..7)\n"
			"  -CN\tcolour palette (0..4)\n"
			"  -d\tdebug mode\n"
			"  -g/G\tenable/disable GeoRAM mode\n"
			"  -j\tenable joystick keys\n"
			"  -J\tdisable joystick\n"
			"  -k0\t64K RAM\n"
			"  -k1\t128K RAM\n"
			"  -k2\t192K RAM\n"
			"  -k3\t320K RAM\n"
			"  -k4\t576K RAM\n"
			//"  -m0\t-\n"
			//"  -m1\t-\n"
			//"  -m2\t-\n"
			//"  -m3\t-\n"
			"  -o/O\tenable/disable onscreen status\n"
			//"  -p/P\-\n"
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
	bdos_load("c1541.rom"); //if (!*c1541_rom&&!disc_disabled) disc_disabled=1; // can't enable the disc drive without its ROM!
	if (k) all_reset(); else if (!m6510_pc.w) mmu_reset(),m6510_reset(); // reset machine again if required; we must also catch the illegal PC=0!
	char *s=session_create(session_menudata); if (s)
		return sprintf(session_scratch,"Cannot create session: %s!",s),printferror(session_scratch),1;
	session_kbdreset();
	session_kbdsetup(kbd_map_xlt,length(kbd_map_xlt)/2);
	video_target=&video_frame[video_pos_y*VIDEO_LENGTH_X+video_pos_x]; audio_target=audio_frame;
	video_main_xlat(),video_xlat_clut(); onscreen_inks(0xAA0000,0x55FF55); session_resize();
	audio_disabled=!session_audio;
	// it begins, "alea jacta est!"
	while (!session_listen())
	{
		while (!session_signal)
		{
			m6510_main( // clump MOS 6510 instructions together to gain speed...
			((VIDEO_LENGTH_X+15-video_pos_x)>>4)
			<<multi_t); // ...without missing any deadlines!
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
					onscreen_byte(+1,-3,disc_track[0],VIA_TABLE_1[0]&4);
					onscreen_byte(+4,-3,disc_track[1],VIA_TABLE_1[0]&8);
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
					if (autorun_mode)
					{
						onscreen_bool(-5,-7,3,1,autorun_t>0),
						onscreen_bool(-5,-4,3,1,autorun_t>0),
						onscreen_bool(-6,-6,1,5,autorun_t>0),
						onscreen_bool(-2,-6,1,5,autorun_t>0);
					}
					else
					{
						onscreen_bool(-5,-6,3,1,kbd_bit_tst(kbd_joy[0])),
						onscreen_bool(-5,-2,3,1,kbd_bit_tst(kbd_joy[1])),
						onscreen_bool(-6,-5,1,3,kbd_bit_tst(kbd_joy[2])),
						onscreen_bool(-2,-5,1,3,kbd_bit_tst(kbd_joy[3])),
						onscreen_bool(-4,-4,1,1,kbd_bit_tst(kbd_joy[4]));
					}
				}
				#ifdef DEBUG
				if (session_audio) // Win32 audio cycle / SDL2 audio queue
				{
					//onscreen_byte(+11,-3,(audio_session/10000)%100,1);
					//onscreen_byte(+13,-3,(audio_session/  100)%100,1);
					//onscreen_byte(+15,-3, audio_session       %100,1);
					onscreen_hgauge(+11,-2,1<<(AUDIO_L2BUFFER-10),1,(audio_session/AUDIO_BYTESTEP)>>10);
				}
				#endif
			}
			// update session and continue
			if (!--autorun_t) autorun_next();
			if (audio_required)
			{
				if (audio_pos_z<AUDIO_LENGTH_Z) audio_main(TICKS_PER_FRAME); // fill sound buffer to the brim!
			}
			if (ym3_file) ym3_write(),ym3_flush();
			sid_frame(); //sid_mute_dumb18(0),sid_mute_dumb18(1),sid_mute_dumb18(2); // force updates!
			//video_clut[31]=video_clut[video_pos_z&7]; // DEBUG COLOUR!
			//if (...) // do this always!
			{
				static BYTE mmu_old=0,mmu_fly=0;
				if (~mmu_old&mmu_out&(128+64+8)) mmu_fly=9; //else if (~mmu_out&mmu_old&(128+64+8)) mmu_fly=0; // CPUPORT complains below 9
				if (!--mmu_fly) mmu_out&=~(128+64+8); // undefined signal loss
				mmu_old=mmu_out; // remember for later
			}
			if (m6510_pc.w>=0XE4E2&&m6510_pc.w<0XE4EB&&mmu_mcr==7&&(tape_skipload|tape_fastload|autorun_mode)) m6510_pc.w=0XE4EB; // tape "FOUND FILENAME" boost
			#ifdef DEBUG
			if (PSID_TRUE) // SID playback mode?
			{
				static BYTE psid_keyz=0;
				i=VIDEO_PIXELS_Y/ONSCREEN_SIZE/2-4;
				j=VIDEO_PIXELS_X>>4;
				onscreen_text(j-20,i+0,&psid_text[00],0);
				onscreen_text(j-20,i+3,&psid_text[33],0);
				onscreen_text(j-20,i+6,&psid_text[66],0);
				onscreen_byte(j+15,i+3,psid_song,psid_keyz);
				onscreen_char(j+17,i+3,'/');
				onscreen_byte(j+18,i+3,psid_last,0);
				onscreen_text(j+15,i+0,psid_info,0);
				onscreen_text(j+15,i+6,"<-O->",0);
				if ((kbd_bit[8]&4)||(kbd_bit[9]&4)) //CURSOR LEFT/JOYSTICK LEFT //(kbd_bit[05]&8)//"MINUS" key
				{
					if (!psid_keyz)
					{
						if (--psid_song<1) psid_song=psid_last;
						psid_init(); psid_keyz=1;
					}
				}
				else if ((kbd_bit[0]&4)||(kbd_bit[9]&8)) //CURSOR RIGHT/JOYSTICK RIGHT //(kbd_bit[05]&1)//"PLUS" key
				{
					if (!psid_keyz)
					{
						if (++psid_song>psid_last) psid_song=1;
						psid_init(); psid_keyz=1;
					}
				}
				else psid_keyz=0;
			}
			#endif
			static int reu_dirtz=-1; if (reu_dirtz!=ram_dirty)
				reu_dirtz=ram_dirty,session_dirty=1;
			if (!tape_fastload) tape_song=0/*,tape_loud=1*/;
			else if (tape_song) /*tape_loud=0,*/--tape_song;
			//else tape_loud=1; // expect song to play for several frames
			if (tape_signal)
			{
				if (tape_signal<2) tape_enabled=0; // stop tape if required
				//tape_output=tape_type<0&&(...); // keep it consistent
				tape_signal=0,session_dirty=1; // update config
			}
			if (tape&&tape_enabled) // tape PLAY bit
				mmu_inp&=~16; // "KNUCKLE BUSTER.TAP" expects something more sophisticated, it hangs if this happens too soon :-/
			else
				mmu_inp|=+16;
			if (tape_browsing) mmu_inp^=--tape_browsing&16; // introduce irregularities in the signal!
			tape_skipping=audio_queue=0; // reset tape and audio flags
			if (tape&&tape_filetell<tape_filesize&&tape_skipload&&!session_filmfile&&!tape_disabled&&!tape_song) // no `tape_loud` but `!tape_song`
				video_framelimit|=(MAIN_FRAMESKIP_MASK+1),session_fast|=2,video_interlaced|=2/*,audio_disabled|=2*/; // abuse binary logic to reduce activity
			else
				video_framelimit&=~(MAIN_FRAMESKIP_MASK+1),session_fast&=~2,video_interlaced&=~2/*,audio_disabled&=~2*/; // ditto, to restore normal activity
			session_update();
			//if (!audio_disabled) audio_main(1+(video_pos_x>>4)); // preload audio buffer
			if (autorun_mode)
				MEMLOAD(kbd_bits,autorun_kbd); // the active keys are fully virtual
			else
			{
				for (i=0;i<length(kbd_bits);++i) kbd_bits[i]=kbd_bit[i]|joy_bit[i]; // mix keyboard + joystick bits
				if (kbd_bit[ 8]) kbd_bits[0]|=kbd_bit[ 8],kbd_bits[1]|=128; // LEFT SHIFT + right side KEY combos (1/2)
				if (kbd_bit[15]) kbd_bits[7]|=kbd_bit[15],kbd_bits[1]|=128; // LEFT SHIFT + left  side KEY combos (2/2)
				if (!(~kbd_bits[9]&3)) kbd_bits[9]-=3; // catch illegal UP+DOWN...
				if (!(~kbd_bits[9]&12)) kbd_bits[9]-=12; // and LEFT+RIGHT joystick bits
			}
		}
	}
	// it's over, "acta est fabula"
	m65xx_close(); cart_remove();
	disc_closeall();
	tape_close(); ym3_close(); if (printer) printer_close();
	return session_byebye(),session_post();
}

BOOTSTRAP

// ============================================ END OF USER INTERFACE //
