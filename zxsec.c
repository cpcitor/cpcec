 // ######  #    #   ####   ######   ####  ------------------------- //
//  """"#"  "#  #"  #""""   #"""""  #""""#  ZXSEC, simple Sinclair ZX //
//     #"    "##"   "####   #####   #    "  Spectrum emulator written //
//    #"      ##     """"#  #""""   #       on top of CPCEC's modules //
//   #"      #""#   #    #  #       #    #  by Cesar Nicolas-Gonzalez //
//  ######  #"  "#  "####"  ######  "####"  since 2019-02-24 till now //
 // """"""  "    "   """"   """"""   """"  ------------------------- //

#define MY_CAPTION "ZXSEC"
#define my_caption "zxsec"

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
// ULA logic+video, Z80 timings and support, snapshots, options...

#include <stdio.h> // printf()...
#include <stdlib.h> // strtol()...
#include <string.h> // strcpy()...

// ZX Spectrum metrics and constants defined as general types ------- //

#define VIDEO_PLAYBACK 50
#define VIDEO_LENGTH_X (56<<4)
#define VIDEO_LENGTH_Y (39<<4)
#ifndef VIDEO_BORDERLESS
#define VIDEO_OFFSET_X (14<<4)
#define VIDEO_OFFSET_Y ( 5<<4) // Pentagon: ( 3<<4)
#define VIDEO_PIXELS_X (40<<4)
#define VIDEO_PIXELS_Y (30<<4) // Pentagon: (34<<4)
#else
#define VIDEO_OFFSET_X (18<<4) // show the original 512x384 screen without the border
#define VIDEO_OFFSET_Y ( 8<<4)
#define VIDEO_PIXELS_X (32<<4)
#define VIDEO_PIXELS_Y (24<<4)
#endif
#define VIDEO_RGB2Y(r,g,b) (video_gamma_prae(r)*77+video_gamma_prae(g)*152+video_gamma_prae(b)*28) // generic RGB-to-Y16 expression

#if defined(SDL2)||!defined(_WIN32)
unsigned short session_icon32xx16[32*32] = {
	#include "zxsec-a4.h"
	};
#endif

// The Spectrum 48K keyboard; later models add pairs f.e. BREAK=CAPS SHIFT+SPACE. Mind the OCTAL values!
// +---------------------------------------------------------------------+
// | 1 30 | 2 31 | 3 32 | 4 33 | 5 34 | 6 44 | 7 43 | 8 42 | 9 41 | 0 40 |
// +---------------------------------------------------------------------+
// | Q 20 | W 21 | E 22 | R 23 | T 24 | Y 54 | U 53 | I 52 | O 51 | P 50 |
// +---------------------------------------------------------------------+
// | A 10 | S 11 | D 12 | F 13 | G 14 | H 64 | J 63 | K 62 | L 61 | # 60 | # 60 RETURN
// +---------------------------------------------------------------------+ $ 00 CAPS SHIFT
// | $ 00 | Z 01 | X 02 | C 03 | V 04 | B 74 | N 73 | M 72 | & 71 | * 70 | & 71 SYMBOL SHIFT
// +---------------------------------------------------------------------+ * 70 SPACE

#define KBD_JOY_UNIQUE 5 // four sides + one fire
unsigned char kbd_joy[]= // ATARI norm: up, down, left, right, fire1-4
	{ 0,0,0,0,0,0,0,0 }; // variable instead of constant, there are several joystick types
#define DEBUG_LONGEST 4 // Z80 opcodes can be up to 4 bytes long
#define MAUS_EMULATION // emulation can examine the mouse
#define MAUS_LIGHTGUNS // lightguns are emulated with the mouse
#define VIDEO_LO_X_RES // no "half" (hi-res) pixels are ever drawn
#define video_hi_x_res 0 // constant, see above
#define INFLATE_RFC1950 // reading SZX files requires inflating RFC1950 data
#define DEFLATE_RFC1950 // writing SZX files may need deflating RFC1950 data
//#define SHA1_CALCULATOR // unused
#define PNG_OUTPUT_MODE 0 // PNG_OUTPUT_MODE implies DEFLATE_RFC1950 and forbids QOI
#define POWER_BOOST1 3 // power_boost default value (enabled)
#define POWER_BOOST0 8
#define AUDIO_ALWAYS_MONO (AUDIO_CHANNELS==1) // false, the PSG is stereo (ACB by default on the 128K)
unsigned char audio_surround=0; // ditto
#include "cpcec-rt.h" // emulation framework!

BYTE joy1_type=1; // i.e. 9867+0 Interface II
BYTE joy1_types[][8]={ // virtual button is repeated for all joystick buttons
	{ 0103,0102,0101,0100,0104,0104,0104,0104 }, // Kempston ("fake" keyboard row 8)
	{ 0041,0042,0044,0043,0040,0040,0040,0040 }, // 9867+0: Sinclair 1, Interface II
	{ 0033,0032,0030,0031,0034,0034,0034,0034 }, // 4312+5: Sinclair 2
	{ 0043,0044,0034,0042,0040,0040,0040,0040 }, // 7658+0: Cursor, Protek, AGF
	{ 0020,0010,0051,0050,0070,0070,0070,0070 }, // QAOP+Space
};

int litegun=0; // 0 = standard joystick, 1 = Gunstick (MHT)
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
	// actual keys; again, notice the octal coding
	KBCODE_1	,0030,	KBCODE_Q	,0020,	KBCODE_A	,0010,	KBCODE_L_SHIFT	,0000,
	KBCODE_2	,0031,	KBCODE_W	,0021,	KBCODE_S	,0011,	KBCODE_Z	,0001,
	KBCODE_3	,0032,	KBCODE_E	,0022,	KBCODE_D	,0012,	KBCODE_X	,0002,
	KBCODE_4	,0033,	KBCODE_R	,0023,	KBCODE_F	,0013,	KBCODE_C	,0003,
	KBCODE_5	,0034,	KBCODE_T	,0024,	KBCODE_G	,0014,	KBCODE_V	,0004,
	KBCODE_6	,0044,	KBCODE_Y	,0054,	KBCODE_H	,0064,	KBCODE_B	,0074,
	KBCODE_7	,0043,	KBCODE_U	,0053,	KBCODE_J	,0063,	KBCODE_N	,0073,
	KBCODE_8	,0042,	KBCODE_I	,0052,	KBCODE_K	,0062,	KBCODE_M	,0072,
	KBCODE_9	,0041,	KBCODE_O	,0051,	KBCODE_L	,0061,	KBCODE_L_CTRL	,0071,
	KBCODE_0	,0040,	KBCODE_P	,0050,	KBCODE_ENTER	,0060,	KBCODE_SPACE	,0070,
	// key mirrors
	KBCODE_R_SHIFT	,0000,	KBCODE_R_CTRL	,0071,
	KBCODE_X_7	,0043,	KBCODE_X_8	,0042,	KBCODE_X_9	,0041,
	KBCODE_X_4	,0033,	KBCODE_X_5	,0034,	KBCODE_X_6	,0044,
	KBCODE_X_1	,0030,	KBCODE_X_2	,0031,	KBCODE_X_3	,0032,
	KBCODE_X_0	,0040,	KBCODE_X_ENTER	,0060,
	// built-in combinations
	// composite key row  8: reserved for KEMPSTON
	// composite key row  9: CAPS SHIFT + row 3
	KBCODE_TAB	,0110, // CAPS SHIFT + "1" (0x18)
	KBCODE_CAPSLOCK	,0111, // CAPS SHIFT + "2" (0x19)
	KBCODE_LEFT	,0114, // CAPS SHIFT + "5" (0x1C) CURSOR LEFT
	// composite key row 10: CAPS SHIFT + row 4
	KBCODE_BKSPACE	,0120, // CAPS SHIFT + "0" (0x20) BACKSPACE
	KBCODE_UP	,0123, // CAPS SHIFT + "7" (0x23) CURSOR UP
	KBCODE_DOWN	,0124, // CAPS SHIFT + "6" (0x24) CURSOR DOWN
	KBCODE_RIGHT	,0122, // CAPS SHIFT + "8" (0x22) CURSOR RIGHT
	//KBCODE_INSERT	,0121, // CAPS SHIFT + "9" (0x21) GRAPH?
	// composite key row 11: CAPS SHIFT + row 7
	KBCODE_ESCAPE	,0130, // CAPS SHIFT + SPACE (0x38)
	// composite key row 12: SYMBOL SHIFT + row 0
	KBCODE_CHR3_1	,0141, // SYMBOL SHIFT + "Z" (0x01) = ":"
	KBCODE_CHR4_3	,0144, // SYMBOL SHIFT + "V" (0x03) = "/"
	KBCODE_CHR4_5	,0143, // SYMBOL SHIFT + "C" (0x03) = "?"
	KBCODE_CHR4_4	,0143, // SYMBOL SHIFT + "C" (0x03) ditto; this key is missing in 104-key layouts!
	// composite key row 13: SYMBOL SHIFT + row 3
	// composite key row 14: SYMBOL SHIFT + row 4
	// composite key row 15: SYMBOL SHIFT + row 7
	KBCODE_CHR4_1	,0173, // SYMBOL SHIFT + "N" (0x3B) = ","
	KBCODE_CHR4_2	,0172, // SYMBOL SHIFT + "M" (0x3A) = "."
	KBCODE_X_DOT	,0172, // SYMBOL SHIFT + "M" (0x3A) ditto
};

const VIDEO_UNIT video_table[16+8+4]= // colour table, 0xRRGGBB style, followed by the 8+4 (R/G+B) components of ULAPLUS
{ // using Gamma = 1.6 as a theoretical middle point between 1.0 (linear) and 2.2 (sRGB)
	0X000000,0X0000A5,0XA50000,0XA500A5,
	0X00A500,0X00A5A5,0XA5A500,0XA5A5A5,
	0X000000,0X0000FF,0XFF0000,0XFF00FF,
	0X00FF00,0X00FFFF,0XFFFF00,0XFFFFFF,
	// ULAPLUS subcomponents: 8G,4B
	//0X0000,0X2400,0X4900,0X6D00,0X9200,0XB600,0XDB00,0XFF00,0X00,0X6D,0XB6,0XFF, // 1.0
	0X0000,0X4C00,0X7500,0X9600,0XB400,0XCF00,0XE800,0XFF00,0X00,0X96,0XCF,0XFF, // 1.6
	//0X0000,0X6900,0X9000,0XAD00,0XC600,0XDB00,0XEE00,0XFF00,0X00,0XAD,0XDB,0XFF, // 2.2
	// n.b.: ULAPLUS 5-5-2 looks like ink 7 (GREY)
};
VIDEO_UNIT video_xlat[16]; // static colours only (dynamic ULAPLUS colours go elsewhere)

// GLOBAL DEFINITIONS =============================================== //

int TICKS_PER_FRAME;// ((VIDEO_LENGTH_X*VIDEO_LENGTH_Y)>>3);
int TICKS_PER_SECOND;// (TICKS_PER_FRAME*VIDEO_PLAYBACK);
// Everything in the ZX Spectrum is tuned to a 3.5 MHz clock,
// using simple binary divisors to adjust the devices' timings;
// the "3.5 MHz" isn't neither exact or the same on each machine:
// 50*312*224= 3494400 Hz on 48K, 50*311*228= 3545400 Hz on 128K.
// (and even the 50Hz screen framerate isn't the exact PAL value!)
int multi_t=0,multi_u=0; // overclocking shift+bitmask

// HARDWARE DEFINITIONS ============================================= //

BYTE mem_ram[10<<14],mem_rom[4<<14]; // memory: 10*16K RAM and 4*16K ROM
BYTE *mmu_ram[4],*mmu_rom[4]; // memory is divided in 4x 16K banks
#define mem_16k (&mem_ram[8<<14]) // dummy bank: write-only area for ROM writes
#define mem_32k (&mem_ram[9<<14]) // dummy bank: read-only area filled with 255
#define PEEK(x) mmu_rom[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!
#define POKE(x) mmu_ram[(x)>>14][x] // WARNING, x cannot be `x=EXPR`!

BYTE type_id=1; // 0=48K, 1=128K, 2=PLUS2, 3=PLUS3
BYTE disc_disabled=0; // disables the disc drive altogether as well as its related logic; +1 = manual, +2 = automatic
BYTE disc_filemode=1; // +1 = read-only by default instead of read-write; +2 = relaxed disc write errors instead of strict
VIDEO_UNIT video_clut[65]; // precalculated colour palette, 16 attr + 48-colour ULAPLUS extra palette + border

// Z80 registers: the hardware and the debugger must be allowed to "spy" on them!

HLII z80_af,z80_bc,z80_de,z80_hl; // Accumulator+Flags, BC, DE, HL
HLII z80_af2,z80_bc2,z80_de2,z80_hl2,z80_ix,z80_iy; // AF', BC', DE', HL', IX, IY
HLII z80_pc,z80_sp,z80_iff,z80_ir; // Program Counter, Stack Pointer, Interrupt Flip-Flops, IR pair
BYTE z80_imd,z80_r7; // Interrupt Mode // low 7 bits of R, required by several `IN X,(Y)` operations

// the Dandanator cartridge system can spy on the Z80 and trap its operations

#define Z80_DANDANATOR
BYTE *mem_dandanator=NULL; char dandanator_path[STRMAX]="";
WORD dandanator_trap,dandanator_temp; // Dandanator-Z80 watchdogs
BYTE dandanator_cfg[8]; // CONFIG + OPCODE + PARAM1 + PARAM2 + active + return + asleep + EEPROM
int dandanator_canwrite=0,dandanator_dirty,dandanator_base; // R/W status

// the BETA128 disc interface can spy on the Z80, too!

#define TRDOS_AVGTRACKS 80 // typical discs are 640K long...
#define TRDOS_MAXTRACKS 86 // but some discs are up to 688K

#define DISKETTE_PAGE 256
#define DISKETTE_TRACK99 99
#define DISKETTE_SECTORS 16
#define DISKETTE_PC (z80_pc.w)
#define diskette_filemode disc_filemode // shared by FDC765 and WD1793
#define diskette_drives 2
#include "cpcec-wd.h"

FILE *trdos[4]={NULL,NULL,NULL,NULL}; char trdos_path[STRMAX]=""; // file handles and current path
int trdos_mapped=0; BYTE trdos_rom[1<<14],diskette_system=0;
#define trdos_setup() diskette_setup()
#define trdos_closeall() (trdos_close(0),trdos_close(1))
int trdos_load(char *s) // load BETA128 TR-DOS BIOS. `s` path; 0 OK, !0 ERROR
{
	*trdos_rom=0; // tag ROM as empty
	FILE *f=fopen(strcat(strcpy(session_substr,session_path),s),"rb"); if (!f) return 1; // fail!
	int i=fread1(trdos_rom,1<<14,f); i+=fread1(trdos_rom,1<<14,f);
	return fclose(f),i!=(1<<14); //||!equalsmmmm(&trdos_rom[0X3D03],0X00181400); // TR-DOS fingerprint
}

