 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The MOS Technology 8580 sound chip and its antecessor 6581 are
// better known as the SID chips that gave the Commodore 64 its famed
// acoustics and made it the most powerful musical chip of 1982.

// This module can emulate up to three SID chips at the same time;
// it can also generate a pseudo YM channel log (cfr. cpcec-ay.h).

// BEGINNING OF MOS 8580/6581 EMULATION ============================== //

BYTE sid_tone_shape[3][3],sid_tone_noisy[3][3],sid_tone_stage[3][3]; // oscillator + ADSR short values
int sid_tone_count[3][4],sid_tone_limit[3][3],sid_tone_pulse[3][3],sid_tone_value[3][3],sid_tone_power[3][3]; // oscillator long values
int sid_tone_cycle[3][3],sid_tone_adsr[3][4][3],*sid_tone_syncc[3][3],*sid_tone_ringg[3][3]; // ADSR long values, counters and pointers
const short sid_adsr_table[16]={1,4,8,12,19,28,34,40,50,125,250,400,500,1500,2500,4000}; // official milliseconds >>1
#if AUDIO_CHANNELS > 1
int sid_stereo[3][3][2]; // the three chips' three channels' LEFT and RIGHT weights
#endif

#if 1 // experimental
int sid_filters=1,sid_filter_mul[3][4],sid_filter_div[3][4],sid_filter_add[3][4];
#endif

BYTE sid_nouveau=0; // MOS 8580:6581 flag, right now limited to the handling of mixer-based samples
BYTE sid_shape_table[8][256]; // shapes: triangle +1 and sawtooth +2; pulse +4 and noise +8 go apart
// the range [0..+128] gives us a reasonable margin, but we must remember to perform `>>7` in the mixer!
void sid_setup(void)
{
	MEMZERO(sid_tone_power); MEMZERO(sid_tone_count); // the fourth channel is the dummy reference when flag bits 1 and 2 are not used
	for (int x=0;x<3;++x)
		sid_tone_syncc[x][0]=sid_tone_syncc[x][1]=sid_tone_syncc[x][2]=sid_tone_ringg[x][0]=sid_tone_ringg[x][1]=sid_tone_ringg[x][2]=&sid_tone_count[x][3]; // ditto
	for (int i=0;i<256;++i)
	{
		sid_shape_table[0][i]=0; // silence!
		sid_shape_table[5][i]=sid_shape_table[1][i]=i<128?i:256-i; // triangle (0..127,128..1)
		sid_shape_table[6][i]=sid_shape_table[2][i]=i<128?i/2:i/2+1; // sawtooth (0..63,65..128)
		sid_shape_table[7][i]=sid_shape_table[3][i]=(sid_shape_table[1][i]*sid_shape_table[2][i])>>7;
		sid_shape_table[4][i]=128; // locked!
	}
}

int sid_mixer[3],sid_voice[3],sid_digis[3]; // the sampled speech and the digidrums go here!
int sid_smash[3]={0,0,0}; // this must be public, unlike `crash` -- it can be requested thru register 27:
// ((smash>>15)&128)+((smash>>14) &64)+((smash>>11) &32)+((smash>> 9) &16)+((smash>> 8) & 8)+((smash>> 5) & 4)+((smash>> 3) & 2)+((smash>> 2) & 1);

