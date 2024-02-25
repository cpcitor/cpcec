 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The Yamaha FM-Operator Type-L (YM2143/OPLL) sound chip is a cutdown
// version of Yamaha's own YM3812/OPL2. We find these chips in several
// systems of the late 1980s and early 1990s, such as the arcade games
// "Rampart" and "Time Soldiers" and the MSX2+ home computers.

// Because the OPLL is poorly suited for rapid changes (unlike the PSG
// AY or the MOS SID) and machines cannot "see" what the OPLL is doing
// the emulation can afford several simplifications and optimisations.
// Its output "piggybacks" the audio signal, shared with other chips.

// BEGINNING OF YAMAHA OPLL YM2413 EMULATION ========================= //

BYTE opll_table[64],opll_index; // register table (64 entries, not 256)
BYTE opll_playing; // zero on reset, one when a channel is first enabled
BYTE opll_ampl[14]; int *opll_smpl[14]; // channel amplitudes and samples
BYTE opll_adsr[14]; int opll_time[14]; // ADSR stage and timer
BYTE opll_loud[14]; // instrument/drum amounts and status
int opll_wave[14]; // cycle within the sample
#define OPLL_SINE_BITS 10
int opll_sine[4][1<<OPLL_SINE_BITS]; //

#if AUDIO_CHANNELS > 1
int opll_stereo[14][2]; // the channels' LEFT and RIGHT weights
#endif

const BYTE opll_const[19][8]= // OPLL preset insts (0-15) and drums (16-18): FMSX, BlueMSX and OpenMSX agree on their values
{ // each line follows the same rules from OPL regs. 0-7
	0,0,0,0,0,0,0,0, //  0: original, user-defined
	0X61,0X61,0X1E,0X17,0XF0,0X7F,0X00,0X17, //  1: violin
	0X13,0X41,0X16,0X0E,0XFD,0XF4,0X23,0X23, //  2: guitar
	0X03,0X01,0X9A,0X04,0XF3,0XF3,0X13,0XF3, //  3: piano
	0X11,0X61,0X0E,0X07,0XFA,0X64,0X70,0X17, //  4: flute
	0X22,0X21,0X1E,0X06,0XF0,0X76,0X00,0X28, //  5: clarinet
	0X21,0X22,0X16,0X05,0XF0,0X71,0X00,0X18, //  6: oboe
	0X21,0X61,0X1D,0X07,0X82,0X80,0X17,0X17, //  7: trumpet
	0X23,0X21,0X2D,0X16,0X90,0X90,0X00,0X07, //  8: organ
	0X21,0X21,0X1B,0X06,0X64,0X65,0X10,0X17, //  9: horn
	0X21,0X21,0X0B,0X1A,0X85,0XA0,0X70,0X07, // 10: synthesizer
	0X23,0X01,0X83,0X10,0XFF,0XB4,0X10,0XF4, // 11: harpsichord
	0X97,0XC1,0X20,0X07,0XFF,0XF4,0X22,0X22, // 12: vibraphone
	0X61,0X00,0X0C,0X05,0XC2,0XF6,0X40,0X44, // 13: synthesizer bass
	0X01,0X01,0X56,0X03,0X94,0XC2,0X03,0X12, // 14: acoustic bass
	0X21,0X01,0X89,0X03,0XF1,0XE4,0XF0,0X23, // 15: electric guitar
	0X07,0X21,0X14,0X00,0XEE,0XF8,0XFF,0XF8, // 16: bass drum (BD in the doc)
	0X01,0X31,0X00,0X00,0XF8,0XF7,0XF8,0XF7, // 17: hi-hat (HH) + snare drum (SD)
	0X25,0X11,0X00,0X00,0XF8,0XFA,0XF8,0X55, // 18: tom-tom (TOM) + top cymbal (T-CT)
}; // drum channels (from 9 to 13) are assigned to HH, T-CT, TOM, SD and BD respectively
void opll_setindex(BYTE b)
	{ opll_index=b&63; }
void opll_load_instr(int c,int i)
{
	opll_smpl[c]=opll_sine[0];
}
void opll_sendbyte(BYTE b)
{
	//cprintf("%08X:OPLL %02X,%02X ",z80_pc.w,opll_index,b);
	char c=opll_index&15; if (opll_index==14)
	{
		if ((opll_table[opll_index]^b)&32) // did the RHYTHM bit change?
		{
			if (b&32)
				opll_loud[ 6]=opll_loud[ 7]=opll_loud[ 8]=0; // instruments 7-9 off
			else
				opll_loud[ 9]=opll_loud[10]=opll_loud[11]=opll_loud[12]=opll_loud[13]=0; // drums off
		}
		if (b&32) // check drums when RHYTHM is enabled
		{
			if (opll_loud[ 9]=(b& 1)) // HH?
				opll_adsr[ 9]=opll_time[ 9]=0,opll_load_instr( 9,17);
			if (opll_loud[10]=(b& 2)) // T-CT?
				opll_adsr[10]=opll_time[10]=0,opll_load_instr(10,18);
			if (opll_loud[11]=(b& 4)) // TOM?
				opll_adsr[11]=opll_time[11]=0,opll_load_instr(11,18);
			if (opll_loud[12]=(b& 8)) // SD?
				opll_adsr[12]=opll_time[12]=0,opll_load_instr(12,17);
			if (opll_loud[13]=(b&16)) // BD?
				opll_adsr[13]=opll_time[13]=0,opll_load_instr(13,16);
		}
	}
	else if (opll_index>=32&&opll_index<32+9)
	{
		opll_playing=1; // notes are playing, the OPLL is active!
		if ((~opll_table[opll_index])&b&16) opll_adsr[c]=opll_time[c]=opll_ampl[c]=0,opll_loud[c]=c<((opll_table[14]&32)?6:9),opll_load_instr(c,c?0:opll_table[47+c]>>4); // set ATTACK!
		else if ((~b)&opll_table[opll_index]&16) opll_adsr[c]=2; // reset ATTACK!
	}
	else if (opll_index>=48&&opll_index<48+9)
	{
		// TODO...
	}
	opll_table[opll_index]=b;
}
void opll_reset(void)
	{ MEMZERO(opll_table); MEMZERO(opll_loud); opll_index=opll_playing=0; } // everything disabled on reset!