#define trdos_reset() (diskette_reset(),diskette_system=4) // reset BETA128 logic; beware, don't touch `trdos_mapped`!
void trdos_close(int d) // close BETA128 disc in drive `d`(0 = A:), if any
{
	if (diskette_size[d]<0)
		fseek(trdos[d],0,SEEK_SET),fwrite1(diskette_mem[d],-diskette_size[d],trdos[d]),fsetsize(trdos[d],-diskette_size[d]);
	if (diskette_mem[d])
		free(diskette_mem[d]),diskette_mem[d]=NULL;
	if (trdos[d])
		puff_fclose(trdos[d]),trdos[d]=NULL;
	diskette_size[d]=0;
}
int trdos_create(char *s) // create a blank BETA128 disc in path `s`; !0 ERROR
{
	FILE *f=fopen(s,"wb"); if (!f)
		return 1; // cannot create disc!
	BYTE t[1<<8]; // a whole sector
	MEMZERO(t);
	for (int i=8;i;--i) fwrite1(t,sizeof(t),f);
	//t[0XE1]=0; // first sector
	t[0XE2]=1; // first track
	t[0XE3]=0X16; // 80 tracks, 2 sides
	//t[0XE4]=0; // no files
	t[0XE5]=240;
	t[0XE6]=9;
	t[0XE7]=0X10; // TR-DOS filesystem
	memset(&t[0XEA],' ',9);
	strcpy(&t[0XF5],"SINCLAIR");
	fwrite1(t,sizeof(t),f);
	MEMZERO(t);
	for (int i=TRDOS_AVGTRACKS*32-9;i;--i) fwrite1(t,sizeof(t),f);
	return fclose(f);
}
int trdos_open(char *s,int d,int canwrite) // insert a BETA128 disc from path `s` in drive `d`; !0 ERROR
{
	trdos_close(d); //if (!*trdos_rom) return 1; // no TR-DOS ROM!
	if (!(diskette_mem[d]=malloc(TRDOS_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE)))
		return 1; // memory error!
	if (!(diskette_canwrite[d]=canwrite)||!(trdos[d]=puff_fopen(s,"rb+"))) // "rb+" allows modifying the disc file;
		if (diskette_canwrite[d]=0,!(trdos[d]=puff_fopen(s,"rb"))) // fall back to "rb" if "rb+" is unfeasible!
			return trdos_close(d),1; // file error!
	int i=fread1(diskette_mem[d],9,trdos[d]); if (i<9) return trdos_close(d),1; // truncated!!
	if (!memcmp(diskette_mem[d],"SINCLAIR",8)&&diskette_mem[d][8]<'B') // valid SCL header?
	{
		int j,k=diskette_mem[d][8];
		memset(diskette_mem[d],0,4096);
		for (i=16,j=0;j<k;++j) // build file list (the first 4k)
		{
			if (fread1(&diskette_mem[d][16*j],14,trdos[d])<14)
				return trdos_close(d),1; // truncated file entry!
			diskette_mem[d][16*j+14]=i&15;
			diskette_mem[d][16*j+15]=i>>4;
			i+=diskette_mem[d][16*j+13];
		}
		//cprintf("SCL header: %d files in %d sectors.\n",k,i);
		diskette_mem[d][0X008E1]=i&15;
		diskette_mem[d][0X008E2]=i>>4;
		diskette_mem[d][0X008E3]=0X16; // 80 tracks, 2 sides
		diskette_mem[d][0X008E4]=k;
		diskette_mem[d][0X008E5]=j=(2560-16-i); // skip the first 4k
		diskette_mem[d][0X008E6]=j>>8;
		diskette_mem[d][0X008E7]=0X10; // TR-DOS filesystem
		memset(&diskette_mem[d][0X008EA],' ',9);
		strcpy(&diskette_mem[d][0X008F5],"SINCLAIR"); //memset(&diskette_mem[d][0X008F5],' ',8);
		i=4096; diskette_canwrite[d]=0; // SCL files are read-only
	}
	// either TRD or SCL, read the remainder of the file
	i+=fread1(&diskette_mem[d][i],(TRDOS_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE)-i,trdos[d]);
	i+=fread1(diskette_mem[d],(TRDOS_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE),trdos[d]); // catch overly big files!
	if (i<4096||i>(TRDOS_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE)||diskette_mem[d][0X008E7]!=0X10||(diskette_mem[d][0X31]>0&&diskette_mem[d][0X31]<3))
		return trdos_close(d),1; // not a valid TR-DOS disc!
	if (i<(TRDOS_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE))
		memset(&diskette_mem[d][i],0,(TRDOS_MAXTRACKS*2*DISKETTE_SECTORS*DISKETTE_PAGE)-i); // sanitize unused space
	diskette_sides[d]=(diskette_mem[d][0X008E3]&8)?1:2;
	diskette_tracks[d]=TRDOS_AVGTRACKS>>(diskette_mem[d][0X008E3]&1); // i.e. 40 if true, 80 if false
	cprintf("TRDOS format: %d tracks and %d sides.\n",diskette_tracks[d],diskette_sides[d]);
	STRCOPY(trdos_path,s);
	return diskette_size[d]=diskette_tracks[d]*diskette_sides[d]*DISKETTE_SECTORS*DISKETTE_PAGE,0;
}

// 0x??FE,0x7FFD,0x1FFD: ULA 48K,128K,PLUS3 ------------------------- //

BYTE ula_v1,ula_v2,ula_v3; // 48K, 128K and PLUS3 respectively
#define ULA_V1_ISSUE3 16 // the Issue-3 ULA port base value
#define ULA_V1_ISSUE2 24 // ditto, on Issue-2
BYTE psg_disabled=0,ula_v1_issue=ULA_V1_ISSUE3,ula_v1_cache=0; // auxiliar ULA variables
BYTE *ula_screen=&mem_ram[0X14000]; int ula_bitmap,ula_attrib; // VRAM pointers
BYTE ula_clash[2][1<<16],*ula_clash_mreq[5],*ula_clash_iorq[5]; // the fifth entry stands for constant clashing
int ula_clash_z; // the in-frame T counter, a 17-bit cursor that follows the ULA clash map
int ula_start_x,ula_limit_x,ula_start_y,ula_limit_y,ula_irq_0_x; // horizontal+vertical limits
char ula_fix_chr,ula_fix_out; // ULA timing adjustments for attribute and border effects
char ula_sixteen=0,ula_pentagon=0,ula_latetiming=1; // 16K mode, Pentagon logic and IRQ type
void ula_setup_clash(int j,int l,int x0,int x1,int x2,int x3,int x4,int x5,int x6,int x7)
{
	MEMZERO(ula_clash); // non-contended areas lack clash
	for (int y=0;y<192;j+=l-128,++y) for (int x=0;x<16;++x)
		ula_clash[1][j++]=x0,
		ula_clash[1][j++]=x1,
		ula_clash[1][j++]=x2,
		ula_clash[1][j++]=x3,
		ula_clash[1][j++]=x4,
		ula_clash[1][j++]=x5,
		ula_clash[1][j++]=x6,
		ula_clash[1][j++]=x7;
}
#define ula_setup() ((void)0)
void ula_update(void) // update ULA settings according to model
{
	// the non-Pentagon models' contention timing tables can be precalculated
	if (type_id<1)
		ula_setup_clash(14335,224,6,5,4,3,2,1,0,0); // ULA v1: 48K
	else if (type_id<3)
		ula_setup_clash(14361,228,6,5,4,3,2,1,0,0); // ULA v2: 128K,PLUS2
	else
		ula_setup_clash(14365,228,1,0,7,6,5,4,3,2); // ULA v3: PLUS3
	// the Pentagon timings were measured with the beamracing-heavy demos "ACROSS THE EDGE" and "KPACKU DELUXE"
	ula_fix_out=ula_pentagon?  7:type_id?  2:  0; // contention tests ULA48, ULA128, ULA128P3, FPGA48ALL and FPGA128
	ula_fix_chr=ula_pentagon? 42:type_id?  1:  0; // NIRVANA games DREAMWALKER, MULTIDUDE, SUNBUCKET, STORMFINCH...
	ula_start_x=ula_pentagon? 38:type_id? 39: 38; // HBLANK position
	ula_limit_x=ula_pentagon? 56:type_id? 57: 56; // characters per scanline
	ula_start_y=ula_pentagon?-17:type_id?  1:  0; // vertical offset
	ula_irq_0_x=ula_pentagon||(type_id>0&&type_id<3)?9:8; // 48K and +3/+2A, 32T; 128K/+2 and Pentagon, 36T
	ula_limit_y=ula_pentagon?303:312; // VBLANK position (not frame height)
	TICKS_PER_SECOND=(TICKS_PER_FRAME=(ula_limit_y-ula_start_y)*ula_limit_x*4)*VIDEO_PLAYBACK;
	if (!type_id&&ula_sixteen)
		memset(mem_32k,-1,1<<14); // used by 16K models, where POKE must do nothing and PEEK must not return ZERO
}
BYTE *ula_stormy=&mem_ram[0X14000]; // the ULA snow bank+offset, default = (5<<14)+0
int ula_snow_disabled=1,ula_snow_a; // the ULA snow flags and parameters
void ula_stormy_calc(BYTE r)
{
	if (!ula_snow_a)
		ula_stormy=ula_screen; // no snow, both VRAM blocks are one and the same
	else if (r&=ula_snow_a,type_id)
	{
		// https://spectrumcomputing.co.uk/forums/viewtopic.php?p=114736#p114736 (Weiv);
		// Upper Page: | Screen 0 (p5) | Screen 1 (p7)
		//   1/3/5/7   |    1/1/5/5    |    3/3/7/7
		// see also the videos by Iceknight featuring snow in the 128K versions of "The Ninja Warriors"
		ula_stormy=mem_ram+0X04000+r; // "r" is just a byte: the worst possible case is $1C0FF..$1EAFF
		if (ula_v2&4) ula_stormy+=0X10000;
		if (ula_v2&8) ula_stormy+=0X08000;
	}
	else
		ula_stormy=ula_screen+r; // 48K snow falls within the same block
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
	if (ula_pentagon||type_id>3||multi_t>0) // models and configurations without contention
	{
		ula_clash_mreq[0]=ula_clash_mreq[1]=ula_clash_mreq[2]=ula_clash_mreq[3]=ula_clash_mreq[4]=
		ula_clash_iorq[0]=ula_clash_iorq[1]=ula_clash_iorq[2]=ula_clash_iorq[3]=ula_clash_iorq[4]=ula_clash[0];
	}
	else
	{
		ula_clash_mreq[4]=ula_clash[1];
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
		if (/*mmu_rom[0]==&mem_rom[0xC000-0x0000]&&*/(trdos_mapped&*trdos_rom)) // 0XF3 in TRDOS.ROM
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
	ula_screen=mem_ram+((ula_v2&8)?0x1C000:0x14000); // bit 3: VRAM is bank 5 (OFF) or 7 (ON)
	ula_stormy_calc((ula_stormy-ula_screen)&255); // the snow bank must be updated with the VRAM (f.e. "ELYSIUM STATE" lady-in-snow part)
	#ifdef Z80_DANDANATOR // Dandanator is always the last part of the MMU update
	if (mem_dandanator) // emulate the Dandanator (and more exactly its Spectrum memory map) only when a card is loaded
		if (dandanator_cfg[4]<32)
			mmu_rom[0]=&mem_dandanator[(dandanator_cfg[4]<<14)-0x0000];
}
#define dandanator_clear() (dandanator_cfg[0]=dandanator_cfg[1]=dandanator_cfg[2]=dandanator_cfg[3]=dandanator_temp=0)
void dandanator_update(void) // parse and run Dandanator commands, if any
{
	if (mem_dandanator) // ignore if empty
	{
		if (dandanator_cfg[0]==46) // wake up!
		{
			if (dandanator_cfg[1]==dandanator_cfg[2]) // parameters must match
			{
				cprintf("DAN! %08X: 046,%03d\n",z80_pc.w,dandanator_cfg[1]);
				if (dandanator_cfg[1]==1) dandanator_cfg[6]|=4; // go to sleep
				else if (dandanator_cfg[1]==16) dandanator_cfg[6]&=~4; // wake up
				else if (dandanator_cfg[1]==31) dandanator_cfg[6]|=8; // sleep till reset
			}
		}
		else if (!dandanator_cfg[6]) // ignore if asleep
		{
			cprintf("DAN! %08X: %03d,%03d,%03d,%03d\n",z80_pc.w,dandanator_cfg[0],dandanator_cfg[1],dandanator_cfg[2],dandanator_cfg[3]);
			if (dandanator_cfg[0]>0&&dandanator_cfg[0]<34) // immediate bank change
				dandanator_cfg[4]=dandanator_cfg[0]-1;
			else switch(dandanator_cfg[0])
			{
				case 34: // vanish and sleep!
					dandanator_cfg[4]=32,dandanator_cfg[6]|=4; break;
				case 36: // reset Z80!
					z80_iff.w=z80_pc.w=0; break;
				case 40: // bank + flags
				{
					if (dandanator_cfg[2]&3) // reset or NMI? changes are immediate!
						dandanator_cfg[4]=dandanator_cfg[1]-1,z80_iff.w=0,z80_pc.w=/*(dandanator_cfg[2]&1)?*/0/*:0x66*/; // is the NMI ever used?
					else // changes aren't immediate, wait till next RET
						dandanator_cfg[5]=dandanator_cfg[1];
					dandanator_cfg[6]=dandanator_cfg[2]&12; break; // +4 wakes up with command 46, +8 sleeps till reset
				}
				case 48: // EEPROM: uses the dummy 16K bank as a bridge
				{
					if (dandanator_cfg[1]==16)
						dandanator_base=-1; // get ready for bridge setup with a later LD (HL),A
					else if (dandanator_cfg[1]==32) // request EEPROM sector for writing
						if (dandanator_cfg[7]=dandanator_cfg[2]&127) // keep EEPROM sector 0 read-only
							;//if (dandanator_canwrite&&mem_dandanator)
								//memset(mem_16k,-1,4<<12),dandanator_dirty=1; // reset dummy bank
					break;
				}
			}
			mmu_update(); // nudge the MMU even if there are no changes; this is summoned on dandanator_reset()!
		}
	}
	dandanator_clear(); // update MMU and reset command queue
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
void dandanator_reset(void)
{
	MEMZERO(dandanator_cfg); dandanator_trap=dandanator_temp=0;
	dandanator_update();
	#endif
}

#define ula_v1_send(i) (ula_v1=i,(ulaplus_table[64]&ulaplus_enabled)||(video_clut[64]=video_xlat[ula_v1&7]))
#define ula_v2_send(i) (ula_v2=i,mmu_update())
#define ula_v3_send(i) (ula_v3=i,mmu_update())

BYTE ulaplus_enabled=1,ulaplus_index,ulaplus_table[65]; // ULAPLUS 64-colour palette + configuration byte (default 0, disabled)

#define ulaplus_clut_calc(i) (video_xlat_rgb(video_table[16+(i>>5)]+video_table[16+((i>>2)&7)]*256+video_table[24+(i&3)]))
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
}
void video_xlat_clut(void) // precalculate palette following `video_type`; part of it is managed by the ULAPLUS palette
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
			video_clut[i]=video_xlat[i];
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
			video_xlat_clut(); // recalculate whole palette on mode change
		else if (ulaplus_table[64]&ulaplus_enabled)
		{
			video_clut[ulaplus_index]=ulaplus_clut_calc(i);
			if (ulaplus_index==8) video_clut[64]=video_clut[ulaplus_index]; // border is CLUT #8
			ula_clut_send(ulaplus_index); // modify just one entry
		}
	}
}

int z80_irq; // IRQ status: +1 IRQ (ULA frame event)
int z80_int; // Z80 INT flag (IFF1 latch + HALT mode)
void ula_reset(void) // reset the ULA
{
	ula_v1=ula_v2=ula_v3=ulaplus_index=ulaplus_table[64]=0;
	video_xlat_clut();
	ula_update(),mmu_update();
	ula_bitmap=0; ula_attrib=0x1800;
}

// 0xBFFD,0xFFFD: PSG AY-3-8910 ------------------------------------- //

#define PSG_MAX_VOICE 10922 // 32768/3= 10922
#define PSG_TICK_STEP 16 // 3.5 MHz /2 /16 = 109375 Hz
#define PSG_KHZ_CLOCK 1750 // compare with the 2000 kHz YM3 standard
#define PSG_MAIN_EXTRABITS 0 // "QUATTROPIC" [http://randomflux.info/1bit/viewtopic.php?id=21] improves weakly with >0
#define PSG_PLAYCITY 1 // the TURBO SOUND card contains one chip...
#define PSG_PLAYCITY_XLAT(x) psg_outputs[(x)] // ...playing at the PSG's same intensity
#define playcity_hiclock TICKS_PER_SECOND // the TURBO SOUND clock is pegged to the main clock
#define playcity_loclock (AUDIO_PLAYBACK*16)
#define PSG_PLAYCITY_RESET (playcity_active=0) // the TURBO SOUND card uses a switch
int playcity_disabled=1,playcity_active=0; // this chip is an extension (disabled by default)
int dac_disabled=1; // Covox $FB DAC, enabled by default on almost every Pentagon 128, but missing everywhere else :-(
#include "cpcec-ay.h"

#include "cpcec-ym.h"
void ym3_write(void) { ym3_write_ay(psg_table,&psg_hard_log,PSG_KHZ_CLOCK); }

int dac_extra=0,dac_delay=0,dac_voice=0,tape_loud=1,tape_song=0; // DAC and tape levels and flags
#define dac_frame() ((dac_extra>>=1)?0:dac_delay>0?dac_voice=dac_voice*63/64:++dac_delay) // soften signal as time passes

// behind the ULA and the PSG: PRINTERS ----------------------------- //

FILE *printer=NULL; int printer_p=0; BYTE printer_t[256+2]; // the buffer MUST be at least 258 bytes long!!
void printer_flush(void) { fwrite1(printer_t,printer_p,printer),printer_p=0; }
void printer_close(void) { printer_flush(),fclose(printer),printer=NULL; }

// each model has its own printer interface: the PLUS3 printer is a 8-bit data port;
int printer_8,printer_1; // the 128K printer is based on a 1-bit serial port;
void printer_line(void) // the 48K ZX Printer is 100% graphical (but we can cheat)
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
#define tape_disabled (!tape_enabled) // the machine cannot disable the tape when we enable it
#define TAPE_TAP_FORMAT // required!
//#define TAPE_CAS_FORMAT // useless outside MSX
#define TAPE_TZX_KANSAS // useless outside MSX, are there any non-MSX tapes (besides our own!) using this method?
//#define FASTTAPE_DUMPER // overkill!
#include "cpcec-k7.h"

