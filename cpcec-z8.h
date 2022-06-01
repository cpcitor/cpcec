 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The ZILOG Z80A that runs inside the Amstrad CPC is very similar to
// other Z80 chips used in 8-bit and 16-bit computers and consoles from
// the 1980s and 1990s. Its timings are slightly different but making
// them fit other machines only requires new timing macros and tables.

// This module also includes the instruments required by the built-in
// general-purpose debugger: a disassembler and a register editor.

// BEGINNING OF Z80 EMULATION ======================================= //

WORD z80_wz; // internal register WZ/MEMPTR //,z80_wz2; // is there a WZ'?
#if Z80_XCF_BUG
BYTE z80_q; // internal register Q
#define Z80_Q_SET(x) (z80_q=z80_af.b.l=(x))
#define Z80_Q_RST() (z80_q=0)
#else
#define Z80_Q_SET(x) (z80_af.b.l=(x))
#define Z80_Q_RST()
#endif

#ifdef DEBUG_HERE
int debug_trap_sp,debug_trap_pc=0;
void debug_clear(void) // cleans volatile breakpoints
{
	debug_inter=0; debug_trap_sp=1<<16; // cancel interrupt+return traps!
	debug_point[debug_trap_pc]&=127; // cancel volatile breakpoint!
}
void debug_reset(void) // sets the debugger's PC and SP
{
	debug_panel0_w=z80_pc.w; debug_panel0_x=0;
	debug_panel3_w=z80_sp.w; debug_panel3_x=0;
}
#endif

BYTE z80_flags_inc[256],z80_flags_dec[256]; // INC,DEC
BYTE z80_flags_sgn[256],z80_flags_add[512],z80_flags_sub[512]; // ADD,ADC,SUB,SBC...
BYTE z80_flags_and[256],z80_flags_xor[256],z80_flags_bit[256]; // AND,XOR,OR,BIT...

void z80_setup(void) // setup the Z80
{
	// flag bit reference:
	// - 7, 0x80: S, Sign
	// - 6, 0x40: Z, Zero
	// - 5, 0x20: Y, undocumented
	// - 4, 0x10: H, Half carry
	// - 3, 0x08: X, undocumented
	// - 2, 0x04: V, Parity/oVerflow
	// - 1, 0x02: N, add/substract
	// - 0, 0x01: C, Carry
	// 9-bit expression RESULT^OP1^OP2 : bit 8 = full Carry, bit 7 = Sign carry, bit 4 = Half carry
	for (int i=0;i<256;++i) // build flags that depend on the 8-bit result: S, Z and sometimes H and V too
	{
		int p=i&1;
		if (i&2) ++p;
		if (i&4) ++p;
		if (i&8) ++p;
		if (i&16) ++p;
		if (i&32) ++p;
		if (i&64) ++p;
		if (i&128) ++p;
		p=(p&1)?0:4; // -----P--
		int j=i?(i&0x80):0x40; // SZ------
		int x=j+(i&0x28); // SZY-X----
		z80_flags_sgn[i]=x; // SZY-X---
		z80_flags_inc[i]=x+(i==128?4:0)+((i&15)== 0?0x10:0); // SZYHXV0-
		z80_flags_dec[i]=x+(i==127?4:0)+((i&15)==15?0x12:2); // SZYHXV1-
		z80_flags_and[i]=0x10+(z80_flags_xor[i]=(x+p)); // SZY1XP--,SZY0XP--
		z80_flags_bit[i]=0x10+j+p; // SZ-1-P--
		j=(i&0x10)+((i&128)>>5); // ---H-V--
		z80_flags_sub[i]=2+(z80_flags_add[i]=j); // ---H-V!0
		z80_flags_sub[256+i]=2+(z80_flags_add[256+i]=j^5); // ---H-V!1
	}
}

// optional Z80-based ROM extension Dandanator ---------------------- //

#ifdef Z80_DANDANATOR
char dandanator_target[STRMAX]="";
int dandanator_insert(char *s)
{
	FILE *f=puff_fopen(s,"rb");
	if (!f)
		return 1;
	if (!mem_dandanator)
		mem_dandanator=malloc(32<<14);
	int i=fread1(mem_dandanator,32<<14,f);
	memset(&mem_dandanator[i],0xFF,(32<<14)-i);
	if (dandanator_target!=s) strcpy(dandanator_target,s);
	return puff_fclose(f),dandanator_dirty=0;
}
void dandanator_remove(void)
{
	if (mem_dandanator)
	{
		if (dandanator_dirty&&dandanator_canwrite&&*dandanator_target)
		{
			cprintf("DNTR save '%s'\n",dandanator_target);
			FILE *f; if (f=puff_fopen(dandanator_target,"wb"))
				fwrite1(mem_dandanator,32<<14,f),puff_fclose(f); // all 16k blocks
		}
		free(mem_dandanator),mem_dandanator=NULL;
	}
	dandanator_dirty=0;
}
#define z80_close() dandanator_remove()
#else
#define z80_close() 0
#endif

// Z80 interpreter -------------------------------------------------- //

void z80_reset(void) // reset the Z80
{
	z80_pc.w=z80_sp.w=z80_ir.w=z80_iff.w=z80_irq=z80_active=z80_imd=0;
	z80_ix.w=z80_iy.w=0xFFFF;
	#ifdef Z80_DANDANATOR
	z80_dandanator_reset();
	#endif
}

// macros are handier than typing the same snippets of code a million times
#define Z80_GET_R8 ((z80_ir.b.l&0x80)+(r7&0x7F)) // rebuild R from R7
#define Z80_FETCH Z80_PEEKZ(z80_pc.w)
#define Z80_RD_PC Z80_PEEK(z80_pc.w)
#define Z80_WZ_PC z80_wz=Z80_RD_PC; ++z80_pc.w; z80_wz+=Z80_RD_PC<<8 // read WZ from WORD[PC++]
#define Z80_RD_HL BYTE b=Z80_PEEK(z80_hl.w)
#define Z80_WR_HL Z80_PEEKPOKE(z80_hl.w,b)
#define Z80_WZ_XY z80_wz=xy->w+(signed char)(Z80_RD_PC); ++z80_pc.w // calculate WZ and XY from BYTE[PC++], without additional delays
#define Z80_WZ_XY_1X(x) z80_wz=xy->w+(signed char)(Z80_RD_PC); Z80_MREQ_1X(x,z80_pc.w); ++z80_pc.w // calculate WZ and XY from BYTE[PC++]
#define Z80_RD_WZ BYTE b=Z80_PEEK(z80_wz)
#define Z80_WR_WZ Z80_PEEKPOKE(z80_wz,b)
#define Z80_LD2(x) x.l=Z80_RD_PC; ++z80_pc.w; x.h=Z80_RD_PC; ++z80_pc.w // load r16 from WORD[PC++]
#define Z80_RD2(x) Z80_WZ_PC; ++z80_pc.w; x.l=Z80_PEEK1(z80_wz); ++z80_wz; x.h=Z80_PEEK2(z80_wz) // load r16 from [WZ]
#define Z80_WR2(x,y) Z80_WZ_PC; ++z80_pc.w; Z80_POKE1(z80_wz,x.l); Z80_STRIDE(y); ++z80_wz; Z80_POKE2(z80_wz,x.h) // write r16 to [WZ]
#define Z80_EXX2(x,y) do{ int w=x; x=y; y=w; }while(0)
#define Z80_INC1(x) Z80_Q_SET((z80_af.b.l&1)+z80_flags_inc[++x])
#define Z80_DEC1(x) Z80_Q_SET((z80_af.b.l&1)+z80_flags_dec[--x])
#define Z80_ADD2(x,y) do{ int z=x+y; Z80_Q_SET((z>>16)+((z>>8)&0x28)+(((z^x^y)>>8)&0x10)+(z80_af.b.l&0xC4)); z80_wz=x+1; x=z; Z80_WAIT_IR1X(7); }while(0)
#define Z80_ADC2(x) do{ int z=z80_hl.w+x.w+(z80_af.b.l&1); Z80_Q_SET((z>>16)+(((z80_hl.w^z^x.w)>>8)&0x10)+(((WORD)z)?((z>>8)&0xA8):0x40)+((((x.w^~z80_hl.w)&(x.w^z))>>13)&4)); z80_wz=z80_hl.w+1; z80_hl.w=z; Z80_WAIT_IR1X(7); }while(0)
#define Z80_SBC2(x) do{ int z=z80_hl.w-x.w-(z80_af.b.l&1); Z80_Q_SET(2+((z>>16)&1)+(((z80_hl.w^z^x.w)>>8)&0x10)+(((WORD)z)?((z>>8)&0xA8):0x40)+((((x.w^z80_hl.w)&(z80_hl.w^z))>>13)&4)); z80_wz=z80_hl.w+1; z80_hl.w=z; Z80_WAIT_IR1X(7); }while(0)
#define Z80_ADDC(x,y) do{ int z=z80_af.b.h+x+y; Z80_Q_SET(z80_flags_sgn[(BYTE)z]+z80_flags_add[(z^z80_af.b.h^x)]); z80_af.b.h=z; }while(0) // ADD safely stays within [0,511]
#define Z80_SUBC(x,y) do{ int z=z80_af.b.h-x-y; Z80_Q_SET(z80_flags_sgn[(BYTE)z]+z80_flags_sub[(z^z80_af.b.h^x)&511]); z80_af.b.h=z; }while(0) // SUB can fall beyond [0,511]
#define Z80_ADD1(x) Z80_ADDC(x,0)
#define Z80_ADC1(x) Z80_ADDC(x,(z80_af.b.l&1))
#define Z80_SUB1(x) Z80_SUBC(x,0)
#define Z80_SBC1(x) Z80_SUBC(x,(z80_af.b.l&1))
#define Z80_AND1(x) Z80_Q_SET(z80_flags_and[z80_af.b.h=z80_af.b.h&(x)])
#define Z80_XOR1(x) Z80_Q_SET(z80_flags_xor[z80_af.b.h=z80_af.b.h^(x)])
#define Z80_OR1(x) Z80_Q_SET(z80_flags_xor[z80_af.b.h=z80_af.b.h|(x)])
#define Z80_CP1(x) do{ int z=z80_af.b.h-x; Z80_Q_SET((z80_flags_sgn[(BYTE)z]&0xD7)+z80_flags_sub[(z^z80_af.b.h^x)&511]+(x&0x28)); }while(0) // unlike SUB, 1.- A intact, 2.- flags 3+5 from argument
#ifdef DEBUG_HERE
#define Z80_RET2 z80_wz=Z80_PEEK(z80_sp.w); if (++z80_sp.w>debug_trap_sp) { _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } z80_pc.w=z80_wz+=Z80_PEEK(z80_sp.w)<<8; ++z80_sp.w; // throw!
#else
#define Z80_RET2 z80_wz=Z80_PEEK(z80_sp.w); ++z80_sp.w; z80_pc.w=z80_wz+=Z80_PEEK(z80_sp.w)<<8; ++z80_sp.w
#endif
#define Z80_POP2(x) x.l=Z80_PEEK(z80_sp.w); ++z80_sp.w; x.h=Z80_PEEK(z80_sp.w); ++z80_sp.w
#define Z80_PUSH2(x,y) --z80_sp.w; Z80_POKE3(z80_sp.w,x.h); Z80_STRIDE(y); --z80_sp.w; Z80_POKE4(z80_sp.w,x.l)
#define Z80_CALL2 --z80_sp.w; Z80_POKE0(z80_sp.w,z80_pc.b.h); --z80_sp.w; Z80_POKE0(z80_sp.w,z80_pc.b.l); z80_pc.w=z80_wz
#define Z80_RLC1(x) x=(x<<1)+(x>>7); Z80_Q_SET(z80_flags_xor[x]+(x&1))
#define Z80_RRC1(x) x=(x>>1)+(x<<7); Z80_Q_SET(z80_flags_xor[x]+((x>>7)&1))
#define Z80_RL1(x) do{ BYTE z=x>>7; Z80_Q_SET(z80_flags_xor[x=(x<<1)+(z80_af.b.l&1)]+z); }while(0)
#define Z80_RR1(x) do{ BYTE z=x&1; Z80_Q_SET(z80_flags_xor[x=(x>>1)+(z80_af.b.l<<7)]+z); }while(0)
#define Z80_SLA1(x) do{ BYTE z=x>>7; Z80_Q_SET(z80_flags_xor[x=x<<1]+z); }while(0)
#define Z80_SRA1(x) do{ BYTE z=x&1; Z80_Q_SET(z80_flags_xor[x=((signed char)x)>>1]+z); }while(0)
#define Z80_SLL1(x) do{ BYTE z=x>>7; Z80_Q_SET(z80_flags_xor[x=(x<<1)+1]+z); }while(0)
#define Z80_SRL1(x) do{ BYTE z=x&1; Z80_Q_SET(z80_flags_xor[x=x>>1]+z); }while(0)
#define Z80_BIT1(n,x,y) Z80_Q_SET((z80_flags_bit[x&(1<<n)]+(y&0x28))+(z80_af.b.l&1))
#define Z80_RES1(n,x) (x&=~(1<<n))
#define Z80_SET1(n,x) (x|=(1<<n))
#define Z80_IN2(x,y) z80_r7=r7; z80_wz=z80_bc.w; Z80_PRAE_RECV(z80_wz); Z80_Q_SET(z80_flags_xor[x=Z80_RECV(z80_wz)]+(z80_af.b.l&1)); r7=z80_r7; Z80_POST_RECV(z80_wz); ++z80_wz; Z80_STRIDE_IO(y)
#define Z80_OUT2(x,y) z80_wz=z80_bc.w; Z80_PRAE_SEND(z80_wz); Z80_SEND(z80_wz,x); Z80_POST_SEND(z80_wz); ++z80_wz; Z80_STRIDE_IO(y)

