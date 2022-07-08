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
// Z80W M65XX_PC -- the Program Counter
// BYTE M65XX_I_T -- the IRQ delay, -2..+inf
// BYTE M65XX_N_T -- the NMI delay, -2..+inf
// void M65XX_IRQ_ACK -- the IRQ acknowledger
// void M65XX_NMI_ACK -- the NMI acknowledger
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

// The old macros M65XX_DUMBPEEKZERO, M65XX_DUMBPOKEZERO,
// M65XX_DUMBPULL and M65XX_DUMBPUSH have been dummied out.

// Notice that M65XX_PEEK and M65XX_POKE must handle the special cases
// $0000 and $0001 when they're emulating a 6510 instead of a 6502.

// Notice also that the difference between real and "dumb" PEEK/POKE
// is that the later can be simplified to "++m65xx_t" if we're sure
// of the null impact in the emulated system of double I/O actions
// such as "LDX #$10: LDA $DFCD,X" (reading from $DC0D and $DD0D)
// and "LSR $D019" (writing twice on $D019: operand and result).

// BEGINNING OF MOS 6510/6502 EMULATION ============================= //

#ifdef DEBUG_HERE
int debug_trap_sp,debug_trap_pc=0;
void debug_clear(void) // cleans volatile breakpoints
{
	debug_inter=0; debug_trap_sp=1<<8; // cancel interrupt+return traps!
	debug_point[debug_trap_pc]&=127; // cancel volatile breakpoint!
}
void debug_reset(void) // sets the debugger's PC and SP
{
	debug_panel0_w=M65XX_PC.w; debug_panel0_x=0;
	debug_panel3_w=256+M65XX_S; debug_panel3_x=0;
}
#endif

#ifndef _M65XX_H_

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
	{
		m65xx_p_adc[256+i]=65-(m65xx_p_adc[i]=(i>>1)&64);
		m65xx_p_sbc[i]=65-(m65xx_p_sbc[i+256]=(~i>>1)&64);
	}
}

#define m65xx_close() 0

#define M65XX_MERGE_P (M65XX_P=(z?48:50)+(n&128)+(M65XX_P&77)) // 77 = 64+8+4+1 (i.e. the flags V+D+I+C)
#define M65XX_BREAK_P (M65XX_P=(z?32:34)+(n&128)+(M65XX_P&77)) // used by the IRQ/NMI handler to reset B
#define M65XX_SPLIT_P (z=~M65XX_P&2,n=M65XX_P&128) // `z` is the latest zero-relevant value; `n` is the latest sign-relevant value

// the memory access modes! notice that only the immediate mode already feeds you the value, other modes expect the operation to do the PEEKs and POKEs

// this happens in operations that waste additional ticks from "dumb" reads from PC
#define M65XX_BADPC do{ M65XX_WAIT; M65XX_DUMBPAGE(M65XX_PC.b.h); M65XX_DUMBPEEK(M65XX_PC.w); }while(0)
// this happens when an operation builds a 16-bit address and the 8-bit carry is true!
#define M65XX_BADXY do{ M65XX_WAIT; M65XX_DUMBPAGE(q); i=q*256+a.b.l; M65XX_DUMBPEEK(i); }while(0)
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
#define M65XX_IND_Y do{ M65XX_ZPG; M65XX_WAIT; q=M65XX_PEEKZERO(a.b.l); ++a.b.l; M65XX_WAIT; a.b.h=M65XX_PEEKZERO(a.b.l); a.b.l=q; q=a.b.h; a.w+=M65XX_Y; if (q!=a.b.h) { M65XX_BADXY; } }while(0)
#define M65XX_IND_Y_BADXY do{ M65XX_ZPG; M65XX_WAIT; q=M65XX_PEEKZERO(a.b.l); ++a.b.l; M65XX_WAIT; a.b.h=M65XX_PEEKZERO(a.b.l); a.b.l=q; q=a.b.h; a.w+=M65XX_Y; M65XX_BADXY; }while(0)
// $NNNN absolute; 2 T
#define M65XX_ABS do{ M65XX_FETCH(a.b.l); M65XX_FETCH(a.b.h); }while(0)
// $NNNN,X absolute, X-indexed; 2/3 T
#define M65XX_ABS_X do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_X; if (q!=a.b.h) { M65XX_BADXY; } }while(0)
#define M65XX_ABS_X_BADXY do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_X; M65XX_BADXY; }while(0)
// $NNNN,Y absolute, Y-indexed; 2/3 T
#define M65XX_ABS_Y do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_Y; if (q!=a.b.h) { M65XX_BADXY; } }while(0)
#define M65XX_ABS_Y_BADXY do{ M65XX_ABS; q=a.b.h; a.w+=M65XX_Y; M65XX_BADXY; }while(0)

// the operation macros!

#define M65XX_CARRY(r) ((r)?(M65XX_P|=1):(M65XX_P&=~1))
#define M65XX_BRANCH(r) do{ M65XX_FETCH(o); if (r) { M65XX_BADPC; q=M65XX_PC.b.h; M65XX_PC.w+=(signed char)o; if (q!=M65XX_PC.b.h) { a.b.l=M65XX_PC.b.l; M65XX_BADXY; } else M65XX_CRUNCH; } }while(0)
#define M65XX_SAX (M65XX_A&M65XX_X) // a.k.a. AXS; it appears in multiple operations
#define M65XX_DEC(r) (z=n=--r)
#define M65XX_INC(r) (z=n=++r)

// the following operations check the input `r` and update the accumulator and the flags

#define M65XX_ORA(r) (z=n=M65XX_A|=r)
#define M65XX_AND(r) (z=n=M65XX_A&=r)
#define M65XX_EOR(r) (z=n=M65XX_A^=r)

// the following operations store the input `r` in `o` and update the accumulator and/or the flags

#define M65XX_ADC(r) ((o=r),(M65XX_P&8)?\
	((i=(M65XX_P&1)),(z=M65XX_A+o+i),(n=(M65XX_A&15)+(o&15)+i),(n>9&&(n+=6)),\
	(n=i=(M65XX_A&240)+(o&240)+(n>31?n-16:n)),(M65XX_P=(M65XX_P&~64)+(m65xx_p_adc[i^M65XX_A^o]&64)),(i>159&&(i+=96)),(M65XX_CARRY(i>255)),(M65XX_A=i)):\
	((i=M65XX_A+o+(M65XX_P&1)),(M65XX_P=(M65XX_P&~65)+m65xx_p_adc[i^M65XX_A^o]),z=n=M65XX_A=i))
