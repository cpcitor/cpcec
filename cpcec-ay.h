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

// This module includes functions to export the sound chip registers
// into a YM3 file that can be read by platform-independent players.

// BEGINNING OF PSG AY-3-8910 EMULATION ============================== //

const BYTE psg_valid[16]={255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255}; // bit masks
BYTE psg_index,psg_table[16]; // index and table
BYTE psg_hard_log=0xFF; // default mode: drop and stay
#define PSG_ULTRASOUND (PSG_KHZ_CLOCK*256/AUDIO_PLAYBACK)
int psg_ultra_beep,psg_ultra_hits; // ultrasound/beeper filter

// frequencies (or more properly, wave lengths) are handled as counters
// that toggle the channels' output status when they reach their current limits.
int psg_tone_count[3]={0,0,0},psg_tone_state[3]={0,0,0};
int psg_tone_limit[3],psg_tone_power[3],psg_tone_mixer[3];
int psg_noise_limit,psg_noise_count=0,psg_noise_state=0,psg_noise_trash=1;
int psg_hard_limit,psg_hard_count,psg_hard_style,psg_hard_level,psg_hard_flag0,psg_hard_flag2;
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
			if (!(psg_noise_limit=psg_table[6]&31))
				psg_noise_limit=1;
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
			if (!(psg_hard_limit=psg_table[11]+psg_table[12]*256))
				psg_hard_limit=1;
			if (psg_hard_count>psg_hard_limit)
				psg_hard_count=psg_hard_limit; // cifra nota supra
			break;
		case 13: // envelope bits
			c=psg_table[13]; psg_hard_count=psg_hard_level=psg_hard_flag0=0;
			psg_hard_flag2=(psg_hard_log=psg_hard_style=c<4?9:c<8?15:c)&4?0:15; // unify styles!
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
		if (psg_table[x]!=(i&=psg_valid[x])||x==13) // reduce register clobbering
			psg_table[x]=i,psg_reg_update(x);
	}
}
#define psg_table_select(i) (psg_index=(i)) // "NODES OF YESOD" for CPC needs all bits because of a programming error!
#define psg_table_send(i) psg_table_sendto(psg_index,(i))
#define psg_table_recv() (psg_index<16?psg_table[psg_index]:0xFF)
#define psg_port_a_lock() (psg_table[7]&64) // used on CPC for the keyboard and on Spectrum 128K for the printer
#define psg_port_b_lock() (psg_table[7]&128) // used on CPC by the PLUS demo "PHAT", unused on Spectrum (AFAIK)

#define psg_setup()

// The CPC PlayCity and Spectrum Turbosound require their own logic, as they're more than an extra set of AY chips!

#ifdef PSG_PLAYCITY
#ifdef PSG_PLAYCITY_HALF
BYTE playcity_table[1][16],playcity_index[1],playcity_hard_new[1];
int playcity_hard_style[1],playcity_hard_count[1],playcity_hard_level[1],playcity_hard_flag0[1],playcity_hard_flag2[1];
#if AUDIO_CHANNELS > 1
int playcity_stereo[1][3][2];
#endif
#else
int playcity_clock=0; BYTE playcity_table[2][16],playcity_index[2],playcity_hard_new[2];
int playcity_hard_style[2],playcity_hard_count[2],playcity_hard_level[2],playcity_hard_flag0[2],playcity_hard_flag2[2];
#if AUDIO_CHANNELS > 1
int playcity_stereo[2][3][2];
#endif
#endif
#ifdef PSG_PLAYCITY_HALF
#else
void playcity_set_config(BYTE b)
	{ if (b<16) playcity_clock=b; }
