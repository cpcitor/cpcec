 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The General Instruments PSG AY-3-8910 sound chip (together with
// compatible replacements such as the YM2149) that plays the sound in
// the Amstrad CPC machines was popular in arcade games of the 1980s,
// as well in home computers: Spectrum 128, +2 and +3, Atari ST, etc.

// YM3 format output can be shared with other chips (cfr. cpcec-ym.h)

// BEGINNING OF PSG AY-3-8910 EMULATION ============================== //

const BYTE psg_valid[16]={ 255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255 }; // bit masks
BYTE psg_index,psg_table[16]; // index and table
BYTE psg_hard_log=0xFF; // default mode: drop and stay
#define PSG_ULTRASOUND (PSG_KHZ_CLOCK*256/AUDIO_PLAYBACK)
int psg_ultra_beep,psg_ultra_hits; // ultrasound/beeper filter

const BYTE psg_envelope[8][32]= // precalc'd tables, better than calculating these values on the fly
{
	{ 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, // \\\\ //
	{ 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // \... //
	{ 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 }, // \/\/ //
	{ 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 }, // \''' //
	{  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 }, // //// //
	{  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 }, // /''' //
	{  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, // /\/\ //
	{  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // /... //
};

// frequencies (or more properly, wave lengths) are handled as counters
// that toggle the channels' output status when they reach their current limits.
int psg_tone_count[3]={0,0,0},psg_tone_state[3]={0,0,0};
int psg_tone_limit[3],psg_tone_power[3],psg_tone_mixer[3];
int psg_noise_limit,psg_noise_count=0;
int psg_hard_limit,psg_hard_count; char psg_hard_style,psg_hard_level;
#if AUDIO_CHANNELS > 1
int psg_stereo[3][2]; // the three channels' LEFT and RIGHT weights
#endif
void psg_reg_update(int c)
{
	switch (c)
	{
		case 0: case 2: case 4: // LO wavelengths
		case 1: case 3: case 5: // HI wavelengths
			// catch overflows, either bad (buggy music in "Thing on a Spring" for CPC)
			// or good (pipe sound effect in its sequel "Thing Bounces Back" for CPC)
			c>>=1; if (psg_tone_count[c]>(psg_tone_limit[c]=psg_table[c*2]+psg_table[c*2+1]*256))
				if ((psg_tone_count[c]=psg_tone_limit[c])<=PSG_ULTRASOUND) // ensure count<=limit
					psg_tone_state[c]=psg_ultra_beep; // is it an ultrasound? catch it already!
			break;
		case 6: // noise wavelength
			if (psg_noise_count>(psg_noise_limit=psg_table[6]&31))
				psg_noise_count=psg_noise_limit; // cfr. nota supra
			break;
		case 7: // mixer bits
			for (c=0;c<3;++c)
				psg_tone_mixer[c]=psg_table[7]>>c; // 8+1, 16+2, 32+4
			break;
		case 8: case 9: case 10: // amplitudes
			if ((psg_tone_power[c-8]=psg_table[c])&16) // hard envelope?
				psg_tone_power[c-8]=16;
			break;
		case 11: case 12: // envelope wavelength
			if (psg_hard_count>(psg_hard_limit=psg_table[11]+psg_table[12]*256))
				psg_hard_count=psg_hard_limit; // cfr. nota supra
			break;
		case 13: // envelope bits
			c=psg_table[13]/*&15*/; psg_hard_count=psg_hard_level=0;
			psg_hard_log=psg_hard_style=c<4?1:c>=8?c&7:7; // unify styles!
			break;
	}
}
void psg_all_update(void) // recalc everything
{
	for (int i=0;i<14;++i)
	{
		psg_table[i]&=psg_valid[i];
		if (i!=0&&i!=2&&i!=4&&i!=11) // don't update the same 16-bit register twice
			psg_reg_update(i);
	}
}

INLINE void psg_table_sendto(BYTE x,BYTE i)
{
	if (x<16) // reject invalid index! the limit isn't 14, tho': the CPC+ demo "PHAT" relies on writing and reading R15!
	{
		if (x==7) // detect Spectrum-like CPC beepers such as "Stormbringer" and "Terminus":
			psg_ultra_hits?(psg_ultra_hits=0):(psg_ultra_beep=0); // PSG-based beepers repeatedly hit the mixer!
		else
			psg_ultra_hits=psg_ultra_beep=-1; // normal audio playback pokes more registers than just the mixer!
		if (psg_table[x]!=(i&=psg_valid[x])||x==13) // monitor register clobbering; exception is reg.13 (envelope retrigger)
			psg_table[x]=i,psg_reg_update(x);
	}
}
#define psg_table_select(i) (psg_index=(i)) // "NODES OF YESOD" for CPC needs all bits because of a programming error!
#define psg_table_send(i) psg_table_sendto(psg_index,(i))
#define psg_table_recv() (psg_index<16?psg_table[psg_index]:0xFF)
#define psg_port_a_lock() (psg_table[7]&64) // used on CPC for the keyboard and on Spectrum 128K for the printer
#define psg_port_b_lock() (psg_table[7]&128) // used on CPC by the PLUS demo "PHAT", unused on Spectrum (AFAIK)

#define psg_setup()

// The CPC PlayCity, Spectrum Turbosound and MSX Second PSG require their own logic, as they're more than an extra set of AY chips!
// Enable then with #define PSG_PLAYCITY 1..n (though right now only CPCEC requires two extra chips; ZXSEC and MSXEC just equip one)

#ifdef PSG_PLAYCITY
BYTE playcity_table[PSG_PLAYCITY][16],playcity_index[PSG_PLAYCITY],playcity_hard_new[PSG_PLAYCITY];
int playcity_hard_style[PSG_PLAYCITY],playcity_hard_count[PSG_PLAYCITY],playcity_hard_level[PSG_PLAYCITY];
#if AUDIO_CHANNELS > 1
int playcity_stereo[PSG_PLAYCITY][3][2];
#endif
void playcity_select(BYTE x,BYTE b)
{
	#if PSG_PLAYCITY == 1
	if (!x)
	#else
	if (x<PSG_PLAYCITY)
	#endif
		if (b<16) playcity_index[x]=b;
}
void playcity_send(BYTE x,BYTE b)
{
	#if PSG_PLAYCITY == 1
	if (!x)
	#else
	if (x<PSG_PLAYCITY)
	#endif
	{
		int y=playcity_index[x];
		playcity_table[x][y]=(b&=psg_valid[y]);
		if (y==13) // envelope bits?
		{
			playcity_hard_count[x]=playcity_hard_level[x]=0;
			playcity_hard_style[x]=b<4?1:b>=8?b&7:7;
		}
	}
}
BYTE playcity_recv(BYTE x)
{
	return (
		#if PSG_PLAYCITY == 1
			!x
		#else
			x<PSG_PLAYCITY
		#endif
		&&playcity_index[x]<16)?playcity_table[x][playcity_index[x]]:0xFF;
}
void playcity_reset(void)
{
	MEMZERO(playcity_table);
	PSG_PLAYCITY_RESET;
	#if PSG_PLAYCITY == 1
	playcity_table[0][7]=0x3F; // channels+noise off
	#else
	playcity_table[0][7]=playcity_table[1][7]=0x3F;
	#endif
}
#endif

void psg_reset(void) // reset the PSG AY-3-8910
{
	psg_index=0;
	MEMZERO(psg_table);
	psg_table[7]=0x38; // all channels enabled, all noise disabled
	psg_all_update();
	#ifdef PSG_PLAYCITY
	playcity_reset();
	#endif
}

// audio output ----------------------------------------------------- //

void psg_main(int t,int d) // render audio output for `t` clock ticks, with `d` as a 16-bit base signal
{
	static int r=0; // audio clock is slower, so remainder is kept here
	if (audio_pos_z>=AUDIO_LENGTH_Z||(r+=t<<PSG_MAIN_EXTRABITS)<0) return; // nothing to do!
	#if AUDIO_CHANNELS > 1
	d=-d<<8;
	#else
	d=-d;
	#endif
	do
	{
		static unsigned int smash=0,crash=1;
		#if AUDIO_CHANNELS > 1
		static int n=0,o0=0,o1=0; // output averaging variables
		#else
		static int n=0,o=0; // output averaging variables
		#endif
		#if PSG_MAIN_EXTRABITS
		static int a=1; if (!--a)
		#endif
		{
			#if PSG_MAIN_EXTRABITS
			a=1<<PSG_MAIN_EXTRABITS;
			#endif
			static char q=0; if (q=~q) // update noise, at half the rate
			{
				if (--psg_noise_count<=0)
				{
					psg_noise_count=psg_noise_limit;
					smash=crash&1; crash<<=1; crash+=(((crash>>23)^(crash>>18))&1); // 23-bit LFSR randomizer
				}
			//}
			//else // update hard envelope, at half the rate
			//{
				psg_outputs[16]=psg_outputs[psg_envelope[psg_hard_style][psg_hard_level]];
				if (--psg_hard_count<=0)
				{
					psg_hard_count=psg_hard_limit;
					if (++psg_hard_level>=32) // end of hard envelope?
						psg_hard_level=(psg_hard_style&1)?16:0; // stop or loop!
				}
			}
			for (int c=0;c<3;++c)
				if (--psg_tone_count[c]<=0) // update channel; ultrasound/beeper filter
					psg_tone_state[c]=(psg_tone_count[c]=psg_tone_limit[c])<=PSG_ULTRASOUND?psg_ultra_beep:~psg_tone_state[c];
		}
		for (int c=0;c<3;++c)
			if ((psg_tone_mixer[c]&1)|psg_tone_state[c]) // is the channel active?
				if ((psg_tone_mixer[c]&8)|smash) // is the channel noisy?
		#if AUDIO_CHANNELS > 1
				{
					int o=psg_outputs[psg_tone_power[c]];
					o0+=o*psg_stereo[c][0],
					o1+=o*psg_stereo[c][1];
				}
		o0+=d,
		o1+=d;
		#else
					o+=   psg_outputs[psg_tone_power[c]];
		o+=d;
		#endif
		++n;
		static int b=0; if ((b-=(AUDIO_PLAYBACK*PSG_TICK_STEP)>>PSG_MAIN_EXTRABITS)<=0)
		{
			b+=TICKS_PER_SECOND;
			#if AUDIO_CHANNELS > 1
			*audio_target++=(o0*2+n)/(n<<(25-AUDIO_BITDEPTH))+AUDIO_ZERO, // rounded average (left)
			*audio_target++=(o1*2+n)/(n<<(25-AUDIO_BITDEPTH))+AUDIO_ZERO; // rounded average (right)
			n=o0=o1=0; // reset output averaging variables
			#else
			*audio_target++=(o*2+n)/(n<<(17-AUDIO_BITDEPTH))+AUDIO_ZERO; // rounded average
			n=o=0; // reset output averaging variables
			#endif
			if (++audio_pos_z>=AUDIO_LENGTH_Z) r%=PSG_TICK_STEP; // end of buffer!
		}
	}
	while ((r-=PSG_TICK_STEP)>=0);
}

// Again, the PlayCity extension requires its own logic, as it "piggybacks" on top of the central AY chip;
// notice how there are several differences in the timing (configurable), the mixing and the streamlining.

#ifdef PSG_PLAYCITY
void playcity_main(AUDIO_UNIT *t,int l)
{
	int playcity_tone_limit[PSG_PLAYCITY][3],playcity_tone_power[PSG_PLAYCITY][3],playcity_tone_mixer[PSG_PLAYCITY][3],playcity_noise_limit[PSG_PLAYCITY],playcity_hard_limit[PSG_PLAYCITY];
	static int playcity_tone_count[PSG_PLAYCITY][3],playcity_noise_count[PSG_PLAYCITY],playcity_hard_power[PSG_PLAYCITY];
	#if PSG_PLAYCITY == 1
	static int playcity_tone_state[PSG_PLAYCITY][3]={{0,0,0}}; static unsigned int smash[PSG_PLAYCITY]={0},crash[PSG_PLAYCITY]={1};
	if (playcity_table[0][7]==0x3F||l<=0) return; // nothing to do? quit!
	const int x=0;
	#else
	static int playcity_tone_state[PSG_PLAYCITY][3]={{0,0,0},{0,0,0}}; static unsigned int smash[PSG_PLAYCITY]={0,0},crash[PSG_PLAYCITY]={1,1};
	int dirty_l=playcity_table[0][7]==0x3F,dirty_h=playcity_table[1][7]!=0x3F;
	if (dirty_l>dirty_h||l<=0) return; // disabled chips? no buffer? quit!
	for (int x=dirty_l;x<=dirty_h;++x)
	#endif
	{
		for (int c=0;c<3;++c) // preload channel limits
		{
			playcity_tone_power[x][c]=playcity_table[x][c*1+8];
			playcity_tone_mixer[x][c]=playcity_table[x][7]>>c;
			if ((playcity_tone_limit[x][c]=playcity_table[x][c*2+0]+playcity_table[x][c*2+1]*256)<=PSG_ULTRASOUND)
				playcity_tone_mixer[x][c]|=1; // catch ultrasounds!
		}
		if (!(playcity_noise_limit[x]=playcity_table[x][6])) // noise limits
			playcity_noise_limit[x]=1;
		if (!(playcity_hard_limit[x]=playcity_table[x][11]+playcity_table[x][12]*256)) // hard envelope limits
			playcity_hard_limit[x]=1;
	}
	#if AUDIO_CHANNELS > 1
	static int n=0,o0=0,o1=0,p=0;
	#else
	static int n=0,o=0,p=0;
	#endif
	const int hiclk=playcity_hiclock,loclk=playcity_loclock; // redundant in systems where these values are constant
	for (;;)
	{
		p+=hiclk; while (p>=0)
		{
			#if PSG_PLAYCITY == 1
			static char q=0; // see below
			#else
			static char q=0; q=~q; // toggle once for ALL chips!
			for (int x=dirty_l;x<=dirty_h;++x) // update all chips
			#endif
			{
				#if PSG_PLAYCITY == 1
				if (q=~q) // update noises, half the rate
				#else
				if (q) // see above
				#endif
				{
					if (--playcity_noise_count[x]<=0)
					{
						playcity_noise_count[x]=playcity_noise_limit[x];
						smash[x]=crash[x]&1; crash[x]<<=1; crash[x]+=(((crash[x]>>23)^(crash[x]>>18))&1); // 23-bit LFSR
					}
				//}
				//else // update hard envelopes, half the rate
				//{
					playcity_hard_power[x]=psg_envelope[playcity_hard_style[x]][playcity_hard_level[x]];
					if (--playcity_hard_count[x]<=0)
					{
						playcity_hard_count[x]=playcity_hard_limit[x];
						if (++playcity_hard_level[x]>=32) // end of hard envelope?
							playcity_hard_level[x]=(playcity_hard_style[x]&1)?16:0; // stop or loop!
					}
				}
				for (int c=0;c<3;++c) // update channels and render output
				{
					if (--playcity_tone_count[x][c]<=0) // end of count?
						playcity_tone_count[x][c]=playcity_tone_limit[x][c],
						playcity_tone_state[x][c]=~playcity_tone_state[x][c];
					if (playcity_tone_state[x][c]|(playcity_tone_mixer[x][c]&1)) // active channel?
						if (smash[x]|(playcity_tone_mixer[x][c]&8)) // noisy channel?
						{
							int z=playcity_tone_power[x][c];
							if (z&16)
								z=playcity_hard_power[x];
							#if AUDIO_CHANNELS > 1
							int o=PSG_PLAYCITY_XLAT(psg_outputs[z]);
							o0+=o*playcity_stereo[x][c][0],
							o1+=o*playcity_stereo[x][c][1];
							#else
							o+=   PSG_PLAYCITY_XLAT(psg_outputs[z]);
							#endif
						}
				}
			}
			++n; p-=loclk;
		}
		// generate negative samples (-50% x2) to avoid overflows against the central AY chip (+100%)
		if (n) // enough data to write a sample? unlike the basic PSG, `n` is >1 at 44100 Hz (5 or 6)
		{
			#if AUDIO_CHANNELS > 1
			*t++-=(o0*2+n)/(n<<(25-AUDIO_BITDEPTH));
			*t++-=(o1*2+n)/(n<<(25-AUDIO_BITDEPTH));
			n=o0=o1=0;
			#else
			*t++-=(o*2+n)/(n<<(17-AUDIO_BITDEPTH));
			n=o=0;
			#endif
			if (!--l)
				break;
		}
	}
}
#endif

// =================================== END OF PSG AY-3-8910 EMULATION //
