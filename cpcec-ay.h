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

const BYTE psg_valid[16]={255,15,255,15,255,15,31,63,31,31,31,255,255,15,255,255}; // bit masks
BYTE psg_index,psg_table[16]; // index and table
BYTE psg_hard_log=0xFF; // default mode: drop and stay
int psg_r7_filter; // safety delay to filter ultrasounds away, cfr. "Terminus" and "Robocop"

// frequencies (or more properly, wave lengths) are handled as counters
// that toggle the channels' output status when they reach their current limits.
int psg_tone_count[3]={0,0,0},psg_tone_state[3]={0,0,0};
int psg_tone_limit[3],psg_tone_power[3],psg_tone_mixer[3];
int psg_noise_limit,psg_noise_count=0,psg_noise_state=0,psg_noise_trash=1;
int psg_hard_limit,psg_hard_count,psg_hard_style,psg_hard_level,psg_hard_flag0,psg_hard_flag2;
void psg_reg_update(int c)
{
	switch (c)
	{
		case 0: case 2: case 4: // LO wavelengths
		case 1: case 3: case 5: // HI wavelengths
			psg_tone_limit[c/2]=psg_table[c&-2]+(psg_table[(c&-2)+1]<<8);
			break;
		case 6: // noise wavelength
			if (!(psg_noise_limit=(psg_table[6]&31)*2))
				psg_noise_limit=1*2; // noise runs at half the rate
			break;
		case 7: // mixer bits
			for (c=0;c<3;++c)
				psg_tone_mixer[c]=psg_table[7]&((1+8)<<c); // 8+1, 16+2, 32+4
			break;
		case 8: case 9: case 10: // amplitudes
			if ((psg_tone_power[c-8]=psg_table[c])&16) // hard envelope?
				psg_tone_power[c-8]=16;
			break;
		case 11: case 12: // envelope wavelength
			if (!(psg_hard_limit=(psg_table[11]+(psg_table[12]<<8))*2))
				psg_hard_limit=1*2; // hard envelope, ditto
			break;
		case 13: // envelope bits
			c=psg_table[13]; psg_hard_count=psg_hard_level=psg_hard_flag0=0;
			psg_hard_flag2=(psg_hard_log=psg_hard_style=c<4?9:c<8?15:c)&4?0:15; // unify styles!
			break;
	}
}
void psg_all_update(void)
{
	for (int i=0;i<14;++i)
		if (i!=1&&i!=3&&i!=5&&i!=12) // high bytes of words are redundant
			psg_reg_update(i);
}

INLINE void psg_table_sendto(BYTE x,BYTE i)
{
	if (x<14) // reject invalid index!
	{
		if (x==7&&((psg_table[7]^i)&7))
			psg_r7_filter=1<<5; // below 1<<3 "Terminus" is dirty; above 1<<7 "SOUND 1,1,100" is hearable
		psg_table[x]=(i&=psg_valid[x]);
		psg_reg_update(x);
	}
}
#define psg_table_select(i) psg_index=(i) // "NODES OF YESOD" needs all bits because of a programming error!
#define psg_table_send(i) psg_table_sendto(psg_index,(i))
#define psg_table_recv() (psg_index<16?psg_table[psg_index]:0xFF)

#define psg_setup()

// The PlayCity extension requires its own logic as it isn't just an extra pair of AY chips!

