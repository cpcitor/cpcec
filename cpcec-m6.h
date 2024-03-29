 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The MOS Technology 6510 and 6502 CPUs inside the Commodore 64 and
// its disc drive are emulated with a single source file that can be
// included as many times as there are chips. Several macros must be
// defined in advance for each chip:

// M65XX_RESET -- name of the RESET() procedure
// M65XX_START -- expression of PC on RESET
// M65XX_MAIN -- name of the interpreter
// HLII M65XX_PC -- the Program Counter
// BYTE M65XX_IRQ -- IRQ (+1..3) and NMI (+128) flags
// BYTE M65XX_INT -- backup of previous M65XX_IRQ
// void M65XX_IRQ_ACK -- the IRQ acknowledger
// void M65XX_NMI_ACK -- the NMI acknowledger (optional)
// BYTE M65XX_P -- the Processor status
// BYTE M65XX_A -- the Accumulator
// BYTE M65XX_X -- the X Register
// BYTE M65XX_Y -- the Y Register
// BYTE M65XX_S -- the Stack pointer
// void M65XX_SYNC(int) -- sync hardware `n` clock ticks
// void M65XX_PAGE(BYTE) -- setup PEEK/POKE address page
// BYTE M65XX_PEEK(WORD) -- receive byte from an address
// void M65XX_POKE(WORD,BYTE) -- send byte to an address
// BYTE M65XX_PEEKZERO(BYTE) -- receive byte from ZEROPAGE
// void M65XX_POKEZERO(BYTE,BYTE) -- send byte to ZEROPAGE
// BYTE M65XX_PULL(BYTE) -- receive byte from STACKPAGE
// void M65XX_PUSH(BYTE,BYTE) -- send byte to STACKPAGE
// void M65XX_DUMBPAGE(BYTE) -- "dumb" PEEK/POKE page setup
// BYTE M65XX_DUMBPEEK(WORD) -- "dumb" receive from address
// void M65XX_DUMBPOKE(WORD,BYTE) -- "dumb" send to address
// M65XX_HLT -- negative READY signal (optional)
// M65XX_SHW -- negative !READY-to-READY event (optional)
// M65XX_XVS -- external OVERFLOW signal (optional)
// M65XX_WAIT -- name of the wait-till-READY clock tick procedure
// M65XX_TICK -- name of the single clock tick procedure (++m65xx_t, etc)
// M65XX_TOCK -- name of the INT=IRQ procedure (M65XX_INT=M65XX_IRQ, etc)

// Please notice that M65XX_PEEK and M65XX_POKE must handle the special
// cases $0000 and $0001 when they emulate a 6510 instead of a 6502.

// Notice also that the difference between real and "dumb" PEEK/POKE
// is that the later can be simplified to "++m65xx_t" if we're sure
// of the null impact in the emulated system of double I/O actions
// such as "LDX #$10: LDA $DFCD,X" (reading from $DC0D and $DD0D)
// and "LSR $D019" (writing twice on $D019: operand and result).

// The macros M65XX_DUMBPUSH and M65XX_DUMBPEEKZERO are never required;
// M65XX_DUMBPULL and M65XX_DUMBPOKEZERO are dummied out because of the
// special status of the address space's pages 0 (ZERO) and 1 (STACK).

// BEGINNING OF MOS 6510/6502 EMULATION ============================= //

#ifdef DEBUG_HERE
int debug_trap_pc=0,debug_trap_sp;
void debug_reset(void) // resets volatile breakpoints and sets the debugger's PC and SP
{
	debug_point[debug_trap_pc]&=~128; // cancel volatile breakpoint!
	debug_inter=0; debug_trap_sp=1<<16; // cancel interrupt+return traps!
	debug_panel0_x=debug_panel3_x=0; debug_panel0_w=M65XX_PC.w; debug_panel3_w=M65XX_S+256;
}
#endif

#ifndef _M65XX_H_
#define _M65XX_H_

BYTE m65xx_p_adc[512],m65xx_p_sbc[512]; // Carry and oVerflow

void m65xx_setup(void) // unlike in the Z80, precalc'd tables are limited to the ADC and SBC operations
{
	// flag bit reference:
	// - 7, 0x80: N, Negative
	// - 6, 0x40: V, oVerflow
	// - 5, 0x20: always 1
	// - 4, 0x10: B, Break (1 for BRK, 0 for IRQ/NMI)
	// - 3, 0x08: D, Decimal
	// - 2, 0x04: I, Interrupt (0 enable, 1 disable)
	// - 1, 0x02: Z, Zero
	// - 0, 0x01: C, Carry (positive in ADC, negative in SBC and CMP)
	for (int i=0;i<256;++i)
		m65xx_p_sbc[255-i]=m65xx_p_adc[256+i]=(m65xx_p_sbc[511-i]=m65xx_p_adc[i]=(i>>1)&64)^65;
}