#define M65XX_SBC(r) ((o=r),(n=~M65XX_P&1),(z=i=M65XX_A-o-n),(M65XX_P=(M65XX_P&~65)+m65xx_p_sbc[(i^M65XX_A^o)&511]),(M65XX_P&8)?\
	((n=(M65XX_A&15)-(o&15)-n),(i=(M65XX_A&240)-(o&240)),((n&16)&&(n-=6,i-=16)),((i<0)&&(i-=96)),(M65XX_A=i+(n&15)),(n=z)):\
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

#ifdef M65XX_SHW
	#define M65XX_SHZ(t,r) do{ i=(q+1)&(o=r); if (M65XX_SHW) o=i; if (q!=a.b.h) a.b.h=i; t=o; }while(0)
#else // simpler version if these quirks don't matter
	#define M65XX_SHZ(t,r) (t=r)
#endif

#ifdef M65XX_NMI_ACK
	#define M65XX_CRUNCH --M65XX_I_T,--M65XX_N_T
#else // simpler version without NMI handling
	#define M65XX_CRUNCH --M65XX_I_T
#endif


// M65XX interpreter ------------------------------------------------ //

void M65XX_RESET(void) // resets the M65XX chip; notice that the start address is defined by the external hardware
	{ M65XX_PC.w=M65XX_START; M65XX_A=M65XX_X=M65XX_Y=0; M65XX_S=0XFF; M65XX_P=32+16+4; } // empty stack, ignore IRQs

