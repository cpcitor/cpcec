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

int psg_outputs[17]={0,(PSG_MAX_VOICE*65536)>>23,(PSG_MAX_VOICE*46341)>>22,(PSG_MAX_VOICE*65536)>>22, // 16 static levels...
	(PSG_MAX_VOICE*46341)>>21,(PSG_MAX_VOICE*65536)>>21,(PSG_MAX_VOICE*46341)>>20,(PSG_MAX_VOICE*65536)>>20,
	(PSG_MAX_VOICE*46341)>>19,(PSG_MAX_VOICE*65536)>>19,(PSG_MAX_VOICE*46341)>>18,(PSG_MAX_VOICE*65536)>>18,
	(PSG_MAX_VOICE*46341)>>17,(PSG_MAX_VOICE*65536)>>17,(PSG_MAX_VOICE*46341)>>16,PSG_MAX_VOICE,0}; // + dynamic level

void psg_weight(int v) // for systems where extra devices may tone the PSG down
{
	psg_outputs[16]=(psg_outputs[16]*v+(psg_outputs[15]>>1))/psg_outputs[15],psg_outputs[15]=v;
	for (int i=0;i<7;++i) psg_outputs[i*2+1]=(v*65536)>>(23-i),psg_outputs[i*2+2]=(v*46341)>>(22-i);
}

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
#if !AUDIO_ALWAYS_MONO
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
#if !AUDIO_ALWAYS_MONO
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
		&&playcity_index[x]<16)?playcity_table[x][playcity_index[x]]:0XFF;
}
void playcity_reset(void)
{
	MEMZERO(playcity_table);
	PSG_PLAYCITY_RESET;
	#if PSG_PLAYCITY == 1
	playcity_table[0][7]=0XFF; // channels+noise off *!* perhaps 0X3F
	#else
	playcity_table[0][7]=playcity_table[1][7]=0XFF;
	#endif
}
#endif

void psg_reset(void) // reset the PSG AY-3-8910
{
	psg_index=0;
	MEMZERO(psg_table);
	psg_table[7]=0XFF; // channels+noise off *!* perhaps 0X3F
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
	#if !AUDIO_ALWAYS_MONO
	d=-d<<8; // flip DAC sign!
	#else
	d=-d; // flip DAC sign!
	#endif
	do
	{
		static unsigned int smash=0,crash=1;
		#if !AUDIO_ALWAYS_MONO
		static int n=0,o0=0,o1=0; // output averages
		#else
		static int n=0,o=0; // output average
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
		#if !AUDIO_ALWAYS_MONO
				{
					int o=psg_outputs[psg_tone_power[c]];
					o0+=o*psg_stereo[c][0],
					o1+=o*psg_stereo[c][1];
				}
		o0+=d,
		o1+=d;
		#else
				o+=psg_outputs[psg_tone_power[c]];
		o+=d;
		#endif
		++n;
		static int b=0; if ((b-=(AUDIO_PLAYBACK*PSG_TICK_STEP)>>PSG_MAIN_EXTRABITS)<=0)
		{
			b+=TICKS_PER_SECOND;
			#if AUDIO_CHANNELS > 1
			#if !AUDIO_ALWAYS_MONO
			int dd=n<<(24-AUDIO_BITDEPTH),qq;
			*audio_target++=(qq=o0/dd)+AUDIO_ZERO,o0-=qq*dd, // rounded average (left)
			*audio_target++=(qq=o1/dd)+AUDIO_ZERO,o1-=qq*dd; // rounded average (right)
			#else
			int dd=n<<(16-AUDIO_BITDEPTH),qq;
			*audio_target++=(qq=o/dd)+AUDIO_ZERO, // rounded average (left)
			*audio_target++=qq+AUDIO_ZERO,o-=qq*dd; // rounded average (right)
			#endif
			#else
			int dd=n<<(16-AUDIO_BITDEPTH),qq;
			*audio_target++=(qq=o /dd)+AUDIO_ZERO,o -=qq*dd; // rounded average
			#endif
			if (n=0,++audio_pos_z>=AUDIO_LENGTH_Z) r%=PSG_TICK_STEP; // end of buffer!
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
	if (playcity_table[0][7]==0XFF||l<=0) return; // nothing to do? quit!
	const int x=0;
	#else
	static int playcity_tone_state[PSG_PLAYCITY][3]={{0,0,0},{0,0,0}}; static unsigned int smash[PSG_PLAYCITY]={0,0},crash[PSG_PLAYCITY]={1,1};
	int dirty_l=playcity_table[0][7]==0XFF,dirty_h=playcity_table[1][7]!=0XFF;
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
	#if !AUDIO_ALWAYS_MONO
	static int n=0,o0=0,o1=0; // output averages
	#else
	static int n=0,o=0; // output average
	#endif
	for (;;)
	{
		const int hiclk=playcity_hiclock,loclk=playcity_loclock; // redundant in systems where these values are constant
		static int p=0; p+=hiclk; while (p>=0)
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
							#if !AUDIO_ALWAYS_MONO
							int o=PSG_PLAYCITY_XLAT(z);
							o0+=o*playcity_stereo[x][c][0],
							o1+=o*playcity_stereo[x][c][1];
							#else
							o+=PSG_PLAYCITY_XLAT(z);
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
			#if !AUDIO_ALWAYS_MONO
			int dd=n<<(24-AUDIO_BITDEPTH),qq;
			*t++-=qq=o0/dd,o0-=qq*dd, // rounded average (left)
			*t++-=qq=o1/dd,o1-=qq*dd; // rounded average (right)
			#else
			int dd=n<<(16-AUDIO_BITDEPTH),qq;
			*t++-=qq=o/dd, // rounded average (left)
			*t++-=qq,o-=qq*dd; // rounded average (right)
			#endif
			#else
			int dd=n<<(16-AUDIO_BITDEPTH),qq;
			*t++-=qq=o /dd,o -=qq*dd; // rounded average
			#endif
			if (n=0,!--l) break;
		}
	}
}
#endif

// =================================== END OF PSG AY-3-8910 EMULATION //