void sid_reg_update(int x,int i)
{
	int c=i/7; ++sid_digis[x]; switch (i)
	{
		case  0: case  7: case 14:
		case  1: case  8: case 15:
			sid_tone_limit[x][c]=SID_TABLE[x][c*7+0]+SID_TABLE[x][c*7+1]*256; break; // voice frequency
		case  2: case  9: case 16:
		case  3: case 10: case 17:
			sid_tone_pulse[x][c]=(SID_TABLE[x][c*7+2]+(SID_TABLE[x][c*7+3]&15)*256)<<8; break; // voice duty cycle
		case  4: case 11: case 18:
			if ((i=SID_TABLE[x][i])&1) // attack/release trigger (bit 0)
				{ if (sid_tone_stage[x][c]>=2) sid_tone_cycle[x][c]=0,sid_tone_stage[x][c]=/*sid_tone_value[x][c]=*/0; } // does the value REALLY reset?
			else
				{ if (sid_tone_stage[x][c]< 2) sid_tone_cycle[x][c]=0,sid_tone_stage[x][c]=2; }
			sid_tone_syncc[x][c>0?c-1:2]=&sid_tone_count[x][(i&2)?c:3]; // SYNC target (bit 1)
			sid_tone_ringg[x][c]=&sid_tone_count[x][(i&4)?c>0?c-1:2:3]; // RING source (bit 2)
			if (i&8) // TEST mode (bit 3) locks the channel! (f.e. "CHIMERA": Spectrum-like speech)
				sid_tone_shape[x][c]=16,sid_tone_count[x][c]=0,sid_tone_value[x][c]=sid_tone_power[x][c]<<7;
			else if (i&128) // NOISE overrides signals!
				sid_tone_shape[x][c]=8;
			else // voice shape (high nibble)
				sid_tone_shape[x][c]=(i>>4)&7;
			break;
		case  5: case 12: case 19:
			sid_tone_adsr[x][0][c]=sid_adsr_table[SID_TABLE[x][i]>>4], // attack delay (high nibble)
			sid_tone_adsr[x][1][c]=sid_adsr_table[SID_TABLE[x][i]&15]; // + decay delay (low nibble)
			if ((i=sid_tone_stage[x][c])<2&&sid_tone_cycle[x][c]>sid_tone_adsr[x][i][c]) sid_tone_cycle[x][c]=sid_tone_adsr[x][i][c]; // catch overflow!
			break;
		case  6: case 13: case 20:
			sid_tone_adsr[x][2][c]=(SID_TABLE[x][i]>>4)*((audio_channel+7)/15), // sustain level (high nibble)
			sid_tone_adsr[x][3][c]=sid_adsr_table[SID_TABLE[x][i]&15]; // + release delay (low nibble)
			if (sid_tone_stage[x][c]==2&&sid_tone_cycle[x][c]>sid_tone_adsr[x][3][c]) sid_tone_cycle[x][c]=sid_tone_adsr[x][3][c]; // catch overflow!
			break;
		#if 1 // experimental
		case 21: // filter cutoff frequency: lo-byte
		case 22: // filter cutoff frequency: hi-byte
		case 23: // filter resonance control
			c=(65535-(SID_TABLE[x][22]*256+SID_TABLE[x][21])+128)/256+(SID_TABLE[x][23]>>4); i=SID_TABLE[x][23]*sid_filters;
			sid_filter_add[x][0]=(sid_filter_div[x][0]=(sid_filter_mul[x][0]=((i&1)?c:0))+16)/2;
			sid_filter_add[x][1]=(sid_filter_div[x][1]=(sid_filter_mul[x][1]=((i&2)?c:0))+16)/2;
			sid_filter_add[x][2]=(sid_filter_div[x][2]=(sid_filter_mul[x][2]=((i&4)?c:0))+16)/2;
			sid_filter_add[x][3]=(sid_filter_div[x][3]=(sid_filter_mul[x][3]=((i&8)?c:0))+16)/2;
			break;
		#endif
		case 24: // how do the differences between the 8580 and the 6581 apply here?
			i=SID_TABLE[x][24]&15; if (sid_nouveau) sid_mixer[x]=i*17; else sid_voice[x]=((15-i)*audio_channel+7)/15;
			break;
	}
}
void sid_update(int x)
{
	for (int i=0;i<25;++i)
		if (i!= 0&&i!= 2&&i!= 7&&i!= 9&&i!=14&&i!=16&&i!=21)
			sid_reg_update(x,i); // don't hit register pairs twice
}
void sid_all_update(void)
	{ for (int x=0;x<3;++x) sid_update(x); }
void sid_reset(int x)
	{ sid_mixer[x]=255; sid_voice[x]=SID_TABLE[x][4]=SID_TABLE[x][11]=SID_TABLE[x][18]=SID_TABLE[x][24]=0; sid_update(x); }
void sid_all_reset(void)
	{ for (int x=0;x<3;++x) sid_reset(x); }

// audio output ----------------------------------------------------- //

const int sid_main_1st=0,sid_main_inc=1; int sid_main_end=1; // emulate just the first chip by default