void opll_setup(void)
{
	for (int i=0,j;i<(1<<OPLL_SINE_BITS);++i)
		opll_sine[0][i]=j=SDL_sin(i*2*M_PI/(1<<OPLL_SINE_BITS))*OPLL_MAX_VOICE+.5, // sinus
		opll_sine[1][i]=j>0?j:0, // half sinus, half zero
		opll_sine[2][i]= // triangular wave
			i<(1<<(OPLL_SINE_BITS-2))?(i*OPLL_MAX_VOICE)>>(OPLL_SINE_BITS-2): // 1/4: rise from zero to max
			i<(2<<(OPLL_SINE_BITS-2))?OPLL_MAX_VOICE-opll_sine[2][i-(1<<(OPLL_SINE_BITS-2))]: // 2/4: fall from max to zero
			i<(3<<(OPLL_SINE_BITS-2))?-opll_sine[2][i-(2<<(OPLL_SINE_BITS-2))]: // 3/4: fall from zero to min
			opll_sine[2][i-(3<<(OPLL_SINE_BITS-2))]-OPLL_MAX_VOICE; // 4/4: rise from min to zero
}
#define opll_update() 0 // nothing to do, really

void opll_main(AUDIO_UNIT *t,int l) // "piggyback" the audio output for nonzero `l` samples!
{
	int e=(TICKS_PER_SECOND+OPLL_TICK_STEP/2)/OPLL_TICK_STEP,s[14]={
		((opll_table[32+0]&1)*256+opll_table[16+0])<<((opll_table[32+0]&14)>>1),
		((opll_table[32+1]&1)*256+opll_table[16+1])<<((opll_table[32+1]&14)>>1),
		((opll_table[32+2]&1)*256+opll_table[16+2])<<((opll_table[32+2]&14)>>1),
		((opll_table[32+3]&1)*256+opll_table[16+3])<<((opll_table[32+3]&14)>>1),
		((opll_table[32+4]&1)*256+opll_table[16+4])<<((opll_table[32+4]&14)>>1),
		((opll_table[32+5]&1)*256+opll_table[16+5])<<((opll_table[32+5]&14)>>1),
		((opll_table[32+6]&1)*256+opll_table[16+6])<<((opll_table[32+6]&14)>>1),
		((opll_table[32+7]&1)*256+opll_table[16+7])<<((opll_table[32+7]&14)>>1),
		((opll_table[32+8]&1)*256+opll_table[16+8])<<((opll_table[32+8]&14)>>1)};
	do
	{
		#if AUDIO_CHANNELS > 1
		int o0=0,o1=0;
		#else
		int o=0;
		#endif
		#if OPLL_MAIN_EXTRABITS
		for (char n=0;n<(1<<OPLL_MAIN_EXTRABITS);++n)
		#endif
		{
			static int i=0; i+=e; // confirmed, the OPLL clock is the Z80's 3.58 MHz
			int m=i/(AUDIO_PLAYBACK<<OPLL_MAIN_EXTRABITS); i%=(AUDIO_PLAYBACK<<OPLL_MAIN_EXTRABITS);
			// TODO: update amplitudes, handle wave shapes...
			for (char c=0;c<14;++c) // update and mix instruments; divisions let us skip many iterations
				if (opll_loud[c])
				{
					opll_wave[c]+=s[c]*m;
					int p=/*opll_ampl[c]**/opll_smpl[c][(opll_wave[c]>>(19-OPLL_SINE_BITS))&((1<<OPLL_SINE_BITS)-1)];
					#if AUDIO_CHANNELS > 1
						o0+=opll_stereo[c][0]*p,
						o1+=opll_stereo[c][1]*p;
					#else
						o+=p;
					#endif
				}
		}
		#if AUDIO_CHANNELS > 1
		o0=(o0>>(OPLL_MAIN_EXTRABITS+9))+*t; *t++=o0>>1;
		o1=(o1>>(OPLL_MAIN_EXTRABITS+9))+*t; *t++=o1>>1;
		#else
		o=(o>>OPLL_MAIN_EXTRABITS)+*t; *t++=o>>1; // assuming 50% OPLL, 50% AY
		#endif
	}
	while (--l>0);
}

// ============================== END OF YAMAHA OPLL YM2413 EMULATION //