WORD debug_dasm_any(char *t,WORD p,BYTE q(WORD)) // where `BYTE q(WORD)` is a function that returns a byte from a memory map
{
	#define DEBUG_DASM_BYTE (w=q(p),++p,w)
	#define DEBUG_DASM_REL8 (WORD)(w=q(p),++p+(signed char)w)
	#define DEBUG_DASM_WORD (w=q(p),++p,w+=q(p)<<8,++p,w)
	const char opcodez[256][4]={
		"BRK","ORA","JAM","SLO", "NOP","ORA","ASL","SLO", "PHP","ORA","ASL","ANC", "NOP","ORA","ASL","SLO",
		"BPL","ORA","JAM","SLO", "NOP","ORA","ASL","SLO", "CLC","ORA","NOP","SLO", "NOP","ORA","ASL","SLO",
		"JSR","AND","JAM","RLA", "BIT","AND","ROL","RLA", "PLP","AND","ROL","ANC", "BIT","AND","ROL","RLA",
		"BMI","AND","JAM","RLA", "NOP","AND","ROL","RLA", "SEC","AND","NOP","RLA", "NOP","AND","ROL","RLA",
		"RTI","EOR","JAM","SRE", "NOP","EOR","LSR","SRE", "PHA","EOR","LSR","ASR", "JMP","EOR","LSR","SRE",
		"BVC","EOR","JAM","SRE", "NOP","EOR","LSR","SRE", "CLI","EOR","NOP","SRE", "NOP","EOR","LSR","SRE",
		"RTS","ADC","JAM","RRA", "NOP","ADC","ROR","RRA", "PLA","ADC","ROR","ARR", "JMP","ADC","ROR","RRA",
		"BVS","ADC","JAM","RRA", "NOP","ADC","ROR","RRA", "SEI","ADC","NOP","RRA", "NOP","ADC","ROR","RRA",
		"NOP","STA","NOP","SAX", "STY","STA","STX","SAX", "DEY","NOP","TXA","ANE", "STY","STA","STX","SAX",
		"BCC","STA","JAM","SHA", "STY","STA","STX","SAX", "TYA","STA","TXS","SHS", "SHY","STA","SHX","SHA",
		"LDY","LDA","LDX","LAX", "LDY","LDA","LDX","LAX", "TAY","LDA","TAX","LXA", "LDY","LDA","LDX","LAX",
		"BCS","LDA","JAM","LAX", "LDY","LDA","LDX","LAX", "CLV","LDA","TSX","LAS", "LDY","LDA","LDX","LAX",
		"CPY","CMP","NOP","DCP", "CPY","CMP","DEC","DCP", "INY","CMP","DEX","SBX", "CPY","CMP","DEC","DCP",
		"BNE","CMP","JAM","DCP", "NOP","CMP","DEC","DCP", "CLD","CMP","NOP","DCP", "NOP","CMP","DEC","DCP",
		"CPX","SBC","NOP","ISB", "CPX","SBC","INC","ISB", "INX","SBC","NOP","SBC", "CPX","SBC","INC","ISB",
		"BEQ","SBC","JAM","ISB", "NOP","SBC","INC","ISB", "SED","SBC","NOP","ISB", "NOP","SBC","INC","ISB"};
	WORD w; BYTE o=q(p); ++p; switch (o)
	{
		case 0X04: case 0X24: case 0X44: case 0X64: case 0X84: case 0XA4: case 0XC4: case 0XE4:
		case 0X05: case 0X25: case 0X45: case 0X65: case 0X85: case 0XA5: case 0XC5: case 0XE5:
		case 0X06: case 0X26: case 0X46: case 0X66: case 0X86: case 0XA6: case 0XC6: case 0XE6:
		case 0X07: case 0X27: case 0X47: case 0X67: case 0X87: case 0XA7: case 0XC7: case 0XE7:
			sprintf(t,"%s  $%02X",opcodez[o],DEBUG_DASM_BYTE); break;
		case 0X10: case 0X30: case 0X50: case 0X70: case 0X90: case 0XB0: case 0XD0: case 0XF0:
			sprintf(t,"%s  $%04X",opcodez[o],DEBUG_DASM_REL8); break;
		case 0X20:
		case 0X0C: case 0X2C: case 0X4C: case 0X8C: case 0XAC: case 0XCC: case 0XEC:
		case 0X0D: case 0X2D: case 0X4D: case 0X6D: case 0X8D: case 0XAD: case 0XCD: case 0XED:
		case 0X0E: case 0X2E: case 0X4E: case 0X6E: case 0X8E: case 0XAE: case 0XCE: case 0XEE:
		case 0X0F: case 0X2F: case 0X4F: case 0X6F: case 0XAF: case 0XCF: case 0XEF: case 0X8F:
			sprintf(t,"%s  $%04X",opcodez[o],DEBUG_DASM_WORD); break;
		case 0X00: case 0X80: case 0XA0: case 0XC0: case 0XE0:
		case 0X82: case 0XA2: case 0XC2: case 0XE2:
		case 0X09: case 0X29: case 0X49: case 0X69: case 0X89: case 0XA9: case 0XC9: case 0XE9:
		case 0X0B: case 0X2B: case 0X4B: case 0X6B: case 0X8B: case 0XAB: case 0XCB: case 0XEB:
			sprintf(t,"%s  #$%02X",opcodez[o],DEBUG_DASM_BYTE); break;
		case 0X01: case 0X21: case 0X41: case 0X61: case 0X81: case 0XA1: case 0XC1: case 0XE1:
		case 0X03: case 0X23: case 0X43: case 0X63: case 0X83: case 0XA3: case 0XC3: case 0XE3:
			sprintf(t,"%s  ($%02X,X)",opcodez[o],DEBUG_DASM_BYTE); break;
		case 0X11: case 0X31: case 0X51: case 0X71: case 0X91: case 0XB1: case 0XD1: case 0XF1:
		case 0X13: case 0X33: case 0X53: case 0X73: case 0X93: case 0XB3: case 0XD3: case 0XF3:
			sprintf(t,"%s  ($%02X),Y",opcodez[o],DEBUG_DASM_BYTE); break;
		case 0X19: case 0X39: case 0X59: case 0X79: case 0X99: case 0XB9: case 0XD9: case 0XF9:
		case 0X1B: case 0X3B: case 0X5B: case 0X7B: case 0X9B: case 0XBB: case 0XDB: case 0XFB:
		case 0X9E: case 0XBE:
		case 0X9F: case 0XBF:
			sprintf(t,"%s  $%04X,Y",opcodez[o],DEBUG_DASM_WORD); break;
		case 0X0A: case 0X2A: case 0X4A: case 0X6A:
			sprintf(t,"%s  A",opcodez[o]); break;
		case 0X6C: // $6C "%s" is "JMP"
			sprintf(t,"%s  ($%04X)",opcodez[o],DEBUG_DASM_WORD); break;
		case 0X14: case 0X34: case 0X54: case 0X74: case 0X94: case 0XB4: case 0XD4: case 0XF4:
		case 0X15: case 0X35: case 0X55: case 0X75: case 0X95: case 0XB5: case 0XD5: case 0XF5:
		case 0X16: case 0X36: case 0X56: case 0X76: case 0XD6: case 0XF6:
		case 0X17: case 0X37: case 0X57: case 0X77: case 0X97: case 0XB7: case 0XD7: case 0XF7:
			sprintf(t,"%s  $%02X,X",opcodez[o],DEBUG_DASM_BYTE); break;
		case 0X1C: case 0X3C: case 0X5C: case 0X7C: case 0X9C: case 0XBC: case 0XDC: case 0XFC:
		case 0X1D: case 0X3D: case 0X5D: case 0X7D: case 0X9D: case 0XBD: case 0XDD: case 0XFD:
		case 0X1E: case 0X3E: case 0X5E: case 0X7E: case 0XDE: case 0XFE:
		case 0X1F: case 0X3F: case 0X5F: case 0X7F: case 0XDF: case 0XFF:
			sprintf(t,"%s  $%04X,X",opcodez[o],DEBUG_DASM_WORD); break;
		case 0X96: case 0XB6:
			sprintf(t,"%s  $%02X,Y",opcodez[o],DEBUG_DASM_BYTE); break;
		default:
		//case 0X40: case 0X60:
		//case 0X02: case 0X12: case 0X22: case 0X32: case 0X42: case 0X52: case 0X62: case 0X72: case 0X92: case 0XB2: case 0XD2: case 0XF2:
		//case 0X08: case 0X18: case 0X28: case 0X38: case 0X48: case 0X58: case 0X68: case 0X78: case 0X88: case 0X98: case 0XA8: case 0XB8: case 0XC8: case 0XD8: case 0XE8: case 0XF8:
		//case 0X1A: case 0X3A: case 0X5A: case 0X7A: case 0X8A: case 0X9A: case 0XAA: case 0XBA: case 0XCA: case 0XDA: case 0XEA: case 0XFA:
			strcpy(t,/*%s*/opcodez[o]); break;
	};
	return p;
}

#define m65xx_close() ((void)0)

#define M65XX_MERGE_P (M65XX_P=(z?48:50)+(n&128)+(M65XX_P&77)) // 77 = 64+8+4+1, i.e. the flags V+D+I+C
#define M65XX_BREAK_P (M65XX_P=(z?32:34)+(n&128)+(M65XX_P&77)) // used by the IRQ/NMI handler to reset flag B
#define M65XX_SPLIT_P (z=~M65XX_P&2,n=M65XX_P&128) // `z` and `n` are the latest relevant zero-value and sign-value

// the memory access modes! notice that only the immediate mode feeds the value proper; other modes expect the operation to do the PEEKs and POKEs

// this happens in operations that waste additional ticks from "dumb" reads from PC
#define M65XX_BADPC do{ M65XX_WAIT; M65XX_DUMBPAGE(M65XX_PC.b.h); M65XX_DUMBPEEK(M65XX_PC.w); }while(0)
#define M65XX_OLDPC do{ M65XX_WAIT; M65XX_DUMBPEEK(M65XX_PC.w); }while(0)
// this happens when an operation builds a 16-bit address and the 8-bit carry is true!
#define M65XX_BADAW do{ M65XX_WAIT; M65XX_DUMBPAGE(q); i=q*256+a.b.l; M65XX_DUMBPEEK(i); }while(0)
#define M65XX_SHZAW do{ M65XX_WHAT; M65XX_DUMBPAGE(q); i=q*256+a.b.l; M65XX_DUMBPEEK(i); }while(0)
// #$NN: immediate; 1 T
#define M65XX_FETCH(o) do{ M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h); o=M65XX_PEEK(M65XX_PC.w); ++M65XX_PC.w; }while(0)
// $NN: zeropage; 1 T
#define M65XX_ZPG M65XX_FETCH(a.w)
// $NN,X: zeropage, X-indexed; 1 T
#define M65XX_ZPG_X_BADPC do{ M65XX_ZPG; a.b.l+=M65XX_X; M65XX_BADPC; }while(0)
// $NN,Y: zeropage, Y-indexed; 1 T
#define M65XX_ZPG_Y_IRQ_BADPC do{ M65XX_ZPG; a.b.l+=M65XX_Y; M65XX_BADPC; }while(0)
// ($NN,X) indirect, X-indexed; 4 T
#define M65XX_IND_X do{ M65XX_ZPG; q=a.b.l+M65XX_X; M65XX_BADPC; M65XX_WAIT; a.b.l=M65XX_PEEKZERO(q); ++q; M65XX_WAIT; a.b.h=M65XX_PEEKZERO(q); }while(0)
// ($NN),Y indirect, Y-indexed; 4/5 T
#define M65XX_IND_Y do{ M65XX_ZPG; M65XX_WAIT; q=M65XX_PEEKZERO(a.b.l); ++a.b.l; M65XX_WAIT; a.b.h=M65XX_PEEKZERO(a.b.l); a.b.l=q; q=a.b.h; a.w+=M65XX_Y; if (q!=a.b.h) M65XX_BADAW; }while(0)
#define M65XX_IND_Y_BADAW do{ M65XX_ZPG; M65XX_WAIT; q=M65XX_PEEKZERO(a.b.l); ++a.b.l; M65XX_WAIT; a.b.h=M65XX_PEEKZERO(a.b.l); a.b.l=q; q=a.b.h; a.w+=M65XX_Y; M65XX_BADAW; }while(0)
#define M65XX_IND_Y_SHZAW do{ M65XX_ZPG; M65XX_WAIT; q=M65XX_PEEKZERO(a.b.l); ++a.b.l; M65XX_WAIT; a.b.h=M65XX_PEEKZERO(a.b.l); a.b.l=q; q=a.b.h; a.w+=M65XX_Y; M65XX_SHZAW; }while(0)
// $NNNN absolute; 2 T
#define M65XX_ABS do{ M65XX_FETCH(a.b.l); M65XX_FETCH(a.b.h); }while(0)
// $NNNN,X absolute, X-indexed; 2/3 T
#define M65XX_ABS_X do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_X; if (q!=a.b.h) M65XX_BADAW; }while(0)
#define M65XX_ABS_X_BADAW do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_X; M65XX_BADAW; }while(0)
#define M65XX_ABS_X_SHZAW do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_X; M65XX_SHZAW; }while(0)
// $NNNN,Y absolute, Y-indexed; 2/3 T
#define M65XX_ABS_Y do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_Y; if (q!=a.b.h) M65XX_BADAW; }while(0)
#define M65XX_ABS_Y_BADAW do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_Y; M65XX_BADAW; }while(0)
#define M65XX_ABS_Y_SHZAW do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_Y; M65XX_SHZAW; }while(0)