#define playcity_get_config() (playcity_clock)
#endif
void playcity_select(BYTE x,BYTE b)
{
	#ifdef PSG_PLAYCITY_HALF
	if (!x)
	#else
	if (x<2)
	#endif
		if (b<16) playcity_index[x]=b;
}
void playcity_send(BYTE x,BYTE b)
{
	#ifdef PSG_PLAYCITY_HALF
	if (!x)
	#else
	if (x<2)
	#endif
	{
		int y=playcity_index[x];
		playcity_table[x][y]=(b&=psg_valid[y]);
		if (y==13) // envelope bits?
		{
			playcity_hard_count[x]=playcity_hard_level[x]=playcity_hard_flag0[x]=0;
			playcity_hard_flag2[x]=(playcity_hard_style[x]=b<4?9:b<8?15:b)&4?0:15;
		}
	}
}
BYTE playcity_recv(BYTE x)
{
	return (
		#ifdef PSG_PLAYCITY_HALF
			!x
		#else
			x<2
		#endif
		&&playcity_index[x]<16)?playcity_table[x][playcity_index[x]]:0xFF;
}
void playcity_reset(void)
{
	MEMZERO(playcity_table);
	#ifdef PSG_PLAYCITY_HALF
	playcity_table[0][7]=0x3F; // channels+noise off
	#else
	playcity_clock=0; playcity_table[0][7]=playcity_table[1][7]=0x3F; // 2 MHz, channels+noise off
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

// YM3 file logging ------------------------------------------------- //

char psg_tmpname[STRMAX];
FILE *psg_logfile=NULL,*psg_tmpfile;
int psg_nextlog=1,psg_tmpsize;
unsigned char psg_tmp[14<<9],psg_log[1<<9]; // `psg_tmp` must be 14 times as big as `psg_log`!
int psg_closelog(void)
{
	if (!psg_logfile)
		return 1; // nothing to do!
	fwrite("YM3!",1,4,psg_logfile);
	fwrite1(psg_tmp,psg_tmpsize,psg_tmpfile);
	fclose(psg_tmpfile);
	if (psg_tmpfile=fopen(psg_tmpname,"rb"))
	{
		for (int c=0,l,o;c<14;++c) // arrange byte dumps into long byte channels
		{
			fseek(psg_tmpfile,0,SEEK_SET);
			while (l=fread1(psg_tmp,sizeof(psg_tmp),psg_tmpfile))
			{
				o=0; for (int i=c;i<l;i+=14)
					psg_log[o++]=psg_tmp[i];
				fwrite1(psg_log,o,psg_logfile);
			}
		}
		fclose(psg_tmpfile);
	}
	remove(psg_tmpname); // destroy temporary file
	fclose(psg_logfile);
	psg_logfile=NULL;
	return 0;
}
int psg_createlog(char *s)
{
	if (psg_logfile)
		return 1; // already busy!
	if (!(psg_tmpfile=fopen(strcat(strcpy(psg_tmpname,s),"$"),"wb")))
		return 1; // cannot create temporary file!
	if (!(psg_logfile=fopen(s,"wb")))
		return fclose(psg_tmpfile),1; // cannot create file!
	return psg_tmpsize=0;
}
void psg_writelog(void)
{
	if (psg_logfile)
	{
		int i; // we must adjust YM values to a 2 MHz clock.
		i=((psg_table[0]+psg_table[1]*256)*2000+PSG_KHZ_CLOCK/2)/PSG_KHZ_CLOCK; // channel 1 wavelength
		psg_tmp[psg_tmpsize++]=i; psg_tmp[psg_tmpsize++]=i>>8;
		i=((psg_table[2]+psg_table[3]*256)*2000+PSG_KHZ_CLOCK/2)/PSG_KHZ_CLOCK; // channel 2 wavelength
		psg_tmp[psg_tmpsize++]=i; psg_tmp[psg_tmpsize++]=i>>8;
		i=((psg_table[4]+psg_table[5]*256)*2000+PSG_KHZ_CLOCK/2)/PSG_KHZ_CLOCK; // channel 3 wavelength
		psg_tmp[psg_tmpsize++]=i; psg_tmp[psg_tmpsize++]=i>>8;
		psg_tmp[psg_tmpsize++]=psg_table[6]; // noise wavelength
		psg_tmp[psg_tmpsize++]=psg_table[7]; // mixer
		psg_tmp[psg_tmpsize++]=psg_table[8]; // channel 1 amplitude
		psg_tmp[psg_tmpsize++]=psg_table[9]; // channel 2 amplitude
		psg_tmp[psg_tmpsize++]=psg_table[10]; // channel 3 amplitude
		i=((psg_table[11]+psg_table[12]*256)*2000+PSG_KHZ_CLOCK/2)/PSG_KHZ_CLOCK; // hard envelope wavelength
		psg_tmp[psg_tmpsize++]=i; psg_tmp[psg_tmpsize++]=i>>8;
		psg_tmp[psg_tmpsize++]=psg_hard_log; // hard envelope type
		psg_hard_log=0xFF; // 0xFF means the hard envelope doesn't change
		if (psg_tmpsize>=sizeof(psg_tmp))
			fwrite1(psg_tmp,sizeof(psg_tmp),psg_tmpfile),psg_tmpsize=0;
	}
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
					if (psg_noise_trash&1) psg_noise_trash+=0x48000; // LFSR x2
					psg_noise_state=(psg_noise_state+(psg_noise_trash>>=1))&1;
				}
			//}
			//else // update hard envelope, at half the rate
			//{
				audio_table[16]=audio_table[psg_hard_level^psg_hard_flag2^((psg_hard_style&2)?psg_hard_flag0:0)]; // update hard envelope
				if (--psg_hard_count<=0)
				{
					psg_hard_count=psg_hard_limit;
					if (++psg_hard_level>15) // end of hard envelope?
					{
						if (psg_hard_style&1)
							psg_hard_level=15,psg_hard_flag0=15; // stop!
						else
							psg_hard_level=0,psg_hard_flag0^=15; // loop!
					}
				}
			}
			for (int c=0;c<3;++c)
				if (--psg_tone_count[c]<=0) // update channel; ultrasound/beeper filter
					psg_tone_state[c]=(psg_tone_count[c]=psg_tone_limit[c])<=PSG_ULTRASOUND?psg_ultra_beep:~psg_tone_state[c];
		}
		for (int c=0;c<3;++c)
			if (psg_tone_state[c]|(psg_tone_mixer[c]&1)) // is the channel active?
				if (psg_noise_state|(psg_tone_mixer[c]&8)) // is the channel noisy?
					#if AUDIO_CHANNELS > 1
					o0+=audio_table[psg_tone_power[c]]*psg_stereo[c][0],
					o1+=audio_table[psg_tone_power[c]]*psg_stereo[c][1];
					#else
					o+=audio_table[psg_tone_power[c]];
					#endif
		#if AUDIO_CHANNELS > 1
		o0+=d,
		o1+=d;
		#else
		o+=d;
		#endif
		++n;
		static int b=0; if ((b-=AUDIO_PLAYBACK*PSG_TICK_STEP>>PSG_MAIN_EXTRABITS)<=0)
		{
			b+=TICKS_PER_SECOND;
			#if AUDIO_CHANNELS > 1
			*audio_target++=(o0+n/2)/(n<<(24-AUDIO_BITDEPTH))+AUDIO_ZERO, // rounded average (left)
			*audio_target++=(o1+n/2)/(n<<(24-AUDIO_BITDEPTH))+AUDIO_ZERO; // rounded average (right)
			n=o0=o1=0; // reset output averaging variables
			#else
			*audio_target++=(o+n/2)/(n<<(16-AUDIO_BITDEPTH))+AUDIO_ZERO; // rounded average
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
	#ifdef PSG_PLAYCITY_HALF
	if (playcity_table[0][7]==0x3F||l<=0) return;
	#else
	int dirty_l=playcity_table[0][7]==0x3F,dirty_h=playcity_table[1][7]!=0x3F;
	if (dirty_l>dirty_h||l<=0) return; // disabled chips? no buffer? quit!
	#endif
	#ifdef PSG_PLAYCITY_HALF
	const int x=0,playcity_clock=0;
	int playcity_tone_limit[1][3],playcity_tone_power[1][3],playcity_tone_mixer[1][3],playcity_noise_limit[1],playcity_hard_limit[1];
	static int playcity_tone_count[1][3],playcity_tone_state[1][3]={{0,0,0}},playcity_noise_state[1],playcity_noise_count[1],playcity_noise_trash[1]={1},playcity_hard_power[1];
	#else
	int playcity_tone_limit[2][3],playcity_tone_power[2][3],playcity_tone_mixer[2][3],playcity_noise_limit[2],playcity_hard_limit[2];
	static int playcity_tone_count[2][3],playcity_tone_state[2][3]={{0,0,0},{0,0,0}},playcity_noise_state[2],playcity_noise_count[2],playcity_noise_trash[2]={1,1},playcity_hard_power[2];
	for (int x=dirty_l;x<=dirty_h;++x)
	#endif
	{
		for (int c=0;c<3;++c) // preload channel limits
		{
			playcity_tone_power[x][c]=playcity_table[x][c*1+8];
			playcity_tone_mixer[x][c]=playcity_table[x][7]>>c;
			if ((playcity_tone_limit[x][c]=playcity_table[x][c*2+0]+playcity_table[x][c*2+1]*256)<=PSG_PLAYCITY*PSG_ULTRASOUND)
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
	int playcity_clock_hi=(playcity_clock?playcity_clock*2-1:2)*(PSG_PLAYCITY*TICKS_PER_SECOND),playcity_clock_lo=(playcity_clock?playcity_clock:1)*2*AUDIO_PLAYBACK*PSG_TICK_STEP;
	for (;;)
	{
		p+=playcity_clock_hi; while (p>=0)
		{
			#ifdef PSG_PLAYCITY_HALF
			static char q=0; // see below
			#else
			static char q=0; q=~q; // toggle once for ALL chips!
			for (int x=dirty_l;x<=dirty_h;++x) // update all chips
			#endif
			{
				#ifdef PSG_PLAYCITY_HALF
				if (q=~q) // update noises, half the rate
				#else
				if (q) // see above
				#endif
				{
					if (--playcity_noise_count[x]<=0)
					{
						playcity_noise_count[x]=playcity_noise_limit[x];
						if (playcity_noise_trash[x]&1) playcity_noise_trash[x]+=0x48000; // LFSR x2
						playcity_noise_state[x]=(playcity_noise_state[x]+(playcity_noise_trash[x]>>=1))&1;
					}
				//}
				//else // update hard envelopes, half the rate
				//{
					playcity_hard_power[x]=playcity_hard_level[x]^playcity_hard_flag2[x]^((playcity_hard_style[x]&2)?playcity_hard_flag0[x]:0);
					if (--playcity_hard_count[x]<=0)
					{
						playcity_hard_count[x]=playcity_hard_limit[x];
						if (++playcity_hard_level[x]>15) // end of hard envelope?
						{
							if (playcity_hard_style[x]&1)
								playcity_hard_level[x]=15,playcity_hard_flag0[x]=15; // stop!
							else
								playcity_hard_level[x]=0,playcity_hard_flag0[x]^=15; // loop!
						}
					}
				}
				for (int c=0;c<3;++c) // update channels and render output
				{
					if (--playcity_tone_count[x][c]<=0) // end of count?
						playcity_tone_count[x][c]=playcity_tone_limit[x][c],
						playcity_tone_state[x][c]=~playcity_tone_state[x][c];
					if (playcity_tone_state[x][c]|(playcity_tone_mixer[x][c]&1)) // active channel?
						if (playcity_noise_state[x]|(playcity_tone_mixer[x][c]&8)) // noisy channel?
						{
							int z=playcity_tone_power[x][c];
							if (z&16)
								z=playcity_hard_power[x];
							if ((z-=2)>0) // each channel in Playcity plays at half the normal intensity
								#if AUDIO_CHANNELS > 1
								o0+=audio_table[z]*playcity_stereo[x][c][0],
								o1+=audio_table[z]*playcity_stereo[x][c][1];
								#else
								o+=audio_table[z];
								#endif
						}
				}
			}
			++n; p-=playcity_clock_lo;
		}
		// generate negative samples (-50% x2) to avoid overflows against the central AY chip (+100%)
		if (n) // enough data to write a sample? unlike the basic PSG, `n` is >1 at 44100 Hz (5 or 6)
		{
			#if AUDIO_CHANNELS > 1
			*t++-=(o0+n/2)/(n<<(24-AUDIO_BITDEPTH));
			*t++-=(o1+n/2)/(n<<(24-AUDIO_BITDEPTH));
			n=o0=o1=0;
			#else
			*t++-=(o+n/2)/(n<<(16-AUDIO_BITDEPTH));
			n=o=0;
			#endif
			if (!--l)
				break;
		}
	}
}
#endif

// =================================== END OF PSG AY-3-8910 EMULATION //