// 0x1FFD,0x2FFD,0x3FFD: FDC 765 ------------------------------------ //

#define DISC_PARMTR_UNIT (disc_parmtr[1]&1) // CPC hardware is limited to two drives;
#define DISC_PARMTR_UNITHEAD (disc_parmtr[1]&5) // however, it allows two-sided discs.
#define DISC_TIMER_INIT ( 4<<6) // rough approximation: cfr. CPCEC for details. "GAUNTLET 3" works fine.
#define DISC_TIMER_BYTE ( 2<<6) // rough approximation, too
#define DISC_WIRED_MODE 0 // PLUS3 FDC is unwired, like the CPC; "HATE.DSK" proves it.
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

#define Z80_DNTR_0X3A(w,b) do{ if (w>=0X4000&&w<=0X7FFF) ula_bus3=b; }while(0) // this isn't Dandanator logic, but it operates the same way
int ula_bus,ula_bus3; // floating bus: -1 if we're beyond the bitmap, latest ATTRIB otherwise; notice that PLUS3 uses a different contended bus
int ula_count_x=0,ula_count_y=0; // horizontal+vertical sync counters
int ula_shown_x,ula_shown_y=192; // horizontal+vertical bitmap/attrib counters
int ula_count_z=0; // current T within a character (0..3)
BYTE chromatrons=0,chromatronz=0; // 0 when disabled, otherwise flips between %01010101 and %10101010
#define ULA_GET_T() (((ula_count_y-ula_start_y)*ula_limit_x+ula_count_x)*4+ula_count_z) // current T within a frame, f.e. 0..69887 in a Spectrum 48K
void ULA_SET_T(int x) // sets the internal ULA clock: `x` is a value between 0 and 69887 (48K), 70907 (128K) or whatever the current machine is using
{
	ula_count_z=x&3; x>>=2;
	ula_count_x=(x%ula_limit_x);
	int i=ula_count_y; ula_count_y=(x/ula_limit_x)+ula_start_y; i=ula_count_y-i;
	video_pos_y+=i*=2; frame_pos_y+=i; video_target+=i*VIDEO_LENGTH_X; // adjust!
}