// the operation macros!

#define M65XX_CARRY(r) ((r)?(M65XX_P|=1):(M65XX_P&=~1))
#define M65XX_SAX (M65XX_A&M65XX_X) // a.k.a. AXS; it appears in multiple operations
#define M65XX_DEC(r) (z=n=--r)
#define M65XX_INC(r) (z=n=++r)

// the following operations check the input `r` and update the accumulator and the flags

#define M65XX_ORA(r) (z=n=M65XX_A|=r)
#define M65XX_AND(r) (z=n=M65XX_A&=r)
#define M65XX_EOR(r) (z=n=M65XX_A^=r)

// the following operations store the input `r` in `o` and update the accumulator and/or the flags

int m65xx_d_adc(BYTE *o,BYTE *n,BYTE *z,BYTE *p,BYTE *a)
{
	int i=*p&1; *z=*a+*o+i; BYTE j=(*a&15)+(*o&15)+i; if (j>9) if ((j+=6)>31) j-=16;
	*n=i=j+(*a&240)+(*o&240); *p=(*p&~65)+(m65xx_p_adc[i^*a^*o]&64);
	return i>159?++*p,i+96:i;
}
int m65xx_d_sbc(BYTE *o,BYTE *n,BYTE *a)
{
	int i=(*a&240)-(*o&240); *n=(*a&15)-(*o&15)-*n; if (*n&16) *n-=6,i-=16;
	return (i<0?i-96:i)+(*n&15);
}
#define M65XX_ADC(r) ((o=r),(UNLIKELY(M65XX_P&8))?\
	M65XX_A=m65xx_d_adc(&o,&n,&z,&M65XX_P,&M65XX_A):\
	((i=M65XX_A+o+(M65XX_P&1)),(M65XX_P=(M65XX_P&~65)+m65xx_p_adc[i^M65XX_A^o]),z=n=M65XX_A=i))
#define M65XX_SBC(r) ((o=r),(n=~M65XX_P&1),(z=i=M65XX_A-o-n),(M65XX_P=(M65XX_P&~65)+m65xx_p_sbc[(i^M65XX_A^o)&511]),(UNLIKELY(M65XX_P&8))?\
	M65XX_A=m65xx_d_sbc(&o,&n,&M65XX_A),n=z:\
	(n=M65XX_A=i))

#define M65XX_CMP(r) (z=n=i=M65XX_A-r,M65XX_CARRY(i>=0))
#define M65XX_CPX(r) (z=n=i=M65XX_X-r,M65XX_CARRY(i>=0))
#define M65XX_CPY(r) (z=n=i=M65XX_Y-r,M65XX_CARRY(i>=0))
#define M65XX_BIT(r) (n=o=r,z=M65XX_A&o,M65XX_P=(M65XX_P&~64)+(o&64))

// the following operations store the input `r` in `o` and the output in `t`

#define M65XX_ASL(t,r) (o=r,z=n=t=o<<1,M65XX_P=(M65XX_P&~1)+(o>>7))
#define M65XX_LSR(t,r) (o=r,z=n=t=o>>1,M65XX_P=(M65XX_P&~1)+(o &1))
#define M65XX_ROL(t,r) (o=r,z=n=t=(o<<1)+(M65XX_P &1),M65XX_P=(M65XX_P&~1)+(o>>7))
#define M65XX_ROR(t,r) (o=r,z=n=t=(o>>1)+(M65XX_P<<7),M65XX_P=(M65XX_P&~1)+(o &1))

#define M65XX_SLO(t,r) (o=r,z=n=M65XX_A|=t=o<<1,M65XX_P=(M65XX_P&~1)+(o>>7)) // a.k.a. ASO
#define M65XX_SRE(t,r) (o=r,z=n=M65XX_A^=t=o>>1,M65XX_P=(M65XX_P&~1)+(o &1)) // a.k.a. LSE
#define M65XX_RLA(t,r) (o=((n=r)<<1)+(M65XX_P &1),M65XX_P=(M65XX_P&~1)+(n>>7),z=n=M65XX_A&=t=o)
#define M65XX_RRA(t,r) (t=o=((n=r)>>1)+(M65XX_P<<7),M65XX_P=(M65XX_P&~1)+(n&1),M65XX_ADC(o))
#define M65XX_DCP(t,r) (t=o=(r-1),M65XX_CMP(o)) // a.k.a. DCM
#define M65XX_ISB(t,r) (t=o=(r+1),M65XX_SBC(o)) // a.k.a. INS

#endif

// the past definitions are shared by all 6510/6502 chips; the following ones are individual

#ifdef M65XX_SHW
	#define M65XX_SHZ(t,r) do{ i=(q+1)&(o=r); if (q!=a.b.h) a.b.h=i; if (LIKELY(M65XX_SHW)) t=i; else t=o; }while(0)
#else // simpler version without RDY handling
	#define M65XX_SHZ(t,r) (t=r)
#endif

// M65XX interpreter ------------------------------------------------ //

void M65XX_RESET(void) // resets the M65XX chip; notice that the start address is defined by the external hardware
{
	#ifdef M65XX_REU
	reu_reset(); reu_size=0;
	#endif
	M65XX_PC.w=M65XX_START; M65XX_A=M65XX_X=M65XX_Y=M65XX_INT=0; M65XX_S=0XFF; M65XX_P=32+16+4;  // empty stack, ignore IRQs
}