void M65XX_MAIN(int _t_) // runs the M65XX chip for at least `_t_` clock ticks; notice that _t_<1 runs exactly one operation
{
	int m65xx_t=0; BYTE n,z; M65XX_SPLIT_P; M65XX_LOCAL; do
	{
		M65XX_TICK;
		#ifdef M65XX_NMI_ACK
		if (M65XX_N_T>=0||(M65XX_I_T>=0&&!(M65XX_P&4))) // catch NMI (always) or IRQ when I is false
		#else
		if (M65XX_I_T>=0&&!(M65XX_P&4)) // catch IRQ when I is false
		#endif
		{
			if (!M65XX_RDY) M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h); M65XX_DUMBPEEK(M65XX_PC.w); // 1st dumb read...
			M65XX_BADPC; // ...2nd dumb read!
			M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.h); --M65XX_S;
			M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.l); --M65XX_S;
			M65XX_TICK; M65XX_BREAK_P; M65XX_PUSH(M65XX_S,M65XX_P); --M65XX_S; M65XX_P|=4; // set I!
			M65XX_WAIT; M65XX_PAGE(0XFF);
			#ifdef M65XX_NMI_ACK
			if (M65XX_N_T>=0)
			{
				M65XX_NMI_ACK;
				M65XX_PC.b.l=M65XX_PEEK(0XFFFA);
				M65XX_WAIT; M65XX_PC.b.h=M65XX_PEEK(0XFFFB);
			}
			else
			#endif
			{
				M65XX_IRQ_ACK;
				M65XX_PC.b.l=M65XX_PEEK(0XFFFE);
				M65XX_WAIT; M65XX_PC.b.h=M65XX_PEEK(0XFFFF);
			}
			if (debug_inter) debug_inter=_t_=0,session_signal|=SESSION_SIGNAL_DEBUG; // throw!
		}
		else
		{
			int i; Z80W a; BYTE o,q;
			if (!M65XX_RDY) M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h);
			o=M65XX_PEEK(M65XX_PC.w); ++M65XX_PC.w; switch (o)
			{
				// opcodes N*8+0 ---------------------------- //
				case 0X00: // BRK #$NN
					#ifdef M65XX_TRAP_0X00
					M65XX_TRAP_0X00;
					#endif
					M65XX_BADPC; ++M65XX_PC.w; // throw away the byte that follows BRK!
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.h); --M65XX_S;
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.l); --M65XX_S;
					M65XX_TICK; M65XX_MERGE_P; M65XX_PUSH(M65XX_S,M65XX_P); --M65XX_S;
					M65XX_WAIT; M65XX_PAGE(0XFF); M65XX_PC.b.l=M65XX_PEEK(0XFFFE);
					M65XX_WAIT; M65XX_PC.b.h=M65XX_PEEK(0XFFFF); M65XX_P|=4; // set I!
					break;
				case 0X08: // PHP
					M65XX_BADPC;
					M65XX_TICK; M65XX_MERGE_P; M65XX_PUSH(M65XX_S,M65XX_P); --M65XX_S;
					break;
				case 0X10: // BPL $RRRR
					#ifdef M65XX_TRAP_0X10
					M65XX_TRAP_0X10;
					#endif
					M65XX_BRANCH(n<128);
					break;
				case 0X18: // CLC
					M65XX_BADPC; M65XX_P&=~1;
					break;
				case 0X20: // JSR $NNNN
					#ifdef M65XX_TRAP_0X20
					M65XX_TRAP_0X20;
					#endif
					M65XX_FETCH(o);
					M65XX_WAIT; //M65XX_DUMBPULL(M65XX_S);
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.h); --M65XX_S;
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_PC.b.l); --M65XX_S;
					M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h); M65XX_PC.b.h=M65XX_PEEK(M65XX_PC.w); M65XX_PC.b.l=o; // don't use M65XX_FETCH here!
					break;
				case 0X28: // PLP
					M65XX_BADPC; M65XX_BADPC; // both dumb reads aim to PC+1!
					M65XX_WAIT; ++M65XX_S; M65XX_P=M65XX_PULL(M65XX_S); M65XX_SPLIT_P;
					break;
				case 0X30: // BMI $RRRR
					#ifdef M65XX_TRAP_0X30
					M65XX_TRAP_0X30;
					#endif
					M65XX_BRANCH(n>=128);
					break;
				case 0X38: // SEC
					M65XX_BADPC; M65XX_P|=1;
					break;
				case 0X40: // RTI
					#ifdef M65XX_TRAP_0X40
					M65XX_TRAP_0X40;
					#endif
					M65XX_BADPC; M65XX_BADPC; // both dumb reads aim to PC+1!
					M65XX_WAIT; ++M65XX_S; M65XX_P=M65XX_PULL(M65XX_S); M65XX_SPLIT_P;
					M65XX_WAIT; ++M65XX_S; M65XX_PC.b.l=M65XX_PULL(M65XX_S);
					M65XX_WAIT; ++M65XX_S; M65XX_PC.b.h=M65XX_PULL(M65XX_S);
					#ifdef DEBUG_HERE
					if (M65XX_S>debug_trap_sp)
						{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#endif
					break;
				case 0X48: // PHA
					M65XX_BADPC;
					M65XX_TICK; M65XX_PUSH(M65XX_S,M65XX_A); --M65XX_S;
					break;
				case 0X50: // BVC $RRRR
					#ifdef M65XX_TRAP_0X50
					M65XX_TRAP_0X50;
					#endif
					M65XX_BRANCH(!(M65XX_P&64));
					break;
				case 0X58: // CLI
					#ifdef M65XX_TRAP_0X58
					M65XX_TRAP_0X58;
					#endif
					M65XX_WAIT; M65XX_PAGE(M65XX_PC.b.h); o=M65XX_PEEK(M65XX_PC.w);
					if ((M65XX_P&4)) if (M65XX_I_T>=-2) if (o!=0X78) M65XX_I_T=-2; // delay next IRQ check: "RIMRUNNER" ($85DE) but not "4KRAWALL" ($8230)!
					M65XX_P&=~4;
					break;
				case 0X60: // RTS
					#ifdef M65XX_TRAP_0X60
					M65XX_TRAP_0X60;
					#endif
					M65XX_BADPC; M65XX_BADPC; // both dumb reads aim to PC+1!
					M65XX_WAIT; ++M65XX_S; M65XX_PC.b.l=M65XX_PULL(M65XX_S);
					M65XX_WAIT; ++M65XX_S; M65XX_PC.b.h=M65XX_PULL(M65XX_S);
					M65XX_WAIT; //M65XX_DUMBPULL(M65XX_S);
					++M65XX_PC.w; // JSR $NNNN pushes PC-1
					#ifdef DEBUG_HERE
					if (M65XX_S>debug_trap_sp)
						{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#endif
					break;
				case 0X68: // PLA
					M65XX_BADPC; M65XX_BADPC; // both dumb reads aim to PC+1!
					M65XX_WAIT; ++M65XX_S; z=n=M65XX_A=M65XX_PULL(M65XX_S);
					break;
				case 0X70: // BVS $RRRR
					#ifdef M65XX_TRAP_0X70
					M65XX_TRAP_0X70;
					#endif
					M65XX_BRANCH(M65XX_P&64);
					break;
				case 0X78: // SEI
					#ifdef M65XX_TRAP_0X78
					M65XX_TRAP_0X78;
					#endif
					M65XX_WAIT; M65XX_DUMBPAGE(M65XX_PC.b.h); /*o=*/M65XX_DUMBPEEK(M65XX_PC.w);
					//if (!(M65XX_P&4)) if (M65XX_I_T>=-2) M65XX_I_T=-2; // delay next NMI check???
					M65XX_P|=+4;
					break;
				case 0X88: // DEY
					z=n=--M65XX_Y; M65XX_BADPC;
					break;
				case 0X90: // BCC $RRRR
					#ifdef M65XX_TRAP_0X90
					M65XX_TRAP_0X90;
					#endif
					M65XX_BRANCH(!(M65XX_P&1));
					break;
				case 0X98: // TYA
					z=n=M65XX_A=M65XX_Y; M65XX_BADPC;
					break;
				case 0XA0: // LDY #$NN
					M65XX_FETCH(M65XX_Y); z=n=M65XX_Y;
					break;
				case 0XA8: // TAY
					z=n=M65XX_Y=M65XX_A; M65XX_BADPC;
					break;
				case 0XB0: // BCS $RRRR
					#ifdef M65XX_TRAP_0XB0
					M65XX_TRAP_0XB0;
					#endif
					M65XX_BRANCH(M65XX_P&1);
					break;
				case 0XB8: // CLV
					M65XX_BADPC; M65XX_P&=~64;
					break;
				case 0XC0: // CPY #$NN
					M65XX_FETCH(o);
					M65XX_CPY(o);
					break;
				case 0XC8: // INY
					z=n=++M65XX_Y; M65XX_BADPC;
					break;
				case 0XD0: // BNE $RRRR
					#ifdef M65XX_TRAP_0XD0
					M65XX_TRAP_0XD0;
					#endif
					M65XX_BRANCH(z);
					break;
				case 0XD8: // CLD
					M65XX_BADPC; M65XX_P&=~8;
					break;
				case 0XE0: // CPX #$NN
					M65XX_FETCH(o);
					M65XX_CPX(o);
					break;
				case 0XE8: // INX
					z=n=++M65XX_X; M65XX_BADPC;
					break;
				case 0XF0: // BEQ $RRRR
					#ifdef M65XX_TRAP_0XF0
					M65XX_TRAP_0XF0;
					#endif
					M65XX_BRANCH(!z);
					break;
				case 0XF8: // SED
					M65XX_BADPC; M65XX_P|=8;
					break;
				// opcodes N*8+1 ---------------------------- //
				case 0X01: // ORA ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ORA(M65XX_PEEK(a.w));
					break;
				case 0X09: // ORA #$NN
					M65XX_FETCH(o);
					M65XX_ORA(o);
					break;
				case 0X11: // ORA ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ORA(M65XX_PEEK(a.w));
					break;
				case 0X19: // ORA $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ORA(M65XX_PEEK(a.w));
					break;
				case 0X21: // AND ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_AND(M65XX_PEEK(a.w));
					break;
				case 0X29: // AND #$NN
					M65XX_FETCH(o);
					M65XX_AND(o);
					break;
				case 0X31: // AND ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_AND(M65XX_PEEK(a.w));
					break;
				case 0X39: // AND $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_AND(M65XX_PEEK(a.w));
					break;
				case 0X41: // EOR ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_EOR(M65XX_PEEK(a.w));
					break;
				case 0X49: // EOR #$NN
					M65XX_FETCH(o);
					M65XX_EOR(o);
					break;
				case 0X51: // EOR ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_EOR(M65XX_PEEK(a.w));
					break;
				case 0X59: // EOR $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_EOR(M65XX_PEEK(a.w));
					break;
				case 0X61: // ADC ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X69: // ADC #$NN
					M65XX_FETCH(o);
					M65XX_ADC(o);
					break;
				case 0X71: // ADC ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X79: // ADC $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X81: // STA ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_A);
					break;
				case 0X91: // STA ($NN),Y
					M65XX_IND_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_A);
					break;
				case 0X99: // STA $NNNN,Y
					M65XX_ABS_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_A);
					break;
				case 0XA1: // LDA ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XA9: // LDA #$NN
					M65XX_FETCH(M65XX_A); z=n=M65XX_A;
					break;
				case 0XB1: // LDA ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XB9: // LDA $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XC1: // CMP ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_CMP(M65XX_PEEK(a.w));
					break;
				case 0XC9: // CMP #$NN
					M65XX_FETCH(o);
					M65XX_CMP(o);
					break;
				case 0XD1: // CMP ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_CMP(M65XX_PEEK(a.w));
					break;
				case 0XD9: // CMP $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_CMP(M65XX_PEEK(a.w));
					break;
				case 0XE1: // SBC ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_SBC(M65XX_PEEK(a.w));
					break;
				case 0XE9: // SBC #$NN
				case 0XEB: // SBC #$NN (illegal!)
					M65XX_FETCH(o);
					M65XX_SBC(o);
					break;
				case 0XF1: // SBC ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_SBC(M65XX_PEEK(a.w));
					break;
				case 0XF9: // SBC $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_SBC(M65XX_PEEK(a.w));
					break;
				// opcodes N*8+2 ---------------------------- //
				case 0X0A: // ASL
					M65XX_ASL(M65XX_A,M65XX_A); M65XX_BADPC;
					break;
				case 0X2A: // ROL
					M65XX_ROL(M65XX_A,M65XX_A); M65XX_BADPC;
					break;
				case 0X4A: // LSR
					M65XX_LSR(M65XX_A,M65XX_A); M65XX_BADPC;
					break;
				case 0X6A: // ROR
					M65XX_ROR(M65XX_A,M65XX_A); M65XX_BADPC;
					break;
				case 0X8A: // TXA
					z=n=M65XX_A=M65XX_X; M65XX_BADPC;
					break;
				case 0X9A: // TXS
					M65XX_S=M65XX_X; M65XX_BADPC;
					break;
				case 0XA2: // LDX #$NN
					M65XX_FETCH(M65XX_X); z=n=M65XX_X;
					break;
				case 0XAA: // TAX
					z=n=M65XX_X=M65XX_A; M65XX_BADPC;
					break;
				case 0XBA: // TSX
					z=n=M65XX_X=M65XX_S; M65XX_BADPC;
					break;
				case 0XCA: // DEX
					z=n=--M65XX_X; M65XX_BADPC;
					break;
				// opcodes N*8+3 ---------------------------- //
				case 0X03: // SLO ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SLO(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X0B: // ANC #$NN
				case 0X2B: // ANC #$NN
					M65XX_FETCH(o);
					z=n=M65XX_A&=o; M65XX_P=(M65XX_P&~1)+(z>>7);
					break;
				case 0X13: // SLO ($NN),Y
					M65XX_IND_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SLO(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X1B: // SLO $NNNN,Y
					M65XX_ABS_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SLO(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X23: // RLA ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RLA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X33: // RLA ($NN),Y
					M65XX_IND_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RLA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X3B: // RLA $NNNN,Y
					M65XX_ABS_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RLA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X4B: // ASR #$NN // a.k.a. ALR
					M65XX_FETCH(o);
					o&=M65XX_A; M65XX_P=(M65XX_P&~1)+(o&1); z=n=M65XX_A=o>>1;
					break;
				case 0X6B: // ARR #$NN
					M65XX_FETCH(o);
					o&=M65XX_A; z=n=M65XX_A=(o>>1)+(M65XX_P<<7);
					if (M65XX_P&8)
					{
						M65XX_P=(M65XX_P&~64)+((o^z)&64);
						if ((o& 15)+(o& 1)> 5)
							M65XX_A=(M65XX_A&240)+((M65XX_A+6)&15);
						if ((o&240)+(o&16)>80)
							M65XX_P|=1,M65XX_A+=96;
						else
							M65XX_P&=~1;
					}
					else
						M65XX_P=(M65XX_P&~65)+(o>>7)+((o^(o>>1))&64);
					break;
				case 0X8B: // ANE #$NN
					// "...Mastertronic variant of the 'burner' tape loader ([...] 'Spectipede', 'BMX Racer') [...]
					// the commonly assumed value $EE for the 'magic constant' will not work, but $EF does..." (Groepaz/Solution)
					M65XX_FETCH(o);
					z=n=M65XX_A=(M65XX_A|(M65XX_XEF))&M65XX_X&o;
					break;
				case 0X43: // SRE ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SRE(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X53: // SRE ($NN),Y
					M65XX_IND_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SRE(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X5B: // SRE $NNNN,Y
					M65XX_ABS_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SRE(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X63: // RRA ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RRA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X73: // RRA ($NN),Y
					M65XX_IND_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RRA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X7B: // RRA $NNNN,Y
					M65XX_ABS_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RRA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X83: // SAX ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_SAX);
					break;
				case 0X93: // SHA ($NN),Y
					M65XX_IND_Y_BADXY;
					M65XX_TICK; M65XX_SHZ(q,M65XX_SAX);
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0X9B: // SHS $NNNN,Y
					M65XX_ABS_Y_BADXY;
					M65XX_TICK; M65XX_SHZ(q,M65XX_S=M65XX_SAX);
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA3: // LAX ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XAB: // LXA #$NN
					// "...a very popular C-64 game, 'Wizball', actually uses the LAX #imm opcode [...]
					// $EE really seems to be the one and only value to make it work correctly..." (Groepaz/Solution)
					M65XX_FETCH(o);
					z=n=M65XX_A=M65XX_X=(M65XX_A|(M65XX_XEE))&o;
					break;
				case 0XB3: // LAX ($NN),Y
					M65XX_IND_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XBB: // LAS $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_S&=M65XX_PEEK(a.w);
					break;
				case 0XCB: // SBX #$NN
					M65XX_FETCH(o);
					z=n=M65XX_X=i=M65XX_SAX-o;
					M65XX_CARRY(i>=0);
					break;
				case 0XC3: // DCP ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_DCP(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XD3: // DCP ($NN),Y
					M65XX_IND_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_DCP(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XDB: // DCP $NNNN,Y
					M65XX_ABS_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_DCP(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XE3: // ISB ($NN,X)
					M65XX_IND_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ISB(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XF3: // ISB ($NN),Y
					M65XX_IND_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ISB(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XFB: // ISB $NNNN,Y
					M65XX_ABS_Y_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ISB(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				// opcodes N*8+4 ---------------------------- //
				case 0X24: // BIT $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_BIT(M65XX_PEEKZERO(a.w));
					break;
				case 0X2C: // BIT $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_BIT(M65XX_PEEK(a.w));
					break;
				case 0X4C: // JMP $NNNN
					#ifdef M65XX_TRAP_0X4C
					M65XX_TRAP_0X4C;
					#endif
					M65XX_FETCH(o);
					M65XX_PAGE(M65XX_PC.b.h); M65XX_WAIT; M65XX_PC.b.h=M65XX_PEEK(M65XX_PC.w);
					M65XX_PC.b.l=o;
					break;
				case 0X6C: // JMP ($NNNN)
					#ifdef M65XX_TRAP_0X6C
					M65XX_TRAP_0X6C;
					#endif
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_PC.b.l=M65XX_PEEK(a.w); ++a.b.l; // not `++a.w`!
					M65XX_WAIT; M65XX_PC.b.h=M65XX_PEEK(a.w);
					break;
				case 0X84: // STY $NN
					M65XX_ZPG;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_Y);
					break;
				case 0X8C: // STY $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_Y);
					break;
				case 0X94: // STY $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_Y);
					break;
				case 0X9C: // SHY $NNNN,X
					M65XX_ABS_X_BADXY;
					M65XX_TICK; M65XX_SHZ(q,M65XX_Y);
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA4: // LDY $NN
					M65XX_ZPG;
					M65XX_WAIT; z=n=M65XX_Y=M65XX_PEEKZERO(a.w);
					break;
				case 0XAC: // LDY $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_Y=M65XX_PEEK(a.w);
					break;
				case 0XB4: // LDY $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; z=n=M65XX_Y=M65XX_PEEKZERO(a.w);
					break;
				case 0XBC: // LDY $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_Y=M65XX_PEEK(a.w);
					break;
				case 0XC4: // CPY $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_CPY(M65XX_PEEKZERO(a.w));
					break;
				case 0XCC: // CPY $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_CPY(M65XX_PEEK(a.w));
					break;
				case 0XE4: // CPX $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_CPX(M65XX_PEEKZERO(a.w));
					break;
				case 0XEC: // CPX $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_CPX(M65XX_PEEK(a.w));
					break;
				// opcodes N*8+5 ---------------------------- //
				case 0X05: // ORA $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_ORA(M65XX_PEEKZERO(a.w));
					break;
				case 0X0D: // ORA $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ORA(M65XX_PEEK(a.w));
					break;
				case 0X15: // ORA $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; M65XX_ORA(M65XX_PEEKZERO(a.w));
					break;
				case 0X1D: // ORA $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ORA(M65XX_PEEK(a.w));
					break;
				case 0X25: // AND $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_AND(M65XX_PEEKZERO(a.w));
					break;
				case 0X2D: // AND $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_AND(M65XX_PEEK(a.w));
					break;
				case 0X35: // AND $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; M65XX_AND(M65XX_PEEKZERO(a.w));
					break;
				case 0X3D: // AND $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_AND(M65XX_PEEK(a.w));
					break;
				case 0X45: // EOR $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_EOR(M65XX_PEEKZERO(a.w));
					break;
				case 0X4D: // EOR $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_EOR(M65XX_PEEK(a.w));
					break;
				case 0X55: // EOR $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; M65XX_EOR(M65XX_PEEKZERO(a.w));
					break;
				case 0X5D: // EOR $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_EOR(M65XX_PEEK(a.w));
					break;
				case 0X65: // ADC $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_ADC(M65XX_PEEKZERO(a.w));
					break;
				case 0X6D: // ADC $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X75: // ADC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; M65XX_ADC(M65XX_PEEKZERO(a.w));
					break;
				case 0X7D: // ADC $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_ADC(M65XX_PEEK(a.w));
					break;
				case 0X85: // STA $NN
					M65XX_ZPG;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_A);
					break;
				case 0X8D: // STA $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_A);
					break;
				case 0X95: // STA $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_A);
					break;
				case 0X9D: // STA $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_A);
					break;
				case 0XA5: // LDA $NN
					M65XX_ZPG;
					M65XX_WAIT; z=n=M65XX_A=M65XX_PEEKZERO(a.w);
					break;
				case 0XAD: // LDA $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XB5: // LDA $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; z=n=M65XX_A=M65XX_PEEKZERO(a.w);
					break;
				case 0XBD: // LDA $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_PEEK(a.w);
					break;
				case 0XC5: // CMP $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_CMP(M65XX_PEEKZERO(a.w));
					break;
				case 0XCD: // CMP $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_CMP(M65XX_PEEK(a.w));
					break;
				case 0XD5: // CMP $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; M65XX_CMP(M65XX_PEEKZERO(a.w));
					break;
				case 0XDD: // CMP $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_CMP(M65XX_PEEK(a.w));
					break;
				case 0XE5: // SBC $NN
					M65XX_ZPG;
					M65XX_WAIT; M65XX_SBC(M65XX_PEEKZERO(a.w));
					break;
				case 0XED: // SBC $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_SBC(M65XX_PEEK(a.w));
					break;
				case 0XF5: // SBC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; M65XX_SBC(M65XX_PEEKZERO(a.w));
					break;
				case 0XFD: // SBC $NNNN,X
					M65XX_ABS_X; M65XX_PAGE(a.b.h);
					M65XX_WAIT; M65XX_SBC(M65XX_PEEK(a.w));
					break;
				// opcodes N*8+6 ---------------------------- //
				case 0X06: // ASL $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ASL(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X0E: // ASL $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ASL(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X16: // ASL $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ASL(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X1E: // ASL $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ASL(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X26: // ROL $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ROL(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X2E: // ROL $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ROL(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X36: // ROL $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ROL(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X3E: // ROL $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ROL(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X46: // LSR $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_LSR(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X4E: // LSR $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_LSR(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X56: // LSR $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_LSR(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X5E: // LSR $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_LSR(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X66: // ROR $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ROR(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X6E: // ROR $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ROR(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X76: // ROR $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ROR(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X7E: // ROR $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ROR(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X86: // STX $NN
					M65XX_ZPG;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_X);
					break;
				case 0X8E: // STX $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_X);
					break;
				case 0X96: // STX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_X);
					break;
				case 0X9E: // SHX $NNNN,Y
					M65XX_ABS_Y_BADXY;
					M65XX_TICK; M65XX_SHZ(q,M65XX_X); // used in "FELLAS-EXT.PRG"
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA6: // LDX $NN
					M65XX_ZPG;
					M65XX_WAIT; z=n=M65XX_X=M65XX_PEEKZERO(a.w);
					break;
				case 0XAE: // LDX $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XB6: // LDX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC;
					M65XX_WAIT; z=n=M65XX_X=M65XX_PEEKZERO(a.w);
					break;
				case 0XBE: // LDX $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XC6: // DEC $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_DEC(o);
					M65XX_TICK; M65XX_POKEZERO(a.w,o);
					break;
				case 0XCE: // DEC $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_DEC(o);
					M65XX_TICK; M65XX_POKE(a.w,o);
					break;
				case 0XD6: // DEC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_DEC(o);
					M65XX_TICK; M65XX_POKEZERO(a.w,o);
					break;
				case 0XDE: // DEC $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_DEC(o);
					M65XX_TICK; M65XX_POKE(a.w,o);
					break;
				case 0XE6: // INC $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_INC(o);
					M65XX_TICK; M65XX_POKEZERO(a.w,o);
					break;
				case 0XEE: // INC $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_INC(o);
					M65XX_TICK; M65XX_POKE(a.w,o);
					break;
				case 0XF6: // INC $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_INC(o);
					M65XX_TICK; M65XX_POKEZERO(a.w,o);
					break;
				case 0XFE: // INC $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_INC(o);
					M65XX_TICK; M65XX_POKE(a.w,o);
					break;
				// opcodes N*8+7 ---------------------------- //
				case 0X07: // SLO $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_SLO(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X0F: // SLO $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SLO(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X17: // SLO $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_SLO(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X1F: // SLO $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SLO(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X27: // RLA $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_RLA(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X2F: // RLA $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RLA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X37: // RLA $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_RLA(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X3F: // RLA $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RLA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X47: // SRE $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_SRE(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X4F: // SRE $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SRE(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X57: // SRE $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_SRE(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X5F: // SRE $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_SRE(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X67: // RRA $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_RRA(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X6F: // RRA $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RRA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X77: // RRA $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_RRA(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0X7F: // RRA $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_RRA(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0X87: // SAX $NN
					M65XX_ZPG;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_SAX);
					break;
				case 0X8F: // SAX $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_TICK; M65XX_POKE(a.w,M65XX_SAX);
					break;
				case 0X97: // SAX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC;
					M65XX_TICK; M65XX_POKEZERO(a.w,M65XX_SAX);
					break;
				case 0X9F: // SHA $NNNN,Y
					M65XX_ABS_Y_BADXY;
					M65XX_TICK; M65XX_SHZ(q,M65XX_SAX);
					M65XX_PAGE(a.b.h); M65XX_POKE(a.w,q);
					break;
				case 0XA7: // LAX $NN
					M65XX_ZPG; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XAF: // LAX $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XB7: // LAX $NN,Y
					M65XX_ZPG_Y_IRQ_BADPC;
					M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_PEEKZERO(a.w);
					break;
				case 0XBF: // LAX $NNNN,Y
					M65XX_ABS_Y; M65XX_PAGE(a.b.h);
					M65XX_WAIT; z=n=M65XX_A=M65XX_X=M65XX_PEEK(a.w);
					break;
				case 0XC7: // DCP $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_DCP(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0XCF: // DCP $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_DCP(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XD7: // DCP $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_DCP(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0XDF: // DCP $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_DCP(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XE7: // ISB $NN
					M65XX_ZPG;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ISB(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0XEF: // ISB $NNNN
					M65XX_ABS; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ISB(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				case 0XF7: // ISB $NN,X
					M65XX_ZPG_X_BADPC;
					M65XX_WAIT; o=M65XX_PEEKZERO(a.w);
					M65XX_TICK; //M65XX_DUMBPOKEZERO(a.w,o);
					M65XX_ISB(q,o);
					M65XX_TICK; M65XX_POKEZERO(a.w,q);
					break;
				case 0XFF: // ISB $NNNN,X
					M65XX_ABS_X_BADXY; M65XX_PAGE(a.b.h);
					M65XX_WAIT; o=M65XX_PEEK(a.w);
					M65XX_TICK; M65XX_DUMBPOKE(a.w,o);
					M65XX_ISB(q,o);
					M65XX_TICK; M65XX_POKE(a.w,q);
					break;
				// NOP and JAM opcodes ---------------------- //
				case 0X80: case 0X82: case 0X89: case 0XC2: case 0XE2: // NOP #$NN
					M65XX_BADPC; ++M65XX_PC.w;
					break;
				case 0X04: case 0X44: case 0X64: // NOP $NN
					M65XX_BADPC; ++M65XX_PC.w;
					M65XX_BADPC;
					break;
				case 0X14: case 0X34: case 0X54: case 0X74: case 0XD4: case 0XF4: // NOP $NN,X
					M65XX_BADPC; ++M65XX_PC.w;
					M65XX_BADPC;
					M65XX_WAIT; M65XX_DUMBPEEK(M65XX_PC.w); // don't repeat M65XX_DUMBPAGE
					break;
				case 0X0C: // NOP $NNNN
					M65XX_ABS; M65XX_DUMBPAGE(a.b.h);
					M65XX_WAIT; M65XX_DUMBPEEK(a.w);
					break;
				case 0X1C: case 0X3C: case 0X5C: case 0X7C: case 0XDC: case 0XFC: // NOP $NNNN,X
					M65XX_ABS_X; M65XX_DUMBPAGE(a.b.h);
					M65XX_WAIT; M65XX_DUMBPEEK(a.w);
					break;
				case 0XFA: // $FA BREAKPOINT
					#ifdef DEBUG_HERE
					if (debug_break) { _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#endif
				case 0X1A: case 0X3A: case 0X5A: case 0X7A: case 0XDA: // NOP (illegal!)
				case 0XEA: // NOP (official)
					M65XX_BADPC;
					break;
				default: // ILLEGAL CODE!
				//case 0X02: case 0X12: case 0X22: case 0X32: case 0X42: case 0X52: // JAM (1/2)
				//case 0X62: case 0X72: case 0X92: case 0XB2: case 0XD2: case 0XF2: // JAM (2/2)
					--M65XX_PC.w;
					#ifdef DEBUG_HERE
					{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
					#else
					if (m65xx_t<_t_) m65xx_t=_t_; // waste all the ticks!
					#endif
			}
		}
		#ifdef DEBUG_HERE
		if (UNLIKELY(debug_point[M65XX_PC.w]))
		{
			if (debug_point[M65XX_PC.w]&8) // log byte?
			{
				int z; switch (debug_point[M65XX_PC.w]&7)
				{
					// M65XX 8-bit "PAXYS" order
					case  0: z=M65XX_P; break;
					case  1: z=M65XX_A; break;
					case  2: z=M65XX_X; break;
					case  3: z=M65XX_Y; break;
					default: z=M65XX_S; break;
				}
				if (debug_logbyte(z))
					debug_point[M65XX_PC.w]=0; // remove breakpoint on failure!
			}
			else // pure breakpoint?
				{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
		}
		#endif
	}
	while (m65xx_t<_t_);
	M65XX_MERGE_P; M65XX_SYNC(m65xx_t);
}

// M65XX debugger --------------------------------------------------- //

#ifdef DEBUG_HERE

char *debug_list(void) { return "PAXYS"; }
void debug_jump(WORD w) { M65XX_PC.w=w; }
WORD debug_this(void) { return M65XX_PC.w; }
WORD debug_that(void) { return 256+M65XX_S; }
void debug_poke(WORD w,BYTE b) { POKE(w)=b; } // send the byte `b` to memory address `w`
BYTE debug_peek(BYTE q,WORD w) { return q?POKE(w):PEEK(w); } // read one byte from `w` as seen from the R/W mode `q`
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
			*t++=(M65XX_P&128)?'N':'-'; *t++=(M65XX_P& 64)?'V':'-'; *t++=(M65XX_P& 32)?'1':'-'; *t++=(M65XX_P& 16)?'B':'-'; *t++=':';
			*t++=(M65XX_P&  8)?'D':'-'; *t++=(M65XX_P&  4)?'I':'-'; *t++=(M65XX_P&  2)?'Z':'-'; *t++=(M65XX_P&  1)?'C':'-'; *t=0; break;
		case  1: sprintf(t,"PC = %04X",M65XX_PC.w); break;
		case  2: sprintf(t,"P  =   %02X",M65XX_P); break;
		case  3: sprintf(t,"A  =   %02X",M65XX_A); break;
		case  4: sprintf(t,"X  =   %02X",M65XX_X); break;
		case  5: sprintf(t,"Y  =   %02X",M65XX_Y); break;
		case  6: sprintf(t,"S  =   %02X",M65XX_S); break;
		case  7: sprintf(t,"NMI%c IRQ%c",(M65XX_N_T>=0)?'*':'-',(M65XX_I_T>=0)?'*':'-'); break;
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
{
	#define DEBUG_DASM_BYTE (w=PEEK(p),++p,w)
	#define DEBUG_DASM_REL8 (WORD)(w=PEEK(p),++p+(signed char)w)
	#define DEBUG_DASM_WORD (w=PEEK(p),++p,w+=PEEK(p)<<8,++p,w)
	const char opcode0[8][4]={"BPL","BMI","BVC","BVS","BCC","BCS","BNE","BEQ"};
	const char opcode1[8][4]={"ORA","AND","EOR","ADC","STA","LDA","CMP","SBC"};
	const char opcode2[8][4]={"ASL","ROL","LSR","ROR","STX","LDX","DEC","INC"};
	const char opcode3[8][4]={"NOP","BIT","JMP","JMP","STY","LDY","CPY","CPX"};
	const char opcode4[8][4]={"SLO","RLA","SRE","RRA","SAX","LAX","DCP","ISB"};
	const char opcode5[8][4]={"ANC","ANC","ASR","ARR","ANE","LXA","SBX","SBC"};
	debug_match=p==M65XX_PC.w; WORD w; BYTE o=PEEK(p); switch (++p,o)
	{
		// opcodes N*8+0 ------------------------------------ //
		case 0X00: sprintf(t,"BRK  $%02X",DEBUG_DASM_BYTE); break;
		case 0X08: sprintf(t,"PHP"); break;
		case 0X10: case 0X30: case 0X50: case 0X70: case 0X90: case 0XB0: case 0XD0: case 0XF0:
			sprintf(t,"%s  $%04X",opcode0[o>>5],DEBUG_DASM_REL8); break;
		case 0X18: sprintf(t,"CLC"); break;
		case 0X20: sprintf(t,"JSR  $%04X",DEBUG_DASM_WORD); break;
		case 0X28: sprintf(t,"PLP"); break;
		case 0X38: sprintf(t,"SEC"); break;
		case 0X40: sprintf(t,"RTI"); break;
		case 0X48: sprintf(t,"PHA"); break;
		case 0X58: sprintf(t,"CLI"); break;
		case 0X60: sprintf(t,"RTS"); break;
		case 0X68: sprintf(t,"PLA"); break;
		case 0X78: sprintf(t,"SEI"); break;
		case 0X88: sprintf(t,"DEY"); break;
		case 0X98: sprintf(t,"TYA"); break;
		case 0XA0: case 0XC0: case 0XE0:
			sprintf(t,"%s  #$%02X",opcode3[o>>5],DEBUG_DASM_BYTE); break;
		case 0XA8: sprintf(t,"TAY"); break;
		case 0XB8: sprintf(t,"CLV"); break;
		case 0XC8: sprintf(t,"INY"); break;
		case 0XD8: sprintf(t,"CLD"); break;
		case 0XE8: sprintf(t,"INX"); break;
		case 0XF8: sprintf(t,"SED"); break;
		// opcodes N*8+1 ------------------------------------ //
		case 0X01: case 0X21: case 0X41: case 0X61: case 0X81: case 0XA1: case 0XC1: case 0XE1:
			sprintf(t,"%s  ($%02X,X)",opcode1[o>>5],DEBUG_DASM_BYTE); break;
		case 0X09: case 0X29: case 0X49: case 0X69: case 0XA9: case 0XC9: case 0XE9:
			sprintf(t,"%s  #$%02X",opcode1[o>>5],DEBUG_DASM_BYTE); break;
		case 0X11: case 0X31: case 0X51: case 0X71: case 0X91: case 0XB1: case 0XD1: case 0XF1:
			sprintf(t,"%s  ($%02X),Y",opcode1[o>>5],DEBUG_DASM_BYTE); break;
		case 0X19: case 0X39: case 0X59: case 0X79: case 0X99: case 0XB9: case 0XD9: case 0XF9:
			sprintf(t,"%s  $%04X,Y",opcode1[o>>5],DEBUG_DASM_WORD); break;
		// opcodes N*8+2 ------------------------------------ //
		case 0X0A: case 0X2A: case 0X4A: case 0X6A: sprintf(t,opcode2[o>>5]); break;
		case 0X8A: sprintf(t,"TXA"); break;
		case 0X9A: sprintf(t,"TXS"); break;
		case 0XA2: sprintf(t,"LDX  #$%02X",DEBUG_DASM_BYTE); break;
		case 0XAA: sprintf(t,"TAX"); break;
		case 0XBA: sprintf(t,"TSX"); break;
		case 0XCA: sprintf(t,"DEX"); break;
		// opcodes N*8+3 ------------------------------------ //
		case 0X03: case 0X23: case 0X43: case 0X63: case 0X83: case 0XA3: case 0XC3: case 0XE3:
			sprintf(t,"%s  ($%02X,X)",opcode4[o>>5],DEBUG_DASM_BYTE); break;
		case 0X0B: case 0X2B: case 0X4B: case 0X6B: case 0X8B: case 0XAB: case 0XCB: case 0XEB:
			sprintf(t,"%s  #$%02X",opcode5[o>>5],DEBUG_DASM_BYTE); break;
		case 0X13: case 0X33: case 0X53: case 0X73: case 0XB3: case 0XD3: case 0XF3:
			sprintf(t,"%s  ($%02X),Y",opcode4[o>>5],DEBUG_DASM_BYTE); break;
		case 0X93: sprintf(t,"SHA  ($%02X),Y",DEBUG_DASM_BYTE); break;
		case 0X1B: case 0X3B: case 0X5B: case 0X7B: case 0XDB: case 0XFB:
		case 0XBF: // special case "LAX $NNNN,Y" creeps here because of the ",Y"
			sprintf(t,"%s  $%04X,Y",opcode4[o>>5],DEBUG_DASM_WORD); break;
		case 0X9B: sprintf(t,"SHS  $%04X,Y",DEBUG_DASM_WORD); break;
		case 0XBB: sprintf(t,"LAS  $%04X,Y",DEBUG_DASM_WORD); break;
		// opcodes N*8+4 ------------------------------------ //
		case 0X6C: sprintf(t,"JMP  ($%04X)",DEBUG_DASM_WORD); break;
		case 0X04: case 0X24: case 0X84: case 0XA4: case 0XC4: case 0XE4:
			sprintf(t,"%s  $%02X",opcode3[o>>5],DEBUG_DASM_BYTE); break;
		case 0X14: case 0X94: case 0XB4:
			sprintf(t,"%s  $%02X,X",opcode3[o>>5],DEBUG_DASM_BYTE); break;
		case 0X0C: case 0X2C: case 0X4C: case 0X8C: case 0XAC: case 0XCC: case 0XEC:
			sprintf(t,"%s  $%04X",opcode3[o>>5],DEBUG_DASM_WORD); break;
		case 0X9C: sprintf(t,"SHY  $%04X,X",DEBUG_DASM_WORD); break;
		case 0XBC: sprintf(t,"LDY  $%04X,X",DEBUG_DASM_WORD); break;
		// opcodes N*8+5 ------------------------------------ //
		case 0X05: case 0X25: case 0X45: case 0X65: case 0X85: case 0XA5: case 0XC5: case 0XE5:
			sprintf(t,"%s  $%02X",opcode1[o>>5],DEBUG_DASM_BYTE); break;
		case 0X15: case 0X35: case 0X55: case 0X75: case 0X95: case 0XB5: case 0XD5: case 0XF5:
			sprintf(t,"%s  $%02X,X",opcode1[o>>5],DEBUG_DASM_BYTE); break;
		case 0X0D: case 0X2D: case 0X4D: case 0X6D: case 0X8D: case 0XAD: case 0XCD: case 0XED:
			sprintf(t,"%s  $%04X",opcode1[o>>5],DEBUG_DASM_WORD); break;
		case 0X1D: case 0X3D: case 0X5D: case 0X7D: case 0X9D: case 0XBD: case 0XDD: case 0XFD:
			sprintf(t,"%s  $%04X,X",opcode1[o>>5],DEBUG_DASM_WORD); break;
		// opcodes N*8+6 ------------------------------------ //
		case 0X06: case 0X26: case 0X46: case 0X66: case 0X86: case 0XA6: case 0XC6: case 0XE6:
			sprintf(t,"%s  $%02X",opcode2[o>>5],DEBUG_DASM_BYTE); break;
		case 0X16: case 0X36: case 0X56: case 0X76: case 0XD6: case 0XF6:
			sprintf(t,"%s  $%02X,X",opcode2[o>>5],DEBUG_DASM_BYTE); break;
		case 0X96: case 0XB6:
			sprintf(t,"%s  $%02X,Y",opcode2[o>>5],DEBUG_DASM_BYTE); break;
		case 0X0E: case 0X2E: case 0X4E: case 0X6E: case 0X8E: case 0XAE: case 0XCE: case 0XEE:
			sprintf(t,"%s  $%04X",opcode2[o>>5],DEBUG_DASM_WORD); break;
		case 0X1E: case 0X3E: case 0X5E: case 0X7E: case 0XDE: case 0XFE:
			sprintf(t,"%s  $%04X,X",opcode2[o>>5],DEBUG_DASM_WORD); break;
		case 0X9E: sprintf(t,"SHX  $%04X,Y",DEBUG_DASM_WORD); break;
		case 0XBE: sprintf(t,"LDX  $%04X,Y",DEBUG_DASM_WORD); break;
		// opcodes N*8+7 ------------------------------------ //
		case 0X07: case 0X27: case 0X47: case 0X67: case 0X87: case 0XA7: case 0XC7: case 0XE7:
			sprintf(t,"%s  $%02X",opcode4[o>>5],DEBUG_DASM_BYTE); break;
		case 0X0F: case 0X2F: case 0X4F: case 0X6F: case 0XAF: case 0XCF: case 0XEF: case 0X8F:
			sprintf(t,"%s  $%04X",opcode4[o>>5],DEBUG_DASM_WORD); break;
		case 0X17: case 0X37: case 0X57: case 0X77: case 0X97: case 0XB7: case 0XD7: case 0XF7:
			sprintf(t,"%s  $%02X,X",opcode4[o>>5],DEBUG_DASM_BYTE); break;
		case 0X1F: case 0X3F: case 0X5F: case 0X7F: case 0XDF: case 0XFF:
			sprintf(t,"%s  $%04X,X",opcode4[o>>5],DEBUG_DASM_WORD); break;
		case 0X9F: sprintf(t,"SHA  $%04X,Y",DEBUG_DASM_WORD); break;
		// NOP and JAM opcodes ------------------------------ //
		case 0X44: case 0X64: // NOP $NN
			sprintf(t,"NOP  $%02X",DEBUG_DASM_BYTE); break;
		case 0X34: case 0X54: case 0X74: case 0XD4: case 0XF4: // NOP $NN,X
			sprintf(t,"NOP  $%02X,X",DEBUG_DASM_BYTE); break;
		case 0X1C: case 0X3C: case 0X5C: case 0X7C: case 0XDC: case 0XFC: // NOP $NNNN,X
			sprintf(t,"NOP  $%04X,X",DEBUG_DASM_WORD); break;
		case 0X80: case 0X82: case 0X89: case 0XC2: case 0XE2: // NOP #$NN
			sprintf(t,"NOP  #$%02X",DEBUG_DASM_BYTE); break;
		case 0X1A: case 0X3A: case 0X5A: case 0X7A: case 0XDA: case 0XFA: // NOP (illegal!)
			//sprintf(t,"*NOP"); break;
		case 0XEA: // NOP
			sprintf(t,opcode3[0]); break;
		case 0X02: case 0X12: case 0X22: case 0X32: case 0X42: case 0X52: // JAM (1/2)
		case 0X62: case 0X72: case 0X92: case 0XB2: case 0XD2: case 0XF2: // JAM (2/2)
			sprintf(t,"JAM"); break;
	}
	return p;
}

#endif

// undefine the configuration macros to ease redefinition

#undef M65XX_LOCAL
#undef M65XX_RESET
#undef M65XX_START
#undef M65XX_MAIN
#undef M65XX_I_T
#undef M65XX_N_T
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
#undef M65XX_PEEK
#undef M65XX_POKE
#undef M65XX_PEEKZERO
#undef M65XX_POKEZERO
#undef M65XX_PULL
#undef M65XX_PUSH
#undef M65XX_DUMBPAGE
#undef M65XX_DUMBPEEK
#undef M65XX_DUMBPOKE
//#undef M65XX_DUMBPEEKZERO
//#undef M65XX_DUMBPOKEZERO
//#undef M65XX_DUMBPULL
//#undef M65XX_DUMBPUSH
#undef M65XX_TICK
#undef M65XX_WAIT

#undef M65XX_RDY
#undef M65XX_SHW
#undef M65XX_SHZ
#undef M65XX_XEE
#undef M65XX_XEF
#undef M65XX_CRUNCH
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

#ifndef _M65XX_H_
#define _M65XX_H_
#endif

// =================================== END OF MOS 6510/6502 EMULATION //