#ifdef PSG_PLAYCITY
int playcity_clock=0; BYTE playcity_table[2][16],playcity_index[2],playcity_hard_new[2];
int playcity_hard_style[2],playcity_hard_count[2],playcity_hard_level[2],playcity_hard_flag0[2],playcity_hard_flag2[2];
#if AUDIO_CHANNELS > 1
int playcity_stereo[2][2];
#endif
void playcity_set_config(BYTE b)
{
	if (b<16)
		playcity_clock=b;
}
#define playcity_get_config() (playcity_clock)
void playcity_select(BYTE x,BYTE b)
{
	if (x<2)
		if (b<16)
			playcity_index[x]=b;
}
void playcity_send(BYTE x,BYTE b)
{
	if (x<2)
	{
		int y=playcity_index[x];
		playcity_table[x][y]=(b&=psg_valid[y]);
		if (y==13)
		{
			playcity_hard_count[x]=playcity_hard_level[x]=playcity_hard_flag0[x]=0;
			playcity_hard_flag2[x]=(playcity_hard_style[x]=b<4?9:b<8?15:b)&4?0:15;
		}
	}
}
void playcity_reset(void)
{
	playcity_clock=0; MEMZERO(playcity_table);
	playcity_table[0][7]=playcity_table[1][7]=0x3F; // 2 MHz, channels+noise off
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
		int c,l,i,o;
		for (c=0;c<14;++c) // arrange byte dumps into long byte channels
		{
			fseek(psg_tmpfile,0,SEEK_SET);
			while (l=fread1(psg_tmp,sizeof(psg_tmp),psg_tmpfile))
			{
				for (i=c,o=0;i<l;i+=14)
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

void psg_main(int t) // render audio output for `t` clock ticks
{
	static int r=0; // audio clock is slower, so remainder is kept here
	if (audio_pos_z>=AUDIO_LENGTH_Z||((r+=t)<=0))
		return; // don't do any calculations if there's nothing to do
	int psg_tone_catch[3];
	for (int c=0;c<3;++c) // catch ultrasounds, but keep any noise channels
		psg_tone_catch[c]=psg_tone_mixer[c]|((psg_tone_limit[c]<=(PSG_KHZ_CLOCK/225)&&!psg_r7_filter)?7*1:0); // safe margin? (200-250)
	if ((psg_r7_filter-=r/PSG_TICK_STEP)<0)
		psg_r7_filter=0;
	while (r>=PSG_TICK_STEP) // not 0, see r%=... below
	{
		r-=PSG_TICK_STEP;
		if (--psg_noise_count<=0) // update noise
		{
			psg_noise_count=psg_noise_limit;
			psg_noise_state=(psg_noise_state^psg_noise_trash)&1;
			psg_noise_trash=(psg_noise_trash>>1)+((psg_noise_trash^(psg_noise_trash>>3))<<16);
		}
		audio_table[16]=audio_table[psg_hard_level^psg_hard_flag2^((psg_hard_style&2)?psg_hard_flag0:0)]; // update hard envelope
		if (--psg_hard_count<=0) // update hard envelope
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
		for (int c=0;c<3;++c)
			if (--psg_tone_count[c]<=0) // update channel
				psg_tone_count[c]=psg_tone_limit[c],psg_tone_state[c]=~psg_tone_state[c];
		#if AUDIO_CHANNELS > 1
		static int n=0,o0=0,o1=0,p=0; // output averaging variables
		#else
		static int n=0,o=0,p=0; // output averaging variables
		#endif
		p+=AUDIO_PLAYBACK<<PSG_MAIN_EXTRABITS;
		while (p>0&&n<(1<<PSG_MAIN_EXTRABITS))
		{
			for (int c=0;c<3;++c)
				if (psg_tone_state[c]|(psg_tone_catch[c]&(7*1))) // is the channel active?
					if (psg_noise_state|(psg_tone_catch[c]&(7*8))) // is the channel noisy?
						#if AUDIO_CHANNELS > 1
						o0+=audio_table[psg_tone_power[c]]*psg_stereo[c][0],
						o1+=audio_table[psg_tone_power[c]]*psg_stereo[c][1];
						#else
						o+=audio_table[psg_tone_power[c]];
						#endif
			++n;
			p-=TICKS_PER_SECOND/PSG_TICK_STEP;
		}
		if (n>=(1<<PSG_MAIN_EXTRABITS)) // enough data to write a sample? (TODO: can `n` be >1<<N ???)
		{
			#if AUDIO_CHANNELS > 1
			*audio_target++=(o0+n/2)/(n<<(24-AUDIO_BITDEPTH))+AUDIO_ZERO; // rounded average (left)
			*audio_target++=(o1+n/2)/(n<<(24-AUDIO_BITDEPTH))+AUDIO_ZERO; // rounded average (right)
			o0=o1=n=0;//%=1<<PSG_MAIN_EXTRABITS; // reset output averaging variables
			#else
			*audio_target++=(o+n/2)/(n<<(16-AUDIO_BITDEPTH))+AUDIO_ZERO; // rounded average
			o=n=0;//%=1<<PSG_MAIN_EXTRABITS; // reset output averaging variables
			#endif
			if (++audio_pos_z>=AUDIO_LENGTH_Z)
			{
				r%=PSG_TICK_STEP; // throw ticks away!
				break; // stop making sound right now!
			}
		}
	}
}

// Again, the PlayCity extension requires its own logic, as it "piggybacks" on top of the central AY chip;
// notice how there are several differences in the timing (configurable), the mixing and the streamlining.

#ifdef PSG_PLAYCITY
void playcity_main(AUDIO_UNIT *t,int l)
{
	int dirty_l=playcity_table[0][7]==0x3F,dirty_h=playcity_table[1][7]!=0x3F;
	if (dirty_l>dirty_h||!l) return; // disabled chips? no buffer!
	int playcity_tone_limit[2][3],playcity_tone_power[2][3],playcity_tone_mixer[2][3],playcity_noise_limit[2],playcity_hard_limit[2];
	static int playcity_tone_count[2][3],playcity_tone_state[2][3]={{0,0,0},{0,0,0}},playcity_noise_state[2],playcity_noise_count[2],playcity_noise_trash[2]={1,1},playcity_hard_power[2];
	for (int x=dirty_l;x<=dirty_h;++x)
	{
		for (int c=0;c<3;++c) // preload channel limits
		{
			playcity_tone_limit[x][c]=playcity_table[x][c*2+0]+playcity_table[x][c*2+1]*256;
			playcity_tone_power[x][c]=playcity_table[x][c*1+8];
			playcity_tone_mixer[x][c]=playcity_table[x][7]>>c;
		}
		if (!(playcity_noise_limit[x]=playcity_table[x][6]*2)) // noise limits
			playcity_noise_limit[x]=2; // half the rate
		if (!(playcity_hard_limit[x]=(playcity_table[x][11]+playcity_table[x][12]*256)*2)) // hard envelope limits
			playcity_hard_limit[x]=2; // half, ditto
	}
	#if AUDIO_CHANNELS > 1
	static int n=0,o0=0,o1=0,p=0;
	#else
	static int n=0,o=0,p=0;
	#endif
	int playcity_clock_hi=(playcity_clock?playcity_clock*2-1:2)*125000,playcity_clock_lo=(playcity_clock?playcity_clock:1)*AUDIO_PLAYBACK;
	for (;;)
	{
		p+=playcity_clock_hi;
		while (p>=0)
		{
			for (int x=dirty_l;x<=dirty_h;++x) // update all chips
			{
				if (--playcity_noise_count[x]<=0) // update noises
					playcity_noise_count[x]=playcity_noise_limit[x],
					playcity_noise_state[x]=(playcity_noise_state[x]^playcity_noise_trash[x])&1,
					playcity_noise_trash[x]=(playcity_noise_trash[x]>>1)+((playcity_noise_trash[x]^(playcity_noise_trash[x]>>3))<<16);
				playcity_hard_power[x]=playcity_hard_level[x]^playcity_hard_flag2[x]^((playcity_hard_style[x]&2)?playcity_hard_flag0[x]:0); // update hard envelopes
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
				for (int c=0;c<3;++c) // update channels
				{
					if (--playcity_tone_count[x][c]<=0)
						playcity_tone_count[x][c]=playcity_tone_limit[x][c],
						playcity_tone_state[x][c]=~playcity_tone_state[x][c];
				/*}
			}
			for (int x=dirty_l;x<=dirty_h;++x) // generate outputs
			{
				for (int c=0;c<3;++c)
				{*/
					if (playcity_tone_state[x][c]|(playcity_tone_mixer[x][c]&1)) // active channel?
						if (playcity_noise_state[x]|(playcity_tone_mixer[x][c]&8)) // noisy channel?
						{
							int z=playcity_tone_power[x][c];
							if (z&16)
								z=playcity_hard_power[x];
							if ((z-=2)>0) // each channel in Playcity plays at half the normal intensity
								#if AUDIO_CHANNELS > 1
								o0+=audio_table[z]*playcity_stereo[x][0],
								o1+=audio_table[z]*playcity_stereo[x][1];
								#else
								o+=audio_table[z];
								#endif
						}
				}
			}
			++n;
			p-=playcity_clock_lo;
		}
		// generate samples, negative (-50% x2) to avoid overflows against the central AY chip (+100%)
		if (n>=(1<<PSG_MAIN_EXTRABITS)) // enough data to write a sample? (TODO: can `n` be >1<<N ???)
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