void M65XX_MAIN(int _t_) // runs the M65XX chip for at least `_t_` clock ticks; notice that _t_<1 runs exactly one operation
{
	int m65xx_t=0; M65XX_LOCAL;
	#ifdef M65XX_REU
	if (reu_size)
		do
		{
			BYTE o; M65XX_PAGE(reu_word.b.h); switch (reu_mode)
			{
				case 0: // REU <-- C64
					M65XX_WAIT; o=M65XX_PEEK(reu_word.w); if (reu_addr<=reu_cap) { reu_ram[reu_addr]=o; reu_dirty|=reu_addr; } // `reu_dirty` only changes here
					break;
				case 1: // C64 <-- REU
					o=(reu_addr<=reu_cap)?reu_ram[reu_addr]:reu_empty; M65XX_TICK; M65XX_POKE(reu_word.w,o);
					break;
				case 2: // REU <-> C64
					M65XX_WAIT; if (reu_addr<=reu_cap) o=reu_ram[reu_addr],reu_ram[reu_addr]=M65XX_PEEK(reu_word.w); else o=reu_empty; M65XX_TICK; M65XX_POKE(reu_word.w,o);
					break;
				case 3: // REU cmp C64
					M65XX_WAIT; if (reu_addr>reu_cap||reu_ram[reu_addr]!=M65XX_PEEK(reu_word.w)) reu_fail(),reu_size=1; // mismatch raises FAULT: BLOCK VERIFY ERROR
					break;
			}
			if (reu_inc_65xx) ++reu_word.w; // C64 ADDRESS CONTROL
			if (reu_inc_addr) reu_addr=(reu_addr+1)&reu_max; // REU ADDRESS CONTROL
			if (!--reu_size) { reu_last(); break; }
		}
		while (m65xx_t<_t_);
	else
	#endif
	{ BYTE n,z; M65XX_SPLIT_P;
	do
	{
		M65XX_PAGE(M65XX_PC.b.h);
		M65XX_TICK; // not first!
		if (UNLIKELY((M65XX_P&4)<M65XX_INT)) // catch NMI (always) or IRQ when I is false
		{
			#ifdef M65XX_HLT
			if (M65XX_HLT) M65XX_WAIT;
			#endif
			M65XX_BREAK_P; M65XX_DUMBPEEK(M65XX_PC.w); M65XX_OLDPC; // perform two dumb fetches!
			#ifdef DEBUG_HERE
			if (debug_inter) debug_inter=_t_=0,session_signal|=SESSION_SIGNAL_DEBUG; // throw!
			#endif
			goto go_to_interrupt;
		}
		else
		{
			#ifdef M65XX_HLT
			if (M65XX_HLT) M65XX_WAIT;
			#endif
			BYTE q,o=M65XX_PEEK(M65XX_PC.w); ++M65XX_PC.w; switch (o)
			{
				int i; HLII a;
				// opcodes N*8+0 ---------------------------- //
				case 0X00: // BRK #$NN
					#ifdef M65XX_TRAP_0X00
					M65XX_TRAP_0X00;
					#endif
					M65XX_MERGE_P; M65XX_BADPC; ++M65XX_PC.w; // throw away the byte that follows BRK!
					go_to_interrupt: // *!* GOTO!
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.h); --M65XX_S;
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.l); --M65XX_S;
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_P); --M65XX_S; M65XX_P|=4; // set I!
					M65XX_PAGE(0XFF);
					#ifdef M65XX_NMI_ACK
					if (UNLIKELY(M65XX_INT&128)) // M65XX_IRQ here crashes "ALL ROADS LEAD TO UIT.PRG"
					{
						M65XX_NMI_ACK;
						M65XX_PC.b.l=M65XX_SAFEPEEK(0XFFFA);
						M65XX_PC.b.h=M65XX_SAFEPEEK(0XFFFB);
					}
					else
					#endif
					{
						M65XX_IRQ_ACK; // *?*
						M65XX_PC.b.l=M65XX_SAFEPEEK(0XFFFE);
						M65XX_PC.b.h=M65XX_SAFEPEEK(0XFFFF);
					}
					M65XX_WAIT;
					M65XX_TOCK; M65XX_WAIT;
					break;
				case 0X20: // JSR $NNNN
					#ifdef M65XX_TRAP_0X20
					M65XX_TRAP_0X20;
					#endif
					M65XX_FETCH(o);
					M65XX_WAIT; //M65XX_DUMBPULL(M65XX_S);
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.h); --M65XX_S;
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.l); --M65XX_S;
					M65XX_TOCK; M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h); M65XX_PC.b.h=M65XX_PEEK(M65XX_PC.w); M65XX_PC.b.l=o; // don't use M65XX_FETCH here!
					break;
				case 0X40: // RTI
					#ifdef M65XX_TRAP_0X40
					M65XX_TRAP_0X40;
					#endif
					M65XX_BADPC; M65XX_OLDPC; // both dumb reads aim to PC+1!
					M65XX_WAIT; ++M65XX_S; M65XX_P=M65XX_PULL(M65XX_S); M65XX_SPLIT_P;
					M65XX_WAIT; ++M65XX_S; M65XX_PC.b.l=M65XX_PULL(M65XX_S);
					M65XX_TOCK; M65XX_WAIT; ++M65XX_S; M65XX_PC.b.h=M65XX_PULL(M65XX_S);
					#ifdef DEBUG_HERE
					if (M65XX_S>debug_trap_sp)
						{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#endif
					break;
				case 0X60: // RTS
					#ifdef M65XX_TRAP_0X60
					M65XX_TRAP_0X60;
					#endif
					M65XX_BADPC; M65XX_OLDPC; // both dumb reads aim to PC+1!
					M65XX_WAIT; ++M65XX_S; M65XX_PC.b.l=M65XX_PULL(M65XX_S);
					M65XX_WAIT; ++M65XX_S; M65XX_PC.b.h=M65XX_PULL(M65XX_S);
					M65XX_TOCK; M65XX_WAIT; //M65XX_DUMBPULL(M65XX_S);
					++M65XX_PC.w; // JSR $NNNN pushes PC-1
					#ifdef DEBUG_HERE
					if (M65XX_S>debug_trap_sp)
						{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#endif
					break;
				case 0XA0: // LDY #$NN
					M65XX_TOCK; M65XX_FETCH(M65XX_Y); z=n=M65XX_Y;
					break;
				case 0XC0: // CPY #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_CPY(o);
					break;
				case 0XE0: // CPX #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_CPX(o);
					break;
				case 0X08: // PHP
					M65XX_BADPC;
					M65XX_TOCK; M65XX_TICK; M65XX_MERGE_P; M65XX_PUSH(M65XX_S,M65XX_P); --M65XX_S;
					break;
				case 0X28: // PLP
					M65XX_BADPC; M65XX_OLDPC; // both dumb reads aim to PC+1!
					M65XX_TOCK; M65XX_WAIT; ++M65XX_S; M65XX_P=M65XX_PULL(M65XX_S); M65XX_SPLIT_P;
					break;
				case 0X48: // PHA
					M65XX_BADPC;
					M65XX_TOCK; M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_A); --M65XX_S;
					break;
				case 0X68: // PLA
					M65XX_BADPC; M65XX_OLDPC; // both dumb reads aim to PC+1!
					M65XX_TOCK; M65XX_WAIT; ++M65XX_S; z=n=M65XX_A=M65XX_PULL(M65XX_S);
					break;
				case 0X88: // DEY
					z=n=--M65XX_Y; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XA8: // TAY
					z=n=M65XX_Y=M65XX_A; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XC8: // INY
					z=n=++M65XX_Y; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XE8: // INX
					z=n=++M65XX_X; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X10: // BPL $RRRR
					#ifdef M65XX_TRAP_0X10
					M65XX_TRAP_0X10;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					if (n<128) goto go_to_branch;
					break;
				case 0X30: // BMI $RRRR
					#ifdef M65XX_TRAP_0X30
					M65XX_TRAP_0X30;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					if (n>=128) goto go_to_branch;
					break;
				case 0X50: // BVC $RRRR
					#ifdef M65XX_TRAP_0X50 // special case: the C1541 relies on the M65XX OVERFLOW pin!
					M65XX_TRAP_0X50;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					#ifdef M65XX_XVS
					if (!M65XX_XVS)
					#endif
					if (!(M65XX_P&64)) goto go_to_branch;
					break;
				case 0X70: // BVS $RRRR
					#ifdef M65XX_TRAP_0X70
					M65XX_TRAP_0X70;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					if (M65XX_P&64) goto go_to_branch;
					break;
				case 0X90: // BCC $RRRR
					#ifdef M65XX_TRAP_0X90
					M65XX_TRAP_0X90;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					if (!(M65XX_P&1)) //goto go_to_branch;
					{
						go_to_branch: // *!* GOTO!
						M65XX_BADPC; q=M65XX_PC.b.h; M65XX_PC.w+=(signed char)o; if (UNLIKELY(q!=M65XX_PC.b.h))
							{ a.b.l=M65XX_PC.b.l; M65XX_TOCK; M65XX_BADAW; break; }
						break; // no M65XX_TOCK here!
					}
					break;
				case 0XB0: // BCS $RRRR
					#ifdef M65XX_TRAP_0XB0
					M65XX_TRAP_0XB0;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					if (M65XX_P&1) goto go_to_branch;
					break;
				case 0XD0: // BNE $RRRR
					#ifdef M65XX_TRAP_0XD0
					M65XX_TRAP_0XD0;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					if (z) goto go_to_branch;
					break;
				case 0XF0: // BEQ $RRRR
					#ifdef M65XX_TRAP_0XF0
					M65XX_TRAP_0XF0;
					#endif
					M65XX_TOCK; M65XX_FETCH(o);
					if (!z) goto go_to_branch;
					break;
				case 0X18: // CLC
					M65XX_P&=~1; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X38: // SEC
					M65XX_P|=+1; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X58: // CLI
					#ifdef M65XX_TRAP_0X58
					M65XX_TRAP_0X58;
					#endif
					M65XX_TOCK; M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h);
					if ((M65XX_PEEK(M65XX_PC.w)!=0X78)/*&&(M65XX_P&4)*/) M65XX_INT&=~  3; // IRQ delay: "RIMRUNNER" ($85DE) but not "4KRAWALL" ($8230)
					M65XX_P&=~4;
					break;
				case 0X78: // SEI
					#ifdef M65XX_TRAP_0X78
					M65XX_TRAP_0X78;
					#endif
					M65XX_TOCK; M65XX_BADPC; // does any title rely on delaying the next NMI check!?
					M65XX_P|=+4;
					break;
				case 0X98: // TYA
					z=n=M65XX_A=M65XX_Y; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XB8: // CLV
					M65XX_P&=~64; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XD8: // CLD
					M65XX_P&=~8; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XF8: // SED
					M65XX_P|=+8; M65XX_TOCK; M65XX_BADPC;
					break;
				// opcodes N*8+1 ---------------------------- //
				case 0X01: // ORA ($NN,X)
					M65XX_IND_X; goto go_to_ora_peek;
				case 0X09: // ORA #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_ORA(o);
					break;
				case 0X11: // ORA ($NN),Y
					M65XX_IND_Y; goto go_to_ora_peek;
				case 0X19: // ORA $NNNN,Y
					M65XX_ABS_Y; goto go_to_ora_peek;
				case 0X21: // AND ($NN,X)
					M65XX_IND_X; goto go_to_and_peek;
				case 0X29: // AND #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_AND(o);
					break;
				case 0X31: // AND ($NN),Y
					M65XX_IND_Y; goto go_to_and_peek;
				case 0X39: // AND $NNNN,Y
					M65XX_ABS_Y; goto go_to_and_peek;
				case 0X41: // EOR ($NN,X)
					M65XX_IND_X; goto go_to_eor_peek;
				case 0X49: // EOR #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_EOR(o);
					break;
				case 0X51: // EOR ($NN),Y
					M65XX_IND_Y; goto go_to_eor_peek;
				case 0X59: // EOR $NNNN,Y
					M65XX_ABS_Y; goto go_to_eor_peek;
				case 0X61: // ADC ($NN,X)
					M65XX_IND_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X69: // ADC #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_ADC(o);
					break;
				case 0X71: // ADC ($NN),Y
					M65XX_IND_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X79: // ADC $NNNN,Y
					M65XX_ABS_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X81: // STA ($NN,X)
					M65XX_IND_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_A);
					break;
				case 0X91: // STA ($NN),Y
					M65XX_IND_Y_BADAW; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_A);
					break;
				case 0X99: // STA $NNNN,Y
					M65XX_ABS_Y_BADAW; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_A);
					break;
				case 0XA1: // LDA ($NN,X)
					M65XX_IND_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XA9: // LDA #$NN
					M65XX_TOCK; M65XX_FETCH(M65XX_A); z=n=M65XX_A;
					break;
				case 0XB1: // LDA ($NN),Y
					M65XX_IND_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XB9: // LDA $NNNN,Y
					M65XX_ABS_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XC1: // CMP ($NN,X)
					M65XX_IND_X; goto go_to_cmp_peek;
				case 0XC9: // CMP #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_CMP(o);
					break;
				case 0XD1: // CMP ($NN),Y
					M65XX_IND_Y; goto go_to_cmp_peek;
				case 0XD9: // CMP $NNNN,Y
					M65XX_ABS_Y; goto go_to_cmp_peek;
				case 0XE1: // SBC ($NN,X)
					M65XX_IND_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_SBC(M65XX_PEEK(a.w));
					break;
				case 0XE9: // SBC #$NN
				case 0XEB: // SBC #$NN (illegal!)
					M65XX_TOCK; M65XX_FETCH(o);
					M65XX_SBC(o);
					break;
				case 0XF1: // SBC ($NN),Y
					M65XX_IND_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_SBC(M65XX_PEEK(a.w));
					break;
				case 0XF9: // SBC $NNNN,Y
					M65XX_ABS_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_SBC(M65XX_PEEK(a.w));
					break;
				// opcodes N*8+2 ---------------------------- //
				case 0X0A: // ASL A
					M65XX_ASL(M65XX_A,M65XX_A); M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X2A: // ROL A
					M65XX_ROL(M65XX_A,M65XX_A); M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X4A: // LSR A
					M65XX_LSR(M65XX_A,M65XX_A); M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X6A: // ROR A
					M65XX_ROR(M65XX_A,M65XX_A); M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X8A: // TXA
					z=n=M65XX_A=M65XX_X; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X9A: // TXS
					M65XX_S=M65XX_X; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XA2: // LDX #$NN
					M65XX_TOCK; M65XX_FETCH(M65XX_X); z=n=M65XX_X;
					break;
				case 0XAA: // TAX
					z=n=M65XX_X=M65XX_A; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XBA: // TSX
					z=n=M65XX_X=M65XX_S; M65XX_TOCK; M65XX_BADPC;
					break;
				case 0XCA: // DEX
					z=n=--M65XX_X; M65XX_TOCK; M65XX_BADPC;
					break;
				// opcodes N*8+3 ---------------------------- //
				case 0X03: // SLO ($NN,X)
					M65XX_IND_X; goto go_to_slo_poke;
				case 0X0B: // ANC #$NN
				case 0X2B: // ANC #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					z=n=M65XX_A&=o; M65XX_P=(M65XX_P&~1)+(z>>7);
					break;
				case 0X13: // SLO ($NN),Y
					M65XX_IND_Y_BADAW; goto go_to_slo_poke;
				case 0X1B: // SLO $NNNN,Y
					M65XX_ABS_Y_BADAW; goto go_to_slo_poke;
				case 0X23: // RLA ($NN,X)
					M65XX_IND_X; goto go_to_rla_poke;
				case 0X33: // RLA ($NN),Y
					M65XX_IND_Y_BADAW; goto go_to_rla_poke;
				case 0X3B: // RLA $NNNN,Y
					M65XX_ABS_Y_BADAW; goto go_to_rla_poke;
				case 0X4B: // ASR #$NN // a.k.a. ALR
					M65XX_TOCK; M65XX_FETCH(o);
					o&=M65XX_A; M65XX_P=(M65XX_P&~1)+(o&1); z=n=M65XX_A=o>>1;
					break;
				case 0X6B: // ARR #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					o&=M65XX_A; z=n=M65XX_A=(o>>1)+(M65XX_P<<7);
					if (UNLIKELY(M65XX_P&8))
					{
						M65XX_P=(M65XX_P&~65)+((o^z)&64);
						if ((o& 15)+(o& 1)> 5)
							M65XX_A=(M65XX_A&240)+((M65XX_A+6)&15);
						if ((o&240)+(o&16)>80)
							++M65XX_P,M65XX_A+=96;
					}
					else
						M65XX_P=(M65XX_P&~65)+(o>>7)+((o^(o>>1))&64);
					break;
				case 0X8B: // ANE #$NN
					// "...Mastertronic variant of the 'burner' tape loader ([...] 'Spectipede', 'BMX Racer') [...]
					// the commonly assumed value $EE for the 'magic constant' will not work, but $EF does..." (Groepaz/Solution)
					M65XX_TOCK; M65XX_FETCH(o);
					z=n=M65XX_A=(M65XX_A|(M65XX_XEF))&M65XX_X&o;
					break;
				case 0X43: // SRE ($NN,X)
					M65XX_IND_X; goto go_to_sre_poke;
				case 0X53: // SRE ($NN),Y
					M65XX_IND_Y_BADAW; goto go_to_sre_poke;
				case 0X5B: // SRE $NNNN,Y
					M65XX_ABS_Y_BADAW; goto go_to_sre_poke;
				case 0X63: // RRA ($NN,X)
					M65XX_IND_X; goto go_to_rra_poke;
				case 0X73: // RRA ($NN),Y
					M65XX_IND_Y_BADAW; goto go_to_rra_poke;
				case 0X7B: // RRA $NNNN,Y
					M65XX_ABS_Y_BADAW; goto go_to_rra_poke;
				case 0X83: // SAX ($NN,X)
					M65XX_IND_X; goto go_to_sax_poke;
				case 0X93: // SHA ($NN),Y
					M65XX_IND_Y_SHZAW; goto go_to_sha_poke;
				case 0X9B: // SHS $NNNN,Y
					M65XX_ABS_Y_SHZAW;
					M65XX_TOCK; M65XX_TICK; M65XX_SHZ(q,M65XX_S=M65XX_SAX); // TICK must happen before SHZ!
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA3: // LAX ($NN,X)
					M65XX_IND_X; goto go_to_lax_peek;
				case 0XAB: // LXA #$NN
					// "...a very popular C-64 game, 'Wizball', actually uses the LAX #imm opcode [...]
					// $EE really seems to be the one and only value to make it work correctly..." (Groepaz/Solution)
					M65XX_TOCK; M65XX_FETCH(o);
					z=n=M65XX_A=M65XX_X=(M65XX_A|(M65XX_XEE))&o;
					break;
				case 0XB3: // LAX ($NN),Y
					M65XX_IND_Y; goto go_to_lax_peek;
				case 0XBB: // LAS $NNNN,Y
					M65XX_ABS_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_A=M65XX_X=M65XX_S&=M65XX_PEEK(a.w);
					break;
				case 0XCB: // SBX #$NN
					M65XX_TOCK; M65XX_FETCH(o);
					z=n=M65XX_X=i=M65XX_SAX-o;
					M65XX_CARRY(i>=0);
					break;
				case 0XC3: // DCP ($NN,X)
					M65XX_IND_X; goto go_to_dcp_poke;
				case 0XD3: // DCP ($NN),Y
					M65XX_IND_Y_BADAW; goto go_to_dcp_poke;
				case 0XDB: // DCP $NNNN,Y
					M65XX_ABS_Y_BADAW; goto go_to_dcp_poke;
				case 0XE3: // ISB ($NN,X)
					M65XX_IND_X; goto go_to_isb_poke;
				case 0XF3: // ISB ($NN),Y
					M65XX_IND_Y_BADAW; goto go_to_isb_poke;
				case 0XFB: // ISB $NNNN,Y
					M65XX_ABS_Y_BADAW; goto go_to_isb_poke;
				// opcodes N*8+4 ---------------------------- //
				case 0X24: // BIT $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; M65XX_BIT(M65XX_PEEKZERO(a.w));
					break;
				case 0X2C: // BIT $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_BIT(M65XX_PEEK(a.w));
					break;
				case 0X4C: // JMP $NNNN
					#ifdef M65XX_TRAP_0X4C
					M65XX_TRAP_0X4C;
					#endif
					M65XX_FETCH(o);
					M65XX_TOCK; M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h); M65XX_PC.b.h=M65XX_PEEK(M65XX_PC.w);
					M65XX_PC.b.l=o;
					break;
				case 0X6C: // JMP ($NNNN)
					#ifdef M65XX_TRAP_0X6C
					M65XX_TRAP_0X6C;
					#endif
					M65XX_ABS; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_PC.b.l=M65XX_PEEK(a.w); ++a.b.l; // not `++a.w`!
					M65XX_TOCK; M65XX_WAIT; M65XX_PC.b.h=M65XX_PEEK(a.w);
					break;
				case 0X84: // STY $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_Y);
					break;
				case 0X8C: // STY $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_Y);
					#ifdef M65XX_REU
					if (reu_size>0) _t_=0; // throw!
					#endif
					break;
				case 0X94: // STY $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TOCK; M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_Y);
					break;
				case 0X9C: // SHY $NNNN,X
					M65XX_ABS_X_SHZAW;
					M65XX_TOCK; M65XX_TICK; M65XX_SHZ(q,M65XX_Y); // TICK must happen before SHZ!
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA4: // LDY $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; z=n=M65XX_Y=M65XX_PEEKZERO(a.w);
					break;
				case 0XAC: // LDY $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_Y=M65XX_PEEK(a.w);
					break;
				case 0XB4: // LDY $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TOCK; M65XX_WAIT; z=n=M65XX_Y=M65XX_PEEKZERO(a.w);
					break;
				case 0XBC: // LDY $NNNN,X
					M65XX_ABS_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_Y=M65XX_PEEK(a.w);
					break;
				case 0XC4: // CPY $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; M65XX_CPY(M65XX_PEEKZERO(a.w));
					break;
				case 0XCC: // CPY $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_CPY(M65XX_PEEK(a.w));
					break;
				case 0XE4: // CPX $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; M65XX_CPX(M65XX_PEEKZERO(a.w));
					break;
				case 0XEC: // CPX $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_CPX(M65XX_PEEK(a.w));
					break;
				// opcodes N*8+5 ---------------------------- //
				case 0X05: // ORA $NN
					M65XX_ZPG; go_to_ora_peekzero: // *!* GOTO!
					M65XX_TOCK; M65XX_WAIT; M65XX_ORA(M65XX_PEEKZERO(a.w));
					break;
				case 0X0D: // ORA $NNNN
					M65XX_ABS; go_to_ora_peek: // *!* GOTO!
					M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_ORA(M65XX_PEEK(a.w));
					break;
				case 0X15: // ORA $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_ora_peekzero;
				case 0X1D: // ORA $NNNN,X
					M65XX_ABS_X; goto go_to_ora_peek;
				case 0X25: // AND $NN
					M65XX_ZPG; go_to_and_peekzero: // *!* GOTO!
					M65XX_TOCK; M65XX_WAIT; M65XX_AND(M65XX_PEEKZERO(a.w));
					break;
				case 0X2D: // AND $NNNN
					M65XX_ABS; go_to_and_peek: // *!* GOTO!
					M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_AND(M65XX_PEEK(a.w));
					break;
				case 0X35: // AND $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_and_peekzero;
				case 0X3D: // AND $NNNN,X
					M65XX_ABS_X; goto go_to_and_peek;
				case 0X45: // EOR $NN
					M65XX_ZPG; go_to_eor_peekzero: // *!* GOTO!
					M65XX_TOCK; M65XX_WAIT; M65XX_EOR(M65XX_PEEKZERO(a.w));
					break;
				case 0X4D: // EOR $NNNN
					M65XX_ABS; go_to_eor_peek: // *!* GOTO!
					M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_EOR(M65XX_PEEK(a.w));
					break;
				case 0X55: // EOR $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_eor_peekzero;
				case 0X5D: // EOR $NNNN,X
					M65XX_ABS_X; goto go_to_eor_peek;
				case 0X65: // ADC $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; M65XX_ADC(M65XX_PEEKZERO(a.w));
					break;
				case 0X6D: // ADC $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X75: // ADC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TOCK; M65XX_WAIT; M65XX_ADC(M65XX_PEEKZERO(a.w));
					break;
				case 0X7D: // ADC $NNNN,X
					M65XX_ABS_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X85: // STA $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_A);
					break;
				case 0X8D: // STA $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_A);
					#ifdef M65XX_REU
					if (reu_size>0) _t_=0; // throw!
					#endif
					break;
				case 0X95: // STA $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TOCK; M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_A);
					break;
				case 0X9D: // STA $NNNN,X
					M65XX_ABS_X_BADAW; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_A);
					break;
				case 0XA5: // LDA $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; z=n=M65XX_A=M65XX_PEEKZERO(a.w);
					break;
				case 0XAD: // LDA $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XB5: // LDA $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TOCK; M65XX_WAIT; z=n=M65XX_A=M65XX_PEEKZERO(a.w);
					break;
				case 0XBD: // LDA $NNNN,X
					M65XX_ABS_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XC5: // CMP $NN
					M65XX_ZPG; go_to_cmp_peekzero: // *!* GOTO!
					M65XX_TOCK; M65XX_WAIT; M65XX_CMP(M65XX_PEEKZERO(a.w));
					break;
				case 0XCD: // CMP $NNNN
					M65XX_ABS; go_to_cmp_peek: // *!* GOTO!
					M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_CMP(M65XX_PEEK(a.w));
					break;
				case 0XD5: // CMP $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_cmp_peekzero;
				case 0XDD: // CMP $NNNN,X
					M65XX_ABS_X; goto go_to_cmp_peek;
				case 0XE5: // SBC $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; M65XX_SBC(M65XX_PEEKZERO(a.w));
					break;
				case 0XED: // SBC $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_SBC(M65XX_PEEK(a.w));
					break;
				case 0XF5: // SBC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TOCK; M65XX_WAIT; M65XX_SBC(M65XX_PEEKZERO(a.w));
					break;
				case 0XFD: // SBC $NNNN,X
					M65XX_ABS_X; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					M65XX_SBC(M65XX_PEEK(a.w));
					break;
				// opcodes N*8+6 ---------------------------- //
				case 0X16: // ASL $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_asl_pokezero;
				case 0X06: // ASL $NN
					M65XX_ZPG; go_to_asl_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ASL(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X0E: // ASL $NNNN
					M65XX_ABS; go_to_asl_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ASL(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X1E: // ASL $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_asl_poke;
				case 0X36: // ROL $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_rol_pokezero;
				case 0X26: // ROL $NN
					M65XX_ZPG; go_to_rol_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ROL(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X2E: // ROL $NNNN
					M65XX_ABS; go_to_rol_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ROL(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X3E: // ROL $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_rol_poke;
				case 0X56: // LSR $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_lsr_pokezero;
				case 0X46: // LSR $NN
					M65XX_ZPG; go_to_lsr_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_LSR(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X4E: // LSR $NNNN
					M65XX_ABS; go_to_lsr_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_LSR(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X5E: // LSR $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_lsr_poke;
				case 0X76: // ROR $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_ror_pokezero;
				case 0X66: // ROR $NN
					M65XX_ZPG; go_to_ror_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ROR(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X6E: // ROR $NNNN
					M65XX_ABS; go_to_ror_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ROR(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X7E: // ROR $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_ror_poke;
				case 0X86: // STX $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_X);
					break;
				case 0X8E: // STX $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_X);
					#ifdef M65XX_REU
					if (reu_size>0) _t_=0; // throw!
					#endif
					break;
				case 0X96: // STX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC;
					M65XX_TOCK; M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_X);
					break;
				case 0X9E: // SHX $NNNN,Y
					M65XX_ABS_Y_SHZAW;
					M65XX_TOCK; M65XX_TICK; M65XX_SHZ(q,M65XX_X); // TICK must happen before SHZ! used in "FELLAS-EXT.PRG"
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA6: // LDX $NN
					M65XX_ZPG;
					M65XX_TOCK; M65XX_WAIT; z=n=M65XX_X=M65XX_PEEKZERO(a.w);
					break;
				case 0XAE: // LDX $NNNN
					M65XX_ABS; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XB6: // LDX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC;
					M65XX_TOCK; M65XX_WAIT; z=n=M65XX_X=M65XX_PEEKZERO(a.w);
					break;
				case 0XBE: // LDX $NNNN,Y
					M65XX_ABS_Y; M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XC6: // DEC $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_DEC(o);
					M65XX_POKEZERO(a.w,o);
					break;
				case 0XCE: // DEC $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_DEC(o);
					M65XX_POKE(a.w,o);
					break;
				case 0XD6: // DEC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_DEC(o);
					M65XX_POKEZERO(a.w,o);
					break;
				case 0XDE: // DEC $NNNN,X
					M65XX_ABS_X_BADAW; M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_DEC(o);
					M65XX_POKE(a.w,o);
					break;
				case 0XE6: // INC $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_INC(o);
					M65XX_POKEZERO(a.w,o);
					break;
				case 0XEE: // INC $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_INC(o);
					M65XX_POKE(a.w,o);
					break;
				case 0XF6: // INC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_INC(o);
					M65XX_POKEZERO(a.w,o);
					break;
				case 0XFE: // INC $NNNN,X
					M65XX_ABS_X_BADAW; M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_INC(o);
					M65XX_POKE(a.w,o);
					break;
				// opcodes N*8+7 ---------------------------- //
				case 0X17: // SLO $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_slo_pokezero;
				case 0X07: // SLO $NN
					M65XX_ZPG; go_to_slo_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_SLO(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X0F: // SLO $NNNN
					M65XX_ABS; go_to_slo_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_SLO(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X1F: // SLO $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_slo_poke;
				case 0X37: // RLA $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_rla_pokezero;
				case 0X27: // RLA $NN
					M65XX_ZPG; go_to_rla_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_RLA(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X2F: // RLA $NNNN
					M65XX_ABS; go_to_rla_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_RLA(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X3F: // RLA $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_rla_poke;
				case 0X57: // SRE $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_sre_pokezero;
				case 0X47: // SRE $NN
					M65XX_ZPG; go_to_sre_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_SRE(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X4F: // SRE $NNNN
					M65XX_ABS; go_to_sre_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_SRE(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X5F: // SRE $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_sre_poke;
				case 0X77: // RRA $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_rra_pokezero;
				case 0X67: // RRA $NN
					M65XX_ZPG; go_to_rra_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_RRA(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0X6F: // RRA $NNNN
					M65XX_ABS; go_to_rra_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_RRA(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0X7F: // RRA $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_rra_poke;
				case 0X97: // SAX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC; goto go_to_sax_pokezero;
				case 0X87: // SAX $NN
					M65XX_ZPG; go_to_sax_pokezero: // *!* GOTO!
					M65XX_TOCK; M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_SAX);
					break;
				case 0X8F: // SAX $NNNN
					M65XX_ABS; go_to_sax_poke: // *!* GOTO!
					M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_TICK;
					M65XX_POKE(a.w,M65XX_SAX);
					break;
				case 0X9F: // SHA $NNNN,Y
					M65XX_ABS_Y_SHZAW; go_to_sha_poke: // *!* GOTO!
					M65XX_TOCK; M65XX_TICK; M65XX_SHZ(q,M65XX_SAX); // TICK must happen before SHZ!
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA7: // LAX $NN
					M65XX_ZPG; go_to_lax_peekzero: // *!* GOTO!
					M65XX_TOCK; M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_PEEKZERO(a.w);
					break;
				case 0XAF: // LAX $NNNN
					M65XX_ABS; go_to_lax_peek: // *!* GOTO!
					M65XX_TOCK; M65XX_PAGE(a.b.h); M65XX_WAIT;
					z=n=M65XX_A=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XB7: // LAX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC; goto go_to_lax_peekzero;
				case 0XBF: // LAX $NNNN,Y
					M65XX_ABS_Y; goto go_to_lax_peek;
				case 0XD7: // DCP $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_dcp_pokezero;
				case 0XC7: // DCP $NN
					M65XX_ZPG; go_to_dcp_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_DCP(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0XCF: // DCP $NNNN
					M65XX_ABS; go_to_dcp_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_DCP(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0XDF: // DCP $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_dcp_poke;
				case 0XF7: // ISB $NN,X
					M65XX_ZPG_X_BADPC; goto go_to_isb_pokezero;
				case 0XE7: // ISB $NN
					M65XX_ZPG; go_to_isb_pokezero: // *!* GOTO!
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ISB(q,o);
					M65XX_POKEZERO(a.w,q);
					break;
				case 0XEF: // ISB $NNNN
					M65XX_ABS; go_to_isb_poke: // *!* GOTO!
					M65XX_PAGE(a.b.h); M65XX_WAIT;
					o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_TOCK; M65XX_TICK; M65XX_ISB(q,o);
					M65XX_POKE(a.w,q);
					break;
				case 0XFF: // ISB $NNNN,X
					M65XX_ABS_X_BADAW; goto go_to_isb_poke;
				// NOP and JAM opcodes ---------------------- //
				case 0X80: case 0X82: case 0X89: case 0XC2: case 0XE2: // NOP #$NN
					M65XX_TOCK; M65XX_BADPC; ++M65XX_PC.w;
					break;
				case 0X04: case 0X44: case 0X64: // NOP $NN
					M65XX_BADPC; ++M65XX_PC.w;
					M65XX_TOCK; M65XX_BADPC;
					break;
				case 0X14: case 0X34: case 0X54: case 0X74: case 0XD4: case 0XF4: // NOP $NN,X
					M65XX_BADPC; ++M65XX_PC.w;
					M65XX_BADPC; M65XX_TOCK; M65XX_OLDPC; // both dumb reads aim to PC+1!
					break;
				case 0X0C: // NOP $NNNN
					M65XX_ABS; M65XX_DUMBPAGE(a.b.h);
					M65XX_TOCK; M65XX_WAIT; M65XX_DUMBPEEK(a.w);
					break;
				case 0X1C: case 0X3C: case 0X5C: case 0X7C: case 0XDC: case 0XFC: // NOP $NNNN,X
					M65XX_ABS_X; M65XX_DUMBPAGE(a.b.h);
					M65XX_TOCK; M65XX_WAIT; M65XX_DUMBPEEK(a.w);
					break;
				case 0XFA: // $FA BREAKPOINT
					#ifdef DEBUG_HERE
					if (debug_break) { _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#endif
				case 0X1A: case 0X3A: case 0X5A: case 0X7A: case 0XDA: // NOP (illegal!)
				case 0XEA: // NOP (official)
					M65XX_TOCK; M65XX_BADPC;
					break;
				//default: // ILLEGAL CODE!
				case 0X02: case 0X12: case 0X22: case 0X32: case 0X42: case 0X52: // JAM (1/2)
				case 0X62: case 0X72: case 0X92: case 0XB2: case 0XD2: case 0XF2: // JAM (2/2)
					#ifdef DEBUG_HERE
					{ --M65XX_PC.w; _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#else
					M65XX_WAIT; M65XX_WAIT; M65XX_CRASH(); //if (m65xx_t<_t_) m65xx_t=_t_; // waste all the ticks!
					#endif
			}
		}
		#ifdef DEBUG_HERE
		if (UNLIKELY(debug_point[M65XX_PC.w]))
		{
			BYTE p=debug_point[M65XX_PC.w]; if (p&(128+16)) // volatile/user breakpoint?
				{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
			#ifdef M65XX_MAGICK
			if (p&64) // virtual magick?
				M65XX_MAGICK();
			#endif
			if (p&=15) // log byte?
			{
				switch (p)
				{
					// see `debug_list`
					case  1: p=M65XX_P; break;
					case  2: p=M65XX_A; break;
					case  3: p=M65XX_X; break;
					case  4: p=M65XX_Y; break;
					default: p=M65XX_S; break;
				}
				if (debug_logbyte(p)) debug_point[M65XX_PC.w]&=~15; // remove breakpoint on failure!
			}
		}
		#endif
	}
	while (m65xx_t<_t_);
	M65XX_MERGE_P; }
	M65XX_SYNC(m65xx_t);
}

// M65XX debugger --------------------------------------------------- //

#ifdef DEBUG_HERE

char *debug_list(void) { return " PAXYS"; }
void debug_jump(WORD w) { M65XX_PC.w=w; }
WORD debug_this(void) { return M65XX_PC.w; }
WORD debug_that(void) { return 256+M65XX_S; }
BYTE debug_peek(WORD w) { return PEEK(w); }
void debug_poke(WORD w,BYTE b) { POKE(w)=b; }
void debug_step(void) { M65XX_MAIN(0); debug_reset(); } // run one operation
void debug_fall(void) { debug_trap_sp=M65XX_S; session_signal&=~SESSION_SIGNAL_DEBUG; } // set a RETURN trap
void debug_drop(WORD w) { debug_point[debug_trap_pc=w]|=128,session_signal&=~SESSION_SIGNAL_DEBUG; }
void debug_leap(void) // run UNTIL meeting the next operation
{
	BYTE q=PEEK(M65XX_PC.w);
	if (q==0X20) q=3; // JSR $NNNN
	else q=q?0:2; // BRK #$NN
	if (q)
		debug_drop(M65XX_PC.w+q);
	else
		debug_step(); // just run one operation
}

void debug_regs(char *t,int i) // prints register information #`i` onto the buffer `t`
{
	if (debug_panel1_w==i) // validate `debug_panel1_w` and `debug_panel1_x` (see below)
	{
		if (debug_panel1_w<1) debug_panel1_w=1;
		if (debug_panel1_w>6) debug_panel1_w=6;
		int j=debug_panel1_w<2?0:2;
		if (debug_panel1_x<j) debug_panel1_x=j;
		if (debug_panel1_x>3) debug_panel1_x=3;
	}
	switch (i)
	{
		case  0:
			*t++=(M65XX_P&128)?'N':'-'; *t++=(M65XX_P& 64)?'V':'-'; *t++='-'; *t++=(M65XX_P& 16)?'B':'-'; *t++=':';
			*t++=(M65XX_P&  8)?'D':'-'; *t++=(M65XX_P&  4)?'I':'-'; *t++=(M65XX_P&  2)?'Z':'-'; *t++=(M65XX_P&  1)?'C':'-'; *t=0; break;
		case  1: sprintf(t,"PC = %04X",M65XX_PC.w); break;
		case  2: sprintf(t,"P  =   %02X",M65XX_P); break;
		case  3: sprintf(t,"A  =   %02X",M65XX_A); break;
		case  4: sprintf(t,"X  =   %02X",M65XX_X); break;
		case  5: sprintf(t,"Y  =   %02X",M65XX_Y); break;
		case  6: sprintf(t,"S  =   %02X",M65XX_S); break;
		case  7: sprintf(t,"NMI%c IRQ%c",(M65XX_IRQ&128)?'*':'-',(M65XX_IRQ&3)?'*':'-'); break;
		default: *t=0;
	}
}
void debug_regz(BYTE k) // sends the nibble `k` to the valid position `debug_panel1_w`:`debug_panel1_x` and updates the later
{
	int m=~(15<<(12-debug_panel1_x*4)),n=k<<(12-debug_panel1_x*4); switch (debug_panel1_w)
	{
		case 1: M65XX_PC.w=(M65XX_PC.w&m)+n; break;
		case 2: M65XX_P=(M65XX_P&m)+n; break;
		case 3: M65XX_A=(M65XX_A&m)+n; break;
		case 4: M65XX_X=(M65XX_X&m)+n; break;
		case 5: M65XX_Y=(M65XX_Y&m)+n; break;
		case 6: M65XX_S=(M65XX_S&m)+n; break;
	}
	if (++debug_panel1_x>3) debug_panel1_x=0; // debug_regs() will fix this later, if required
}
WORD debug_pull(WORD s) // gets an item from the stack `s`
	{ debug_match=s==256+M65XX_S; return PEEK((WORD)(s+2))*256+PEEK((WORD)(s+1)); }
void debug_push(WORD s,WORD w) // puts an item in the stack `s`
	{ POKE((WORD)(s+2))=w>>8; POKE((WORD)(s+1))=w; }
WORD debug_dasm(char *t,WORD p) // disassembles the code at address `p` onto the buffer `t`; returns the next value of `p`
	{ debug_match=p==M65XX_PC.w; return debug_dasm_any(t,p,debug_peek); }
void debug_clean(void) // reset the machine if we're leaving the debugger and the 65XX is JAMmed!
{
	if (!(session_signal&SESSION_SIGNAL_DEBUG))
		{ BYTE b=PEEK(M65XX_PC.w); if ((b&15)==2&&(b&(128+16))!=128) M65XX_CRASH(); }
}

#endif

// undefine the configuration macros to ease redefinition

#undef M65XX_LOCAL
#undef M65XX_CRASH
#undef M65XX_RESET
#undef M65XX_START
#undef M65XX_MAIN
#undef M65XX_IRQ
#undef M65XX_INT
#undef M65XX_IRQ_ACK
#undef M65XX_NMI_ACK
#undef M65XX_PC
#undef M65XX_P
#undef M65XX_A
#undef M65XX_X
#undef M65XX_Y
#undef M65XX_S
#undef M65XX_SYNC
#undef M65XX_PAGE
#undef M65XX_SAFEPEEK
#undef M65XX_PEEK
#undef M65XX_POKE
#undef M65XX_PEEKZERO
#undef M65XX_POKEZERO
#undef M65XX_PULL
#undef M65XX_PUSH
#undef M65XX_DUMBPAGE
#undef M65XX_DUMBPEEK
#undef M65XX_DUMBPOKE
#undef M65XX_WAIT
#undef M65XX_WHAT
#undef M65XX_TICK
#undef M65XX_TOCK
#undef M65XX_HLT
#undef M65XX_SHW
#undef M65XX_SHZ
#undef M65XX_XEE
#undef M65XX_XEF
#undef M65XX_TRAP_0X00
#undef M65XX_TRAP_0X10
#undef M65XX_TRAP_0X20
#undef M65XX_TRAP_0X30
#undef M65XX_TRAP_0X40
#undef M65XX_TRAP_0X4C
#undef M65XX_TRAP_0X50
#undef M65XX_TRAP_0X58
#undef M65XX_TRAP_0X60
#undef M65XX_TRAP_0X6C
#undef M65XX_TRAP_0X70
#undef M65XX_TRAP_0X78
#undef M65XX_TRAP_0X90
#undef M65XX_TRAP_0XB0
#undef M65XX_TRAP_0XD0
#undef M65XX_TRAP_0XF0
#undef M65XX_XVS
#undef M65XX_MAGICK
#undef M65XX_REU

// =================================== END OF MOS 6510/6502 EMULATION //