INLINE void z80_main(int _t_) // emulate the Z80 for `_t_` clock ticks
{
	int z80_t=0; // clock tick counter
	BYTE r7=z80_ir.b.l; // split R7+R8!
	Z80_LOCAL; do
	{
		++r7; // "Timing Tests 48k Spectrum" requires this!
		if (z80_irq&&z80_active) // ignore IRQs when either is zero!
		{
			#ifdef Z80_NMI_ACK
			if (z80_active<0) // NMI?
			{
				Z80_WAIT(7); Z80_STRIDE_ZZ(0x3A); z80_wz=0x66; // NMI timing equals LD A,($NNNN) : 13 T / 4 NOP (see below IM 0 and IM 1)
				z80_active=z80_iff.b.l=0; // notice that z80_iff.b.h (IFF2) stays untouched; RETN uses it to restore z80_iff.b.l (IFF1)
				Z80_NMI_ACK;
			}
			else
			#endif
			{
				if (z80_active>1) // HALT?
					++z80_pc.w; // skip!
				if (z80_imd&2)
				{
					Z80_WAIT(7); Z80_STRIDE_ZZ(0xE3); Z80_STRIDE(0x1E3); // IM 2 timing equals EX HL,(SP) : 19 T / 6 NOP
					z80_wz=(z80_ir.b.h<<8)+z80_irq_bus; // the address is built according to I and the bus
					z80_iff.b.l=Z80_PEEK(z80_wz);
					++z80_wz;
					z80_iff.b.h=Z80_PEEK(z80_wz);
					z80_wz=z80_iff.w;
					Z80_TRDOS_LEAVE(z80_iff); // see RETI opcode below
				}
				else
				{
					Z80_WAIT(7); Z80_STRIDE_ZZ(0x3A); // IM 0 and IM 1 timing equals LD A,($NNNN) : 13 T / 4 NOP (RST N is actually 11 T!)
					z80_wz=(z80_imd&1)?0x38:(z80_irq_bus&0x38); // IM 0 reads the address from the bus, `RST x` style; any opcode but a RST crashes anyway.
				}
				z80_active=z80_iff.w=0; // IFF1 and IFF2 are reset when an IRQ is acknowledged; setting them is up to the interrupt handler
				z80_irq_ack(); if (debug_inter) debug_inter=_t_=0,session_signal|=SESSION_SIGNAL_DEBUG; // throw!
			}
			Z80_CALL2;
		}
		#if !Z80_HALT_STRIDE // careful HALT
		else if (z80_active>1)
			{ z80_wz=z80_pc.w+1; Z80_PEEKZ(z80_wz); Z80_STRIDE(0X000); continue; } // the Z80 is actually performing NOPs rather than HALTs
		#endif
		else
		{
			BYTE o=Z80_FETCH; ++z80_pc.w;
			Z80_STRIDE_0; Z80_STRIDE(o);
			z80_active=z80_iff.b.l;
			switch (o) // consume EI delay
			{
				// 0x00-0x3F
				case 0x01: // LD BC,$NNNN
					Z80_LD2(z80_bc.b);
					break;
				case 0x11: // LD DE,$NNNN
					Z80_LD2(z80_de.b);
					break;
				case 0x21: // LD HL,$NNNN
					Z80_LD2(z80_hl.b);
					break;
				case 0x31: // LD SP,$NNNN
					Z80_LD2(z80_sp.b);
					// no `break`!
				case 0x00: // NOP
					break;
				case 0x02: // LD (BC),A
					Z80_POKE(z80_bc.w,z80_af.b.h);
					#ifdef Z80_DNTR_0X02
					Z80_DNTR_0X02(z80_bc.w,z80_af.b.h);
					#endif
					z80_wz=((z80_bc.w+1)&0x00FF)|(z80_af.b.h<<8);
					break;
				case 0x12: // LD (DE),A
					Z80_POKE(z80_de.w,z80_af.b.h);
					#ifdef Z80_DNTR_0X12
					Z80_DNTR_0X12(z80_de.w,z80_af.b.h);
					#endif
					z80_wz=((z80_de.w+1)&0x00FF)|(z80_af.b.h<<8);
					break;
				case 0x0A: // LD A,(BC)
					z80_af.b.h=Z80_PEEK(z80_bc.w);
					z80_wz=z80_bc.w+1;
					break;
				case 0x1A: // LD A,(DE)
					z80_af.b.h=Z80_PEEK(z80_de.w);
					z80_wz=z80_de.w+1;
					break;
				case 0x22: // LD ($NNNN),HL
					Z80_WR2(z80_hl.b,0X122);
					break;
				case 0x32: // LD ($NNNN),A
					Z80_WZ_PC; ++z80_pc.w;
					Z80_POKE(z80_wz,z80_af.b.h);
					#ifdef Z80_DNTR_0X32
					Z80_DNTR_0X32(z80_wz,z80_af.b.h);
					#endif
					z80_wz=((z80_wz+1)&0x00FF)|(z80_af.b.h<<8);
					break;
				case 0x2A: // LD HL,($NNNN)
					Z80_RD2(z80_hl.b);
					break;
				case 0x3A: // LD A,($NNNN)
					Z80_WZ_PC; ++z80_pc.w;
					z80_af.b.h=Z80_PEEK(z80_wz);
					#ifdef Z80_DNTR_0X3A
					Z80_DNTR_0X3A(z80_wz,z80_af.b.h);
					#endif
					++z80_wz;
					break;
				case 0x03: // INC BC
					++z80_bc.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x0B: // DEC BC
					--z80_bc.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x13: // INC DE
					++z80_de.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x1B: // DEC DE
					--z80_de.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x23: // INC HL
					++z80_hl.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x2B: // DEC HL
					--z80_hl.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x33: // INC SP
					++z80_sp.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x3B: // DEC SP
					--z80_sp.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0x04: // INC B
					Z80_INC1(z80_bc.b.h);
					break;
				case 0x05: // DEC B
					Z80_DEC1(z80_bc.b.h);
					break;
				case 0x0C: // INC C
					Z80_INC1(z80_bc.b.l);
					break;
				case 0x0D: // DEC C
					Z80_DEC1(z80_bc.b.l);
					break;
				case 0x14: // INC D
					Z80_INC1(z80_de.b.h);
					break;
				case 0x15: // DEC D
					Z80_DEC1(z80_de.b.h);
					break;
				case 0x1C: // INC E
					Z80_INC1(z80_de.b.l);
					break;
				case 0x1D: // DEC E
					Z80_DEC1(z80_de.b.l);
					break;
				case 0x24: // INC H
					Z80_INC1(z80_hl.b.h);
					break;
				case 0x25: // DEC H
					Z80_DEC1(z80_hl.b.h);
					break;
				case 0x2C: // INC L
					Z80_INC1(z80_hl.b.l);
					break;
				case 0x2D: // DEC L
					Z80_DEC1(z80_hl.b.l);
					break;
				case 0x34: // INC (HL)
					{ Z80_RD_HL; Z80_INC1(b); Z80_IORQ_NEXT(1); Z80_WR_HL; }
					break;
				case 0x35: // DEC (HL)
					{ Z80_RD_HL; Z80_DEC1(b); Z80_IORQ_NEXT(1); Z80_WR_HL; }
					break;
				case 0x3C: // INC A
					Z80_INC1(z80_af.b.h);
					break;
				case 0x3D: // DEC A
					Z80_DEC1(z80_af.b.h);
					break;
				case 0x06: // LD B,$NN
					z80_bc.b.h=Z80_RD_PC; ++z80_pc.w;
					break;
				case 0x0E: // LD C,$NN
					z80_bc.b.l=Z80_RD_PC; ++z80_pc.w;
					break;
				case 0x16: // LD D,$NN
					z80_de.b.h=Z80_RD_PC; ++z80_pc.w;
					break;
				case 0x1E: // LD E,$NN
					z80_de.b.l=Z80_RD_PC; ++z80_pc.w;
					break;
				case 0x26: // LD H,$NN
					z80_hl.b.h=Z80_RD_PC; ++z80_pc.w;
					break;
				case 0x2E: // LD L,$NN
					z80_hl.b.l=Z80_RD_PC; ++z80_pc.w;
					break;
				case 0x36: // LD (HL),$NN
					{ BYTE b=Z80_RD_PC; Z80_POKE(z80_hl.w,b); ++z80_pc.w; }
					break;
				case 0x3E: // LD A,$NN
					z80_af.b.h=Z80_RD_PC; ++z80_pc.w;
					break;
				case 0x07: // RLCA
					z80_af.b.h=(z80_af.b.h<<1)+(z80_af.b.h>>7); Z80_Q_SET((z80_af.b.h&1)+(z80_af.b.h&0x28)+(z80_af.b.l&0xC4)); // flags --503-0C
					break;
				case 0x0F: // RRCA
					z80_af.b.h=(z80_af.b.h>>1)+(z80_af.b.h<<7); Z80_Q_SET((z80_af.b.h>>7)+(z80_af.b.h&0x28)+(z80_af.b.l&0xC4)); // flags --503-0C
					break;
				case 0x17: // RLA
					{ BYTE b=z80_af.b.h>>7; z80_af.b.h=(z80_af.b.h<<1)+(z80_af.b.l&1); Z80_Q_SET(b+(z80_af.b.h&0x28)+(z80_af.b.l&0xC4)); } // flags --503-0C
					break;
				case 0x1F: // RRA
					{ BYTE b=z80_af.b.h&1; z80_af.b.h=(z80_af.b.h>>1)+(z80_af.b.l<<7); Z80_Q_SET(b+(z80_af.b.h&0x28)+(z80_af.b.l&0xC4)); } // flags --503-0C
					break;
				case 0x08: // EX AF,AF'
					Z80_EXX2(z80_af.w,z80_af2.w);
					break;
				case 0x09: // ADD HL,BC
					Z80_ADD2(z80_hl.w,z80_bc.w);
					break;
				case 0x19: // ADD HL,DE
					Z80_ADD2(z80_hl.w,z80_de.w);
					break;
				case 0x29: // ADD HL,HL
					Z80_ADD2(z80_hl.w,z80_hl.w);
					break;
				case 0x39: // ADD HL,SP
					Z80_ADD2(z80_hl.w,z80_sp.w);
					break;
				case 0x10: // DJNZ $RRRR
					Z80_WAIT_IR1X(1); if (--z80_bc.b.h)
					{
						Z80_STRIDE(0x110);
						z80_wz=(signed char)Z80_RD_PC;
						Z80_IORQ_1X_NEXT(5);
						z80_pc.w=z80_wz+=z80_pc.w+1;
					}
					else
					{
						/*o=*/Z80_RD_PC; ++z80_pc.w; // dummy!
						#ifdef Z80_DNTR_0X10
						Z80_DNTR_0X10(z80_pc.w);
						#endif
					}
					break;
				case 0x18: // JR $RRRR
					z80_wz=(signed char)Z80_RD_PC;
					Z80_IORQ_1X_NEXT(5);
					z80_pc.w=z80_wz+=z80_pc.w+1;
					break;
				case 0x20: // JR NZ,$RRRR
					if (!(z80_af.b.l&0x40))
					{
						Z80_STRIDE(0x120);
						z80_wz=(signed char)Z80_RD_PC;
						Z80_IORQ_1X_NEXT(5);
						z80_pc.w=z80_wz+=z80_pc.w+1;
					}
					else
					{
						/*o=*/Z80_RD_PC; ++z80_pc.w; // dummy!
					}
					break;
				case 0x28: // JR Z,$RRRR
					if (z80_af.b.l&0x40)
					{
						Z80_STRIDE(0x128);
						z80_wz=(signed char)Z80_RD_PC;
						Z80_IORQ_1X_NEXT(5);
						z80_pc.w=z80_wz+=z80_pc.w+1;
					}
					else
					{
						/*o=*/Z80_RD_PC; ++z80_pc.w; // dummy!
					}
					break;
				case 0x30: // JR NC,$RRRR
					if (!(z80_af.b.l&0x01))
					{
						Z80_STRIDE(0x130);
						z80_wz=(signed char)Z80_RD_PC;
						Z80_IORQ_1X_NEXT(5);
						z80_pc.w=z80_wz+=z80_pc.w+1;
					}
					else
					{
						/*o=*/Z80_RD_PC; ++z80_pc.w; // dummy!
					}
					break;
				case 0x38: // JR C,$RRRR
					if (z80_af.b.l&0x01)
					{
						Z80_STRIDE(0x138);
						z80_wz=(signed char)Z80_RD_PC;
						Z80_IORQ_1X_NEXT(5);
						z80_pc.w=z80_wz+=z80_pc.w+1;
					}
					else
					{
						/*o=*/Z80_RD_PC; ++z80_pc.w; // dummy!
					}
					break;
				case 0x27: // DAA
					{
						BYTE x=z80_af.b.h,z=0,b=(((x&0x0F)>9)||(z80_af.b.l&0x10))?0x06:0;
						if ((x>0x99)||(z80_af.b.l&0x01))
							z=1,b+=0x60;
						if (z80_af.b.l&2)
							z80_af.b.h-=b;
						else
							z80_af.b.h+=b;
						Z80_Q_SET(z80_flags_xor[z80_af.b.h]+((z80_af.b.h^x)&0x10)+(z80_af.b.l&2)+z);
					}
					break;
				case 0x2F: // CPL
					Z80_Q_SET((z80_af.b.l&0xC5)+((z80_af.b.h=~z80_af.b.h)&0x28)+0x12); // --513-1-
					break;
				case 0x37: // SCF
					#if Z80_XCF_BUG > 1
					Z80_Q_SET((z80_af.b.l&0xC4)+((z80_af.b.h|(z80_xcf255&(z80_af.b.l^z80_q)))&0x28)+1);
					#elif Z80_XCF_BUG
					Z80_Q_SET((z80_af.b.l&0xC4)+((z80_af.b.h|(z80_af.b.l^z80_q))&0x28)+1);
					#else
					z80_af.b.l=(z80_af.b.l&0xC4)+(z80_af.b.h&0x28)+1; // --503--1
					#endif
					break;
				case 0x3F: // CCF
					#if Z80_XCF_BUG > 1
					Z80_Q_SET((z80_af.b.l&0xC4)+((z80_af.b.h|(z80_xcf255&(z80_af.b.l^z80_q)))&0x28)+((z80_af.b.l&1)<<4)+((~z80_af.b.l)&1));
					#elif Z80_XCF_BUG
					Z80_Q_SET((z80_af.b.l&0xC4)+((z80_af.b.h|(z80_af.b.l^z80_q))&0x28)+((z80_af.b.l&1)<<4)+((~z80_af.b.l)&1));
					#else
					z80_af.b.l=(z80_af.b.l&0xC4)+(z80_af.b.h&0x28)+((z80_af.b.l&1)<<4)+((~z80_af.b.l)&1); // --5H3--C
					#endif
					break;
				// 0x40-0x7F
				case 0x41: // LD B,C
					z80_bc.b.h=z80_bc.b.l;
					// no `break`!
				case 0x40: // LD B,B
					break;
				case 0x42: // LD B,D
					z80_bc.b.h=z80_de.b.h;
					break;
				case 0x43: // LD B,E
					z80_bc.b.h=z80_de.b.l;
					break;
				case 0x44: // LD B,H
					z80_bc.b.h=z80_hl.b.h;
					break;
				case 0x45: // LD B,L
					z80_bc.b.h=z80_hl.b.l;
					break;
				case 0x46: // LD B,(HL)
					z80_bc.b.h=Z80_PEEK(z80_hl.w);
					break;
				case 0x47: // LD B,A
					z80_bc.b.h=z80_af.b.h;
					break;
				case 0x48: // LD C,B
					z80_bc.b.l=z80_bc.b.h;
					// no `break`!
				case 0x49: // LD C,C
					break;
				case 0x4A: // LD C,D
					z80_bc.b.l=z80_de.b.h;
					break;
				case 0x4B: // LD C,E
					z80_bc.b.l=z80_de.b.l;
					break;
				case 0x4C: // LD C,H
					z80_bc.b.l=z80_hl.b.h;
					break;
				case 0x4D: // LD C,L
					z80_bc.b.l=z80_hl.b.l;
					break;
				case 0x4E: // LD C,(HL)
					z80_bc.b.l=Z80_PEEK(z80_hl.w);
					break;
				case 0x4F: // LD C,A
					z80_bc.b.l=z80_af.b.h;
					break;
				case 0x50: // LD D,B
					z80_de.b.h=z80_bc.b.h;
					break;
				case 0x51: // LD D,C
					z80_de.b.h=z80_bc.b.l;
					break;
				case 0x53: // LD D,E
					z80_de.b.h=z80_de.b.l;
					// no `break`!
				case 0x52: // LD D,D
					break;
				case 0x54: // LD D,H
					z80_de.b.h=z80_hl.b.h;
					break;
				case 0x55: // LD D,L
					z80_de.b.h=z80_hl.b.l;
					break;
				case 0x56: // LD D,(HL)
					z80_de.b.h=Z80_PEEK(z80_hl.w);
					break;
				case 0x57: // LD D,A
					z80_de.b.h=z80_af.b.h;
					break;
				case 0x58: // LD E,B
					z80_de.b.l=z80_bc.b.h;
					break;
				case 0x59: // LD E,C
					z80_de.b.l=z80_bc.b.l;
					break;
				case 0x5A: // LD E,D
					z80_de.b.l=z80_de.b.h;
					// no `break`!
				case 0x5B: // LD E,E
					break;
				case 0x5C: // LD E,H
					z80_de.b.l=z80_hl.b.h;
					break;
				case 0x5D: // LD E,L
					z80_de.b.l=z80_hl.b.l;
					break;
				case 0x5E: // LD E,(HL)
					z80_de.b.l=Z80_PEEK(z80_hl.w);
					break;
				case 0x5F: // LD E,A
					z80_de.b.l=z80_af.b.h;
					break;
				case 0x60: // LD H,B
					z80_hl.b.h=z80_bc.b.h;
					break;
				case 0x61: // LD H,C
					z80_hl.b.h=z80_bc.b.l;
					break;
				case 0x62: // LD H,D
					z80_hl.b.h=z80_de.b.h;
					break;
				case 0x63: // LD H,E
					z80_hl.b.h=z80_de.b.l;
					break;
				case 0x65: // LD H,L
					z80_hl.b.h=z80_hl.b.l;
					// no `break`!
				case 0x64: // LD H,H
					break;
				case 0x66: // LD H,(HL)
					z80_hl.b.h=Z80_PEEK(z80_hl.w);
					break;
				case 0x67: // LD H,A
					z80_hl.b.h=z80_af.b.h;
					break;
				case 0x68: // LD L,B
					z80_hl.b.l=z80_bc.b.h;
					break;
				case 0x69: // LD L,C
					z80_hl.b.l=z80_bc.b.l;
					break;
				case 0x6A: // LD L,D
					z80_hl.b.l=z80_de.b.h;
					break;
				case 0x6B: // LD L,E
					z80_hl.b.l=z80_de.b.l;
					break;
				case 0x6C: // LD L,H
					z80_hl.b.l=z80_hl.b.h;
					// no `break`!
				case 0x6D: // LD L,L
					break;
				case 0x6E: // LD L,(HL)
					z80_hl.b.l=Z80_PEEK(z80_hl.w);
					break;
				case 0x6F: // LD L,A
					z80_hl.b.l=z80_af.b.h;
					break;
				case 0x70: // LD (HL),B
					Z80_POKE(z80_hl.w,z80_bc.b.h);
					break;
				case 0x71: // LD (HL),C
					Z80_POKE(z80_hl.w,z80_bc.b.l);
					break;
				case 0x72: // LD (HL),D
					Z80_POKE(z80_hl.w,z80_de.b.h);
					break;
				case 0x73: // LD (HL),E
					Z80_POKE(z80_hl.w,z80_de.b.l);
					break;
				case 0x74: // LD (HL),H
					Z80_POKE(z80_hl.w,z80_hl.b.h);
					break;
				case 0x75: // LD (HL),L
					Z80_POKE(z80_hl.w,z80_hl.b.l);
					break;
				case 0x77: // LD (HL),A
					Z80_POKE(z80_hl.w,z80_af.b.h);
					#ifdef Z80_DNTR_0X77
					Z80_DNTR_0X77(z80_hl.w,z80_af.b.h);
					#endif
					break;
				case 0x78: // LD A,B
					z80_af.b.h=z80_bc.b.h;
					break;
				case 0x79: // LD A,C
					z80_af.b.h=z80_bc.b.l;
					break;
				case 0x7A: // LD A,D
					z80_af.b.h=z80_de.b.h;
					break;
				case 0x7B: // LD A,E
					z80_af.b.h=z80_de.b.l;
					break;
				case 0x7C: // LD A,H
					z80_af.b.h=z80_hl.b.h;
					break;
				case 0x7D: // LD A,L
					z80_af.b.h=z80_hl.b.l;
					break;
				case 0x7E: // LD A,(HL)
					z80_af.b.h=Z80_PEEK(z80_hl.w);
					// no `break`!
				case 0x7F: // LD A,A
					break;
				case 0x76: // HALT
					if (!z80_iff.b.l)
						--z80_pc.w,Z80_SLEEP(_t_); // the Z80 is stuck!!!
					else
					{
						#if Z80_HALT_STRIDE // optimal HALT
						int z; if (!z80_irq&&(z=(_t_-z80_t))>0)
						{
							r7+=z/=Z80_HALT_STRIDE;
							Z80_SLEEP(z*Z80_HALT_STRIDE);
						}
						#endif
						--z80_pc.w; // go back to get more!
						z80_active<<=1; // -1=>-2, +0=>+0 and +1=>+2
					}
					break;
				// 0x80-0xBF
				case 0x80: // ADD B
					Z80_ADD1(z80_bc.b.h);
					break;
				case 0x81: // ADD C
					Z80_ADD1(z80_bc.b.l);
					break;
				case 0x82: // ADD D
					Z80_ADD1(z80_de.b.h);
					break;
				case 0x83: // ADD E
					Z80_ADD1(z80_de.b.l);
					break;
				case 0x84: // ADD H
					Z80_ADD1(z80_hl.b.h);
					break;
				case 0x85: // ADD L
					Z80_ADD1(z80_hl.b.l);
					break;
				case 0x86: // ADD (HL)
					{ Z80_RD_HL; Z80_ADD1(b); }
					break;
				case 0x87: // ADD A
					Z80_ADD1(z80_af.b.h);
					break;
				case 0x88: // ADC B
					Z80_ADC1(z80_bc.b.h);
					break;
				case 0x89: // ADC C
					Z80_ADC1(z80_bc.b.l);
					break;
				case 0x8A: // ADC D
					Z80_ADC1(z80_de.b.h);
					break;
				case 0x8B: // ADC E
					Z80_ADC1(z80_de.b.l);
					break;
				case 0x8C: // ADC H
					Z80_ADC1(z80_hl.b.h);
					break;
				case 0x8D: // ADC L
					Z80_ADC1(z80_hl.b.l);
					break;
				case 0x8E: // ADC (HL)
					{ Z80_RD_HL; Z80_ADC1(b); }
					break;
				case 0x8F: // ADC A
					Z80_ADC1(z80_af.b.h);
					break;
				case 0x90: // SUB B
					Z80_SUB1(z80_bc.b.h);
					break;
				case 0x91: // SUB C
					Z80_SUB1(z80_bc.b.l);
					break;
				case 0x92: // SUB D
					Z80_SUB1(z80_de.b.h);
					break;
				case 0x93: // SUB E
					Z80_SUB1(z80_de.b.l);
					break;
				case 0x94: // SUB H
					Z80_SUB1(z80_hl.b.h);
					break;
				case 0x95: // SUB L
					Z80_SUB1(z80_hl.b.l);
					break;
				case 0x96: // SUB (HL)
					{ Z80_RD_HL; Z80_SUB1(b); }
					break;
				case 0x97: // SUB A
					#if Z80_XCF_BUG
					z80_q=
					#endif
					z80_af.w=0x0042;//Z80_SUB1(z80_af.b.h);
					break;
				case 0x98: // SBC B
					Z80_SBC1(z80_bc.b.h);
					break;
				case 0x99: // SBC C
					Z80_SBC1(z80_bc.b.l);
					break;
				case 0x9A: // SBC D
					Z80_SBC1(z80_de.b.h);
					break;
				case 0x9B: // SBC E
					Z80_SBC1(z80_de.b.l);
					break;
				case 0x9C: // SBC H
					Z80_SBC1(z80_hl.b.h);
					break;
				case 0x9D: // SBC L
					Z80_SBC1(z80_hl.b.l);
					break;
				case 0x9E: // SBC (HL)
					{ Z80_RD_HL; Z80_SBC1(b); }
					break;
				case 0x9F: // SBC A
					Z80_SBC1(z80_af.b.h);
					break;
				case 0xA0: // AND B
					Z80_AND1(z80_bc.b.h);
					break;
				case 0xA1: // AND C
					Z80_AND1(z80_bc.b.l);
					break;
				case 0xA2: // AND D
					Z80_AND1(z80_de.b.h);
					break;
				case 0xA3: // AND E
					Z80_AND1(z80_de.b.l);
					break;
				case 0xA4: // AND H
					Z80_AND1(z80_hl.b.h);
					break;
				case 0xA5: // AND L
					Z80_AND1(z80_hl.b.l);
					break;
				case 0xA6: // AND (HL)
					{ Z80_RD_HL; Z80_AND1(b); }
					break;
				case 0xA7: // AND A
					Z80_AND1(z80_af.b.h);
					break;
				case 0xA8: // XOR B
					Z80_XOR1(z80_bc.b.h);
					break;
				case 0xA9: // XOR C
					Z80_XOR1(z80_bc.b.l);
					break;
				case 0xAA: // XOR D
					Z80_XOR1(z80_de.b.h);
					break;
				case 0xAB: // XOR E
					Z80_XOR1(z80_de.b.l);
					break;
				case 0xAC: // XOR H
					Z80_XOR1(z80_hl.b.h);
					break;
				case 0xAD: // XOR L
					Z80_XOR1(z80_hl.b.l);
					break;
				case 0xAE: // XOR (HL)
					{ Z80_RD_HL; Z80_XOR1(b); }
					break;
				case 0xAF: // XOR A
					#if Z80_XCF_BUG
					z80_q=
					#endif
					z80_af.w=0x0044;//Z80_XOR1(z80_af.b.h);
					break;
				case 0xB0: // OR B
					Z80_OR1(z80_bc.b.h);
					break;
				case 0xB1: // OR C
					Z80_OR1(z80_bc.b.l);
					break;
				case 0xB2: // OR D
					Z80_OR1(z80_de.b.h);
					break;
				case 0xB3: // OR E
					Z80_OR1(z80_de.b.l);
					break;
				case 0xB4: // OR H
					Z80_OR1(z80_hl.b.h);
					break;
				case 0xB5: // OR L
					Z80_OR1(z80_hl.b.l);
					break;
				case 0xB6: // OR (HL)
					{ Z80_RD_HL; Z80_OR1(b); }
					break;
				case 0xB7: // OR A
					Z80_OR1(z80_af.b.h);
					break;
				case 0xB8: // CP B
					Z80_CP1(z80_bc.b.h);
					break;
				case 0xB9: // CP C
					Z80_CP1(z80_bc.b.l);
					break;
				case 0xBA: // CP D
					Z80_CP1(z80_de.b.h);
					break;
				case 0xBB: // CP E
					Z80_CP1(z80_de.b.l);
					break;
				case 0xBC: // CP H
					Z80_CP1(z80_hl.b.h);
					break;
				case 0xBD: // CP L
					Z80_CP1(z80_hl.b.l);
					break;
				case 0xBE: // CP (HL)
					{ Z80_RD_HL; Z80_CP1(b); }
					break;
				case 0xBF: // CP A
					Z80_CP1(z80_af.b.h);
					break;
				// 0xC0-0xFF
				case 0xC0: // RET NZ
					Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x40))
					{
						Z80_STRIDE(0x1C0);
						Z80_RET2;
						Z80_TRDOS_CATCH(z80_pc); // required by TR-DOS
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xC8: // RET Z
					Z80_WAIT_IR1X(1); if (z80_af.b.l&0x40)
					{
						Z80_STRIDE(0x1C8);
						Z80_RET2;
						Z80_TRDOS_CATCH(z80_pc); // required by TR-DOS
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xD0: // RET NC
					Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x01))
					{
						Z80_STRIDE(0x1D0);
						Z80_RET2;
						Z80_TRDOS_ENTER(z80_pc); // used by TR-DOS
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xD8: // RET C
					Z80_WAIT_IR1X(1); if (z80_af.b.l&0x01)
					{
						Z80_STRIDE(0x1D8);
						Z80_RET2;
						Z80_TRDOS_ENTER(z80_pc); // used by TR-DOS
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xE0: // RET NV
					Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x04))
					{
						Z80_STRIDE(0x1E0);
						Z80_RET2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xE8: // RET V
					Z80_WAIT_IR1X(1); if (z80_af.b.l&0x04)
					{
						Z80_STRIDE(0x1E8);
						Z80_RET2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xF0: // RET NS
					Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x80))
					{
						Z80_STRIDE(0x1F0);
						Z80_RET2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xF8: // RET S
					Z80_WAIT_IR1X(1); if (z80_af.b.l&0x80)
					{
						Z80_STRIDE(0x1F8);
						Z80_RET2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						Z80_STRIDE_1;
					break;
				case 0xC9: // RET
					Z80_RET2;
					#ifdef Z80_DNTR_0XC9
					Z80_DNTR_0XC9();
					#endif
					Z80_TRDOS_CATCH(z80_pc); // required by TR-DOS
					break;
				case 0xC1: // POP BC
					Z80_POP2(z80_bc.b);
					break;
				case 0xC5: // PUSH BC
					Z80_WAIT_IR1X(1);
					Z80_PUSH2(z80_bc.b,0x1C5);
					break;
				case 0xD1: // POP DE
					Z80_POP2(z80_de.b);
					break;
				case 0xD5: // PUSH DE
					Z80_WAIT_IR1X(1);
					Z80_PUSH2(z80_de.b,0x1D5);
					break;
				case 0xE1: // POP HL
					Z80_POP2(z80_hl.b);
					break;
				case 0xE5: // PUSH HL
					Z80_WAIT_IR1X(1);
					Z80_PUSH2(z80_hl.b,0x1E5);
					break;
				case 0xF1: // POP AF
					Z80_POP2(z80_af.b);
					break;
				case 0xF5: // PUSH AF
					Z80_WAIT_IR1X(1);
					Z80_PUSH2(z80_af.b,0x1F5);
					break;
				case 0xC2: // JP NZ,$NNNN
					Z80_WZ_PC; if (!(z80_af.b.l&0x40))
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xCA: // JP Z,$NNNN
					Z80_WZ_PC; if (z80_af.b.l&0x40)
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xD2: // JP NC,$NNNN
					Z80_WZ_PC; if (!(z80_af.b.l&0x01))
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xDA: // JP C,$NNNN
					Z80_WZ_PC; if (z80_af.b.l&0x01)
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xE2: // JP NV,$NNNN
					Z80_WZ_PC; if (!(z80_af.b.l&0x04))
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xEA: // JP V,$NNNN
					Z80_WZ_PC; if (z80_af.b.l&0x04)
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xF2: // JP NS,$NNNN
					Z80_WZ_PC; if (!(z80_af.b.l&0x80))
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xFA: // JP S,$NNNN
					Z80_WZ_PC; if (z80_af.b.l&0x80)
					{
						z80_pc.w=z80_wz;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					else
						++z80_pc.w;
					break;
				case 0xC3: // JP $NNNN
					Z80_WZ_PC;
					z80_pc.w=z80_wz;
					Z80_TRDOS_ENTER(z80_pc); // used with TR-DOS (robin_wc.zip)
					break;
				case 0xC4: // CALL NZ,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (!(z80_af.b.l&0x40))
					{
						Z80_STRIDE(0x1C4);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xCC: // CALL Z,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (z80_af.b.l&0x40)
					{
						Z80_STRIDE(0x1CC);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xD4: // CALL NC,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (!(z80_af.b.l&0x01))
					{
						Z80_STRIDE(0x1D4);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xDC: // CALL C,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (z80_af.b.l&0x01)
					{
						Z80_STRIDE(0x1DC);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xE4: // CALL NV,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (!(z80_af.b.l&0x04))
					{
						Z80_STRIDE(0x1E4);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xEC: // CALL V,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (z80_af.b.l&0x04)
					{
						Z80_STRIDE(0x1EC);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xF4: // CALL NS,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (!(z80_af.b.l&0x80))
					{
						Z80_STRIDE(0x1F4);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xFC: // CALL S,$NNNN
					Z80_WZ_PC; ++z80_pc.w; if (z80_af.b.l&0x80)
					{
						Z80_STRIDE(0x1FC);
						Z80_IORQ_NEXT(1); Z80_CALL2;
						//Z80_TRDOS_ENTER(z80_pc); // unused
					}
					break;
				case 0xCD: // CALL $NNNN
					Z80_WZ_PC; ++z80_pc.w;
					Z80_IORQ_NEXT(1); Z80_CALL2;
					Z80_TRDOS_ENTER(z80_pc); // used with TR-DOS (rocman_.scl)
					break;
				case 0xC6: // ADD $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_ADD1(b); }
					break;
				case 0xCE: // ADC $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_ADC1(b); }
					break;
				case 0xD6: // SUB $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_SUB1(b); }
					break;
				case 0xDE: // SBC $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_SBC1(b); }
					break;
				case 0xE6: // AND $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_AND1(b); }
					break;
				case 0xEE: // XOR $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_XOR1(b); }
					break;
				case 0xF6: // OR $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_OR1(b); }
					break;
				case 0xFE: // CP $NN
					{ BYTE b=Z80_RD_PC; ++z80_pc.w; Z80_CP1(b); }
					break;
				case 0xDB: // IN A,($NN)
					z80_wz=(z80_af.b.h<<8)+Z80_RD_PC; ++z80_pc.w;
					Z80_PRAE_RECV(z80_wz);
					z80_af.b.h=Z80_RECV(z80_wz);
					Z80_POST_RECV(z80_wz);
					Z80_STRIDE_IO(0x1DB);
					++z80_wz;
					break;
				case 0xD3: // OUT ($NN),A
					z80_wz=(z80_af.b.h<<8)+Z80_RD_PC; ++z80_pc.w;
					Z80_PRAE_SEND(z80_wz);
					Z80_SEND(z80_wz,z80_af.b.h);
					Z80_POST_SEND(z80_wz);
					Z80_STRIDE_IO(0x1D3);
					z80_wz=((z80_wz+1)&0x00FF)|(z80_af.b.h<<8);
					break;
				case 0xD9: // EXX
					Z80_EXX2(z80_bc.w,z80_bc2.w);
					Z80_EXX2(z80_de.w,z80_de2.w);
					Z80_EXX2(z80_hl.w,z80_hl2.w);
					//Z80_EXX2(z80_wz,z80_wz2); // is WZ dual?
					break;
				case 0xEB: // EX DE,HL
					Z80_EXX2(z80_de.w,z80_hl.w);
					break;
				case 0xE3: // EX HL,(SP)
					z80_wz=Z80_PEEK(z80_sp.w);
					++z80_sp.w;
					z80_wz+=Z80_PEEKZ(z80_sp.w)<<8;
					Z80_POKE5(z80_sp.w,z80_hl.b.h);
					Z80_STRIDE(0x1E3);
					--z80_sp.w;
					Z80_POKE6(z80_sp.w,z80_hl.b.l);
					z80_hl.w=z80_wz;
					Z80_IORQ_1X_NEXT(2);
					Z80_STRIDE_1;
					break;
				case 0xE9: // JP HL
					z80_pc.w=z80_hl.w;
					Z80_TRDOS_ENTER(z80_pc); // used with TR-DOS (robin_.scl)
					break;
				case 0xF9: // LD SP,HL
					z80_sp.w=z80_hl.w;
					Z80_WAIT_IR1X(2);
					Z80_STRIDE_1;
					break;
				case 0xF3: // DI
					z80_iff.w=z80_active=0; // disable interruptions at once
					break;
				case 0xFB: // EI
					#ifdef Z80_DNTR_0XFB
					Z80_DNTR_0XFB();
					#endif
					z80_iff.w=0x0101; z80_active=0; // enable interruptions after the next instruction: CPC demo "KKB FIRST" does DI...EI:EI:HALT...DI; Spectrum 48K test "EI48K"
					break;
				case 0xC7: // RST 0
				case 0xCF: // RST 1
				case 0xD7: // RST 2
				case 0xDF: // RST 3
				case 0xE7: // RST 4
				case 0xEF: // RST 5
				case 0xF7: // RST 6
				case 0xFF: // RST 7
					z80_wz=o&0x38;
					Z80_WAIT_IR1X(1); Z80_CALL2;
					break;
				case 0xCB: // PREFIX: CB SUBSET
					o=Z80_FETCH; ++r7; ++z80_pc.w;
					Z80_STRIDE(o+0x400);
					// the CB set is extremely repetitive and thus worth abridging
					#define CASE_Z80_CB_OP1(xx,yy) \
						case xx+0: yy(z80_bc.b.h); break; case xx+1: yy(z80_bc.b.l); break; \
						case xx+2: yy(z80_de.b.h); break; case xx+3: yy(z80_de.b.l); break; \
						case xx+4: yy(z80_hl.b.h); break; case xx+5: yy(z80_hl.b.l); break; \
						case xx+7: yy(z80_af.b.h); break; case xx+6: { Z80_RD_HL; yy(b); Z80_IORQ_NEXT(1); Z80_WR_HL; } break
					#define CASE_Z80_CB_BIT(xx,yy) \
						case xx+0: Z80_BIT1(yy,z80_bc.b.h,z80_bc.b.h); break; case xx+1: Z80_BIT1(yy,z80_bc.b.l,z80_bc.b.l); break; \
						case xx+2: Z80_BIT1(yy,z80_de.b.h,z80_de.b.h); break; case xx+3: Z80_BIT1(yy,z80_de.b.l,z80_de.b.l); break; \
						case xx+4: Z80_BIT1(yy,z80_hl.b.h,z80_hl.b.h); break; case xx+5: Z80_BIT1(yy,z80_hl.b.l,z80_hl.b.l); break; \
						case xx+7: Z80_BIT1(yy,z80_af.b.h,z80_af.b.h); break; case xx+6: { Z80_RD_HL; Z80_BIT1(yy,b,(z80_wz>>8)); Z80_IORQ_NEXT(1); } break
					#define CASE_Z80_CB_OP2(xx,yy,zz) \
						case xx+0: yy(zz,z80_bc.b.h); break; case xx+1: yy(zz,z80_bc.b.l); break; \
						case xx+2: yy(zz,z80_de.b.h); break; case xx+3: yy(zz,z80_de.b.l); break; \
						case xx+4: yy(zz,z80_hl.b.h); break; case xx+5: yy(zz,z80_hl.b.l); break; \
						case xx+7: yy(zz,z80_af.b.h); break; case xx+6: { Z80_RD_HL; yy(zz,b); Z80_IORQ_NEXT(1); Z80_WR_HL; } break
					switch (o)
					{
						// 0xCB00-0xCB3F
						CASE_Z80_CB_OP1(0x00,Z80_RLC1); // RLC ...
						CASE_Z80_CB_OP1(0x08,Z80_RRC1); // RRC ...
						CASE_Z80_CB_OP1(0x10,Z80_RL1 ); // RL  ...
						CASE_Z80_CB_OP1(0x18,Z80_RR1 ); // RR  ...
						CASE_Z80_CB_OP1(0x20,Z80_SLA1); // SLA ...
						CASE_Z80_CB_OP1(0x28,Z80_SRA1); // SRA ...
						CASE_Z80_CB_OP1(0x30,Z80_SLL1); // SLL ...
						CASE_Z80_CB_OP1(0x38,Z80_SRL1); // SRL ...
						// 0xCB40-0xCB7F
						CASE_Z80_CB_BIT(0x40,0); // BIT 0,...
						CASE_Z80_CB_BIT(0x48,1); // BIT 1,...
						CASE_Z80_CB_BIT(0x50,2); // BIT 2,...
						CASE_Z80_CB_BIT(0x58,3); // BIT 3,...
						CASE_Z80_CB_BIT(0x60,4); // BIT 4,...
						CASE_Z80_CB_BIT(0x68,5); // BIT 5,...
						CASE_Z80_CB_BIT(0x70,6); // BIT 6,...
						CASE_Z80_CB_BIT(0x78,7); // BIT 7,...
						// 0xCB80-0xCBBF
						CASE_Z80_CB_OP2(0x80,Z80_RES1,0); // RES 0,...
						CASE_Z80_CB_OP2(0x88,Z80_RES1,1); // RES 1,...
						CASE_Z80_CB_OP2(0x90,Z80_RES1,2); // RES 2,...
						CASE_Z80_CB_OP2(0x98,Z80_RES1,3); // RES 3,...
						CASE_Z80_CB_OP2(0xA0,Z80_RES1,4); // RES 4,...
						CASE_Z80_CB_OP2(0xA8,Z80_RES1,5); // RES 5,...
						CASE_Z80_CB_OP2(0xB0,Z80_RES1,6); // RES 6,...
						CASE_Z80_CB_OP2(0xB8,Z80_RES1,7); // RES 7,...
						// 0xCBC0-0xCBFF
						CASE_Z80_CB_OP2(0xC0,Z80_SET1,0); // SET 0,...
						CASE_Z80_CB_OP2(0xC8,Z80_SET1,1); // SET 1,...
						CASE_Z80_CB_OP2(0xD0,Z80_SET1,2); // SET 2,...
						CASE_Z80_CB_OP2(0xD8,Z80_SET1,3); // SET 3,...
						CASE_Z80_CB_OP2(0xE0,Z80_SET1,4); // SET 4,...
						CASE_Z80_CB_OP2(0xE8,Z80_SET1,5); // SET 5,...
						CASE_Z80_CB_OP2(0xF0,Z80_SET1,6); // SET 6,...
						CASE_Z80_CB_OP2(0xF8,Z80_SET1,7); // SET 7,...
					}
					#undef CASE_Z80_CB_OP1
					#undef CASE_Z80_CB_BIT
					#undef CASE_Z80_CB_OP2
					break;
				case 0xDD: // PREFIX: XY SUBSET (IX)
				case 0xFD: // PREFIX: XY SUBSET (IY)
					{
						Z80_BEWARE; // see default case
						Z80W *xy=(o&0x20)?&z80_iy:&z80_ix;
						o=Z80_FETCH; ++r7; ++z80_pc.w;
						Z80_STRIDE(o+0x600);
						switch (o)
						{
							// 0xDD00-0xDD3F
							case 0x21: // LD IX,$NNNN
								Z80_LD2(xy->b);
								break;
							case 0x22: // LD ($NNNN),IX
								Z80_WR2(xy->b,0X122); // ==0X722
								break;
							case 0x2A: // LD IX,($NNNN)
								Z80_RD2(xy->b);
								break;
							case 0x03: // *INC BC
								++z80_bc.w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x13: // *INC DE
								++z80_de.w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x23: // INC IX
								++xy->w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x33: // *INC SP
								++z80_sp.w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x0B: // *DEC BC
								--z80_bc.w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x1B: // *DEC DE
								--z80_de.w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x2B: // DEC IX
								--xy->w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x3B: // *DEC SP
								--z80_sp.w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0x24: // INC XH
								Z80_INC1(xy->b.h);
								break;
							case 0x25: // DEC XH
								Z80_DEC1(xy->b.h);
								break;
							case 0x2C: // INC XL
								Z80_INC1(xy->b.l);
								break;
							case 0x2D: // DEC XL
								Z80_DEC1(xy->b.l);
								break;
							case 0x34: // INC (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_INC1(b); Z80_IORQ_NEXT(1); Z80_WR_WZ; }
								break;
							case 0x35: // DEC (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_DEC1(b); Z80_IORQ_NEXT(1); Z80_WR_WZ; }
								break;
							case 0x26: // LD XH,$NN
								xy->b.h=Z80_RD_PC; ++z80_pc.w;
								break;
							case 0x2E: // LD XL,$NN
								xy->b.l=Z80_RD_PC; ++z80_pc.w;
								break;
							case 0x36: // LD (IX+$XX),$NN
								{ Z80_WZ_XY; BYTE b=Z80_RD_PC; Z80_MREQ_1X(2,z80_pc.w); Z80_POKE(z80_wz,b); ++z80_pc.w; }
								break;
							case 0x09: // ADD IX,BC
								Z80_ADD2(xy->w,z80_bc.w);
								break;
							case 0x19: // ADD IX,DE
								Z80_ADD2(xy->w,z80_de.w);
								break;
							case 0x29: // ADD IX,IX
								Z80_ADD2(xy->w,xy->w);
								break;
							case 0x39: // ADD IX,SP
								Z80_ADD2(xy->w,z80_sp.w);
								break;
							// 0xDD40-0xDD7F
							case 0x44: // LD B,XH
								z80_bc.b.h=xy->b.h;
								break;
							case 0x45: // LD B,XL
								z80_bc.b.h=xy->b.l;
								break;
							case 0x46: // LD B,(IX+$XX)
								Z80_WZ_XY_1X(5); z80_bc.b.h=Z80_PEEK(z80_wz);
								break;
							case 0x4C: // LD C,XH
								z80_bc.b.l=xy->b.h;
								break;
							case 0x4D: // LD C,XL
								z80_bc.b.l=xy->b.l;
								break;
							case 0x4E: // LD C,(IX+$XX)
								Z80_WZ_XY_1X(5); z80_bc.b.l=Z80_PEEK(z80_wz);
								break;
							case 0x54: // LD D,XH
								z80_de.b.h=xy->b.h;
								break;
							case 0x55: // LD D,XL
								z80_de.b.h=xy->b.l;
								break;
							case 0x56: // LD D,(IX+$XX)
								Z80_WZ_XY_1X(5); z80_de.b.h=Z80_PEEK(z80_wz);
								break;
							case 0x5C: // LD E,XH
								z80_de.b.l=xy->b.h;
								break;
							case 0x5D: // LD E,XL
								z80_de.b.l=xy->b.l;
								break;
							case 0x5E: // LD E,(IX+$XX)
								Z80_WZ_XY_1X(5); z80_de.b.l=Z80_PEEK(z80_wz);
								break;
							case 0x60: // LD XH,B
								xy->b.h=z80_bc.b.h;
								break;
							case 0x61: // LD XH,C
								xy->b.h=z80_bc.b.l;
								break;
							case 0x62: // LD XH,D
								xy->b.h=z80_de.b.h;
								break;
							case 0x63: // LD XH,E
								xy->b.h=z80_de.b.l;
								break;
							case 0x65: // LD XH,XL
								xy->b.h=xy->b.l;
								// no `break`!
							case 0x64: // LD XH,XH
								break;
							case 0x66: // LD H,(IX+$XX)
								Z80_WZ_XY_1X(5); z80_hl.b.h=Z80_PEEK(z80_wz);
								break;
							case 0x67: // LD XH,A
								xy->b.h=z80_af.b.h;
								break;
							case 0x68: // LD XL,B
								xy->b.l=z80_bc.b.h;
								break;
							case 0x69: // LD XL,C
								xy->b.l=z80_bc.b.l;
								break;
							case 0x6A: // LD XL,D
								xy->b.l=z80_de.b.h;
								break;
							case 0x6B: // LD XL,E
								xy->b.l=z80_de.b.l;
								break;
							case 0x6C: // LD XL,XH
								xy->b.l=xy->b.h;
								// no `break`!
							case 0x6D: // LD XL,XL
								break;
							case 0x6E: // LD L,(IX+$XX)
								Z80_WZ_XY_1X(5); z80_hl.b.l=Z80_PEEK(z80_wz);
								break;
							case 0x6F: // LD XL,A
								xy->b.l=z80_af.b.h;
								break;
							case 0x70: // LD (IX+$XX),B
								Z80_WZ_XY_1X(5); Z80_POKE(z80_wz,z80_bc.b.h);
								#ifdef Z80_DNTR_0XFD70
								Z80_DNTR_0XFD70(z80_pc.w,z80_bc.b.h); // catch "FDFDFD70xx"
								#endif
								break;
							case 0x71: // LD (IX+$XX),C
								Z80_WZ_XY_1X(5); Z80_POKE(z80_wz,z80_bc.b.l);
								#ifdef Z80_DNTR_0XFD71
								Z80_DNTR_0XFD71(z80_pc.w,z80_bc.b.l); // catch "FDFDFD71xx"
								#endif
								break;
							case 0x72: // LD (IX+$XX),D
								Z80_WZ_XY_1X(5); Z80_POKE(z80_wz,z80_de.b.h);
								break;
							case 0x73: // LD (IX+$XX),E
								Z80_WZ_XY_1X(5); Z80_POKE(z80_wz,z80_de.b.l);
								break;
							case 0x74: // LD (IX+$XX),H
								Z80_WZ_XY_1X(5); Z80_POKE(z80_wz,z80_hl.b.h);
								break;
							case 0x75: // LD (IX+$XX),L
								Z80_WZ_XY_1X(5); Z80_POKE(z80_wz,z80_hl.b.l);
								break;
							case 0x77: // LD (IX+$XX),A
								Z80_WZ_XY_1X(5); Z80_POKE(z80_wz,z80_af.b.h);
								#ifdef Z80_DNTR_0XFD77
								Z80_DNTR_0XFD77(z80_pc.w,z80_af.b.h); // catch "FDFDFD77xx"
								#endif
								break;
							case 0x7C: // LD A,XH
								z80_af.b.h=xy->b.h;
								break;
							case 0x7D: // LD A,XL
								z80_af.b.h=xy->b.l;
								break;
							case 0x7E: // LD A,(IX+$XX)
								Z80_WZ_XY_1X(5); z80_af.b.h=Z80_PEEK(z80_wz);
								break;
							// 0xDD80-0xDDBF
							case 0x84: // ADD XH
								Z80_ADD1(xy->b.h);
								break;
							case 0x85: // ADD XL
								Z80_ADD1(xy->b.l);
								break;
							case 0x86: // ADD (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_ADD1(b); }
								break;
							case 0x8C: // ADC XH
								Z80_ADC1(xy->b.h);
								break;
							case 0x8D: // ADC XL
								Z80_ADC1(xy->b.l);
								break;
							case 0x8E: // ADC (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_ADC1(b); }
								break;
							case 0x94: // SUB XH
								Z80_SUB1(xy->b.h);
								break;
							case 0x95: // SUB XL
								Z80_SUB1(xy->b.l);
								break;
							case 0x96: // SUB (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_SUB1(b); }
								break;
							case 0x9C: // SBC XH
								Z80_SBC1(xy->b.h);
								break;
							case 0x9D: // SBC XL
								Z80_SBC1(xy->b.l);
								break;
							case 0x9E: // SBC (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_SBC1(b); }
								break;
							case 0xA4: // AND XH
								Z80_AND1(xy->b.h);
								break;
							case 0xA5: // AND XL
								Z80_AND1(xy->b.l);
								break;
							case 0xA6: // AND (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_AND1(b); }
								break;
							case 0xAC: // XOR XH
								Z80_XOR1(xy->b.h);
								break;
							case 0xAD: // XOR XL
								Z80_XOR1(xy->b.l);
								break;
							case 0xAE: // XOR (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_XOR1(b); }
								break;
							case 0xB4: // OR XH
								Z80_OR1(xy->b.h);
								break;
							case 0xB5: // OR XL
								Z80_OR1(xy->b.l);
								break;
							case 0xB6: // OR (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_OR1(b); }
								break;
							case 0xBC: // CP XH
								Z80_CP1(xy->b.h);
								break;
							case 0xBD: // CP XL
								Z80_CP1(xy->b.l);
								break;
							case 0xBE: // CP (IX+$XX)
								{ Z80_WZ_XY_1X(5); Z80_RD_WZ; Z80_CP1(b); }
								break;
							// 0xDDC0-0xDDFF
							case 0xC0: // *RET NZ
								Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x40))
								{
									Z80_STRIDE(0x1C0);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xC8: // *RET Z
								Z80_WAIT_IR1X(1); if (z80_af.b.l&0x40)
								{
									Z80_STRIDE(0x1C8);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xD0: // *RET NC
								Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x01))
								{
									Z80_STRIDE(0x1D0);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xD8: // *RET C
								Z80_WAIT_IR1X(1); if (z80_af.b.l&0x01)
								{
									Z80_STRIDE(0x1D8);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xE0: // *RET NV
								Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x04))
								{
									Z80_STRIDE(0x1E0);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xE8: // *RET V
								Z80_WAIT_IR1X(1); if (z80_af.b.l&0x04)
								{
									Z80_STRIDE(0x1E8);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xF0: // *RET NS
								Z80_WAIT_IR1X(1); if (!(z80_af.b.l&0x80))
								{
									Z80_STRIDE(0x1F0);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xF8: // *RET S
								Z80_WAIT_IR1X(1); if (z80_af.b.l&0x80)
								{
									Z80_STRIDE(0x1F8);
									Z80_RET2;
									//Z80_TRDOS_ENTER(z80_pc); // unused
								}
								else
									Z80_STRIDE_1;
								break;
							case 0xE1: // POP IX
								Z80_POP2(xy->b);
								break;
							case 0xE5: // PUSH IX
								Z80_WAIT_IR1X(1);
								Z80_PUSH2(xy->b,0x1E5);// ==0X7E5
								break;
							case 0xE3: // EX IX,(SP)
								z80_wz=Z80_PEEK(z80_sp.w);
								++z80_sp.w;
								z80_wz+=Z80_PEEKZ(z80_sp.w)<<8;
								Z80_POKE5(z80_sp.w,xy->b.h);
								Z80_STRIDE(0x1E3);// ==0X7E3
								--z80_sp.w;
								Z80_POKE6(z80_sp.w,xy->b.l);
								xy->w=z80_wz;
								Z80_IORQ_1X_NEXT(2);
								Z80_STRIDE_1;
								break;
							case 0xE9: // JP IX
								z80_pc.w=xy->w;
								//Z80_TRDOS_ENTER(z80_pc); // unused
								break;
							case 0xF9: // LD SP,IX
								z80_sp.w=xy->w;
								Z80_WAIT_IR1X(2);
								Z80_STRIDE_1;
								break;
							case 0xCB: // PREFIX: XYCB SUBSET
								{
									Z80_WZ_XY;
									o=Z80_RD_PC; ++z80_pc.w;
									Z80_STRIDE(o+0x500);
									Z80_IORQ_1X_NEXT(2);
									Z80_RD_WZ; Z80_IORQ_NEXT(1);
									// the XYCB set is extremely repetitive and thus worth abridging
									#define CASE_Z80_XYCB_OP1(xx,yy) \
										case xx+0: yy; Z80_WR_WZ; z80_bc.b.h=b; break; case xx+1: yy; Z80_WR_WZ; z80_bc.b.l=b; break; \
										case xx+2: yy; Z80_WR_WZ; z80_de.b.h=b; break; case xx+3: yy; Z80_WR_WZ; z80_de.b.l=b; break; \
										case xx+4: yy; Z80_WR_WZ; z80_hl.b.h=b; break; case xx+5: yy; Z80_WR_WZ; z80_hl.b.l=b; break; \
										case xx+7: yy; Z80_WR_WZ; z80_af.b.h=b; break; case xx+6: yy; Z80_WR_WZ; break
									#define CASE_Z80_XYCB_BIT(xx,yy) \
										case xx+0: case xx+1: case xx+2: case xx+3: case xx+4: case xx+5: case xx+7: \
										case xx+6: Z80_BIT1(yy,b,z80_wz>>8); break;
									switch (o)
									{
										// 0xDDCBXX00-0xDDCBXX3F
										CASE_Z80_XYCB_OP1(0x00,Z80_RLC1(b)); // RLC (IX+$XX)
										CASE_Z80_XYCB_OP1(0x08,Z80_RRC1(b)); // RRC (IX+$XX)
										CASE_Z80_XYCB_OP1(0x10,Z80_RL1 (b)); // RL  (IX+$XX)
										CASE_Z80_XYCB_OP1(0x18,Z80_RR1 (b)); // RR  (IX+$XX)
										CASE_Z80_XYCB_OP1(0x20,Z80_SLA1(b)); // SLA (IX+$XX)
										CASE_Z80_XYCB_OP1(0x28,Z80_SRA1(b)); // SRA (IX+$XX)
										CASE_Z80_XYCB_OP1(0x30,Z80_SLL1(b)); // SLL (IX+$XX)
										CASE_Z80_XYCB_OP1(0x38,Z80_SRL1(b)); // SRL (IX+$XX)
										// 0xDDCBXX40-0xDDCBXX7F
										CASE_Z80_XYCB_BIT(0x40,0); // BIT 0,(IX+$XX)
										CASE_Z80_XYCB_BIT(0x48,1); // BIT 1,(IX+$XX)
										CASE_Z80_XYCB_BIT(0x50,2); // BIT 2,(IX+$XX)
										CASE_Z80_XYCB_BIT(0x58,3); // BIT 3,(IX+$XX)
										CASE_Z80_XYCB_BIT(0x60,4); // BIT 4,(IX+$XX)
										CASE_Z80_XYCB_BIT(0x68,5); // BIT 5,(IX+$XX)
										CASE_Z80_XYCB_BIT(0x70,6); // BIT 6,(IX+$XX)
										CASE_Z80_XYCB_BIT(0x78,7); // BIT 7,(IX+$XX)
										// 0xDDCBXX80-0xDDCBXXBF
										CASE_Z80_XYCB_OP1(0x80,Z80_RES1(0,b)); // RES 0,(IX+$XX)
										CASE_Z80_XYCB_OP1(0x88,Z80_RES1(1,b)); // RES 1,(IX+$XX)
										CASE_Z80_XYCB_OP1(0x90,Z80_RES1(2,b)); // RES 2,(IX+$XX)
										CASE_Z80_XYCB_OP1(0x98,Z80_RES1(3,b)); // RES 3,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xA0,Z80_RES1(4,b)); // RES 4,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xA8,Z80_RES1(5,b)); // RES 5,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xB0,Z80_RES1(6,b)); // RES 6,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xB8,Z80_RES1(7,b)); // RES 7,(IX+$XX)
										// 0xDDCBXXC0-0xDDCBXXFF
										CASE_Z80_XYCB_OP1(0xC0,Z80_SET1(0,b)); // SET 0,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xC8,Z80_SET1(1,b)); // SET 1,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xD0,Z80_SET1(2,b)); // SET 2,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xD8,Z80_SET1(3,b)); // SET 3,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xE0,Z80_SET1(4,b)); // SET 4,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xE8,Z80_SET1(5,b)); // SET 5,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xF0,Z80_SET1(6,b)); // SET 6,(IX+$XX)
										CASE_Z80_XYCB_OP1(0xF8,Z80_SET1(7,b)); // SET 7,(IX+$XX)
									}
									#undef CASE_Z80_XYCB_OP1
									#undef CASE_Z80_XYCB_BIT
									break;
								}
							#ifdef Z80_DNTR_0XFD
							case 0xFD: // ILLEGAL FDXX used by DANDANATOR
								if (xy==&z80_iy) Z80_DNTR_0XFD(z80_pc.w); // detect "...FDFDFD..."
								// no `break`! it's still an ILLEGAL opcode!
							#endif
							default: // ILLEGAL DDXX/FDXX!
								z80_active=0; // delay interruption (if any)
								--r7; --z80_pc.w; // undo increases!!
								// Z80_STRIDE(o) MUST BE ZERO if `o` is illegal!!
								Z80_REWIND; // terrible hack to undo the opcode fetching!
						}
					}
					break;
				case 0xED: // PREFIX: ED SUBSET
					o=Z80_FETCH; ++r7; ++z80_pc.w;
					Z80_STRIDE(o+0x200);
					switch (o)
					{
						// 0xED00-0xED3F
						// 0xED40-0xED7F
						case 0x40: // IN B,(C)
							Z80_IN2(z80_bc.b.h,0x340);
							break;
						case 0x48: // IN C,(C)
							Z80_IN2(z80_bc.b.l,0x348);
							break;
						case 0x50: // IN D,(C)
							Z80_IN2(z80_de.b.h,0x350);
							break;
						case 0x58: // IN E,(C)
							Z80_IN2(z80_de.b.l,0x358);
							break;
						case 0x60: // IN H,(C)
							Z80_IN2(z80_hl.b.h,0x360);
							break;
						case 0x68: // IN L,(C)
							Z80_IN2(z80_hl.b.l,0x368);
							break;
						case 0x70: // IN (C)
							Z80_IN2(o,0x370); // dummy!
							break;
						case 0x78: // IN A,(C)
							Z80_IN2(z80_af.b.h,0x378);
							break;
						case 0x41: // OUT (C),B
							Z80_OUT2(z80_bc.b.h,0x341);
							break;
						case 0x49: // OUT (C),C
							Z80_OUT2(z80_bc.b.l,0x349);
							break;
						case 0x51: // OUT (C),D
							Z80_OUT2(z80_de.b.h,0x351);
							break;
						case 0x59: // OUT (C),E
							Z80_OUT2(z80_de.b.l,0x359);
							break;
						case 0x61: // OUT (C),H
							Z80_OUT2(z80_hl.b.h,0x361);
							break;
						case 0x69: // OUT (C),L
							Z80_OUT2(z80_hl.b.l,0x369);
							break;
						case 0x71: // OUT (C)
							Z80_OUT2(Z80_0XED71,0x371);
							break;
						case 0x79: // OUT (C),A
							Z80_OUT2(z80_af.b.h,0x379);
							break;
						case 0x42: // SBC HL,BC
							Z80_SBC2(z80_bc);
							break;
						case 0x52: // SBC HL,DE
							Z80_SBC2(z80_de);
							break;
						case 0x62: // SBC HL,HL
							Z80_SBC2(z80_hl);
							break;
						case 0x72: // SBC HL,SP
							Z80_SBC2(z80_sp);
							break;
						case 0x4A: // ADC HL,BC
							Z80_ADC2(z80_bc);
							break;
						case 0x5A: // ADC HL,DE
							Z80_ADC2(z80_de);
							break;
						case 0x6A: // ADC HL,HL
							Z80_ADC2(z80_hl);
							break;
						case 0x7A: // ADC HL,SP
							Z80_ADC2(z80_sp);
							break;
						case 0x43: // LD ($NNNN),BC
							Z80_WR2(z80_bc.b,0X343);
							break;
						case 0x53: // LD ($NNNN),DE
							Z80_WR2(z80_de.b,0X353);
							break;
						case 0x63: // *LD ($NNNN),HL
							Z80_WR2(z80_hl.b,0X363);
							break;
						case 0x73: // LD ($NNNN),SP
							Z80_WR2(z80_sp.b,0X373); // Z80_Q_RST(); // overkill!
							break;
						case 0x4B: // LD BC,($NNNN)
							Z80_RD2(z80_bc.b);
							break;
						case 0x5B: // LD DE,($NNNN)
							Z80_RD2(z80_de.b);
							break;
						case 0x6B: // *LD HL,($NNNN)
							Z80_RD2(z80_hl.b);
							break;
						case 0x7B: // LD SP,($NNNN)
							Z80_RD2(z80_sp.b); Z80_Q_RST();
							break;
						case 0x44: // NEG
						case 0x4C: // *NEG
						case 0x54: // *NEG
						case 0x5C: // *NEG
						case 0x64: // *NEG
						case 0x6C: // *NEG
						case 0x74: // *NEG
						case 0x7C: // *NEG
							{ BYTE b=z80_af.b.h; z80_af.b.h=0; Z80_SUB1(b); }
							break;
						case 0x45: // RETN
						case 0x55: // *RETN
						case 0x65: // *RETN
						case 0x75: // *RETN
						case 0x4D: // RETI
						case 0x5D: // *RETI
						case 0x6D: // *RETI
						case 0x7D: // *RETI
							z80_active=z80_iff.b.l; // delay interruptions if IFF1 was false; reported by TonyB and ZjoyKiler
							#ifdef Z80_NMI_ACK
							z80_iff.b.l=z80_iff.b.h; // NMI! the only real difference between RETI and RETN is that the hardware can check the opcode's bit 3
							#endif
							Z80_RET2;
							Z80_TRDOS_ENTER(z80_pc); // see IM 2 INT event above
							break;
						case 0x46: // IM 0
						case 0x4E: // *IM 0
						case 0x66: // *IM 0
						case 0x6E: // *IM 0
							z80_imd=0;
							break;
						case 0x56: // IM 1
						case 0x76: // *IM 1
							z80_imd=1;
							break;
						case 0x5E: // IM 2
						case 0x7E: // *IM 2
							z80_imd=2;
							break;
						case 0x47: // LD I,A
							Z80_WAIT_IR1X(1); // memory contention needs this to happen first!
							z80_ir.b.h=z80_af.b.h;
							Z80_STRIDE_1;
							break;
						case 0x4F: // LD R,A
							Z80_WAIT_IR1X(1); // ditto!
							r7=z80_ir.b.l=z80_af.b.h;
							Z80_STRIDE_1;
							break;
						case 0x57: // LD A,I
							Z80_WAIT_IR1X(1);
							//Z80_SYNC(); // early releases of the Z80 had a bug: the chip preemptively disabled INTs when accepting IRQs; it would show here!
							Z80_Q_SET((z80_af.b.l&1)+z80_flags_sgn[z80_af.b.h=z80_ir.b.h]+((z80_iff.b.l&&!z80_irq)?4:0)); // SZ000V0-
							Z80_STRIDE_1;
							break;
						case 0x5F: // LD A,R
							Z80_WAIT_IR1X(1);
							//Z80_SYNC(); // ditto! this quirk appeared in early Spectrum machines, but it never happened in any CPC models AFAIK!
							Z80_Q_SET((z80_af.b.l&1)+z80_flags_sgn[z80_af.b.h=Z80_GET_R8]+((z80_iff.b.l&&!z80_irq)?4:0)); // SZ000V0-
							Z80_STRIDE_1;
							break;
						#if 0 // limited to some Z80 clones!?
						case 0x77: // *LD I
						case 0x7F: // *LD R
							Z80_Q_SET((z80_af.b.l&0xFB)+(z80_iff.b.l?4:0)); // -----V--
							break;
						#endif
						case 0x67: // RRD
							{
								BYTE b=Z80_PEEK(z80_hl.w),z=(b>>4)+(z80_af.b.h<<4);
								Z80_IORQ_1X_NEXT(4);
								Z80_PEEKPOKE(z80_hl.w,z);
								z80_af.b.h=(z80_af.b.h&240)+(b&15);
								Z80_Q_SET(z80_flags_xor[z80_af.b.h]|(z80_af.b.l&1));
								z80_wz=z80_hl.w+1;
							}
							break;
						case 0x6F: // RLD
							{
								BYTE b=Z80_PEEK(z80_hl.w),z=(b<<4)+(z80_af.b.h&15);
								Z80_IORQ_1X_NEXT(4);
								Z80_PEEKPOKE(z80_hl.w,z);
								z80_af.b.h=(z80_af.b.h&240)+(b>>4);
								Z80_Q_SET(z80_flags_xor[z80_af.b.h]|(z80_af.b.l&1));
								z80_wz=z80_hl.w+1;
							}
							break;
						// 0xED80-0xEDBF
						case 0xB0: // LDIR
							if (z80_bc.w!=1) // cfr. Hoglet67's article "LDxR/CPxR interrupted"
							{
								BYTE b=Z80_PEEK(z80_hl.w); Z80_POKE(z80_de.w,b);
								++z80_hl.w,++z80_de.w;
								Z80_IORQ_1X_NEXT(2);
								z80_wz=--z80_pc.w; --z80_pc.w; --z80_bc.w;
								Z80_Q_SET((z80_af.b.l&0XC1)+(z80_pc.b.h&0X28)+4);
								Z80_IORQ_1X_NEXT(5);
								Z80_STRIDE(0x3B0);
								Z80_STRIDE_1;
							}
							else // no `break`!
						case 0xA0: // LDI
							{
								BYTE b=Z80_PEEK(z80_hl.w); Z80_POKE(z80_de.w,b);
								++z80_hl.w,++z80_de.w;
								Z80_IORQ_1X_NEXT(2);
								b+=z80_af.b.h;
								Z80_Q_SET((z80_af.b.l&0xC1)+(b&8)+((b&2)<<4)+(--z80_bc.w?4:0));
								Z80_STRIDE(0x3A0);
								Z80_STRIDE_1;
							}
							break;
						case 0xB8: // LDDR
							if (z80_bc.w!=1) // cfr. Hoglet67's article "LDxR/CPxR interrupted"
							{
								BYTE b=Z80_PEEK(z80_hl.w); Z80_POKE(z80_de.w,b);
								--z80_hl.w,--z80_de.w;
								Z80_IORQ_1X_NEXT(2);
								z80_wz=--z80_pc.w; --z80_pc.w; --z80_bc.w;
								Z80_Q_SET((z80_af.b.l&0XC1)+(z80_pc.b.h&0X28)+4);
								Z80_IORQ_1X_NEXT(5);
								Z80_STRIDE(0x3B8);
								Z80_STRIDE_1;
							}
							else // no `break`!
						case 0xA8: // LDD
							{
								BYTE b=Z80_PEEK(z80_hl.w); Z80_POKE(z80_de.w,b);
								--z80_hl.w,--z80_de.w;
								Z80_IORQ_1X_NEXT(2);
								b+=z80_af.b.h;
								Z80_Q_SET((z80_af.b.l&0xC1)+(b&8)+((b&2)<<4)+(--z80_bc.w?4:0));
								Z80_STRIDE(0x3A8);
								Z80_STRIDE_1;
							}
							break;
						// these last opcodes are rare yet repetitive and thus worth collapsing
						case 0xA1: // CPI
						case 0xA9: // CPD
						case 0xB1: // CPIR
						case 0xB9: // CPDR
							{
								BYTE b=Z80_PEEK(z80_hl.w),z=z80_af.b.h-b;
								z80_af.b.l=((z^z80_af.b.h^b)&0x10)+(z?(z&0x80):0x40)+(--z80_bc.w?6:2)+(z80_af.b.l&1);
								Z80_IORQ_1X_NEXT(5);
								if (o&8)
									--z80_hl.w,--z80_wz;
								else
									++z80_hl.w,++z80_wz;
								if ((o&16)&&((z80_af.b.l&0x44)==0X04)) // cfr. Hoglet67's article "LDxR/CPxR interrupted"
								{
									z80_wz=--z80_pc.w; --z80_pc.w;
									Z80_Q_SET(z80_af.b.l+(z80_pc.b.h&0X28));
									Z80_STRIDE(0x3B1); // ==0X3B9
									Z80_IORQ_1X_NEXT(5);
									Z80_STRIDE_1;
								}
								else
									b=z-((z80_af.b.l>>4)&1),Z80_Q_SET(z80_af.b.l+(b&8)+((b&2)<<4)); // ZS5H3V1-
							}
							break;
						case 0xA2: // INI
						case 0xAA: // IND
						case 0xB2: // INIR
						case 0xBA: // INDR
						case 0xA3: // OUTI
						case 0xAB: // OUTD
						case 0xB3: // OTIR
						case 0xBB: // OTDR
							{
								BYTE b,z; Z80_WAIT_IR1X(1);
								if (o&1) // OUTI...
								{
									--z80_bc.b.h; // B decreases BEFORE the OUTPUT!
									z80_wz=z80_bc.w; b=Z80_PEEK(z80_hl.w); Z80_PRAE_SEND(z80_wz); Z80_SEND(z80_wz,b); Z80_POST_SEND(z80_wz);
									Z80_STRIDE_IO(0X3A3); // ==0X3AB
									if (o&8)
										--z80_hl.w,--z80_wz;
									else
										++z80_hl.w,++z80_wz;
									z=b+z80_hl.b.l; // unlike INI et al. the value of `z` does NOT reflect INC/DEC!
								}
								else // INI...
								{
									z80_wz=z80_bc.w; Z80_PRAE_RECV(z80_wz); b=Z80_RECV(z80_wz); Z80_POST_RECV(z80_wz); Z80_POKE(z80_hl.w,b);
									Z80_STRIDE_IO(0x3A2); // ==0X3AA
									--z80_bc.b.h; // B decreases AFTER the INPUT!
									if (o&8)
										z=b+z80_bc.b.l-1,--z80_hl.w,--z80_wz;
									else
										z=b+z80_bc.b.l+1,++z80_hl.w,++z80_wz;
								}
								z80_af.b.l=(z80_flags_xor[z80_bc.b.h]&0xE8)+(z<b?1:0)+(b&0x80?2:0); // ZS5-3-NC
								if ((o&16)&&z80_bc.b.h) // cfr. Hoglet67's article "INxR/OTxR interrupted"
								{
									Z80_STRIDE(0x3B2); // ==0X3B3 ==0X3BA ==0X3BB
									Z80_IORQ_1X_NEXT(5);
									if (z80_af.b.l&1)
										if (z80_af.b.l&2)
											z80_af.b.l+=(~(z80_flags_xor[(z&7)^z80_bc.b.h]^z80_flags_xor[((z80_bc.b.h-1)&7)])&4)
												+((z80_bc.b.h&0X0F)?0:16);
										else
											z80_af.b.l+=(~(z80_flags_xor[(z&7)^z80_bc.b.h]^z80_flags_xor[((z80_bc.b.h+1)&7)])&4)
												+((~z80_bc.b.h&0X0F)?0:16);
									else
										z80_af.b.l+=~(z80_flags_xor[(z&7)^z80_bc.b.h]^z80_flags_xor[(z80_bc.b.h&7)])&4;
									Z80_Q_SET((z80_af.b.l&0XD7)+(((z80_pc.w-=2)>>8)&0X28)); // --5-3--- from PC
								}
								else
									Z80_Q_SET(z80_af.b.l+(z80_flags_xor[(z&7)^z80_bc.b.h]&4)+((z80_af.b.l&1)<<4));
							}
							break;
						// 0xEDC0-0xEDFF
						#ifdef DEBUG_HERE
						case 0xFF: // WINAPE-LIKE $EDFF BREAKPOINT
							if (debug_break)
								{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
							// no `break`!
						#endif
						//default: // ILLEGAL EDXX!
					}
					break;
			}
		}
		#ifdef DEBUG_HERE
		if (UNLIKELY(debug_point[z80_pc.w]))
		{
			if (debug_point[z80_pc.w]&8) // log byte?
			{
				int z; switch (debug_point[z80_pc.w]&7)
				{
					// Z80 8-bit "BCDEHLFA" order
					case  0: z=z80_bc.b.h; break;
					case  1: z=z80_bc.b.l; break;
					case  2: z=z80_de.b.h; break;
					case  3: z=z80_de.b.l; break;
					case  4: z=z80_hl.b.h; break;
					case  5: z=z80_hl.b.l; break;
					case  6: z=z80_af.b.l; break;
					default: z=z80_af.b.h; break;
				}
				if (debug_logbyte(z))
					debug_point[z80_pc.w]=0; // remove breakpoint on failure!
			}
			else // pure breakpoint?
				{ _t_=0,session_signal|=SESSION_SIGNAL_DEBUG; } // throw!
		}
		#endif
	}
	while (z80_t<_t_);
	z80_ir.b.l=Z80_GET_R8; // unify R7+R8!
	z80_sync(z80_t); // flush accumulated T!
}

// Z80 disassembler ------------------------------------------------- //

#ifdef DEBUG_HERE

char *debug_list(void) { return "BCDEHLFA"; }
void debug_jump(WORD w) { z80_pc.w=w; }
WORD debug_this(void) { return z80_pc.w; }
WORD debug_that(void) { return z80_sp.w; }
void debug_poke(WORD w,BYTE b) { POKE(w)=b; } // send the byte `b` to memory address `w`
BYTE debug_peek(BYTE q,WORD w) { return q?POKE(w):PEEK(w); } // read one byte from `w` as seen from the R/W mode `q`
void debug_step(void) { z80_main(0); debug_reset(); } // run one operation
void debug_fall(void) { debug_trap_sp=z80_sp.w; session_signal&=~SESSION_SIGNAL_DEBUG; } // set a RETURN trap
void debug_drop(WORD w) { debug_point[debug_trap_pc=w]|=128,session_signal&=~SESSION_SIGNAL_DEBUG; }
void debug_leap(void) // run UNTIL meeting the next operation
{
	BYTE q=0; switch (PEEK(z80_pc.w))
	{
		case 0X10: q=2; break; // DJNZ $RRRR
		case 0XCD: // CALL $NNNN
		case 0XC4: case 0XCC: case 0XD4: case 0XDC: case 0XE4: case 0XEC: case 0XF4: case 0XFC: q=3; break; // CALL NZ/Z/NC/C/NV/V/NS/S,$NNNN
		case 0X76: // HALT
		case 0XC7: case 0XCF: case 0XD7: case 0XDF: case 0XE7: case 0XEF: case 0XF7: case 0XFF: q=1; break; // RST 0/1/2/3/4/5/6/7
		case 0XED: if ((q=PEEK((z80_pc.w+1)))>=0xB0&&q<0xC0&&!(q&4)) q=2; else q=0; break; // LDIR/CPIR/INIR/OTIR+LDDR/CPDR/INDR/OTDR
	}
	if (q)
		debug_drop(z80_pc.w+q);
	else
		debug_step(); // just run one operation
}

void debug_regs(char *t,int i) // prints register information #`i` onto the buffer `t`
{
	if (debug_panel1_w==i) // validate `debug_panel1_w` and `debug_panel1_x` (see below)
	{
		if (debug_panel1_w< 1) debug_panel1_w= 1;
		if (debug_panel1_w>14) debug_panel1_w=14;
		if (debug_panel1_x< 0) debug_panel1_x= 0;
		if (debug_panel1_x> 3) debug_panel1_x= 3;
	}
	switch (i)
	{
		case  0:
			*t++=(z80_af.b.l&128)?'S':'-'; *t++=(z80_af.b.l& 64)?'Z':'-'; *t++=(z80_af.b.l& 32)?'Y':'-'; *t++=(z80_af.b.l& 16)?'H':'-'; *t++=':';
			*t++=(z80_af.b.l&  8)?'X':'-'; *t++=(z80_af.b.l&  4)?'V':'-'; *t++=(z80_af.b.l&  2)?'N':'-'; *t++=(z80_af.b.l&  1)?'C':'-'; *t=0; break;
		case  1: sprintf(t,"PC = %04X",z80_pc .w); break;
		case  2: sprintf(t,"AF = %04X",z80_af .w); break;
		case  3: sprintf(t,"BC = %04X",z80_bc .w); break;
		case  4: sprintf(t,"DE = %04X",z80_de .w); break;
		case  5: sprintf(t,"HL = %04X",z80_hl .w); break;
		case  6: sprintf(t,"AF'= %04X",z80_af2.w); break;
		case  7: sprintf(t,"BC'= %04X",z80_bc2.w); break;
		case  8: sprintf(t,"DE'= %04X",z80_de2.w); break;
		case  9: sprintf(t,"HL'= %04X",z80_hl2.w); break;
		case 10: sprintf(t,"IX = %04X",z80_ix .w); break;
		case 11: sprintf(t,"IY = %04X",z80_iy .w); break;
		case 12: sprintf(t,"IR = %04X",z80_ir .w); break;
		case 13: sprintf(t,"WZ = %04X",z80_wz   ); break;
		case 14: sprintf(t,"SP = %04X",z80_sp .w); break;
		case 15: sprintf(t,"IM %c %cI %c",48+z80_imd,z80_iff.b.l?'E':'D',z80_irq?'*':'-'); break;
		default: *t=0;
	}
}
void debug_regz(BYTE k) // sends the nibble `k` to the valid position `debug_panel1_w`:`debug_panel1_x` and updates the later
{
	int m=~(15<<(12-debug_panel1_x*4)),n=k<<(12-debug_panel1_x*4); switch (debug_panel1_w)
	{
		case  1: z80_pc .w=(z80_pc .w&m)+n; break;
		case  2: z80_af .w=(z80_af .w&m)+n; break;
		case  3: z80_bc .w=(z80_bc .w&m)+n; break;
		case  4: z80_de .w=(z80_de .w&m)+n; break;
		case  5: z80_hl .w=(z80_hl .w&m)+n; break;
		case  6: z80_af2.w=(z80_af2.w&m)+n; break;
		case  7: z80_bc2.w=(z80_bc2.w&m)+n; break;
		case  8: z80_de2.w=(z80_de2.w&m)+n; break;
		case  9: z80_hl2.w=(z80_hl2.w&m)+n; break;
		case 10: z80_ix .w=(z80_ix .w&m)+n; break;
		case 11: z80_iy .w=(z80_iy .w&m)+n; break;
		case 12: z80_ir .w=(z80_ir .w&m)+n; break;
		case 13: z80_wz   =(z80_wz   &m)+n; break;
		case 14: z80_sp .w=(z80_sp .w&m)+n; break;
	}
	if (++debug_panel1_x>3) debug_panel1_x=0;
}
WORD debug_pull(WORD s) // gets an item from the stack `s`
	{ debug_match=z80_sp.w==s; return PEEK((WORD)(s+1))*256+PEEK(s); }
void debug_push(WORD s,WORD w) // puts an item in the stack `s`
	{ POKE((WORD)(s+1))=w>>8; POKE((WORD)(s))=w; }
WORD debug_dasm(char *t,WORD p) // disassembles the code at address `p` onto the buffer `t`; returns the next value of `p`
{
	#define DEBUG_DASM_BYTE (w=PEEK(p),++p,w)
	#define DEBUG_DASM_REL8 (WORD)(x=PEEK(p),++p+x)
	#define DEBUG_DASM_WORD (w=PEEK(p),++p,w+=PEEK(p)<<8,++p,w)
	#define DEBUG_DASM_IXIY (x=PEEK(p),++p,y=x<0?x=-x,'-':'+')
	const char twos[4][3]={"BC","DE","HL","SP"};
	const char regs[8][5]={"B","C","D","E","H","L","(HL)","A"};
	const char alus[8][4]={"ADD","ADC","SUB","SBC","AND","XOR","OR ","CP "};
	const char cbxs[8][4]={"RLC","RRC","RL ","RR ","SLA","SRA","SLL","SRL"};
	const char flgs[8][3]={"NZ","Z","NC","C","NV","V","NS","S"};
	debug_match=p==z80_pc.w; WORD w; char x,y,*z; BYTE o=PEEK(p); switch (++p,o)
	{
		case 0X00: sprintf(t,"NOP"); break;
		case 0X08: sprintf(t,"EX   AF,AF'"); break;
		case 0X10: sprintf(t,"DJNZ $%04X",DEBUG_DASM_REL8); break;
		case 0X18: sprintf(t,"JR   $%04X",DEBUG_DASM_REL8); break;
		case 0X20: case 0X28: case 0X30: case 0X38:
			sprintf(t,"JR   %s,$%04X",flgs[(o-0X20)>>3],DEBUG_DASM_REL8); break;
		case 0X01: case 0X11: case 0X21: case 0X31:
			sprintf(t,"LD   %s,$%04X",twos[o>>4],DEBUG_DASM_WORD); break;
		case 0X06: case 0X0E: case 0X16: case 0X1E: case 0X26: case 0X2E: case 0X36: case 0X3E:
			sprintf(t,"LD   %s,$%02X",regs[o>>3],DEBUG_DASM_BYTE); break;
		case 0X03: case 0X13: case 0X23: case 0X33:
			sprintf(t,"INC  %s",twos[o>>4]); break;
		case 0X0B: case 0X1B: case 0X2B: case 0X3B:
			sprintf(t,"DEC  %s",twos[o>>4]); break;
		case 0X04: case 0X0C: case 0X14: case 0X1C: case 0X24: case 0X2C: case 0X34: case 0X3C:
			sprintf(t,"INC  %s",regs[o>>3]); break;
		case 0X05: case 0X0D: case 0X15: case 0X1D: case 0X25: case 0X2D: case 0X35: case 0X3D:
			sprintf(t,"DEC  %s",regs[o>>3]); break;
		case 0X76: sprintf(t,"HALT"); break;
		case 0X40: case 0X41: case 0X42: case 0X43: case 0X44: case 0X45: case 0X46: case 0X47:
		case 0X48: case 0X49: case 0X4A: case 0X4B: case 0X4C: case 0X4D: case 0X4E: case 0X4F:
		case 0X50: case 0X51: case 0X52: case 0X53: case 0X54: case 0X55: case 0X56: case 0X57:
		case 0X58: case 0X59: case 0X5A: case 0X5B: case 0X5C: case 0X5D: case 0X5E: case 0X5F:
		case 0X60: case 0X61: case 0X62: case 0X63: case 0X64: case 0X65: case 0X66: case 0X67:
		case 0X68: case 0X69: case 0X6A: case 0X6B: case 0X6C: case 0X6D: case 0X6E: case 0X6F:
		case 0X70: case 0X71: case 0X72: case 0X73: case 0X74: case 0X75: case 0X77:
		case 0X78: case 0X79: case 0X7A: case 0X7B: case 0X7C: case 0X7D: case 0X7E: case 0X7F:
			sprintf(t,"LD   %s,%s",regs[(o-0X40)>>3],regs[o&7]); break;
		case 0X80: case 0X81: case 0X82: case 0X83: case 0X84: case 0X85: case 0X86: case 0X87:
		case 0X88: case 0X89: case 0X8A: case 0X8B: case 0X8C: case 0X8D: case 0X8E: case 0X8F:
		case 0X90: case 0X91: case 0X92: case 0X93: case 0X94: case 0X95: case 0X96: case 0X97:
		case 0X98: case 0X99: case 0X9A: case 0X9B: case 0X9C: case 0X9D: case 0X9E: case 0X9F:
		case 0XA0: case 0XA1: case 0XA2: case 0XA3: case 0XA4: case 0XA5: case 0XA6: case 0XA7:
		case 0XA8: case 0XA9: case 0XAA: case 0XAB: case 0XAC: case 0XAD: case 0XAE: case 0XAF:
		case 0XB0: case 0XB1: case 0XB2: case 0XB3: case 0XB4: case 0XB5: case 0XB6: case 0XB7:
		case 0XB8: case 0XB9: case 0XBA: case 0XBB: case 0XBC: case 0XBD: case 0XBE: case 0XBF:
			sprintf(t,"%s  %s",alus[(o-0X80)>>3],regs[o&7]); break;
		case 0XC6: case 0XCE: case 0XD6: case 0XDE: case 0XE6: case 0XEE: case 0XF6: case 0XFE:
			sprintf(t,"%s  $%02X",alus[(o-0XC0)>>3],DEBUG_DASM_BYTE); break;
		case 0XC7: case 0XCF: case 0XD7: case 0XDF: case 0XE7: case 0XEF: case 0XF7: case 0XFF:
			sprintf(t,"RST  %d",(o-0XC0)>>3); break;
		case 0XC9: sprintf(t,"RET"); break;
		case 0XF3: sprintf(t,"DI"); break;
		case 0XFB: sprintf(t,"EI"); break;
		case 0X02: case 0X12: sprintf(t,"LD   (%s),A",twos[o>>4]); break;
		case 0X22: case 0X32: sprintf(t,"LD   ($%04X),%s",DEBUG_DASM_WORD,o&16?"A":"HL"); break;
		case 0X0A: case 0X1A: sprintf(t,"LD   A,(%s)",twos[o>>4]); break;
		case 0X2A: case 0X3A: sprintf(t,"LD   %s,($%04X)",o&16?"A":"HL",DEBUG_DASM_WORD); break;
		case 0X09: case 0X19: case 0X29: case 0X39: sprintf(t,"ADD  HL,%s",twos[o>>4]); break;
		case 0X07: sprintf(t,"RLCA"); break;
		case 0X0F: sprintf(t,"RRCA"); break;
		case 0X17: sprintf(t,"RLA"); break;
		case 0X1F: sprintf(t,"RRA"); break;
		case 0X27: sprintf(t,"DAA"); break;
		case 0X2F: sprintf(t,"CPL"); break;
		case 0X37: sprintf(t,"SCF"); break;
		case 0X3F: sprintf(t,"CCF"); break;
		case 0XC0: case 0XC8: case 0XD0: case 0XD8: case 0XE0: case 0XE8: case 0XF0: case 0XF8:
			sprintf(t,"RET  %s",flgs[(o-0XC0)>>3]); break;
		case 0XC1: case 0XD1: case 0XE1: sprintf(t,"POP  %s",twos[(o-0XC0)>>4]); break;
		case 0XF1: sprintf(t,"POP  AF"); break;
		case 0XC5: case 0XD5: case 0XE5: sprintf(t,"PUSH %s",twos[(o-0XC0)>>4]); break;
		case 0XF5: sprintf(t,"PUSH AF"); break;
		case 0XC2: case 0XCA: case 0XD2: case 0XDA: case 0XE2: case 0XEA: case 0XF2: case 0XFA:
			sprintf(t,"JP   %s,$%04X",flgs[(o-0XC0)>>3],DEBUG_DASM_WORD); break;
		case 0XC4: case 0XCC: case 0XD4: case 0XDC: case 0XE4: case 0XEC: case 0XF4: case 0XFC:
			sprintf(t,"CALL %s,$%04X",flgs[(o-0XC0)>>3],DEBUG_DASM_WORD); break;
		case 0XC3: sprintf(t,"JP   $%04X",DEBUG_DASM_WORD); break;
		case 0XCD: sprintf(t,"CALL $%04X",DEBUG_DASM_WORD); break;
		case 0XD3: sprintf(t,"OUT  ($%02X),A",DEBUG_DASM_BYTE); break;
		case 0XDB: sprintf(t,"IN   A,($%02X)",DEBUG_DASM_BYTE); break;
		case 0XD9: sprintf(t,"EXX"); break;
		case 0XE3: sprintf(t,"EX   HL,(SP)"); break;
		case 0XE9: sprintf(t,"JP   HL"); break;
		case 0XEB: sprintf(t,"EX   DE,HL"); break;
		case 0XF9: sprintf(t,"LD   SP,HL"); break;
		case 0XCB: switch (o=PEEK(p),++p,o)
			{
				case 0X00: case 0X01: case 0X02: case 0X03: case 0X04: case 0X05: case 0X06: case 0X07:
				case 0X08: case 0X09: case 0X0A: case 0X0B: case 0X0C: case 0X0D: case 0X0E: case 0X0F:
				case 0X10: case 0X11: case 0X12: case 0X13: case 0X14: case 0X15: case 0X16: case 0X17:
				case 0X18: case 0X19: case 0X1A: case 0X1B: case 0X1C: case 0X1D: case 0X1E: case 0X1F:
				case 0X20: case 0X21: case 0X22: case 0X23: case 0X24: case 0X25: case 0X26: case 0X27:
				case 0X28: case 0X29: case 0X2A: case 0X2B: case 0X2C: case 0X2D: case 0X2E: case 0X2F:
				case 0X30: case 0X31: case 0X32: case 0X33: case 0X34: case 0X35: case 0X36: case 0X37:
				case 0X38: case 0X39: case 0X3A: case 0X3B: case 0X3C: case 0X3D: case 0X3E: case 0X3F:
					sprintf(t,"%s  %s",cbxs[o>>3],regs[o&7]); break;
				case 0X40: case 0X41: case 0X42: case 0X43: case 0X44: case 0X45: case 0X46: case 0X47:
				case 0X48: case 0X49: case 0X4A: case 0X4B: case 0X4C: case 0X4D: case 0X4E: case 0X4F:
				case 0X50: case 0X51: case 0X52: case 0X53: case 0X54: case 0X55: case 0X56: case 0X57:
				case 0X58: case 0X59: case 0X5A: case 0X5B: case 0X5C: case 0X5D: case 0X5E: case 0X5F:
				case 0X60: case 0X61: case 0X62: case 0X63: case 0X64: case 0X65: case 0X66: case 0X67:
				case 0X68: case 0X69: case 0X6A: case 0X6B: case 0X6C: case 0X6D: case 0X6E: case 0X6F:
				case 0X70: case 0X71: case 0X72: case 0X73: case 0X74: case 0X75: case 0X76: case 0X77:
				case 0X78: case 0X79: case 0X7A: case 0X7B: case 0X7C: case 0X7D: case 0X7E: case 0X7F:
					sprintf(t,"BIT  %d,%s",(o-0X40)>>3,regs[o&7]); break;
				case 0X80: case 0X81: case 0X82: case 0X83: case 0X84: case 0X85: case 0X86: case 0X87:
				case 0X88: case 0X89: case 0X8A: case 0X8B: case 0X8C: case 0X8D: case 0X8E: case 0X8F:
				case 0X90: case 0X91: case 0X92: case 0X93: case 0X94: case 0X95: case 0X96: case 0X97:
				case 0X98: case 0X99: case 0X9A: case 0X9B: case 0X9C: case 0X9D: case 0X9E: case 0X9F:
				case 0XA0: case 0XA1: case 0XA2: case 0XA3: case 0XA4: case 0XA5: case 0XA6: case 0XA7:
				case 0XA8: case 0XA9: case 0XAA: case 0XAB: case 0XAC: case 0XAD: case 0XAE: case 0XAF:
				case 0XB0: case 0XB1: case 0XB2: case 0XB3: case 0XB4: case 0XB5: case 0XB6: case 0XB7:
				case 0XB8: case 0XB9: case 0XBA: case 0XBB: case 0XBC: case 0XBD: case 0XBE: case 0XBF:
					sprintf(t,"RES  %d,%s",(o-0X80)>>3,regs[o&7]); break;
				case 0XC0: case 0XC1: case 0XC2: case 0XC3: case 0XC4: case 0XC5: case 0XC6: case 0XC7:
				case 0XC8: case 0XC9: case 0XCA: case 0XCB: case 0XCC: case 0XCD: case 0XCE: case 0XCF:
				case 0XD0: case 0XD1: case 0XD2: case 0XD3: case 0XD4: case 0XD5: case 0XD6: case 0XD7:
				case 0XD8: case 0XD9: case 0XDA: case 0XDB: case 0XDC: case 0XDD: case 0XDE: case 0XDF:
				case 0XE0: case 0XE1: case 0XE2: case 0XE3: case 0XE4: case 0XE5: case 0XE6: case 0XE7:
				case 0XE8: case 0XE9: case 0XEA: case 0XEB: case 0XEC: case 0XED: case 0XEE: case 0XEF:
				case 0XF0: case 0XF1: case 0XF2: case 0XF3: case 0XF4: case 0XF5: case 0XF6: case 0XF7:
				case 0XF8: case 0XF9: case 0XFA: case 0XFB: case 0XFC: case 0XFD: case 0XFE: case 0XFF:
					sprintf(t,"SET  %d,%s",(o-0XC0)>>3,regs[o&7]); break;
			}
			break;
		case 0XED: switch (o=PEEK(p),++p,o)
			{
				case 0X40: case 0X48: case 0X50: case 0X58: case 0X60: case 0X68: case 0X78:
					sprintf(t,"IN   %s,(C)",regs[(o-0X40)>>3]); break;
				case 0X70: sprintf(t,"IN   (C)"); break;
				case 0X41: case 0X49: case 0X51: case 0X59: case 0X61: case 0X69: case 0X79:
					sprintf(t,"OUT  (C),%s",regs[(o-0X40)>>3]); break;
				case 0X71: sprintf(t,"OUT  (C)"); break;
				case 0X42: case 0X52: case 0X62: case 0X72:
					sprintf(t,"SBC  HL,%s",twos[(o-0X40)>>4]); break;
				case 0X4A: case 0X5A: case 0X6A: case 0X7A:
					sprintf(t,"ADC  HL,%s",twos[(o-0X40)>>4]); break;
				case 0X43: case 0X53: case 0X63: case 0X73:
					sprintf(t,"LD   ($%04X),%s",DEBUG_DASM_WORD,twos[(o-0X40)>>4]); break;
				case 0X4B: case 0X5B: case 0X6B: case 0X7B:
					sprintf(t,"LD   %s,($%04X)",twos[(o-0X40)>>4],DEBUG_DASM_WORD); break;
				case 0X44: case 0X4C: case 0X54: case 0X5C: case 0X64: case 0X6C: case 0X74: case 0X7C:
					sprintf(t,"NEG"); break;
				case 0X45: case 0X55: case 0X65: case 0X75: sprintf(t,"RETN"); break;
				case 0X4D: case 0X5D: case 0X6D: case 0X7D: sprintf(t,"RETI"); break;
				case 0X46: case 0X4E: case 0X66: case 0X6E: sprintf(t,"IM   0"); break;
				case 0X56: case 0X76: sprintf(t,"IM   1"); break;
				case 0X5E: case 0X7E: sprintf(t,"IM   2"); break;
				case 0X47: sprintf(t,"LD   I,A"); break;
				case 0X4F: sprintf(t,"LD   R,A"); break;
				case 0X57: sprintf(t,"LD   A,I"); break;
				case 0X5F: sprintf(t,"LD   A,R"); break;
				case 0X67: sprintf(t,"RRD"); break;
				case 0X6F: sprintf(t,"RLD"); break;
				case 0XA0: sprintf(t,"LDI"); break;
				case 0XA8: sprintf(t,"LDD"); break;
				case 0XB0: sprintf(t,"LDIR"); break;
				case 0XB8: sprintf(t,"LDDR"); break;
				case 0XA1: sprintf(t,"CPI"); break;
				case 0XA9: sprintf(t,"CPD"); break;
				case 0XB1: sprintf(t,"CPIR"); break;
				case 0XB9: sprintf(t,"CPDR"); break;
				case 0XA2: sprintf(t,"INI"); break;
				case 0XAA: sprintf(t,"IND"); break;
				case 0XB2: sprintf(t,"INIR"); break;
				case 0XBA: sprintf(t,"INDR"); break;
				case 0XA3: sprintf(t,"OUTI"); break;
				case 0XAB: sprintf(t,"OUTD"); break;
				case 0XB3: sprintf(t,"OTIR"); break;
				case 0XBB: sprintf(t,"OTDR"); break;
				default: sprintf(t,"*NOP");
			}
			break;
		case 0XDD: case 0XFD:
			switch (z=o==0XDD?"IX":"IY",o=PEEK(p),++p,o)
			{
				case 0X09: case 0X19: case 0X29: case 0X39:
					sprintf(t,"ADD  %s,%s",z,twos[o>>4]); break;
				case 0X21: sprintf(t,"LD   %s,$%04X",z,DEBUG_DASM_WORD); break;
				case 0X22: sprintf(t,"LD   ($%04X),%s",DEBUG_DASM_WORD,z); break;
				case 0X2A: sprintf(t,"LD   %s,($%04X)",z,DEBUG_DASM_WORD); break;
				case 0X23: sprintf(t,"INC  %s",z); break;
				case 0X24: sprintf(t,"INC  %cH",z[1]); break;
				case 0X25: sprintf(t,"DEC  %cH",z[1]); break;
				case 0X26: sprintf(t,"LD   %cH,$%02X",z[1],DEBUG_DASM_BYTE); break;
				case 0X2B: sprintf(t,"DEC  %s",z); break;
				case 0X2C: sprintf(t,"INC  %cL",z[1]); break;
				case 0X2D: sprintf(t,"DEC  %cL",z[1]); break;
				case 0X2E: sprintf(t,"LD   %cL,$%02X",z[1],DEBUG_DASM_BYTE); break;
				case 0X34: DEBUG_DASM_IXIY; sprintf(t,"INC  (%s%c$%02X)",z,y,x); break;
				case 0X35: DEBUG_DASM_IXIY; sprintf(t,"DEC  (%s%c$%02X)",z,y,x); break;
				case 0X36: DEBUG_DASM_IXIY; w=PEEK(p); ++p; sprintf(t,"LD   (%s%c$%02X),$%02X",z,y,x,w); break;
				case 0X44: case 0X4C: case 0X54: case 0X5C: case 0X64: case 0X6C: case 0X7C:
					sprintf(t,"LD   %s,%cH",regs[(o-0X40)>>3],z[1]); break;
				case 0X74: DEBUG_DASM_IXIY; sprintf(t,"LD   (%s%c$%02X),H",z,y,x); break;
				case 0X45: case 0X4D: case 0X55: case 0X5D: case 0X65: case 0X6D: case 0X7D:
					sprintf(t,"LD   %s,%cL",regs[(o-0X40)>>3],z[1]); break;
				case 0X75: DEBUG_DASM_IXIY; sprintf(t,"LD   (%s%c$%02X),L",z,y,x); break;
				case 0X46: case 0X4E: case 0X56: case 0X5E: case 0X66: case 0X6E: case 0X7E:
					DEBUG_DASM_IXIY; sprintf(t,"LD   %s,(%s%c$%02X)",regs[(o-0X40)>>3],z,y,x); break;
				case 0X60: case 0X61: case 0X62: case 0X63: case 0X67:
					sprintf(t,"LD   %cH,%s",z[1],regs[o&7]); break;
				case 0X68: case 0X69: case 0X6A: case 0X6B: case 0X6F:
					sprintf(t,"LD   %cL,%s",z[1],regs[o&7]); break;
				case 0X70: case 0X71: case 0X72: case 0X73: case 0X77:
					DEBUG_DASM_IXIY; sprintf(t,"LD   (%s%c$%02X),%s",z,y,x,regs[o&7]); break;
				case 0X84: case 0X8C: case 0X94: case 0X9C: case 0XA4: case 0XAC: case 0XB4: case 0XBC:
					sprintf(t,"%s  %cH",alus[(o-0X80)>>3],z[1]); break;
				case 0X85: case 0X8D: case 0X95: case 0X9D: case 0XA5: case 0XAD: case 0XB5: case 0XBD:
					sprintf(t,"%s  %cL",alus[(o-0X80)>>3],z[1]); break;
				case 0X86: case 0X8E: case 0X96: case 0X9E: case 0XA6: case 0XAE: case 0XB6: case 0XBE:
					DEBUG_DASM_IXIY; sprintf(t,"%s  (%s%c$%02X)",alus[(o-0X80)>>3],z,y,x); break;
				case 0XE1: sprintf(t,"POP  %s",z); break;
				case 0XE5: sprintf(t,"PUSH %s",z); break;
				case 0XE3: sprintf(t,"EX   %s,(SP)",z); break;
				case 0XE9: sprintf(t,"JP   %s",z); break;
				case 0XF9: sprintf(t,"LD   SP,%s",z); break;
				case 0XCB: switch (DEBUG_DASM_IXIY,o=PEEK(p),++p,o)
					{
						case 0X00: case 0X01: case 0X02: case 0X03: case 0X04: case 0X05: case 0X07:
						case 0X08: case 0X09: case 0X0A: case 0X0B: case 0X0C: case 0X0D: case 0X0F:
						case 0X10: case 0X11: case 0X12: case 0X13: case 0X14: case 0X15: case 0X17:
						case 0X18: case 0X19: case 0X1A: case 0X1B: case 0X1C: case 0X1D: case 0X1F:
						case 0X20: case 0X21: case 0X22: case 0X23: case 0X24: case 0X25: case 0X27:
						case 0X28: case 0X29: case 0X2A: case 0X2B: case 0X2C: case 0X2D: case 0X2F:
						case 0X30: case 0X31: case 0X32: case 0X33: case 0X34: case 0X35: case 0X37:
						case 0X38: case 0X39: case 0X3A: case 0X3B: case 0X3C: case 0X3D: case 0X3F:
							sprintf(t,"%s  %s,(%s%c$%02X)",cbxs[o>>3],regs[o&7],z,y,x); break;
						case 0X06: case 0X0E: case 0X16: case 0X1E: case 0X26: case 0X2E: case 0X36: case 0X3E:
							sprintf(t,"%s  (%s%c$%02X)",cbxs[o>>3],z,y,x); break;
						case 0X40: case 0X41: case 0X42: case 0X43: case 0X44: case 0X45: case 0X47:
						case 0X48: case 0X49: case 0X4A: case 0X4B: case 0X4C: case 0X4D: case 0X4F:
						case 0X50: case 0X51: case 0X52: case 0X53: case 0X54: case 0X55: case 0X57:
						case 0X58: case 0X59: case 0X5A: case 0X5B: case 0X5C: case 0X5D: case 0X5F:
						case 0X60: case 0X61: case 0X62: case 0X63: case 0X64: case 0X65: case 0X67:
						case 0X68: case 0X69: case 0X6A: case 0X6B: case 0X6C: case 0X6D: case 0X6F:
						case 0X70: case 0X71: case 0X72: case 0X73: case 0X74: case 0X75: case 0X77:
						case 0X78: case 0X79: case 0X7A: case 0X7B: case 0X7C: case 0X7D: case 0X7F:
							sprintf(t,"*BIT %d,(%s%c$%02X)",(o-0X40)>>3,z,y,x); break;
						case 0X46: case 0X4E: case 0X56: case 0X5E: case 0X66: case 0X6E: case 0X76: case 0X7E:
							sprintf(t,"BIT  %d,(%s%c$%02X)",(o-0X40)>>3,z,y,x); break;
						case 0X80: case 0X81: case 0X82: case 0X83: case 0X84: case 0X85: case 0X87:
						case 0X88: case 0X89: case 0X8A: case 0X8B: case 0X8C: case 0X8D: case 0X8F:
						case 0X90: case 0X91: case 0X92: case 0X93: case 0X94: case 0X95: case 0X97:
						case 0X98: case 0X99: case 0X9A: case 0X9B: case 0X9C: case 0X9D: case 0X9F:
						case 0XA0: case 0XA1: case 0XA2: case 0XA3: case 0XA4: case 0XA5: case 0XA7:
						case 0XA8: case 0XA9: case 0XAA: case 0XAB: case 0XAC: case 0XAD: case 0XAF:
						case 0XB0: case 0XB1: case 0XB2: case 0XB3: case 0XB4: case 0XB5: case 0XB7:
						case 0XB8: case 0XB9: case 0XBA: case 0XBB: case 0XBC: case 0XBD: case 0XBF:
							sprintf(t,"RES  %d,%s,(%s%c$%02X)",(o-0X80)>>3,regs[o&7],z,y,x); break;
						case 0X86: case 0X8E: case 0X96: case 0X9E: case 0XA6: case 0XAE: case 0XB6: case 0XBE:
							sprintf(t,"RES  %d,(%s%c$%02X)",(o-0X80)>>3,z,y,x); break;
						case 0XC0: case 0XC1: case 0XC2: case 0XC3: case 0XC4: case 0XC5: case 0XC7:
						case 0XC8: case 0XC9: case 0XCA: case 0XCB: case 0XCC: case 0XCD: case 0XCF:
						case 0XD0: case 0XD1: case 0XD2: case 0XD3: case 0XD4: case 0XD5: case 0XD7:
						case 0XD8: case 0XD9: case 0XDA: case 0XDB: case 0XDC: case 0XDD: case 0XDF:
						case 0XE0: case 0XE1: case 0XE2: case 0XE3: case 0XE4: case 0XE5: case 0XE7:
						case 0XE8: case 0XE9: case 0XEA: case 0XEB: case 0XEC: case 0XED: case 0XEF:
						case 0XF0: case 0XF1: case 0XF2: case 0XF3: case 0XF4: case 0XF5: case 0XF7:
						case 0XF8: case 0XF9: case 0XFA: case 0XFB: case 0XFC: case 0XFD: case 0XFF:
							sprintf(t,"SET  %d,%s,(%s%c$%02X)",(o-0XC0)>>3,regs[o&7],z,y,x); break;
						case 0XC6: case 0XCE: case 0XD6: case 0XDE: case 0XE6: case 0XEE: case 0XF6: case 0XFE:
							sprintf(t,"SET  %d,(%s%c$%02X)",(o-0XC0)>>3,z,y,x); break;
					}
					break;
				default: sprintf(t,"*NOP"); --p;
			}
			break;
	}
	return p;
}

#endif

// ============================================= END OF Z80 EMULATION //