void sid_main(int t,int d)
{
	static int r=0; // audio clock is slower, so remainder is kept here
	if (audio_pos_z>=AUDIO_LENGTH_Z||(r+=t<<SID_MAIN_EXTRABITS)<0) return; // nothing to do!
	#if AUDIO_CHANNELS > 1
	d=-d<<8;
	int f[3][2]={ // the following values can be precalc'd (no playback changing)
		{sid_voice[0]*sid_stereo[0][1][0],sid_voice[0]*sid_stereo[0][1][1]},
		{sid_voice[1]*sid_stereo[1][1][0],sid_voice[1]*sid_stereo[1][1][1]},
		{sid_voice[2]*sid_stereo[2][1][0],sid_voice[2]*sid_stereo[2][1][1]}};
	#else
	d=-d;
	#endif
	do
	{
		static int crash[3]={1,1,1};
		#if AUDIO_CHANNELS > 1
		static int n=0,o0=0,o1=0; // output averaging variables
		#else
		static int n=0,o=0; // output averaging variables
		#endif
		#if SID_MAIN_EXTRABITS
		static int a=1; if (!--a)
		#endif
		{
			#if SID_MAIN_EXTRABITS
			a=1<<SID_MAIN_EXTRABITS;
			#endif
			for (int x=sid_main_1st;x<sid_main_end;x+=sid_main_inc)
			{
				// update the white noise
				if (crash[x]&1) crash[x]+=0X840000; sid_smash[x]^=(crash[x]>>=1); // LFSR x2
				const int x3=(audio_channel+5*8)/(5*16),x1=(audio_channel+15*8)/(15*16);
				for (int c=0,d,e;c<3;++c)
				{
					if (--sid_tone_cycle[x][c]<=0) // update the channels' ADSR?
						switch (sid_tone_stage[x][c])
						{
							// assuming a purely linear model of 0-15 levels of 16 steps each
							case 0: sid_tone_cycle[x][c]=sid_tone_adsr[x][0][c]; if ((sid_tone_power[x][c]+=x3)>=audio_channel) sid_tone_stage[x][c]=1,sid_tone_power[x][c]=audio_channel; break; // ATTACK
							case 1: sid_tone_cycle[x][c]=sid_tone_adsr[x][1][c]; // float towards the right volume
								if ((d=(e=sid_tone_adsr[x][2][c])-sid_tone_power[x][c])<0)
									{ if ((sid_tone_power[x][c]-=x1)<e) sid_tone_power[x][c]=e; }
								else if (d>0)
									{ if ((sid_tone_power[x][c]+=x1)>e) sid_tone_power[x][c]=e; }
								else
									sid_tone_cycle[x][c]=1<<9;
								break; // DECAY + SUSTAIN
							case 2: sid_tone_cycle[x][c]=sid_tone_adsr[x][3][c]; if ((sid_tone_power[x][c]-=x1)<=0) sid_tone_stage[x][c]=3,sid_tone_power[x][c]=0; break; // RELEASE
							default: sid_tone_cycle[x][c]=1<<9; break; // /SILENCE
						}
					if ((d=sid_tone_shape[x][c])<16) // update the channels' wave generators?
					{
						if ((sid_tone_count[x][c]=(e=sid_tone_count[x][c])+sid_tone_limit[x][c])&~0XFFFFF) // OVERFLOW?
							*sid_tone_syncc[x][c]=sid_tone_count[x][c]&=0XFFFFF;
						if (d<4) // TRIANGLE/SAWTOOTH/BOTH/NEITHER?
							sid_tone_value[x][c]=sid_shape_table[d][((*sid_tone_ringg[x][c]&0X80000)^sid_tone_count[x][c])>>12]*sid_tone_power[x][c];
						else if (d<8) // PULSE? (plus TRIANGLE)
							sid_tone_value[x][c]=sid_tone_count[x][c]<sid_tone_pulse[x][c]?0:sid_shape_table[d][sid_tone_count[x][c]>>12]*sid_tone_power[x][c];
						else // NOISE?
						{
							// beware! when a noisy channel is the target of "ringg" we cannot do `sid_tone_count[x][c]&=0XFFFF`! ("Rasputin", "Swingers"...)
							if ((e^sid_tone_count[x][c])&~0XFFFF) sid_tone_noisy[x][c]=sid_smash[x]&127;
							sid_tone_value[x][c]=sid_tone_noisy[x][c]*sid_tone_power[x][c];
						}
					}
				}
			}
		}
		// mix all channels' output together: +A -B +C -DIGI
		for (int m,x=sid_main_1st;x<sid_main_end;x+=sid_main_inc)
		{
			#if 1 // experimental
			static int g[3][3]={{0,0,0},{0,0,0},{0,0,0}};
			#define SID_TRANSFORM_VALUE(x,c) ((m=(sid_tone_value[x][c]*sid_mixer[x])>>15),(g[x][c]=(m*16+(g[x][c]+(m>g[x][c]))*sid_filter_mul[x][c]+sid_filter_add[x][c])/sid_filter_div[x][c]))
			#else
			#define SID_TRANSFORM_VALUE(x,c) ((sid_tone_value[x][c]*sid_mixer[x])>>15)
			#endif
			#if AUDIO_CHANNELS > 1
			o0-=f[x][0];
			o1-=f[x][1];
			m=SID_TRANSFORM_VALUE(x,0);
			o0+=m*sid_stereo[x][0][0],
			o1+=m*sid_stereo[x][0][1];
			m=SID_TRANSFORM_VALUE(x,1);
			o0-=m*sid_stereo[x][1][0],
			o1-=m*sid_stereo[x][1][1];
			if (!(SID_TABLE[x][24]&128))
			{
				m=SID_TRANSFORM_VALUE(x,2);
				o0+=m*sid_stereo[x][2][0],
				o1+=m*sid_stereo[x][2][1];
			}
			#else
			o-=sid_voice[x];
			o+=SID_TRANSFORM_VALUE(x,0);
			o-=SID_TRANSFORM_VALUE(x,1);
			if (!(SID_TABLE[x][24]&128))
				o+=SID_TRANSFORM_VALUE(x,2);
			#endif
		}
		#if AUDIO_CHANNELS > 1
		o0+=d,
		o1+=d;
		#else
		o+=d;
		#endif
		++n;
		static int b=0; if ((b-=AUDIO_PLAYBACK*SID_TICK_STEP>>SID_MAIN_EXTRABITS)<=0)
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
			if (++audio_pos_z>=AUDIO_LENGTH_Z) r%=SID_TICK_STEP; // end of buffer!
		}
	}
	while ((r-=SID_TICK_STEP)>=0);
}