INLINE void video_main(int t) // render video output for `t` clock ticks; t is always nonzero!
{
	int a=ula_bus,b; // `ula_bus` is required because the video loop may fail if the Z80 is overclocked
	for (ula_count_z+=t;ula_count_z>=4;ula_count_z-=4)
	{
		if (UNLIKELY(ula_shown_x==ula_start_x)) // HBLANK? (the Pentagon timings imply this test is done in advance)
		{
			if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y&&frame_pos_y==video_pos_y)
			{
				if (ula_shown_y>=0&&ula_shown_y<192) if (chromatronz)
				{
					VIDEO_UNIT *tt=video_target-video_pos_x+(VIDEO_OFFSET_X+VIDEO_PIXELS_X/2)-256;
					char qq=((video_pos_y)^(video_interlaces<<1))&2;
					for (int nn=512/4;nn;++qq,tt+=4,--nn) // let's do a quick jab at "Chromatrons Attack!!!"
					{
						VIDEO_UNIT ss[2]={tt[0],tt[2]}; // the pattern for green-on-magenta is white-black-white-black...
						if (qq&2) // ...but it flips to magenta-on-green every four pixels, and on every line and frame too
						{
							tt[0]=tt[1]=(ss[0]&0XFF00FF)|((((ss[1]&0X00FF00)+(ss[0]&0X00FF00)+0X000100)>>1)&0X00FF00);
							tt[2]=tt[3]=(ss[1]&0X00FFFF)|((((ss[1]&0XFF0000)+(ss[0]&0XFF0000)+0X010000)>>1)&0XFF0000);
						}
						else
						{
							tt[0]=tt[1]=(ss[0]&0X00FFFF)|((((ss[1]&0XFF0000)+(ss[0]&0XFF0000)+0X010000)>>1)&0XFF0000);
							tt[2]=tt[3]=(ss[1]&0XFF00FF)|((((ss[1]&0X00FF00)+(ss[0]&0X00FF00)+0X000100)>>1)&0X00FF00);
						}
					}
				}
				video_drawscanline();
			}
			video_nextscanline(0); // scanline event!
		}
		if ((video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)&&(video_pos_x>VIDEO_OFFSET_X-16&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X))
		{
			if ((ula_shown_y>=0&&ula_shown_y<192)&&(ula_shown_x>=0&&ula_shown_x<32))
			{
				if (ula_shown_x&1)
					a=ula_screen[ula_attrib++],b=ula_screen[ula_bitmap++];
				else
					a=ula_stormy[ula_attrib++],b=ula_stormy[ula_bitmap++],ula_bus3=ula_screen[ula_attrib]; // the PLUS3 floating bus is special
			}
			else
				a=-1; // border! (no matter how badly we do, the bitmap is always inside the visible screen)
			if (frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
			{
				static BYTE a0,b0;
				if (a<0) // BORDER
				{
					VIDEO_UNIT p=video_clut[64];
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p; VIDEO_NEXT=p;
					video_pos_x+=16;
				}
				else if (ula_shown_x&1) // BITMAP, 2nd character
				{
					if (chromatronz) // this is where "NOP.szx" and "BrightMiner.z80" get their hybrid brightness from
					{
						if (b0==0XAA) if ((a0&100)==32) a0+=64;
						if (b ==0XAA) if ((a &100)==32) a +=64;
					}
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
			video_pos_x+=16,video_target+=16;
		if (UNLIKELY(++ula_shown_x>=ula_limit_x)) // end of bitmap?
		{
			ula_shown_x=0; ++ula_shown_y;
			if (ula_shown_y>=0&&ula_shown_y<192)
			{
				ula_bitmap=((ula_shown_y&192)<<5)+((ula_shown_y&56)<<2)+((ula_shown_y&7)<<8);
				ula_attrib=0x1800+((ula_shown_y&248)<<2); ula_stormy_calc(z80_ir.b.l);
			}
		}
		if (UNLIKELY(++ula_count_x>=ula_limit_x)) // end of scanline?
		{
			chromatronz=chromatrons&&(video_type>0&&video_type<4);
			ula_count_x=0;
			// for lack of a more precise timer, the scanline is used to refresh the ULA's unstable input bit 6:
			// - Issue-2 systems (the first batch of 48K) make the bit depend on ULA output bits 3 and 4.
			// - Issue-3 systems (later batches of 48K) make the bit depend on ULA output bit 4.
			// - Whatever the issue ID, 48K doesn't update the unstable bit at once; it takes a while.
			// - 128K and later systems always mask the bit out.
			ula_v1_cache=type_id?64:ula_v1&ula_v1_issue?0:64;
			// "Abu Simbel Profanation" (menu doesn't obey keys; in-game is stuck jumping to the right) and "Rasputin" (menu fails to play the music) rely on this on 48K.
			if (++ula_count_y>=ula_limit_y) // end of frame?
			{
				if (!(video_pos_z&15)) // FLASH update?
					if (!(ulaplus_table[64]&ulaplus_enabled))
						ula_clut_flash(); // ULAPLUS lacks FLASH!
				ula_shown_x=ula_fix_chr; // renew frame vars
				ula_shown_y=(ula_count_y=ula_start_y)-64; // lines of BORDER above the bitmap
				video_newscanlines(video_pos_x,(ula_start_y+(ula_fix_chr>ula_start_x))*2); // end of frame!
				ula_snow_a=(!ula_snow_disabled&&(ula_clash_mreq[z80_ir.b.h>>6]!=ula_clash[0]))?31:0; // ULA snow is tied to memory contention: "Egghead 4"
				ula_clash_z=ULA_GET_T(); // keep consistency across frames
				z80_irq=1; // snap_load() must be able to reset `z80_irq` to avoid nasty surprises, f.e. COSANOSA.SNA
			}
		}
	}
	ula_bus=a; // -1 in border, 0..255 (ATTRIB) in bitmap; cfr. z80_recv_ula()
	if (ula_count_x>=ula_irq_0_x) z80_irq=0; // "TIMING TESTS 128K" relies on LATE TIMINGS (-1..31 T), but we stick to EARLY TIMINGS (0..32 T): they're stable!
}

void audio_main(int t) // render audio output for `t` clock ticks; t is always nonzero!
	{ psg_main(t,((tape_status&tape_loud)<<12)+dac_voice); } // ULA MIXER: tape input (BIT 2) + beeper (BIT 4) + tape output (BIT 3; "Manic Miner" song, "Cobra's Arc" speech)

// autorun runtime logic -------------------------------------------- //

BYTE snap_done; // avoid accidents with ^F2, see all_reset()
char autorun_path[STRMAX]="",autorun_s[STRMAX]; BYTE autorun_m=0; int autorun_t=0;
BYTE autorun_k,kbd_bits[9]; // automatic keypresses
int autorun_kbd_bit(int k) // combined autorun+keyboard+joystick bits
{
	if (autorun_m) return k==6?autorun_k:0; // RETURN is the only key that matters
	if (litegun)
	{
		#define kbd_bit_gunstick(button,sensor) (session_maus_z?button+((video_litegun&0x00C000)?sensor:0):0) // GUNSTICK detects bright pixels
		if (k==8) return kbd_bit_gunstick(16,4); // GUNSTICK on KEMPSTON
		if (k==4) return kbd_bit_gunstick(1,4); // GUNSTICK on SINCLAIR 1 ("SOLO")
		//if (k==3) return kbd_bit_gunstick(16,4); // GUNSTICK on SINCLAIR 2
		#undef kbd_bit_gunstick
	}
	return kbd_bits[k];
}
INLINE void autorun_next(void) // handle AUTORUN
{
	switch (autorun_m)
	{
		case 1: // 48K and menu-less 128K: type 'LOAD ""' and press RETURN
		case 2: // BETA128: type 'RANDOMIZE USR 15619: REM: RUN"filename"' and press RETURN
			if (diskette_mem[0])
			{
				BYTE *t=NULL,s[]={249,192,'1','5','6','1','9',58,234,58,247,34,'b','o','o','t',32,32,32,32,34,13,128};
				MEMSAVE(&POKE(0X5CCC),s); POKE(0X5C61)=0XCC+sizeof(s);
				for (int i=0;i<0x0800;i+=16)
					if (diskette_mem[0][i+8]=='B') // found a BASIC file?
					{
						if (!memcmp(&diskette_mem[0][i],&s[12],8))
							{ t=NULL; break; } // found the autoboot file!
						if (diskette_mem[0][i]&&!t)
							t=&diskette_mem[0][i]; // remember this BASIC file, but keep looking for autoboot files
					}
				if (t) memcpy(&POKE(0X5CD8),t,8); // overwrite the filename
			}
			else
			{
				BYTE s[]={239,34,34,13,128};
				MEMSAVE(&POKE(0X5CCC),s); POKE(0X5C61)=0XCC+sizeof(s);
			}
			// no `break`!
		case 3: // menu-ready 128K: hit RETURN
			autorun_k=1; // PRESS RETURN
			autorun_m=4; autorun_t=3;
			break;
		case 4: // release RETURN
			autorun_k=0; // RELEASE RETURN
			disc_disabled&=1,autorun_m=0; // end of AUTORUN
			break;
	}
}

// Z80-hardware procedures ------------------------------------------ //

// the Spectrum hands the Z80 a mainly empty data bus value
#define Z80_IRQ_BUS 255
// the Spectrum doesn't obey the Z80 IRQ ACK signal
#define Z80_IRQ_ACK ((void)0)

void z80_sync(int t) // the Z80 asks the hardware/video/audio to catch up
{
	static int r=0; main_t+=t;
	if (type_id==3) //if (!disc_disabled) // redundant, in most cases disc_main() does nothing
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
	if (!(~p&31)) // BETA128 interface
	{
		if (trdos_mapped)
			if (p&128) // 0x??FF: SYSTEM
			{
				if (4&b&~diskette_system) diskette_send_busy(0); // setting bit 2 resets the drive status (!?)
				diskette_system=b; // bits 0 and 1 select the drive, bit 4 selects the side
				diskette_side=!(b&16); diskette_drive=b&3;
			}
			else if (p&64) if (p&32) // 0x??7F: DATA
				diskette_send_data(b);
			else // 0x??5F: SECTOR
				diskette_send_sector(b);
			else if (p&32) // 0x??3F: TRACK
				diskette_send_track(b);
			else // 0x??1F: COMMAND
			{
				diskette_motor=1; // *!* motor is always on!?
				diskette_send_command(b);
			}
		// else ... // is the KEMPSTON port writeable!?
		return; // the following conditions cannot be true if we got here
	}
	if (!(p&1)) // 0x??FE, ULA 48K
	{
		ula_v1_send(b),tape_output=tape_type<0&&(b&8); // tape record signal
		if (!dac_extra) // play beeper only when the external DAC isn't busy!
			{ const int k[4]={0,1024,10240,10922}; static BYTE bbb=0; BYTE bb=(b>>3)&3; if (bbb!=bb) dac_delay=0,dac_voice=k[bbb=bb]; } // see also psg_outputs[]
		if (ula_pentagon&&ula_bus<0&&frame_pos_y>=VIDEO_OFFSET_Y&&frame_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y
			&&video_pos_x>VIDEO_OFFSET_X&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X+16) // special case: Pentagon border?
			switch (ula_count_z&3) // editing the bitmap from here is a dirty kludge, but the performance hit is lower
			{
				case 0: video_target[-12]=video_target[-11]=video_target[-10]=video_target[- 9]=video_clut[64]; // no `break`!
				case 1: video_target[- 8]=video_target[- 7]=video_target[- 6]=video_target[- 5]=video_clut[64]; // no `break`!
				case 2: video_target[- 4]=video_target[- 3]=video_target[- 2]=video_target[- 1]=video_clut[64]; // no `break`!
			}
		if (z80_pc.w==0X11D0&&equalsmmmm(&mmu_rom[0][0X11D0],0X3E3FED47)&&power_boosted) // 48K: power-up boost
		{
			memset(&mmu_ram[1][1<<14],0,1<<14),memset(&mmu_ram[2][2<<14],0,1<<14),memset(&mmu_ram[3][3<<14],0,1<<14);
			z80_pc.w=0X11EF; z80_ir.b.h=0X3F; z80_hl.w=(ula_sixteen&&!type_id)?0X8000:0X0000; // skip test and fill registers
		}
	}
	if (!(p&2)) // 0x??FD, MULTIPLE DEVICES
	{
		if (type_id&&!(ula_v2&32)) // 48K mode forbids further changes!
		{
			if ((type_id!=3)?!(p&0x8000):((p&0xC000)==0x4000)) // 0x7FFD: ULA 128K
			{
				if (b==7&&z80_pc.w==(type_id==3?0x0119:0x00D1)) // warm reset?
					if (snap_done=0,power_boosted&&mmu_rom[0]==(type_id==3?mem_rom:&mem_rom[0X8000])) // 128K 2/2: power-up boost
						MEMZERO(mem_ram),z80_pc.w=type_id==3?0X0129:0X00ED,b=0;
				ula_v2_send(b);
				session_dirty|=b&32; // show memory mode change on window
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
					ula_v3_send(b&15); // top 4 bits are useless :-/
					if (!disc_disabled)
						disc_motor_set(b&8); // BIT 3: MOTOR
				}
				else if ((p&0xF000)==0x3000) // 0x3FFD: FDC DATA I/O
					if (!disc_disabled) // PLUS3?
						disc_data_send(b);
			}
		}
		if (p&0x8000) // PSG 128K
			if (type_id||!psg_disabled) // optional on 48K
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
						dac_extra=dac_voice=0,playcity_send(0,b); // Covox and Turbosound cannot coexist!
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
						{ psg_table_send(b); if (psg_index<11&&b&&psg_index!=7) tape_song=(tape_disabled||(audio_disabled&1))?0:240; }
				}
			}
	}
	if (!(p&4)) // 0x??FB: ZX PRINTER and other extensions
	{
		#ifdef PSG_PLAYCITY
		if (!dac_disabled)
			if (!printer) { dac_extra=1<<7,dac_delay=0,dac_voice=((b-128)*10922)>>7; } // Covox $FB DAC, unsigned 8-bit audio
		// putting an `else` here isn't a good idea: the DAC and ULAPLUS can coexist
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

BYTE z80_tape_index[1<<16]; // full Z80 16-bit cache, 255 when unused
BYTE z80_tape_fastload[][32] = { // codes that read pulses : <offset, length, data> x N) -------------------------------------------------------------- MAXIMUM WIDTH //
/*  0 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   9,0XDB,0XFE,0X1F,0XD0,0XA9,0XE6,0X20,0X28,0XF3 }, // ZX SPECTRUM FIRMWARE
/*  1 */ {  -2,  11,0XDB,0XFE,0X1F,0XE6,0X20,0XF6,0X02,0X4F,0XBF,0XC0,0XCD,  +7,   3,0X10,0XFE,0X2B,  +2,   2,0X20,0XF9 }, // ZX SPECTRUM FIRMWARE (SETUP)
/*  2 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   9,0XDB,0XFE,0X1F,0X00,0XA9,0XE6,0X20,0X28,0XF3 }, // TOPO
/*  3 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   9,0XDB,0XFE,0XA9,0XE6,0X40,0XD8,0X00,0X28,0XF3 }, // MULTILOAD V1+V2
/*  4 */ {  -4,   8,0X1C,0XC8,0XDB,0XFE,0XE6,0X40,0XBA,0XCA,-128, -10 }, // "LA ABADIA DEL CRIMEN"
/*  5 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   9,0XDB,0XFE,0X1F,0XA9,0XD8,0XE6,0X20,0X28,0XF3 }, // "HYDROFOOL" (1/2)
/*  6 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   8,0XDB,0XFE,0X1F,0XA9,0XE6,0X20,0X28,0XF4 }, // "HYDROFOOL" (2/2)
/*  7 */ {  -8,   3,0X04,0X20,0X03,  +3,   9,0XDB,0XFE,0X1F,0XC8,0XA9,0XE6,0X20,0X28,0XF1 }, // ALKATRAZ
/*  8 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   9,0XDB,0XFE,0X1F,0XC8,0XA9,0XE6,0X20,0X28,0XF3 }, // "RAINBOW ISLANDS"
/*  9 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   7,0XDB,0XFE,0XA9,0XE6,0X40,0X28,0XF5 }, // MINILOAD-ZXS
/* 10 */ {  -6,   3,0X06,0XFF,0X3E,  +1,   7,0XDB,0XFE,0XE6,0X40,0XA9,0X28,0X09 }, // SPEEDLOCK V1 SETUP
/* 11 */ {  -2,   6,0XDB,0XFE,0X1F,0XE6,0X20,0X4F }, // SPEEDLOCK V2 SETUP
/* 12 */ {  -5,   2,0X4F,0X3E,  +1,   8,0XDB,0XFE,0XE6,0X40,0XA9,0X28,0X0E,0X2A }, // SPEEDLOCK V3 SETUP
/* 13 */ {  -6,  13,0X04,0X20,0X01,0XC9,0XDB,0XFE,0X1F,0XC8,0XA9,0XE6,0X20,0X28,0XF3 }, // "CHIP'S CHALLENGE"
/* 14 */ {  -2,   9,0XDB,0XFE,0X14,0XC8,0XE6,0X40,0XA9,0X28,0XF7 }, // "BOBBY CARROT"
/* 15 */ {  -4,   9,0X04,0XC8,0XDB,0XFE,0XA9,0XE6,0X40,0X28,0XF7 }, // "TIMING TESTS (48K+128K)"
/* 16 */ {  -3,   5,0X2C,0XDB,0XFE,0XA4,0XCA,-128,  -7 }, // GREMLIN (1/2)
/* 17 */ {  -3,   5,0X2C,0XDB,0XFE,0XA4,0XC2,-128,  -7 }, // GREMLIN (2/2)
/* 18 */ {  -0,   8,0XA9,0XE6,0X40,0X20,0X04,0X05,0X20,0XF4 }, // "BC'S QUEST FOR TIRES"
/* 19 */ {  -8,   5,0X78,0XC6,0X02,0X47,0X38,  +3,   5,0XAD,0XE6,0X40,0X20,0XF3 }, // AMSTRAD CPC FIRMWARE
/* 20 */ {  -6,   3,0X14,0XC8,0X3E,  +1,   9,0XDB,0XFE,0X1F,0XD0,0XAB,0XE6,0X20,0X28,0XF3 }, // "SIL4.TZX"
/* 21 */ {  -4,   7,0X04,0XC8,0XDB,0XFE,0XA9,0XA2,0XCA,-128,  -9 }, // AST A.MOORE
/* 22 */ {  -4,   6,0X04,0XC8,0XDB,0XFE,0X87,0XF2,-128,  -8 }, // "NN_COMALL.TZX" (1/2)
/* 23 */ {  -4,   6,0X04,0XC8,0XDB,0XFE,0X87,0XFA,-128,  -8 }, // "NN_COMALL.TZX" (2/2)
/* 24 */ {  -4,   8,0X24,0XC8,0XED,0X78,0XA8,0XE6,0X40,0XCA,-128, -10 }, // UNILODE
/* 25 */ {  -7,   4,0X04,0XC8,0X7E,0X3E,  +1,   8,0XDB,0XFE,0X1F,0XA9,0XE6,0X20,0X28,0XF3 }, // "GENIUS 18"
/* 26 */ {  -6,   3,0X04,0XC8,0X3E,  +1,   8,0XDB,0XFE,0XA9,0XD8,0XE6,0X40,0X28,0XF4 }, // "NETHER EARTH" + JSW (PRESTO)
/* 27 */ {  -4,   5,0X3C,0XC8,0XED,0X70,0XE2,-128,  -7 }, // "MOON CRESTA (SLOWLOADER)" (1/2)
/* 28 */ {  -4,   5,0X3C,0XC8,0XED,0X70,0XEA,-128,  -7 }, // "MOON CRESTA (SLOWLOADER)" (2/2)
/* 29 */ {  -7,  15,0X04,0X28,0X1D,0X3E,0X7F,0XDB,0XFE,0X1F,0X30,0X1B,0XA9,0XE6,0X20,0X28,0XF1 }, // "GARY LINEKER SUPER SKILLS (LEVELS)"
/* 30 */ {  -5,   7,0X0C,0X28,0X16,0XDB,0XFE,0XA0,0XCA,-128,  -9 }, // "LONE WOLF 3: THE MIRROR OF DEATH" (1/2)
/* 31 */ {  -5,   7,0X0C,0X28,0X0D,0XDB,0XFE,0XA0,0XC2,-128,  -9 }, // "LONE WOLF 3: THE MIRROR OF DEATH" (2/2)
/* 32 */ {  -4,  11,0X3E,0X7F,0XDB,0XFE,0X1F,0XD0,0X04,0XC8,0XA9,0XA4,0XCA,-128, -13 }, // "LA ABADIA DEL CRIMEN" (MAC)
/* 33 */ {  -5,   2,0X0C,0X28,  +1,   3,0XDB,0XFE,0XEE,  +1,   4,0XE6,0X40,0X28,0XF5 }, // "THE GREAT ESCAPE" (XLR-8)
/* 34 */ {  -8,   2,0X04,0XCA,  +2,   1,0X3E,  +1,   8,0XDB,0XFE,0X1F,0XA9,0XE6,0X20,0X28,0XF2 }, // OS9 ("FORBIDDEN PLANET")
/* 35 */ { -14,   4,0X04,0X20,0X09,0XC9,  +8,   9,0XDB,0XFE,0X1F,0XC8,0XA9,0XE6,0X20,0X28,0XEB }, // "TURBO OUT RUN" LEVELS
};
BYTE z80_tape_fastfeed[][32] = { // codes that build bytes
/*  0 */ {  -0,   2,0XD0,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -14 }, // ZX SPECTRUM FIRMWARE + TOPO
/*  1 */ {  -0,   3,0X30,0X11,0X3E,  +1,   4,0XB8,0XCB,0X15,0X06,  +1,   1,0XD2,-128, -15 }, // BLEEPLOAD
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
/* 17 */ {  -8,   3,0X06,0X00,0XCD,  +5,   1,0X28,  +1,   1,0X30,  +1,   1,0X3E,  +1,   4,0XB8,0XCB,0X15,0XD2,-128, -20 }, // "LA ABADIA DEL CRIMEN" (MAC)
/* 18 */ {  -0,   1,0X30,  +1,   1,0X3E,  +1,   4,0XB8,0XCB,0X13,0X06,  +1,   2,0X30,0XF2 }, // "70908" (SCOOPEX)
};
#ifdef FASTTAPE_DUMPER
BYTE z80_tape_fastdump[][32] = { // codes that fill blocks
/*  0 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +19,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCA }, // ZX SPECTRUM FIRMWARE
/*  1 */ { -29,   8,0X08,0X20,0X05,0XDD,0X75,0X00,0X18,0X0A, +10,   5,0XDD,0X23,0X1B,0X08,0X06, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD1 }, // GREMLIN OLD
/*  2 */ { -31,  10,0X08,0X20,0X07,0X30,0XFE,0XDD,0X75,0X00,0X18,0X0A, +10,   5,0XDD,0X23,0X1B,0X08,0X06, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCE }, // BLEEPLOAD
/*  3 */ { -31,  10,0X08,0X20,0X07,0X20,0X00,0XDD,0X75,0X00,0X18,0X0A, +10,   5,0XDD,0X23,0X1B,0X08,0X06, +17,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCF }, // "RAINBOW ISLANDS"
/*  4 */ {  -0,  12,0XDD,0X75,0X00,0XDD,0X2B,0XE1,0X2B,0X7C,0XB5,0XC8,0XE5,0XC3,-128, -17 }, // "LA ABADIA DEL CRIMEN"
/*  5 */ { -36,  10,0X08,0X20,0X07,0X30,0X0F,0XDD,0X75,0X00,0X18,0X0F, +15,   3,0XDD,0X23,0X1B, +18,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XCB }, // "ABU SIMBEL PROFANATION"
/*  6 */ { -28,   7,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X06,  +1,   4,0X2E,0X01,0X00,0X3E, +29,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XD0 }, // SPEEDLOCK V1
/*  7 */ { -16,   4,0XDD,0X75,0X00,0XDD,  +1,   4,0X1B,0X2E,0X01,0X06, +13,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XE3 }, // MINILOAD-ZXS
/*  8 */ { -21,   3,0X08,0X28,0X04,  +4,   8,0XDD,0X75,0X00,0XDD,0X23,0X1B,0X08,0X06, +16,   7,0X7C,0XAD,0X67,0X7A,0XB3,0X20,0XDA }, // "CRAY-5"
};
#endif

#if 1 // TAPE_FASTLOAD
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
#endif

void z80_tape_trap(void)
{
	int i,j; WORD w; BYTE k; if ((i=z80_tape_index[z80_pc.w])>length(z80_tape_fastload))
	{
		for (i=0;i<length(z80_tape_fastload)&&!fasttape_test(z80_tape_fastload[i],z80_pc.w);++i) {}
		z80_tape_index[z80_pc.w]=i; cprintf("FASTLOAD: %04X=%02d\n",z80_pc.w,(i<length(z80_tape_fastload))?i:-1);
	}
	if (i>=length(z80_tape_fastload)) return; // only known methods can reach here!
	if (tape_enabled>=0&&tape_enabled<2) // automatic tape playback/stop?
		tape_enabled=2; // amount of frames the tape should keep moving
	if (!tape_skipping) tape_skipping=-1;
	#if 1 // TAPE_FASTLOAD
	switch (i) // always handle these special cases
	{
		case  2: // "BOOK OF THE DEAD" has a very long init wait
			if (z80_pc.w==0XFFA2&&z80_sp.w==0XFFFA&&PEEK(0XFFFA)==0X27&&PEEK(0XFFFB)==0XFF)
		case  1: // ZX SPECTRUM FIRMWARE (SETUP)
			tape_enabled|=128; // shorten setup (64 is too short for the 1" pauses between header and body)
			break;
		case 10: // SPEEDLOCK V1 SETUP ("BOUNTY BOB STRIKES BACK")
		case 11: // SPEEDLOCK V2 SETUP ("ATHENA 48K / 128K")
		case 12: // SPEEDLOCK V3 SETUP ("THE ADDAMS FAMILY")
		case 20: /*tape_enabled|=32;*/ // this loader must "pull" the tape
		case 25: // ditto
			tape_enabled|=8; // play tape and listen to decryption noises
			break;
	}
	if (tape_fastload&&tape_loud) switch (i) // handle tape analysis upon status
	{
		case  0: // ZX SPECTRUM FIRMWARE, "ABU SIMBEL PROFANATION", SOFTLOCK ("ELITE 48K", "RASPUTIN 48K")
		case  5: // "HYDROFOOL" (1/2)
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==3||j==11))
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==0||k==5||k==8)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1&&((WORD)(z80_ix.w-z80_sp.w)>2))
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>5,59),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else if (z80_hl.b.l==0x80&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==15) // SOFTLOCK
			{
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if (z80_tape_testdump(z80_tape_spystack(0))==0)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1&&((WORD)(z80_ix.w-z80_sp.w)>2))
						k=rbit8(fasttape_dump()), // RR L instead of RL L!
						z80_hl.b.h^=POKE(z80_ix.w)=k,++z80_ix.w,--z80_de.w;
				#endif
				k=rbit8(fasttape_feed(z80_bc.b.l>>5,59)), // ditto!
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
				#ifdef FASTTAPE_DUMPER
				if (z80_af2.b.l&0x40) if ((k=z80_tape_testdump(z80_tape_spystack(0)))==0||k==1||k==2)
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
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
				#ifdef FASTTAPE_DUMPER
				if (z80_tape_testdump(z80_tape_spystack(2))==4)
					while (FASTTAPE_CAN_DUMP()&&(w=z80_sp.w+4,POKE(w)>1)
						POKE(z80_ix.w)=fasttape_dump(),--z80_ix.w,--POKE(w);
				#endif
				k=fasttape_feed(z80_de.b.h>>6,41),tape_skipping=z80_hl.b.l=128+(k>>1),z80_de.b.l=-(k&1);
			}
			else
				fasttape_add8(z80_de.b.h>>6,41,&z80_de.b.l,1); // z80_r7+=...*6;
			break;
		case  6: // "HYDROFOOL" (2/2), GREMLIN OLD ("THING BOUNCES BACK"), SPEEDLOCK V1+V2+V3 ("BOUNTY BOB STRIKES BACK", "ATHENA", "THE ADDAMS FAMILY"), DINAMIC ("COBRA'S ARC"), MIKRO-GEN ("AUTOMANIA")
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==3||j==5||j==6||j==14))
			{
				#ifdef FASTTAPE_DUMPER
				if ((((k=z80_tape_testdump(z80_tape_spystack(0)))==1||k==5)&&z80_af2.b.l&0x40)||k==6) // SPEEDLOCK V1 ignores AF2
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>5,54),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else if (z80_hl.b.l==0x80&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==15) // "GARY LINEKER SUPER SKILLS" (TITLE + CODE)
				k=rbit8(fasttape_feed(z80_bc.b.l>>5,54)),tape_skipping=z80_hl.b.l=1+(k<<1),z80_bc.b.h=(k&128)?-1:0; // RR L instead of RL L!
			else
				fasttape_add8(z80_bc.b.l>>5,54,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case  7: // ALKATRAZ ("HATE")
		case  8: // "RAINBOW ISLANDS"
		case 13: // "CHIP'S CHALLENGE"
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&((j=z80_tape_testfeed(z80_tape_spystack(0)))==0||j==2||j==7))
			{
				#ifdef FASTTAPE_DUMPER
				if (((k=z80_tape_testdump(z80_tape_spystack(0)))==0&&j)||k==3) // "&&j" avoids bugs in "Advanced Soccer Simulator"
					while (FASTTAPE_CAN_DUMP()&&z80_de.b.l>1)
						z80_hl.b.h^=POKE(z80_ix.w)=fasttape_dump(),++z80_ix.w,--z80_de.w;
				#endif
				k=fasttape_feed(z80_bc.b.l>>5,59),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			}
			else
				fasttape_add8(z80_bc.b.l>>5,59,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case  9: // MINILOAD-ZXS + "70908" (SCOOPEX)
			if (FASTTAPE_CAN_FEED()&&((((j=z80_tape_testfeed(z80_tape_spystack(0)))==8||j==9)&&z80_hl.b.l==0x01)||(j==18&&z80_de.b.l==0X01)))
			{
				k=fasttape_feed(z80_bc.b.l>>6,50);
				if (j==18)
					tape_skipping=z80_de.b.l=128+(k>>1);
				else
					tape_skipping=z80_hl.b.l=128+(k>>1);
				z80_bc.b.h=-(k&1);
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
			z80_r7+=fasttape_add8(~z80_hl.b.l>>6,57,&z80_bc.b.h,2)*9;
			break;
		case 20: // "SIL4.TZX" (possibly a bad idea because of EI)
			fasttape_add8(z80_de.b.l>>5,59,&z80_de.b.h,1); // z80_r7+=...*9;
			break;
		case 21: // AST A.MOORE ("A YANKEE IN IRAQ")
			fasttape_add8(z80_bc.b.l>>6,38,&z80_bc.b.h,1); // z80_r7+=...*6;
			break;
		case 24: // UNILODE ("TRIVIAL PURSUIT")
			if ((z80_af.b.l&1)&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==16) // we must force a RET in this loader :-(
				k=fasttape_feed(z80_bc.b.h>>6,42),tape_skipping=z80_bc.b.h=128+(k>>1),z80_hl.b.h=-(k&1),k=z80_tape_spystack(0)+2,z80_pc.w=z80_tape_spystack(0)+2,z80_sp.w+=2;
			else
				fasttape_add8(z80_bc.b.h>>6,42,&z80_hl.b.h,1); // z80_r7+=...*7;
			break;
		case 25: // "GENIUS 18"
			fasttape_add8(z80_bc.b.l>>5,61,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case 26: // "NETHER EARTH" + JSW (PRESTO)
			fasttape_add8(z80_bc.b.l>>6,55,&z80_bc.b.h,1); // z80_r7+=...*8; // this loader includes a music player, we'll rarely enable this
			break;
		case 29: // "GARY LINEKER SUPER SKILLS (LEVELS)"
			if (z80_hl.b.l==0x80&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==15)
				k=rbit8(fasttape_feed(z80_bc.b.l>>5,63)),tape_skipping=z80_hl.b.l=1+(k<<1),z80_bc.b.h=(k&128)?-1:0; // RR L instead of RL L!
			else
				fasttape_add8(z80_bc.b.l>>5,63,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case 30: // "LONE WOLF 3: THE MIRROR OF DEATH" (1/2)
		case 31: // "LONE WOLF 3: THE MIRROR OF DEATH" (2/2)
			/*if (z80_bc.b.h==64)*/ z80_r7+=fasttape_add8(i&1,36,&z80_bc.b.l,1)*5; // see also "DRAGONS OF FLAME"
			break;
		case 32: // "LA ABADIA DEL CRIMEN" (MAC)
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==17)
				k=fasttape_feed(z80_bc.b.l>>5,54),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=(-(k&1))*2; // INC B happens after IN A,($FE)!
			else
				fasttape_add8(z80_bc.b.l>>5,54,&z80_bc.b.h,1); // z80_r7+=...*9;
			break;
		case 33: // "THE GREAT ESCAPE" (XLR-8)
			w=z80_pc.w+1,z80_r7+=fasttape_add8(PEEK(w)>>6,48,&z80_bc.b.l,1)*6; // self-modifying code :-(
			break;
		case 34: // OS9 ("FORBIDDEN PLANET")
		case 35: // "TURBO OUT RUN" LEVELS
			if (z80_hl.b.l==0x01&&FASTTAPE_CAN_FEED()&&z80_tape_testfeed(z80_tape_spystack(0))==(i==34?0:2))
				k=fasttape_feed(z80_bc.b.l>>5,59),tape_skipping=z80_hl.b.l=128+(k>>1),z80_bc.b.h=-(k&1);
			else
				fasttape_add8(z80_bc.b.l>>5,59,&z80_bc.b.h,1); // z80_r7+=...*8;
			break;
	}
	#endif
}

int z80_recv_ula(void) // "Cobra" and "Arkanoid" were happy with `ula_bus` but the tests "FLOATSPY" and "HALT2INT" need more precision
{
	if (!(ula_shown_x&1)&&(ula_shown_y>=0&&ula_shown_y<192)&&(ula_shown_x>=0&&ula_shown_x<32))
		switch (ula_count_z&3)
		{
			case 0: return ula_screen[ula_bitmap];
			case 1: return ula_screen[ula_attrib];
			case 2: return ula_screen[ula_bitmap+1];
			case 3: return ula_screen[ula_attrib+1];
		}
	return type_id==3?ula_bus3:255;
}
BYTE z80_recv(WORD p) // the Z80 receives a byte from a hardware port
{
	if (!(~p&31)) // tell apart between KEMPSTON and BETA128 ports
	{
		if (trdos_mapped) // BETA128 interface
		{
			if (p&128) // 0x??FF: SYSTEM
				return diskette_recv_busy()>0?0X40:0X80; // TODO: emulate BUSY (0X80/0X00? STOP, 0X40 FEED, 0XC0 BUSY)
			else if (p&64) if (p&32) // 0x??7F: DATA
				return diskette_recv_data();
			else // 0x??5F: SECTOR
				return diskette_recv_sector();
			else if (p&32) // 0x??3F: TRACK
				return diskette_recv_track();
			else // 0x??1F: STATUS
				return diskette_recv_status();
		}
		if (!(p&32)) // KEMPSTON port
			return autorun_kbd_bit(8); // catch special case: lightgun or joystick ("TARGET PLUS", "MIKE GUNNER")
		if (ula_pentagon)
			return ula_bus; // http://worldofspectrum.net/rusfaq/ : "*port FF - port of current screen attributes. [...] on border [...] gives FF."
		if (type_id<3) // non-PLUS3 floating bus?
			return (p>=0X4000&&p<=0X7FFF)?(ula_shown_x&1)?ula_bus:255:z80_recv_ula(); // kludge: "A Yankee in Iraq" uses a contended floating bus address :-/
		return 255;
	}
	switch (p&7)
	{
		case 6: // 0x??FE, ULA 48K
			#ifdef Z80_DANDANATOR
			dandanator_clear(); // Dandanator timeouts happen when polling the keyboard or the tape
			#endif
			int j=0;
			for (int i=0;i<8;++i)
				if (!(p&(256<<i))) // the bit mask in the upper byte can merge keyboard rows together
					j|=autorun_kbd_bit(i); // bits 0-4: keyboard rows
			if (tape)//&&!z80_iff.b.l) // at least one tape loader enables interrupts: the musical demo "SIL4.TZX"
				z80_tape_trap();
			if (tape&&tape_enabled)
			{
				if (!tape_status)
					j|=64; // bit 6: tape input state
			}
			else
				j^=ula_v1_cache; // Issue-2/3 difference, see above
			return ~j;
		case 5: // 0x??FD, MULTIPLE DEVICES
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
					//else
					#endif
					if (psg_index==14&&printer&&type_id>0&&type_id<3)//&&z80_pc.w<0x2000) // debug
						return printer_1=1,printer_8=0; // SPECTRUM 128K gets PRINTER BUSY from R14 BIT6!
					//else
						return psg_table_recv();
				}
			return 255;
		case 3: // 0x??FB: ZX PRINTER and other EXTENSIONS
			if (p==0XFF3B&&ulaplus_enabled) // the ULAPLUS input
				return ulaplus_table_recv();
			if (!type_id&&printer)
				return 128+(32+16+8+4+2)+1; // BIT 7 = READY, BIT 6 = NO PRINTER, BIT 0 = NOT BUSY (briefly ZERO after sending a pixel byte); FUSE sets the unused bits 5-1, is it right?
			// no `break`!
		default:
			return 255;
	}
}

// CPU: ZILOG Z80 MICROPROCESSOR ==================================== //

#define Z80_MREQ_PAGE(t,p) ( z80_t+=(z80_aux2=(ula_clash_mreq[p][(WORD)ula_clash_z]+t)), ula_clash_z+=z80_aux2 )
#define Z80_IORQ_PAGE(t,p) ( z80_t+=(z80_aux2=(ula_clash_iorq[p][(WORD)ula_clash_z]+t)), ula_clash_z+=z80_aux2 )
// input/output
#define Z80_SYNC() ( _t_-=z80_t, z80_sync(z80_t), z80_t=0 )
#define Z80_SYNC_IO(t) ( _t_-=z80_t-t, z80_sync(z80_t-t), z80_t=t ) // `t` sets a delay between updates
#define Z80_PRAE_RECV(w) do{ Z80_IORQ(1,w); Z80_SYNC_IO(ula_fix_out); }while(0)
#define Z80_RECV z80_recv
#define Z80_POST_RECV(w) do{ if (w&1) Z80_IORQ_1X_NEXT(3); else Z80_IORQ_PAGE(3,4); }while(0)
#define Z80_PRAE_SEND(w) do{ if (w&3) audio_dirty=1; Z80_IORQ(1,w); Z80_SYNC_IO(ula_fix_out); }while(0)
#define Z80_SEND z80_send
#define Z80_POST_SEND(w) do{ if (w&1) Z80_IORQ_1X_NEXT(3); else Z80_IORQ_PAGE(3,4); }while(0)
// fine timings
#define Z80_LOCAL BYTE z80_aux1,z80_aux2 // they must stick between macros :-/
#define Z80_MREQ(t,w) Z80_MREQ_PAGE(t,z80_aux1=(w>>14)) // these contended memory pauses are common to all official Sinclair/Amstrad ZX Spectrum machines
#define Z80_MREQ_1X(t,w) do{ z80_aux1=w>>14; if (/*t>1&&*/type_id<3) { BYTE z80_auxx=t; do Z80_MREQ_PAGE(1,z80_aux1); while (--z80_auxx); } else Z80_MREQ_PAGE(t,z80_aux1); }while(0)
#define Z80_MREQ_NEXT(t) Z80_MREQ_PAGE(t,z80_aux1) // when the very last z80_aux1 is the same as the current one
#define Z80_MREQ_1X_NEXT(t) do{ if (/*t>1&&*/type_id<3) { BYTE z80_auxx=t; do Z80_MREQ_NEXT(1); while (--z80_auxx); } else Z80_WAIT(t); }while(0) // *Z80_MREQ_NEXT => Z80_WAIT ("Mask 3: Venom Strikes Back")
#define Z80_IORQ(t,w) Z80_IORQ_PAGE(t,z80_aux1=(w>>14)) // these pauses are different in Spectrum PLUS3 (and PLUS2A) because only MREQ actions are contended:
#define Z80_IORQ_1X(t,w) do{ z80_aux1=w>>14; if (t>1) { BYTE z80_auxx=t; do Z80_IORQ_PAGE(1,z80_aux1); while (--z80_auxx); } else Z80_IORQ_PAGE(t,z80_aux1); }while(0) // *UNUSED* Z80_IORQ is always ZERO on PLUS3!
#define Z80_IORQ_NEXT(t) Z80_IORQ_PAGE(t,z80_aux1) // when the very last z80_aux1 is the same as the current one
#define Z80_IORQ_1X_NEXT(t) do{ /*if (t>1)*/ { BYTE z80_auxx=t; do Z80_IORQ_NEXT(1); while (--z80_auxx); } /*else Z80_WAIT(t);*/ }while(0) // Z80_IORQ is always ZERO on PLUS3! *Z80_IORQ_NEXT => Z80_WAIT (?)
#define Z80_WAIT(t) ( z80_t+=t, ula_clash_z+=t )
#define Z80_WAIT_IR1X(t) do{ if (type_id<3) { z80_aux1=z80_ir.w>>14; if (t>1) { BYTE z80_auxx=t; do Z80_MREQ_PAGE(1,z80_aux1); while (--z80_auxx); } else { Z80_MREQ_PAGE(t,z80_aux1); } } else Z80_WAIT(t); }while(0)
#define Z80_DUMB_M1(w) Z80_MREQ(4,w) // dumb 4-T MREQ
#define Z80_DUMB(w) Z80_MREQ(3,w) // dumb 3-T MREQ
#define Z80_NEXT_M1(w) ( Z80_MREQ(4,w), mmu_rom[z80_aux1][w] ) // 4-T PEEK
#define Z80_NEXT(w) ( Z80_MREQ(3,w), mmu_rom[z80_aux1][w] ) // 3-T PEEK
#define Z80_PEEK(w) ( Z80_MREQ(3,w), mmu_rom[z80_aux1][w] )
#define Z80_PEEK0 Z80_PEEK // untrappable single read, use with care
#define Z80_PEEK1WZ Z80_PEEK // 1st twin read from LD rr,($hhll)
#define Z80_PEEK2WZ Z80_PEEK // 2nd twin read
#define Z80_PEEK1SP Z80_PEEK // 1st twin read from POP rr
#define Z80_PEEK2SP Z80_PEEK // 2nd twin read
#define Z80_PEEK1EX Z80_PEEK // 1st twin read from EX rr,(SP)
#define Z80_PEEK2EX Z80_PEEK // 2nd twin read
#define Z80_PRAE_NEXTXY(w) (z80_aux1=(w>>14),mmu_rom[z80_aux1][w]) // special DD/FD PEEK (1/2)
#define Z80_POST_NEXTXY Z80_MREQ_NEXT(4) // special DD/FD PEEK (2/2)
#define Z80_POKE(w,b) ( Z80_MREQ(3,w), mmu_ram[z80_aux1][w]=(b) ) // trappable single write
#define Z80_PEEKPOKE(w,b) ( Z80_MREQ_NEXT(3), mmu_ram[z80_aux1][w]=(b) ) // a POKE that follows a same-address PEEK, f.e. INC (HL)
#define Z80_POKE0 Z80_POKE // untrappable single write, use with care
#define Z80_POKE1WZ(w,b) ( Z80_MREQ(3,w), (w>=0X5800&&w<=0X5AFF&&(Z80_SYNC_IO(0))), mmu_ram[z80_aux1][w]=(b) ) // 1st twin write from LD ($hhll),rr; NIRVANA games on PLUS3 require it
#define Z80_POKE2WZ Z80_POKE1WZ // 2nd twin write; NIRVANA games on 48K and 128K require it
#define Z80_POKE1SP Z80_POKE1WZ // 1st twin write from PUSH rr; NIRVANA games always need it
#define Z80_POKE2SP Z80_POKE0 // 2nd twin write; no ATTRIB effects seem to need it
#define Z80_POKE1EX Z80_PEEKPOKE // 1st twin write from EX rr,(SP)
#define Z80_POKE2EX Z80_POKE0 // 2nd twin write
// coarse timings
#define Z80_STRIDE(o)
#define Z80_STRIDE_ZZ(o) // no slow/fast ACK quirk on Spectrum
#define Z80_STRIDE_IO(o)
//#define Z80_LIST_PEEKXY(o) z80_delays[o+0x600] // undef'd, the Z80 will use its own Z80 DD/FD boolean list
#define Z80_SLEEP(t) Z80_WAIT(t) // `t` must be nonzero!
#define Z80_HALT_STRIDE 0 // i.e. careful HALT, requires gradual emulation
// Z80 quirks
BYTE z80_xcf=0; // the XCF quirk flag
#define Z80_QUIRK(x) (z80_xcf=(x)&1) // XCF quirk only
#define Z80_QUIRK_M1 // nothing to do!
#define Z80_XCF_BUG ((z80_xcf|z80_xcf_bit)?0:z80_af.b.l) // replicate the XCF quirk...
BYTE z80_xcf_bit=0; // ...but let the user adjust it in runtime
BYTE Z80_0XED71=0; // whether OUT (C) sends 0 (NMOS) or 255 (CMOS)
#define Z80_0XED5X (1) // whether LD A,I/R checks the IRQ in-opcode
#define Z80_TRDOS_CATCH(r) if (trdos_mapped) { if (r.b.h>=0X40) z80_trdos_leave(); } else if (r.b.h==0X3D) z80_trdos_enter() // page TR-DOS in and out
#define Z80_TRDOS_ENTER(r) if (r.b.h==0X3D) z80_trdos_enter() // optimisation, TR-DOS uses a limited set of opcodes to leave
#define Z80_TRDOS_LEAVE(r) if (trdos_mapped) if (r.b.h>=0X40) z80_trdos_leave() // used only in IM2 interrupts
#define Z80_TRDOS_GOTOS // the Z80_TRDOS_ENTER macro destroys the benefits of code repetition
void z80_trdos_enter(void) // allow entering TR-DOS only when it's available and enabled
	{ if (!(trdos_mapped|disc_disabled)&&type_id!=3&&ula_v2&16) trdos_mapped=1,mmu_update(); }
void z80_trdos_leave(void) { trdos_mapped=0,mmu_update(); } // always allow exiting TR-DOS!
#define Z80_TRAP_0XD7 if (printer&&(!type_id/*||(ula_v2&48)*/)&&z80_iy.w==0X5C3A&&(PEEK(0X5C3B)&2)) {if (printer_t[printer_p]=z80_af.b.h,++printer_p>=256) printer_flush(); break; } // trap SPECTRUM 48K firmware printer

#define bios_magick() (debug_point[0X0004]=debug_point[0X2140]=DEBUG_MAGICK)
void z80_magick(void) // virtual magick!
{
	//cprintf("MAGICK:%08X ",z80_pc.w);
	/**/ if (z80_pc.w==0X0004) { if (power_boosted&&equalsmmmm(&mmu_rom[0][5],0X78B120FB)) z80_bc.w=0,z80_af.w=0X44,z80_pc.w=0X0009; } // 128K 1/2: power-up boost
	else if (z80_pc.w==0X2140) { if (mmu_rom[0]==&mem_rom[0X8000-0X0000]&&type_id==power_boost&&mem_rom[0XA140]==0XF5&&z80_sp.w==0X5BED&&z80_af.b.h>1) z80_pc.w=0X214D; } // PLUS3DOS: power-up boost (4.0 + 4.1 only)
}