void sid_frame(void) // avoid hissing: for example, "Stormlord" intro plays 3-bit samples ($35..$3C) but its menu just clobbers the mixer ($3F)
{
	for (int x=0,i;x<3;++x) if (sid_nouveau) sid_voice[x]>>=1; else
	{
		if (sid_digis[x]<4) // don't reduce hissing in "International Karate 1/2/Plus"
			sid_voice[x]=(sid_voice[x]*7)>>3;
		else
		{
			i=(SID_TABLE[x][24]&15)*17;
			sid_mixer[x]=(i+(sid_mixer[x]+(i>sid_mixer[x]))*7)>>3;
		}
		sid_digis[x]=0;
	}
}

// YM3 file logging ------------------------------------------------- //

char psg_tmpname[STRMAX]="";
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
			while (l=fread1(psg_tmp,sizeof(psg_log)*14,psg_tmpfile))
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
int psg_writelog_aux(int i)
	{ int j=16,k=audio_channel; for (;;) if (!--j||(i>=(k=(k*181)>>8))) return j; } // rough approximation
void psg_writelog(void)
{
	if (psg_logfile)
	{
		int i,n=511,x=0X38; // we must adjust YM values to a 2 MHz clock.
		i=SID_TABLE_0[ 0]+SID_TABLE_0[ 1]*256; if (sid_tone_shape[0][0]&16) i=0,x|=1; else if (sid_tone_shape[0][0]&8) n&=(i>>5)&31,i=0,x&=~ 8;
		i=i>512?(512*4096+i/2)/i:0; // convert channel 1 wavelength on the fly
		psg_tmp[psg_tmpsize++]=i; psg_tmp[psg_tmpsize++]=i>>8;
		i=SID_TABLE_0[ 7]+SID_TABLE_0[ 8]*256; if (sid_tone_shape[0][1]&16) i=0,x|=2; else if (sid_tone_shape[0][1]&8) n&=(i>>5)&31,i=0,x&=~16;
		i=i>512?(512*4096+i/2)/i:0; // ditto, channel 2 wavelength
		psg_tmp[psg_tmpsize++]=i; psg_tmp[psg_tmpsize++]=i>>8;
		i=SID_TABLE_0[14]+SID_TABLE_0[15]*256; if (sid_tone_shape[0][2]&16) i=0,x|=4; else if (sid_tone_shape[0][2]&8) n&=(i>>5)&31,i=0,x&=~32;
		i=i>512?(512*4096+i/2)/i:0; // ditto, channel 3 wavelength
		psg_tmp[psg_tmpsize++]=i; psg_tmp[psg_tmpsize++]=i>>8;
		psg_tmp[psg_tmpsize++]=n; // noise wavelength
		psg_tmp[psg_tmpsize++]=x+(sid_tone_shape[0][0]&16?1:0)+(sid_tone_shape[0][1]&16?2:0)+(sid_tone_shape[0][2]&16?4:0); // mixer
		psg_tmp[psg_tmpsize++]=psg_writelog_aux(sid_tone_power[0][0]); // convert channel 1 amplitude on the fly
		psg_tmp[psg_tmpsize++]=psg_writelog_aux(sid_tone_power[0][1]); // ditto, channel 2 amplitude
		psg_tmp[psg_tmpsize++]=psg_writelog_aux(sid_tone_power[0][2]); // ditto, channel 3 amplitude
		psg_tmp[psg_tmpsize++]=0; psg_tmp[psg_tmpsize++]=0; // hard envelope wavelength
		psg_tmp[psg_tmpsize++]=0; // hard envelope type
		//psg_hard_log=0xFF; // 0xFF means the hard envelope doesn't change
		if (psg_tmpsize>=sizeof(psg_tmp))
			fwrite1(psg_tmp,sizeof(psg_tmp),psg_tmpfile),psg_tmpsize=0;
	}
}

// =================================== END OF MOS 8580/6581 EMULATION //