#define DEBUG_HERE
#define DEBUG_INFOX 20 // panel width
#define debug_info_mreq(q) (ula_clash_mreq[q]!=ula_clash[0]?'*':'-')
#define debug_info_iorq(q) (ula_clash_iorq[q]!=ula_clash[0]?'*':'-')
BYTE debug_pook(int q,WORD w) { return PEEK(w); } // `q` doesn't matter on Spectrum! we must hide the fake RAM at $0000-$3FFF!
void debug_info(int q)
{
	sprintf(DEBUG_INFOZ(0),"ULA:       (%05d:%d)",ULA_GET_T(),ula_clash_mreq[1][(WORD)ULA_GET_T()]);
	sprintf(DEBUG_INFOZ(1)+4,type_id<1?"%04X:%02X %02X:--:--":type_id<3?"%04X:%02X %02X:%02X:--":"%04X:%02X %02X:%02X:%02X",
		ula_bitmap,ula_bus&255,ula_v1,ula_v2,ula_v3); // hide 128K and PLUS3 fields on 48K, hide PLUS3 field on 128K
	sprintf(DEBUG_INFOZ(2)+4,"MEM:%c%c%c%c IO:%c%c%c%c",
		debug_info_mreq(0),debug_info_mreq(1),debug_info_mreq(2),debug_info_mreq(3),
		debug_info_iorq(0),debug_info_iorq(1),debug_info_iorq(2),debug_info_iorq(3));
	strcpy (DEBUG_INFOZ(3),"PSG:");
	//sprintf(DEBUG_INFOZ(4),"%02X: ",psg_index);
	byte2hexa(DEBUG_INFOZ(4)+4,&psg_table[0],8);
	byte2hexa(DEBUG_INFOZ(5)+4,&psg_table[8],8);
	debug_hilight2(DEBUG_INFOZ(4+(psg_index&15)/8)+4+(psg_index&7)*2);
	if (!(q&1)) // 1/2
	{
		#ifdef PSG_PLAYCITY
		//sprintf(DEBUG_INFOZ(6),"%02X: ",playcity_index[0]);
		byte2hexa(DEBUG_INFOZ(6)+4,&playcity_table[0][0],8);
		byte2hexa(DEBUG_INFOZ(7)+4,&playcity_table[0][8],8);
		debug_hilight2(DEBUG_INFOZ(6+(playcity_index[0]&15)/8)+4+(playcity_index[0]&7)*2);
		#endif
		#ifdef Z80_DANDANATOR
		sprintf(DEBUG_INFOZ(8),"DANDANATOR%c",mem_dandanator?'*':'-');
		sprintf(DEBUG_INFOZ(9)+0,"    %c %02X:%02X:%02X %02X:%02X",(dandanator_cfg[6]&8)?'X':(dandanator_cfg[6]&4)?'Z':'0'+dandanator_temp
			,dandanator_cfg[0],dandanator_cfg[1],dandanator_cfg[2],dandanator_cfg[4],dandanator_cfg[5]);
		#endif
	}
	else // 2/2
	{
		sprintf(DEBUG_INFOZ(6),"ULAPLUS%c",(ulaplus_enabled&ulaplus_table[64])?'*':'-');
		//sprintf(DEBUG_INFOZ(7),"%02X:",ulaplus_index&255);
		byte2hexa(DEBUG_INFOZ( 7)+4,&ulaplus_table[ 0],8);
		byte2hexa(DEBUG_INFOZ( 8)+4,&ulaplus_table[ 8],8);
		byte2hexa(DEBUG_INFOZ( 9)+4,&ulaplus_table[16],8);
		byte2hexa(DEBUG_INFOZ(10)+4,&ulaplus_table[24],8);
		byte2hexa(DEBUG_INFOZ(11)+4,&ulaplus_table[32],8);
		byte2hexa(DEBUG_INFOZ(12)+4,&ulaplus_table[40],8);
		byte2hexa(DEBUG_INFOZ(13)+4,&ulaplus_table[48],8);
		byte2hexa(DEBUG_INFOZ(14)+4,&ulaplus_table[56],8);
		if (ulaplus_index<64) debug_hilight2(DEBUG_INFOZ(7+ulaplus_index/8)+4+(ulaplus_index&7)*2);
	}
}
int grafx_mask(void) { return 0XFFFF; }
BYTE grafx_peek(int w) { return debug_peek(w); }
void grafx_poke(int w,BYTE b) { debug_poke(w,b); }
int grafx_size(int i) { return i*8; }
int grafx_show(VIDEO_UNIT *t,int g,int n,int w,int o)
{
	const VIDEO_UNIT p0=0,p1=0XFFFFFF; BYTE z=-(o&1); g-=8; do
	{
		BYTE b=grafx_peek(w)^z; // readable only
		*t++=b&128?p1:p0; *t++=b& 64?p1:p0;
		*t++=b& 32?p1:p0; *t++=b& 16?p1:p0;
		*t++=b&  8?p1:p0; *t++=b&  4?p1:p0;
		*t++=b&  2?p1:p0; *t++=b&  1?p1:p0;
	}
	while (++w,t+=g,--n); return w&grafx_mask();
}
void grafx_info(VIDEO_UNIT *t,int g,int o)
{
	t-=16*8; for (int y=(ulaplus_table[64]&ulaplus_enabled)?4*12:12;--y>=0;)
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
	ula_setup();
	tape_setup();
	disc_setup();
	trdos_setup();
	psg_setup();
	z80_setup();
}
void all_reset(void) // reset everything!
{
	const int k=4; // the extended ZX RAM reveals a pattern that isn't erased by the firmware
	for (int i=0;i<sizeof(mem_ram);i+=k) memset(&mem_ram[i],0,k),i+=k,memset(&mem_ram[i],-1,k);
	//MEMZERO(mem_ram);
	autorun_k=0;
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
	dac_voice=0;
	z80_reset();
	debug_reset();
	disc_disabled&=1,z80_irq=snap_done=autorun_m=autorun_t=0; // avoid accidents!
	MEMBYTE(z80_tape_index,-1); // TAPE_FASTLOAD, avoid false positives!
}

// firmware ROM file handling operations ---------------------------- //

BYTE old_type_id=99; // the last loaded BIOS type
char bios_system[][13]={"spectrum.rom","spec128k.rom","spec-p-2.rom","spec-p-3.rom"};
char bios_path[STRMAX]="";

int bios_wrong_dword(DWORD t) // catch fingerprints that belong to other file types; DWORD is MMMM-styled
{
	#if 0 // overkill
	return //t==0x4D56202D|| // "MV - CPC" (floppy disc image) and "MV - SNA" (CPC Snapshot)
		//t==0x45585445|| // "EXTENDED" (advanced floppy disc image)
		t==0x5A585461|| // "ZXTape!" (advanced tape image)
		t==0x505A5854|| // "PZXT" ("perfect" tape image)
		//t==0x436F6D70|| // "Compressed Wave File" (advanced audio file)
		t==0x52494646|| // "RIFF" (WAVE audio file, CPC PLUS cartridge)
	#else // simpler!
	return (t>=0X41000000&&t<=0X5AFFFFFF)|| // reject all ASCII upper case letters!
	#endif
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
	if ((i!=(1<<14)&&i!=(1<<15)&&i!=(1<<16))||j!=0xD3FE37C9) // 16/32/64K + Spectrum TAPE LOAD fingerprint
		return puff_fclose(f),1; // reject!
	fseek(f,0,SEEK_SET);
	fread1(mem_rom,sizeof(mem_rom),f);
	puff_fclose(f);
	if (i<(1<<15))
		memcpy(&mem_rom[1<<14],mem_rom,1<<14); // mirror 16K ROM up
	if (i<(1<<16))
		memcpy(&mem_rom[1<<15],mem_rom,1<<15); // mirror 32K ROM up
	old_type_id=type_id=i>(1<<14)?i>(1<<15)?3:mem_rom[0x5540]=='A'?2:1:0; // original Sinclair 128K or modified Amstrad PLUS2?
	//cprintf("Firmware %dk m%d\n",i>>10,type_id);
	if (session_substr!=s) STRCOPY(bios_path,s);
	return 0;
}
int bios_reload(void) // ditto, but from the current type_id
	{ return old_type_id==type_id?0:type_id>=length(bios_system)?1:bios_load(strcat(strcpy(session_substr,session_path),bios_system[type_id])); }

#ifdef Z80_DANDANATOR
int dandanator_load(char *s) // inserts Dandanator cartridge, performs several tests, removes cartridge if they fail
{
	FILE *f=puff_fopen(s,"rb"); if (!f) return 1; // fail!
	fread1(session_scratch,0X1000,f); fseek(f,0X0601-0X4000,SEEK_END); int i=fgetmmmm(f);
	if (i!=0xD3FE37C9) fseek(f,0,SEEK_END),i=ftell(f); // Spectrum TAPE LOAD fingerprint
	puff_fclose(f); if (i<0X4000||i>(8<<16)||i&0x3FFF||bios_wrong_dword(mgetmmmm(session_scratch)))
		return 1; // ensure that the filesize is right and that the header doesn't belong to other filetypes!
	// the weak ZX Dandanator heuristic relies on FAILING the strong CPC Dandanator test :-(
	for (i=8;--i>=0;) if (session_scratch[i]<0X20||session_scratch[i]>=0X80) break;
	if (i<0) return 1; // reject files whose first 8 characters are ZERO or ASCII text (f.e. TRDOS discs)
	dandanator_remove(); if (dandanator_insert(s)) return 1;
	STRCOPY(dandanator_path,s); return 0;
}
#else
#define dandanator_remove() (0)
#endif

// snapshot file handling operations -------------------------------- //

char snap_pattern[]="*.szx;*.sna;*.z80"; // SZX must go first if we want it to be the default choice!
char snap_path[STRMAX]="",snap_extended=1; // flexible behavior (i.e. 128K-format snapshots with 48K-dumps)

#define SNAP_SAVE_Z80W(x,r) (header[x]=r.b.l,header[x+1]=r.b.h)
void snap_save_ramp(int i,FILE *f) // save a SZX RAM PAGE
{
	kputmmmm(0X52414D50,f); // "RAMP"
	#if DEFLATE_LEVEL
	int j; if (snap_extended&&(j=huff_zlib(session_scratch,0X4000,&mem_ram[i<<14],1<<14))>0) // is the compression successful? did it actually shrink anything?
		{ fputiiii(j+3,f); kputii(1,f); fputc(i,f); fwrite1(session_scratch,j,f); }
	else
	#endif
		{ kputiiii(0X4000+3,f); kputii(0,f); fputc(i,f); fwrite1(&mem_ram[i<<14],1<<14,f); }
}
void snap_save_rled(int h,int i,FILE *f) // save a Z80 RAM PAGE
{
	int j=0X4000; if (snap_extended)
	{
		BYTE *t=session_scratch,*s=&mem_ram[i<<14],*r=s+0X4000; BYTE q=255; // `q` avoids encoding ED0000000000 as *EDEDED0500
		while (s<r)
		{
			BYTE k=*s,l=1; while (++s<r&&*s==k&&l<q) ++l;
			if (l>(k==0XED?1:4)) *t++=0XED,*t++=0XED,*t++=l,*t++=k; else { q=k==0XED?1:255; do *t++=k; while (--l); }
		}
		j=t-session_scratch;
	}
	if (j<0X4000)
		fputii(j,f),fputc(h,f),fwrite1(session_scratch,j,f);
	else
		fputii(0XFFFF,f),fputc(h,f),fwrite1(&mem_ram[i<<14],1<<14,f);
}
int snap_save(char *s) // save a snapshot. `s` path, NULL to resave; 0 OK, !0 ERROR
{
	BYTE header[96]; MEMZERO(header); FILE *f; if (globbing("*.sna",s,1))
	{
		if (!(f=puff_fopen(s,"wb"))) return 1;
		int i=z80_sp.w; if (!(snap_extended|type_id)) // strict and 48K machine?
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
				header[3]=dandanator_cfg[4]+32; // DANDANATOR: 32|DANDANATOR_ROM_PAGE
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
	}
	else if (globbing("*.szx",s,1))
	{
		if (!(f=puff_fopen(s,"wb"))) return 1;
		kputmmmm(0X5A585354,f); // "ZXST"
		kputii(0X0101,f); fputii(!type_id?!ula_sixteen:type_id==3?5:ula_pentagon?7:1+type_id,f);
		kputmmmm(0X43525452,f); kputiiii(36,f); // "CRTR"
		strcpy(header,session_caption); fwrite1(header,36,f);
		kputmmmm(0X5A383052,f); kputiiii(0X25,f); // "Z80R"
		SNAP_SAVE_Z80W( 0,z80_af);
		SNAP_SAVE_Z80W( 2,z80_bc);
		SNAP_SAVE_Z80W( 4,z80_de);
		SNAP_SAVE_Z80W( 6,z80_hl);
		SNAP_SAVE_Z80W( 8,z80_af2);
		SNAP_SAVE_Z80W(10,z80_bc2);
		SNAP_SAVE_Z80W(12,z80_de2);
		SNAP_SAVE_Z80W(14,z80_hl2);
		SNAP_SAVE_Z80W(16,z80_ix);
		SNAP_SAVE_Z80W(18,z80_iy);
		SNAP_SAVE_Z80W(20,z80_sp);
		SNAP_SAVE_Z80W(22,z80_pc);
		header[24]=z80_ir.b.h;
		header[25]=z80_ir.b.l;
		header[26]=header[27]=z80_iff.b.l&1; // should we be more precise here?
		header[28]=z80_imd;
		fwrite1(header,29,f);
		fputiiii(ULA_GET_T(),f); // cycle count; unused?
		fputiiii((z80_irq&1)+(z80_wz<<16),f); // extra flags; unused?
		kputmmmm(0X53504352,f); kputiiii(8,f); // "SPCR"
		fputc(ula_v1&7,f);
		fputc(type_id?ula_v2:0,f);
		fputc(type_id==3?ula_v3:0,f);
		fputc(ula_v1,f);
		kputiiii(0,f); // extra flags
		//#ifdef _WIN32 // performance measurement (1/2)
		//int ttt=GetTickCount();
		//#endif // used to tell whether ZLIB is slow
		if (type_id&&!(ula_v2&32)) // 128K?
			for (int i=0;i<8;++i)
				snap_save_ramp(i,f);
		else // 48K
		{
			snap_save_ramp(5,f);
			if (type_id||!ula_sixteen) // skip last 32K on a 16K machine
				snap_save_ramp(2,f),snap_save_ramp(0,f);
		}
		//#ifdef _WIN32 // performance measurement (2/2)
		//cprintf("SZX: %d ms\n",(int)(GetTickCount()-ttt));
		//#endif // useless thanks to quick huff_main()
		if (type_id||!psg_disabled) // AY chip?
		{
			kputmmmm(0X41590000,f),kputiiii(1+1+16,f); // "AY\000\000"
			fputc(2,f); // ZXSTAYF_128AY
			fputc(psg_index,f);
			fwrite1(psg_table,16,f);
		}
		if (ulaplus_enabled&&ulaplus_table[64]) // ULAPLUS?
		{
			kputmmmm(0X504C5454,f),kputiiii(1+1+64,f); // "PLTT"
			fputc(ulaplus_table[64],f);
			fputc(ulaplus_index,f);
			fwrite1(ulaplus_table,64,f);
		}
		if (!dac_disabled&&dac_voice)
			kputmmmm(0X434F5658,f),kputiiii(4,f),kputiiii(0,f); // "COVX"
		#ifdef Z80_DANDANATOR
		if (mem_dandanator&&dandanator_cfg[4]<32) // DANDANATOR?
			kputmmmm(0X444E5452,f),kputiiii(4,f),fwrite1(&dandanator_cfg[4],4,f); // "DNTR"
		#endif
	}
	else if (globbing("*.z80",s,1))
	{
		if (!(f=puff_fopen(s,"wb"))) return 1;
		header[ 0]=z80_af.b.h,header[ 1]=z80_af.b.l;
		SNAP_SAVE_Z80W( 2,z80_bc);
		SNAP_SAVE_Z80W( 4,z80_hl);
		//SNAP_SAVE_Z80W( 6,z80_pc); // V1 only!
		SNAP_SAVE_Z80W( 8,z80_sp);
		header[10]=z80_ir.b.h,header[11]=z80_ir.b.l&127;
		header[12]=(z80_ir.b.l>>7)+(ula_v1&7)*2;
		SNAP_SAVE_Z80W(13,z80_de);
		SNAP_SAVE_Z80W(15,z80_bc2);
		SNAP_SAVE_Z80W(17,z80_de2);
		SNAP_SAVE_Z80W(19,z80_hl2);
		header[21]=z80_af2.b.h,header[22]=z80_af2.b.l;
		SNAP_SAVE_Z80W(23,z80_iy);
		SNAP_SAVE_Z80W(25,z80_ix);
		header[28]=header[27]=z80_iff.b.l&1;
		header[29]=z80_imd&3;
		header[30]=55; // V3!
		SNAP_SAVE_Z80W(32,z80_pc);
		header[34]=!type_id?0:type_id<3?ula_pentagon?9:4:7; // i.e. 48K, PENTAGON 128K, SINCLAIR 128K, PLUS3; see [37] for 16K, PLUS2 and PLUS2A
		if (type_id) header[35]=ula_v2; // 128K mode
		header[37]=(!type_id?ula_sixteen:type_id<3?type_id==2:disc_disabled)?135:7; // 48K becomes 16K, 128K becomes PLUS2, PLUS3 becomes PLUS2A
		header[38]=psg_index;
		MEMSAVE(&header[39],psg_table);
		header[55]=9; // compatible value after IRQ is over
		#ifdef Z80_DANDANATOR
		if (mem_dandanator&&dandanator_cfg[4]<32) // unused MGT type becomes 32|DANDANATOR_ROM_PAGE
			header[83]=dandanator_cfg[4]+32;
		#endif
		if (type_id>2) header[86]=ula_v3; // PLUS3 config
		header[61]=header[62]=(ula_v3&1)?255:0; // ROM/RAM flags, not sure if actually checked...
		fwrite1(header,30+2+55,f);
		if (!type_id) // non-128K machine?
		{
			snap_save_rled(8,5,f); // first 16K
			if (!ula_sixteen) snap_save_rled(4,2,f),snap_save_rled(5,0,f); // 48K?
		}
		else if (ula_v2&32) // 128K in 48K mode?
			snap_save_rled(8,5,f),snap_save_rled(5,2,f),snap_save_rled(3+(ula_v2&7),ula_v2&7,f);
		else for (int i=0;i<8;++i)
			snap_save_rled(i+3,i,f);
	}
	#if 0 // the SP format is useless outside 48K :-(
	else if (globbing("*.sp",s,1))
	{
		if (!(f=puff_fopen(s,"wb"))) return 1;
		mputii(0X5350,f); // "SP"
		fputii(!type_id&&ula_sixteen?0X4000:0XC000,f);
		kputii(0X4000,f);
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
		fputii(z80_pc .w,f);
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
	STRCOPY(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

#define SNAP_LOAD_Z80W(x,r) (r.b.l=header[x],r.b.h=header[x+1])
int snap_load_rled(BYTE *t,int o,BYTE *s,int i) // unpack a Z80-type compressed block; 0 OK, !0 ERROR
{
	int x,y; while (i>0&&o>0)
		if (--i,(x=*s++)!=0xED)
			--o,*t++=x;
		else if (--i,(y=*s++)!=0xED)
			o-=2,*t++=x,*t++=y;
		else // can size be zero!?
			{ i-=2,x=*s++; for (y=*s++,o-=x;x;--x) *t++=y; }
	return i|o;
}
int snap_load(char *s) // load a snapshot. `s` path, NULL to reload; 0 OK, !0 ERROR
{
	FILE *f=puff_fopen(s,"rb");
	if (!f) return 1;
	BYTE header[96]; int i,q;
	if (fread1(header,8,f)!=8||equalsmmmm(header,0x4D56202D)) // truncated file? disc image or CPC snapshot ("MV -"...)?
		return puff_fclose(f),1;
	if (equalsmmmm(header,0X5A585354)) // SZX format; it has its own ID, "ZXST"
	{
		cprintf("SZX snapshot: "); ula_pentagon=0;
		ula_v3=4,ula_v2=48; // disable PLUS3 and 128K by default
		type_id=(header[6]<2||header[6]==15)?0:header[6]<3?1:header[6]<4?2:header[6]<7?3:(ula_pentagon=1); // chMachineId
		if (!type_id) ula_sixteen=!header[6]; // just one way to set the Spectrum 16K mode from this snapshot
		#ifdef Z80_DANDANATOR
		dandanator_cfg[4]=32,dandanator_cfg[5]=dandanator_cfg[6]=dandanator_cfg[7]=0;
		#endif
		#ifdef PSG_PLAYCITY
		playcity_reset();
		#endif
		while ((i=fgetmmmm(f),q=fgetiiii(f))>=0)
		{
			cprintf("%08X:%04X ",i,q);
			if (i==0X5A383052) // "Z80R", the Z80 registers
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
			else if (i==0X53504352) // "SPCR", the ULA config
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
			else if (i==0X52414D50&&q<0X8000) // "RAMP", the RAM contents
			{
				fread1(header,3,f);
				BYTE *t=&mem_ram[(header[2]&7)<<14]; // 128K max!
				if (header[0]&1)
				{
					fread1(mem_16k,q-3,f); // temp.buffer
					if (q=(puff_zlib(t,0X4000,mem_16k,q-3)!=0X4000))
					{
						cprintf("ERROR %08X/%08X:%08X! ",puff_data-puff_src,puff_srcl,puff_tgtl);
						break;
					} //else q=0;
				}
				else
				{
					fread1(t,0X4000,f);
					q-=3+0X4000;
				}
			}
			else if (i==0X41590000) // "AY\000\000", the AY chip
			{
				fgetc(f);
				psg_table_select(fgetc(f));
				fread1(psg_table,16,f);
				q-=1+1+16;
			}
			else if (i==0X434F5658) // "COVX", the Covox $FB DAC
				dac_disabled=0;
			#ifdef Z80_DANDANATOR
			else if (i==0X444E5452) // "DNTR", the Dandanator
			{
				fread1(&dandanator_cfg[4],4,f); // only cfg[4] (ROM page) is truly essential
				if (!mem_dandanator&&*dandanator_path)
					if (dandanator_load(dandanator_path)) // insert last known Dandanator file
						dandanator_reset(); // nuke these variables on failure :-(
				q-=4;
			}
			#endif
			else if (i==0X504C5454) // "PLTT", the ULAPLUS palette
			{
				ulaplus_table[64]=fgetc(f); // configuration
				ulaplus_index=fgetc(f); if (ulaplus_index>64) ulaplus_index=64; // palette index
				fread1(ulaplus_table,64,f); // palette
				ulaplus_enabled=1; video_xlat_clut();
				q-=1+1+64;
			}
			#if 0 // useless?
			else if (i==0X4A4F5900) // "JOY\000", the joysticks
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
	else if (globbing("*.sp",s,1)) // "SP" isn't very useful nowadays, but stays worth reading
	{
		if (!strcmp("SP",header)&&header[5]==64) // there are two versions, one with a 6-byte prefix...
		{
			SNAP_LOAD_Z80W( 6,z80_bc);
			ula_sixteen=header[3]<128; // 16K or 48K
			fread1(&header[2],38-8,f); // overwrite
		}
		else // ...and another one without prefix
		{
			fread1(&header[8],32-8,f); // append
			SNAP_LOAD_Z80W( 0,z80_bc);
		}
		SNAP_LOAD_Z80W( 2,z80_de);
		SNAP_LOAD_Z80W( 4,z80_hl);
		SNAP_LOAD_Z80W( 6,z80_af);
		SNAP_LOAD_Z80W( 8,z80_ix);
		SNAP_LOAD_Z80W(10,z80_iy);
		SNAP_LOAD_Z80W(12,z80_bc2);
		SNAP_LOAD_Z80W(14,z80_de2);
		SNAP_LOAD_Z80W(16,z80_hl2);
		SNAP_LOAD_Z80W(18,z80_af2);
		SNAP_LOAD_Z80W(20,z80_ir);
		SNAP_LOAD_Z80W(22,z80_sp);
		SNAP_LOAD_Z80W(24,z80_pc);
		// WORD at 26 is unused
		ula_v1_send(header[28]&7);
		z80_iff.w=header[30]&1?257:0;
		z80_imd=header[30]&2?2:1;
		// BYTE at 31 is unused
		fread1(&mem_ram[5<<14],1<<14,f);
		fread1(&mem_ram[2<<14],1<<14,f);
		fread1(&mem_ram[0<<14],1<<14,f);
		type_id=0,ula_v3=4,ula_v2=48; // never 128K :-(
	}
	#if 0
	else if (globbing("*.sit",s,1)) // "SIT" is another obsolete yet simple snapshot format
	{
		fread1(&header[8],28-8,f); // append
		SNAP_LOAD_Z80W( 0,z80_bc);
		SNAP_LOAD_Z80W( 2,z80_de);
		SNAP_LOAD_Z80W( 4,z80_hl);
		SNAP_LOAD_Z80W( 6,z80_af);
		SNAP_LOAD_Z80W( 8,z80_ix);
		SNAP_LOAD_Z80W(10,z80_iy);
		SNAP_LOAD_Z80W(12,z80_sp);
		SNAP_LOAD_Z80W(14,z80_pc);
		SNAP_LOAD_Z80W(16,z80_ir);
		SNAP_LOAD_Z80W(18,z80_bc2);
		SNAP_LOAD_Z80W(20,z80_de2);
		SNAP_LOAD_Z80W(22,z80_hl2);
		SNAP_LOAD_Z80W(24,z80_af2);
		z80_imd=header[26]&2?2:1;
		ula_v1_send(header[27]&7);
		fread1(&mem_ram[5<<14],1<<14,f);
		fread1(&mem_ram[2<<14],1<<14,f);
		fread1(&mem_ram[0<<14],1<<14,f);
		type_id=0,ula_v3=4,ula_v2=48; // never 128K :-(
	}
	#endif
	else if (globbing("*.z80",s,1)) // Z80 format files are defined by extension -- no magic numbers at all!
	{
		cprintf("Z80 snapshot: ");
		ula_pentagon=type_id=ula_sixteen=0,ula_v3=4,ula_v2=48; // set 48K and disable PLUS3 and 128K by default
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
		#ifdef Z80_DANDANATOR
		dandanator_cfg[4]=32,dandanator_cfg[5]=dandanator_cfg[6]=dandanator_cfg[7]=0;
		#endif
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
				#ifdef Z80_DANDANATOR
				if (header[83]>=32)
				{
					if (dandanator_cfg[4]=header[83]-32,(!mem_dandanator&&*dandanator_path))
						if (dandanator_load(dandanator_path)) // insert last known Dandanator file
							dandanator_reset(); // nuke these variables on failure :-(
				}
				#endif
				if (i>=55&&header[34]==7)
				{
					type_id=3,ula_v3=header[86]&15; // PLUS3
					disc_disabled=header[37]>=128; // PLUS2A
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
			while ((i=fgetii(f))>0&&(q=fgetc(f))>=0) // q<0 = feof
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
					}
				if (i==0xFFFF) // uncompressed
					fread1(&mem_ram[q<<14],1<<14,f);
				else
				{
					fread1(&mem_ram[8<<14],i,f); if (q!=8) // don't unpack dummy 16K
						if (snap_load_rled(&mem_ram[q<<14],1<<14,&mem_ram[8<<14],i))
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
				snap_load_rled(&mem_ram[5<<14],3<<14,mem_ram,i-4);
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
		int prev_id=type_id; type_id=ula_sixteen=0,ula_v3=4,ula_v2=48; // set 48K and reset PLUS3 and 128K again
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
		dandanator_cfg[4]=32,dandanator_cfg[5]=dandanator_cfg[6]=dandanator_cfg[7]=0;
		#endif
		if (fread1(header,4,f)==4) // new snapshot type, 128K-ready?
		{
			SNAP_LOAD_Z80W(0,z80_pc);
			if (!(header[2]&32)) // 128K snapshot?
			{
				#ifdef Z80_DANDANATOR
				if (header[3]>=32) // Dandanator snapshot?
				{
					if (dandanator_cfg[4]=header[3]-32,(!mem_dandanator&&*dandanator_path))
						if (dandanator_load(dandanator_path)) // insert last known Dandanator file
							dandanator_reset(); // nuke these variables on failure :-(
				}
				else
				#endif
				if (header[3]>=16) // PLUS3 snapshot?
					type_id=prev_id=3,ula_v3=header[3]&15; // *force* upgrade to PLUS3!
				else // 128K/PLUS2 snapshot
					trdos_mapped=header[3]&1,type_id=prev_id=prev_id<2?1:2; // upgrade to 128K or downgrade to PLUS2
			}
			ula_v2=header[2]&63; // set 128K mode
			MEMNCPY(&mem_ram[(ula_v2&7)<<14],&mem_ram[0x00000],1<<14); // move bank 0 to active bank
			if (!(ula_v2&32)) // don't read more datas on 48K mode
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
		if (snap_extended&&type_id<prev_id) type_id=prev_id; // don't "downgrade" on flexible mode
	}
	else // unknown type
		return puff_fclose(f),1;
	bios_reload(); // reload BIOS if required!
	z80_int=z80_irq=0; // avoid nasty surprises!
	psg_all_update();
	ula_update(),mmu_update(); // adjust RAM and ULA models

	debug_reset();
	MEMBYTE(z80_tape_index,-1); // TAPE_FASTLOAD, avoid false positives!
	STRCOPY(snap_path,s);
	return snap_done=!puff_fclose(f),0;
}

// "autorun" file and logic operations ------------------------------ //

int any_load(char *s,int q) // load a file regardless of format. `s` path, `q` autorun; 0 OK, !0 ERROR
{
	autorun_m=0; if (!s||!*s) return -1; // cancel any autoloading yet; reject invalid paths
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
					{
						if (trdos_open(s,0,!(disc_filemode&1)))
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
					dandanator_remove(),all_reset(),bios_reload(); // force a firmware RELOAD (redundant?)
					if (diskette_mem[0]) // TR-DOS lacks a precise boot procedure, it needs some hacking :-(
					{
						z80_pc.w=0; // skip 128K power-up boost, it's incompatible with USR0
						ula_v2_send(0X10); // force USR0 mode
						autorun_m=2; autorun_t=96; // USR0 always needs typing; OPENSE boots more slowly than the source BIOS
					}
					else
					{
						if (type_id&&mem_rom[0X8020]==0XEF) // detect machines without a main menu
							autorun_m=3,autorun_t=(type_id==3?150:75); // menu-based 128K machines; PLUS3 is very slow
						else
							autorun_m=1,autorun_t=99; // 48K and menu-less 128K machines, f.e. OPENSE with 128K stub
					}
				}
			}
			else dandanator_remove(),all_reset();//,tape_close(),disc_closeall(),trdos_closeall(); // load bios? reset!
		#ifdef Z80_DANDANATOR
		else all_reset(); // dandanator? reset!
		#endif
	}
	if (q) STRCOPY(autorun_path,s);
	return 0;
}

// auxiliary user interface operations ------------------------------ //

int session_kbjoy(void) // update keys+joystick
{
	MEMLOAD(kbd_bits,kbd_bit);
	for (int i=0;i<8;++i) if (joy_bit&(1<<i)) { int j=kbd_joy[i]; kbd_bits[j>>3]|=+1<<(j&7); }
	// handle composite keys
	kbd_bits[0]|=((kbd_bit[ 9]|kbd_bit[10]|kbd_bit[11])?1:0)|kbd_bit[12]; // CAPS SHIFT is [0]&1
	kbd_bits[3]|=  kbd_bit[ 9]|kbd_bit[13]; // SINCLAIR 2
	kbd_bits[4]|=  kbd_bit[10]|kbd_bit[14]; // SINCLAIR 1
	kbd_bits[7]|=((kbd_bit[12]|kbd_bit[13]|kbd_bit[14]|kbd_bit[15])?2:0)|kbd_bit[11]|kbd_bit[15]; // SYMBOL SHIFT is [7]&2
	return 0; // should never fail!
}

char txt_error_snap_save[]="Cannot save snapshot!";
char file_pattern[]="*.csw;*.dsk;*.mld;*.pzx;*.rom;*.scl;*.sna;*.sit;*.sp;*.szx;*.tap;*.trd;*.tzx;*.wav;*.z80"; // from A to Z

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
	"0x4800 Play tape\tCtrl-Shift-F8\n"
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
	"=\n"
	"0xC600 Pentagon timings\tShift-F6\n"
	"0x8513 ULAplus hi-colour\n"
	"0x8512 ULA Issue-2 type\n"
	"0x8514 Reduce 48K to 16K\n"
	"0x8511 DRAM screen snow\n"
	"0x8515 NMOS-like OUT (C)\n"
	"0x8516 NEC-like SCF/CCF\n"
	"0x851A Chromatron effects\n"
	"=\n"
	"0x8521 Kempston joystick\n"
	"0x8522 Sinclair 1 joystick\n" // 98670: this is port 2 of Interface 2
	"0x8523 Sinclair 2 joystick\n" // 43125: ditto, this is port 1 of IF2
	"0x8524 Cursor/AGF joystick\n" // 76580 (without CAPS SHIFT)
	"0x8525 QAOP+Space joystick\n"
	"0x8528 Gunstick (MHT)\n"
	"0x8517 AY-Melodik audio\n"
	#ifdef PSG_PLAYCITY
	"0x8518 Turbosound audio\n"
	"0x8519 Covox $FB audio\n"
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
	"0xC408 Surround mode\n"
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
	"0x8908 Fine X-blending\n"
	"0x8909 Fine Gigascreen\n"
	#endif
	"=\n"
	"0x9100 Raise frameskip\tNum.+\n"
	"0x9200 Lower frameskip\tNum.-\n"
	"0x9300 Max frameskip\tNum.*\n"
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
	session_menucheck(0x8700,type_id!=3?!!diskette_mem[0]:!!disc[0]);
	session_menucheck(0xC700,type_id!=3?!!diskette_mem[1]:!!disc[1]);
	session_menucheck(0x0701,disc_flip[0]);
	session_menucheck(0x4701,disc_flip[1]);
	session_menucheck(0x8800,tape_type>=0&&tape);
	session_menucheck(0xC800,tape_type<0&&tape);
	session_menucheck(0x4800,tape_enabled<0);
	session_menucheck(0x4801,tape_polarity);
	session_menucheck(0x0900,tape_skipload);
	session_menucheck(0x0901,tape_rewind);
	session_menucheck(0x4900,tape_fastload);
	session_menucheck(0x0400,session_key2joy);
	session_menuradio(0x0601+multi_t,0x0601,0x0604);
	session_menucheck(0x0605,power_boost-POWER_BOOST0);
	session_menuradio(0x8521+joy1_type,0x8521,0x8525);
	session_menucheck(0x8528,litegun);
	session_menucheck(0x8590,!(disc_filemode&2));
	session_menucheck(0x8591,disc_filemode&1);
	session_menucheck(0x8510,!(disc_disabled&1));
	session_menucheck(0xC600,ula_pentagon);
	session_menucheck(0x8511,!(ula_snow_disabled));
	session_menucheck(0x8512,ula_v1_issue!=ULA_V1_ISSUE3);
	session_menucheck(0x8513,ulaplus_enabled);
	session_menucheck(0x8514,ula_sixteen);
	session_menucheck(0x8515,!Z80_0XED71);
	session_menucheck(0x8516,z80_xcf_bit);
	session_menucheck(0x8517,!psg_disabled);
	#ifdef Z80_DANDANATOR
	session_menucheck(0xC500,!!mem_dandanator);
	session_menucheck(0x0510,dandanator_canwrite);
	#endif
	#ifdef PSG_PLAYCITY
	session_menucheck(0x8518,!playcity_disabled);
	session_menucheck(0x8519,!dac_disabled);
	#endif
	session_menucheck(0x851A,chromatrons&1);
	session_menucheck(0x851F,!(snap_extended));
	session_menucheck(0x852F,!!printer);
	session_menucheck(0x9300,video_framelimit==MAIN_FRAMESKIP_MASK);
	session_menucheck(0x9400,!video_framelimit);
	session_menucheck(0x8901,onscreen_flag);
	session_menucheck(0x8902,video_filter&VIDEO_FILTER_MASK_Y);
	session_menucheck(0x8903,video_filter&VIDEO_FILTER_MASK_X);
	session_menucheck(0x8904,video_filter&VIDEO_FILTER_MASK_Z);
	session_menucheck(0x8905,video_lineblend);
	session_menucheck(0x8906,video_pageblend);
	#ifdef VIDEO_FILTER_BLUR0
	session_menucheck(0x8908,video_finemicro);
	session_menucheck(0x8909,video_fineblend);
	#endif
	session_menuradio(0x8A10+(session_fullblit?0:1+session_zoomblit),0X8A10,0X8A15);
	session_menucheck(0x8A00,!session_softblit);
	session_menuradio(0x8A01+session_softplay,0x8A01,0x8A04);
	session_menuradio(0x8B01+video_type,0x8B01,0x8B05);
	session_menuradio(0x0B01+video_scanline,0x0B01,0x0B04);
	MEMLOAD(kbd_joy,joy1_types[joy1_type]);
	#if AUDIO_CHANNELS > 1
	session_menucheck(0xC408,audio_surround);
	session_menuradio(0xC401+audio_mixmode,0xC401,0xC404);
	#if !AUDIO_ALWAYS_MONO
	for (int i=0,x=audio_surround?0:audio_mixmode;i<3;++i)
	{
		int j=i?!psg_disabled?3-i:i:0; // swap B and C if AY-Melodik is enabled: 128K is LEFT:RIGHT:MIDDLE, Melodik is LEFT:MIDDLE:RIGHT
		psg_stereo[i][0]=256+audio_stereos[x][j],psg_stereo[i][1]=256-audio_stereos[x][j];
		#ifdef PSG_PLAYCITY
		playcity_stereo[0][i][0]=psg_stereo[i][0],playcity_stereo[0][i][1]=psg_stereo[i][1]; // TURBO SOUND chip shares stereo with the PSG
		#endif
	}
	#endif
	#endif
	video_resetscanline(),debug_dirty=1; sprintf(session_info,"%d:%s %s %0.1fMHz"//" | disc %s | tape %s | %s"
		,(ula_v2&32)?ula_sixteen&&!type_id?16:48:128,
		type_id?type_id>1?type_id>2?!disc_disabled?"Plus3":"Plus2A":"Plus2":"128K":"48K",
		ula_pentagon?"Pentagon":type_id?type_id>2?"ULA v3":"ULA v2":"ULA v1",3.5*(1<<multi_t));
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
				"\t(shift: play/stop)" // "\t"
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
				"Num.+\tRaise frameskip" MESSAGEBOX_WIDETAB
				"Num.*\tMax frameskip"
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
				"ZX Spectrum emulator written by Cesar Nicolas-Gonzalez\n"
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
		case 0x8408: { if (session_shift) audio_surround^=1; } break;
		case 0x0400: // ^F4: TOGGLE JOYSTICK
			session_key2joy=!session_key2joy;
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
		case 0x8521: // KEMPSTON JOYSTICK
		case 0x8522: // SINCLAIR 1 STICK
		case 0x8523: // SINCLAIR 2 STICK
		case 0x8524: // CURSOR JOYSTICK
		case 0x8525: // QAOP+M JOYSTICK
			joy1_type=k-0x8521; // 0,1,2,3,4
			break;
		case 0x8528: // MHT GUNSTICK
			litegun=!litegun;
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
			video_xlat_clut();
			break;
		case 0x8514: // 16K MODE
			if (!type_id) { ula_sixteen=!ula_sixteen; ula_update(),mmu_update(); }
			break;
		case 0x8515: // NMOS/CMOS Z80
			Z80_0XED71=~Z80_0XED71;
			break;
		case 0x8516: // NEC/ZILOG Z80
			z80_xcf_bit=!z80_xcf_bit; // SCF/CCF (XCF quirk)
			break;
		case 0x8517:
			if ((psg_disabled=!psg_disabled)&&!type_id) // mute on 48K!
				psg_table_sendto(8,0),psg_table_sendto(9,0),psg_table_sendto(10,0);
			break;
		#ifdef PSG_PLAYCITY
		case 0x8518: // PLAYCITY
			if (playcity_disabled=!playcity_disabled)
				playcity_reset();
			break;
		case 0x8519: // COVOX
			if (dac_disabled=!dac_disabled)
				dac_voice=0;
			break;
		#endif
		case 0x851A: // CHROMATRONS!
			chromatrons^=1;
			break;
		case 0x851F:
			snap_extended=!snap_extended;
			break;
		case 0x8500: // F5: LOAD FIRMWARE.. // INSERT DANDANATOR..
			#ifdef Z80_DANDANATOR
			if (session_shift)
			{
				if (s=puff_session_getfile(dandanator_path,"*.mld;*.rom","Insert Dandanator card"))
				{
					if (dandanator_load(s)) // error? warn!
						session_message(txt_error_any_load,txt_error);
					else
						all_reset(); // setup and reset
				}
			}
			else
			#endif
			if (s=puff_session_getfile(bios_path,"*.rom","Load firmware"))
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
			//case 0xC600: // +SHIFT: PENTAGON TIMINGS
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
			multi_u=(1<<(multi_t=(multi_t+(session_shift?-1:1))&3))-1;
			mmu_update(); // turbo cancels all contention!
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
						session_message("Cannot open disc!",txt_error); // *!* shall we show a warning when disc_open() ignores "canwrite"?
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
			else if (s=puff_session_getfile(tape_path,"*.csw;*.pzx;*.tap;*.tzx;*.wav","Insert tape"))
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
		#ifdef VIDEO_FILTER_BLUR0
		case 0x8908: // MICROWAVES
			video_finemicro^=1;
			break;
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
				{ video_scanline=0,video_pageblend^=1; }
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
			if (video_framelimit<MAIN_FRAMESKIP_MASK)
				++video_framelimit;
			break;
		case 0x9200: // ^NUM.-
		case 0x1200: // NUM.-: DECREASE FRAMESKIP
			if (video_framelimit>0)
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

void session_configreadmore(char *s) // parse a pre-processed configuration line: `session_parmtr` keeps the value name, `s` points to its value
{
	int i; char *t=UTF8_BOM(session_parmtr); if (!s||!*s||!session_parmtr[0]) {} // ignore if empty or internal!
	else if (i=eval_hex(*s)&31,!strcasecmp(t,"type")) { if (i<length(bios_system)) type_id=i; }
	else if (!strcasecmp(t,"joy1")) { if (i<length(joy1_types)) joy1_type=i; }
	else if (!strcasecmp(t,"ulas")) ulaplus_enabled=i&1,ula_pentagon=(i>>1)&1;
	else if (!strcasecmp(t,"unit")) disc_filemode=i&3,disc_disabled=(i>>2)&1;
	else if (!strcasecmp(t,"misc")) snap_extended=i&1,ula_snow_disabled=(i>>1)&1,psg_disabled=(i>>2)&1;
	#ifdef PSG_PLAYCITY
	else if (!strcasecmp(t,"play")) playcity_disabled=~i&1,dac_disabled=(~i>>1)&1;
	#endif
	else if (!strcasecmp(t,"file")) strcpy(autorun_path,s);
	else if (!strcasecmp(t,"snap")) strcpy(snap_path,s);
	else if (!strcasecmp(t,"tape")) strcpy(tape_path,s);
	else if (!strcasecmp(t,"disc")) strcpy(disc_path,s),strcpy(trdos_path,s);
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
	native2usbkey(kbd_k2j,KBD_JOY_UNIQUE); fprintf(f,"type %d\njoy1 %d\nulas %d\nunit %d\nmisc %d\n"
		#ifdef PSG_PLAYCITY
		"play %d\n"
		#endif
		"file %s\nsnap %s\ntape %s\ndisc %s\nbios %s\n"
		#ifdef Z80_DANDANATOR
		"cart %s\n"
		#endif
		"vjoy %s\npalette %d\ncasette %d\ndebug %d\n",
		type_id,joy1_type,(ulaplus_enabled&1)+(ula_pentagon&1)*2,(disc_disabled&1)*4+disc_filemode,(psg_disabled&1)*4+(ula_snow_disabled&1)*2+snap_extended,
		#ifdef PSG_PLAYCITY
		(playcity_disabled?0:1)+(dac_disabled?0:2),
		#endif
		autorun_path,snap_path,tape_path,type_id==3?disc_path:trdos_path,bios_path, // PLUS3 and TRDOS discs are different
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
						joy1_type=(BYTE)(argv[i][j++]-'0');
						if (joy1_type<0||joy1_type>=length(joy1_types))
							i=argc; // help!
						break;
					//case 'i': break;
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
						k=1; type_id=(BYTE)(argv[i][j++]-'0');
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
					case 's':
						session_audio=1;
						break;
					case 'S':
						session_audio=0;
						break;
					case 't':
						#if AUDIO_CHANNELS > 1
						audio_mixmode=length(audio_stereos)-1;
						#endif
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
		else if (k=0,any_load(puff_makebasepath(argv[i]),1)) // a succesful any_load() would be destroyed by a "k=1"!
			i=argc; // help!
	if (i>argc)
		return
			printfusage("usage: " my_caption " [option..] [file..]\n"
			"  -cN\tscanline type (0..7)\n"
			"  -CN\tcolour palette (0..4)\n"
			"  -d\tdebug mode\n"
			"  -g0\tKempston joystick\n"
			"  -g1\tSinclair 1 joystick\n"
			"  -g2\tSinclair 2 joystick\n"
			"  -g3\tCursor/AGF joystick\n"
			"  -g4\tQAOP+Space joystick\n"
			"  -I\temulate Issue-2 ULA line\n"
			"  -j\tenable joystick keys\n"
			"  -J\tdisable joystick\n"
			"  -K\tenable Spectrum 16K mode\n"
			"  -m0\t48K firmware\n"
			"  -m1\t128K firmware\n"
			"  -m2\t+2 firmware\n"
			"  -m3\t+3 firmware\n"
			"  -o/O\tenable/disable onscreen status\n"
			"  -p/P\tenable/disable Pentagon timings\n"
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
	trdos_load("trdos.rom"); // not sure if we should check whether TR-DOS was loaded properly here: PLUS3 has its own disc system.
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
				UNLIKELY(z80_irq)?0: // IRQ events ("TIMING TESTS")
				((ula_limit_x-ula_count_x-1)<<2)<<multi_t); // ...without missing any IRQ and ULA deadlines!
		if (session_signal&SESSION_SIGNAL_FRAME) // end of frame?
		{
			if (audio_required)
			{
				if (audio_pos_z<AUDIO_LENGTH_Z) audio_main(TICKS_PER_FRAME); // fill sound buffer to the brim!
				#ifdef PSG_PLAYCITY
				if (!playcity_disabled)
					playcity_main(audio_frame,AUDIO_LENGTH_Z);
				#endif
				#if AUDIO_CHANNELS > 1
				if (audio_surround) if (audio_mixmode) session_surround(length(audio_stereos)-1-audio_mixmode);
				#endif
				audio_playframe();
			}
			if (video_required&&onscreen_flag)
			{
				if (disc_disabled)
					onscreen_text(+1,-3,"--\t--",0);
				else if (type_id!=3)
				{
					if (k=diskette_length) onscreen_char(+3,-3,diskette_target>=0?'W':'R');
					onscreen_byte(+1,-3,diskette_cursor[0],k&&(diskette_drive==0));
					onscreen_byte(+4,-3,diskette_cursor[1],k&&(diskette_drive==1));
				}
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
					if (autorun_m) // big letter "A"
						onscreen_bool(-7,-7,3,1,1),
						onscreen_bool(-7,-4,3,1,1),
						onscreen_bool(-8,-6,1,5,1),
						onscreen_bool(-4,-6,1,5,1);
					else
					{
						onscreen_bool(-7,-6,3,1,joy_bit&(1<<0)),
						onscreen_bool(-7,-2,3,1,joy_bit&(1<<1)),
						onscreen_bool(-8,-5,1,3,joy_bit&(1<<2)),
						onscreen_bool(-4,-5,1,3,joy_bit&(1<<3)),
						onscreen_bool(-6,-4,1,1,joy_bit&(15<<4));
					}
				}
				#ifdef DEBUG
				onscreen_byte(+1,+1,ula_fix_out,0);
				onscreen_byte(+4,+1,ula_fix_chr,0);
				#endif
				session_status();
			}
			{ static int z=-1; if (z!=!!dac_extra) z=!!dac_extra,psg_weight(dac_disabled?PSG_MAX_VOICE:PSG_MAX_VOICE*2/3); } // the COVOX DAC is loud and can't coexist with normal PSGs!
			// update session and continue
			if (!--autorun_t) autorun_next();
			dac_frame(); if (ym3_file) ym3_write(),ym3_flush();
			tape_skipping=audio_queue=0; // reset tape and audio flags
			if (tape_type<0&&tape) // tape is recording? play always!
				tape_enabled|=4;
			else if (tape_enabled>0) // tape is still busy?
				--tape_enabled;
			if (!tape_fastload) tape_song=0,tape_loud=1; else if (!tape_song) tape_loud=1;
			else tape_loud=0,--tape_song; // expect song to play for several frames
			tape_output=tape_type<0&&(ula_v1&8); // keep tape output consistent
			if (tape_signal)
			{
				if ((tape_signal<2||(ula_v2&32))) tape_enabled=0; // stop tape if required
				tape_signal=0,session_dirty=1; // update config
			}
			if (tape&&tape_filetell<tape_filesize&&tape_skipload&&!session_filmfile&&!tape_disabled&&tape_loud) // `tape_loud` implies `!tape_song`
				session_fast|=+2,audio_disabled|=+2; // abuse binary logic to reduce activity
			else
				session_fast&=~2,audio_disabled&=~2; // ditto, to restore normal activity
			session_update();
			//if (!audio_disabled) audio_main(1+ula_clash_z); // preload audio buffer
		}
	}
	// it's over, "acta est fabula"
	z80_close();
	trdos_closeall(); disc_closeall(); if (!*trdos_path) strcpy(trdos_path,disc_path); else if (!*disc_path) strcpy(disc_path,trdos_path); // avoid accidental losses
	tape_close(); ym3_close(); if (printer) printer_close();
	return session_byebye(),session_post();
}

BOOTSTRAP

// ============================================ END OF USER INTERFACE //
