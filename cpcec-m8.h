 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The MOS Technology 6581 sound chip and its updated 8580 version are
// better known as the SID chips that gave the Commodore 64 its famed
// acoustics and made it the most powerful musical chip of 1982.

// This module can emulate up to three SID chips at the same time;
// it can also generate a pseudo YM channel log (cfr. cpcec-ym.h).

// BEGINNING OF MOS 6581/8580 EMULATION ============================== //

char sid_tone_shape[3][3],sid_tone_noisy[3][3],sid_tone_stage[3][3]; // oscillator + ADSR short values
int sid_tone_count[3][4],sid_tone_limit[3][3],sid_tone_pulse[3][3],sid_tone_value[3][3],sid_tone_power[3][3]; // oscillator long values
int sid_tone_cycle[3][3],sid_tone_adsr[3][4][3],*sid_tone_syncc[3][3],*sid_tone_ringg[3][3]; // ADSR long values, counters and pointers
const int sid_adsr_table[16]={ 1,4,8,12,19,28,34,40,50,125,250,400,500,1500,2500,4000 }; // official milliseconds >>1
#if AUDIO_CHANNELS > 1
int sid_stereo[3][2]; // the three chips' LEFT and RIGHT weights
#endif
char sid_chips=1; // emulate just the first chip by default; up to three chips are supported
char sid_nouveau=1; // MOS 8580 (new) and 6581 (old) have slighly different wave shape tables and filters
#ifdef DEBUG
BYTE sid_quiet[3]={0,0,0}; // optional muting of channels
#endif

#define SID_STEP_FAST_ADSR (128+9)
#define SID_STEP_SLOW_ADSR (512-6)

// the normalised range [-128..+127] is a reasonable margin, but the mixer must remember to shift right!
const INT8 sid_shape_model[2][4][256]={ // crudely adapted from several tables: FRODO 4.1, SIDPLAYER 4.4...
{ // MOS 6581
	{ // 3: hybrid
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-120,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-112,- 68,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-120,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-112,- 68,
	},
	{ // 5: pulse + triangle
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,+  0,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,+  0,
		-128,-128,-128,-128,-128,-128,+  0,+ 64,-128,+  0,+  0,+ 96,+  0,+ 96,+112,+124,
		+127,+124,+122,+112,+118,+ 96,+ 96,+  0,+110,+ 96,+ 96,+  0,+ 64,-128,-128,-128,
		+ 94,+ 64,+ 64,-128,+  0,-128,-128,-128,+  0,-128,-128,-128,-128,-128,-128,-128,
		+ 62,+  0,+  0,-128,+  0,-128,-128,-128,+  0,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-  2,- 64,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
	},
	{ // 6: pulse + sawtooth
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-  8,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-  8,
	},
	{ // 7: pulse + hybrid
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
	},
},
{ // MOS 8580
	{ // 3: hybrid
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-120,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-104,- 68,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-100,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,+  0,-128,+  0,+  0,
		+ 64,+ 64,+ 64,+ 64,+ 64,+ 64,+ 64,+ 96,+112,+112,+112,+112,+120,+120,+124,+126,
	},
	{ // 5: pulse + triangle
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		+127,+124,+120,+112,+116,+112,+112,+ 96,+108,+ 96,+ 96,+ 64,+ 96,+ 64,+ 64,+ 64,
		+ 92,+ 64,+ 64,+ 64,+ 64,+ 64,+  0,+  0,+ 64,+  0,+  0,+  0,+  0,+  0,-128,-128,
		+ 62,+ 32,+  0,+  0,+  0,+  0,+  0,-128,+  0,+  0,-128,-128,-128,-128,-128,-128,
		+  0,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-  2,- 16,- 32,-128,- 64,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
	},
	{ // 6: pulse + sawtooth
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,+  0,
		-128,-128,-128,-128,-128,-128,+  0,+  0,-128,+  0,+  0,+  0,+  0,+  0,+ 48,+ 62,
		-128,-128,-128,-128,-128,-128,-128,+  0,-128,-128,-128,+  0,+  0,+  0,+  0,+ 64,
		-128,+  0,+  0,+  0,+  0,+  0,+  0,+ 64,+  0,+  0,+ 64,+ 64,+ 64,+ 64,+ 64,+ 92,
		+  0,+  0,+  0,+ 64,+ 64,+ 64,+ 64,+ 64,+ 64,+ 64,+ 64,+ 96,+ 96,+ 96,+ 96,+108,
		+ 64,+ 96,+ 96,+ 96,+ 96,+112,+112,+116,+112,+112,+120,+120,+120,+124,+126,+127,
	},
	{ // 7: pulse + hybrid
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,
		-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,+  0,+  0,
		+  0,+  0,+  0,+  0,+  0,+  0,+ 64,+ 64,+ 64,+ 64,+ 96,+ 96,+ 96,+112,+120,+124,
	},
},
};
INT8 sid_shape_table[8][512]; // shapes: +1 TRIANGLE +2 SAWTOOTH +4 PULSE; +8 NOISE goes elsewhere
void sid_shape_setup(void)
{
	sid_nouveau&=1; for (int i=0;i<512;++i)
	{
		sid_shape_table[0][i]=+0; // 0: none (all +0)
		sid_shape_table[1][i]=i<256?i-128:255-i+128; // 1: triangle (-128..+127,+127..-128)
		sid_shape_table[2][i]=i/2-128; // 2: sawtooth (-128..+127)
		sid_shape_table[3][i]=sid_shape_model[sid_nouveau][0][i>>1];
		sid_shape_table[4][i]=+127; // 4: pulse + none (all +127)
		sid_shape_table[5][i]=sid_shape_model[sid_nouveau][1][i>>1];
		sid_shape_table[6][i]=sid_shape_model[sid_nouveau][2][i>>1];
		sid_shape_table[7][i]=sid_shape_model[sid_nouveau][3][i>>1];
	}
}

BYTE sid_mute_i8[256],sid_mute_h8[256],sid_mute_m8[256],sid_mute_l8[256]; // noise precalc'd tables, see below
void sid_setup(void)
{
	MEMZERO(sid_tone_power); MEMZERO(sid_tone_count); // the fourth channel is the dummy reference when flag bits 1 and 2 are not used
	for (int x=0;x<3;++x)
		sid_tone_syncc[x][0]=sid_tone_syncc[x][1]=sid_tone_syncc[x][2]=sid_tone_ringg[x][0]=sid_tone_ringg[x][1]=sid_tone_ringg[x][2]=&sid_tone_count[x][3]; // ditto
	//sid_mute_byte27[x]=((u>>13)&128)+((u>>12)&64)+((u>> 9)&32)+((u>> 7)&16)+((u>> 6)&8)+((u>> 3)&4)+((u>> 1)&2)+((u>>0)&1); // scramble bits!
	for (int i=0;i<256;++i) // let's hope the new docs are right this time... the old expression was set two bits too far to the right :-(
		sid_mute_i8[i]=((i>>(22-16))^(i>>(17-16)))&1,
		sid_mute_h8[i]=((i<<1)&128)+((i<<2)&64)+((i<<5)&32), // .7.6...5
		sid_mute_m8[i]=((i>>1)& 16)            +((i<<0)& 8), // ........:..4.3...
		sid_mute_l8[i]=((i>>5)&  4)+((i>>3)& 2)+((i>>2)& 1); // ........:........:2..1.0..
	sid_shape_setup();
}

char sid_filters=1,sid_samples=1,sid_delay[3]; INT8 sid_filter_raw[3][3],sid_filter_flt[3][3]; // filter states, mixing bitmasks and output values
int sid_voice[3],sid_mixer[3],sid_filtered[3]; // sampled speech and digidrums
double sid_filter_hw[3],sid_filter_bw[3],sid_filter_lw[3]; // I hate mixing [long] ints and [double] floats...
double sid_filter_qu[3],sid_filter_fu[3]; // Chamberlin filter parameters; they're always above 0.0 and below 2.0 but they need precision
double sid_filter_h[3],sid_filter_b[3],sid_filter_l[3],sid_filter_m[3]; // Chamberlin temporary values; `int` causes noisy precision loss
#define sid_filter_zero(x) (sid_filter_h[x]=sid_filter_b[x]=sid_filter_l[x]=sid_filter_m[x]=0)

void sid_reg_update(int x,int i)
{
	int c=i/7; switch (i)
	{
		case  0: case  7: case 14: // voice frequency lo-byte
		case  1: case  8: case 15: // voice frequency hi-byte
			sid_tone_limit[x][c]=SID_TABLE[x][c*7+0]+SID_TABLE[x][c*7+1]*256; break; // voice frequency
		case  2: case  9: case 16: // pulse wave duty lo-byte
		case  3: case 10: case 17: // pulse wave duty hi-byte
			if ((sid_tone_pulse[x][c]=(SID_TABLE[x][c*7+2]+(SID_TABLE[x][c*7+3]&15)*256)<<8)>=0XFFF00) sid_tone_pulse[x][c]=0X100000; // voice duty cycle; pad it if required
			break;
		case  4: case 11: case 18: // voice control
			i=SID_TABLE[x][i];
			sid_tone_syncc[x][c>0?c-1:2]=&sid_tone_count[x][(i&2)?c:3]; // SYNC target (bit 1)
			sid_tone_ringg[x][c]=&sid_tone_count[x][(i&4)?c>0?c-1:2:3]; // RING source (bit 2)
			if (i&1) // attack/release trigger (bit 0)
				{ if (sid_tone_stage[x][c]>=2) sid_tone_cycle[x][c]=0,sid_tone_stage[x][c]=0; }
			else
				{ if (sid_tone_stage[x][c]< 2) sid_tone_cycle[x][c]=0,sid_tone_stage[x][c]=2; }
			if (i&8) // TEST mode (bit 3) locks the channel! it can be used to play samples in two ways:
			{
				// 1.- "CHIMERA" (Spectrum-like speech in menu and game over) sends $41 (PULSE+NONE) and $49 (PULSE+NONE+TEST), playing a loud 1-bit sample;
				// the setup enables PULSE (bit 6) and sets its parameter to $FFF (max)
				// 2.- "LMAN - AMAZING DISCOVERIES" writes $11 and $09 to reg.18, then a byte to reg.15, and finally $01 to reg.18, playing an 8-bit sample (reg.15) as a TRIANGLE wave;
				// the setup disables PULSE and sets it to $000 (min); the samples rely on 0 (NONE) behaving like TEST (at least for a short while) if it was its previous state;
				sid_tone_value[x][c]=(i<64?SID_TABLE[x][c*7+1]-128:127)*sid_tone_power[x][c],sid_tone_count[x][c]=0; // i<64:"LMAN":"CHIMERA"
				sid_tone_shape[x][c]=(i>>4)+16; // "ALL ROADS LEAD TO UIT" uses the same player from "LMAN..." with sampled speech at the beginning of the song.
			}
			else sid_tone_shape[x][c]=i>>4; // set the wave type: 0..3 NONE/TRIANGLE/SAWTOOTH/HYBRID, +4 PULSE, +8 NOISE
			break;
		case  5: case 12: case 19: // ADSR attack/decay
			sid_tone_adsr[x][0][c]=sid_adsr_table[SID_TABLE[x][i]>>4]; // attack delay (high nibble)
			sid_tone_adsr[x][1][c]=sid_adsr_table[SID_TABLE[x][i]&15]; // + decay delay (low nibble)
			if ((i=sid_tone_stage[x][c])<2&&sid_tone_cycle[x][c]>sid_tone_adsr[x][i][c]) sid_tone_cycle[x][c]=sid_tone_adsr[x][i][c]; // catch overflow!
			break;
		case  6: case 13: case 20: // ADSR sustain/release
			sid_tone_adsr[x][2][c]=sid_adsr_table[SID_TABLE[x][i]&15]; // release delay (low nibble)
			sid_tone_adsr[x][3][c]=((SID_TABLE[x][i]>>4)*SID_MAX_VOICE+7)/15; // + sustain level (high nibble); it goes last to make the indices match the stage
			if (sid_tone_stage[x][c]==2&&sid_tone_cycle[x][c]>sid_tone_adsr[x][2][c]) sid_tone_cycle[x][c]=sid_tone_adsr[x][2][c]; // catch overflow!
			break;
		case 24: // filter mode/volume control
			i=SID_TABLE[x][24]; sid_filter_hw[x]=(i>>6)&1,sid_filter_bw[x]=(i>>5)&1,sid_filter_lw[x]=(i>4)&1;
			if (sid_samples)
			{
				// a linear table makes "STORMLORD" (range 5-12) and "TURBO OUT RUN" MENU (range 4-11) sound softer than expected.
				// (question: why does "TURBO OUT RUN" TITLE use the full range 0-15!?)
				const INT8 k[16]={ -126,-122,-116,-108,-96,-80,-56,-24,+24,+56,+80,+96,+108,+116,+122,+126 };
				i=k[i&15]*(SID_MAX_VOICE*2); // empirical multiplier, mostly comparing "STORMLORD" and "ECHOFIED"
				if (sid_voice[x]!=i) sid_voice[x]=i,sid_delay[x]=0;
			}
			// no `break`!
		case 23: // filter resonance control
			for (c=0,i=(sid_filters&&(SID_TABLE[x][24]&112))?SID_TABLE[x][23]:0;c<3;i>>=1,++c)
				sid_filter_flt[x][c]=~(sid_filter_raw[x][c]=(i&1)-1); // channel output masks
			if (SID_TABLE[x][24]&128) sid_filter_raw[x][2]=sid_filter_flt[x][2]=0; // bit 7: channel 3 is disabled!!
			#ifdef DEBUG
			if (!x) for (c=0;c<3;++c) if (sid_quiet[c]) sid_filter_raw[x][c]=sid_filter_flt[x][c]=0;
			#endif
			// the following expression is just a guess :-(
			sid_filter_qu[x]=1.44-(SID_TABLE[x][23]>>4)/16.0; // "Eliminator", "LED Storm" and "Turbo Out Run" are very sensible to this!
			break;
		case 22: // filter cutoff frequency: hi-byte
		//case 21: // filter cutoff frequency: lo-byte. Attention: the valid bits (bottom 3) are ALMOST always zero!
			i=(SID_TABLE[x][22]<<3);//+(SID_TABLE[x][21]&7); // i.e. 0..2047
			// the following expressions are guesses, too :-(
			//sid_filter_fu[x]=i*0.480/2048.0;
			//sid_filter_fu[x]=SDL_sin(i*M_PI*0.480/4096.0); // noise appears if SIN(x) can reach 1.0, cfr. "Swingers"
			sid_filter_fu[x]=SDL_pow(i/2048.0,1.25); // the wind in the intro of "Last Ninja 3" relies on this!
			break;
	}
}
void sid_update(int x)
{
	for (int i=1;i<25;++i) // i=0;...
		if (i!= 2&&i!= 7&&i!= 9&&i!=14&&i!=16&&i!=21) // i!= 0&&...
			sid_reg_update(x,i); // don't hit register pairs twice
}
void sid_all_update(void)
	{ for (int x=0;x<3;++x) sid_update(x);  }
void sid_reset(int x)
	{ sid_mixer[x]=255; memset(SID_TABLE[x],0,32); sid_voice[x]=0,sid_filter_zero(x); sid_update(x); }
void sid_all_reset(void)
	{ for (int x=0;x<3;++x) sid_reset(x); }

// audio output ----------------------------------------------------- //

void sid_main(int t/*,int d*/)
{
	static int r=0; // audio clock is slower, so remainder is kept here
	if (audio_pos_z>=AUDIO_LENGTH_Z||(r+=t<<SID_MAIN_EXTRABITS)<0) return; // nothing to do!
	#if AUDIO_CHANNELS > 1
	/*d=-d<<8;*/
	#else
	/*d=-d;*/
	#endif
	do
	{
		static unsigned int crash[3]={1,1,1};
		#if AUDIO_CHANNELS > 1
		static int n=0,o0=0,o1=0; // output averages
		#else
		static int n=0,o=0; // output average
		#endif
		#if SID_MAIN_EXTRABITS
		static int a=1; if (!--a)
		#endif
		{
			#if SID_MAIN_EXTRABITS
			a=1<<SID_MAIN_EXTRABITS;
			#endif
			for (int x=sid_chips;x--;)
			{
				// notice that the "real" LFSR is handled outside this function, as it must "tick" even when sound is off
				crash[x]<<=1; crash[x]+=(((crash[x]>>23)^(crash[x]>>18))&1); // 23-bit LFSR randomizer
				// "On shifting, bit 0 is filled with bit 22 EXOR bit 17." ( http://www.oxyron.de/html/registers_sid.html )
				for (int c=0,u,v;c<3;++c)
				{
					if (--sid_tone_cycle[x][c]<=0) // update the channels' ADSR?
						switch (sid_tone_stage[x][c]) // notice that the SID is internally handling the amplitudes as 8-bit values
						{
							case 0: // ATTACK
								sid_tone_cycle[x][c]=sid_tone_adsr[x][0][c];
								if ((sid_tone_power[x][c]+=(SID_MAX_VOICE+64)/SID_STEP_FAST_ADSR)>=SID_MAX_VOICE)
									sid_tone_stage[x][c]=1,sid_tone_power[x][c]=SID_MAX_VOICE; // rise is linear
								break;
							case 1: // DECAY + SUSTAIN
								sid_tone_cycle[x][c]=sid_tone_adsr[x][1][c]; // float towards the right volume (NOT linear though)
								if (LIKELY((/*u=*/(v=sid_tone_adsr[x][3][c])-sid_tone_power[x][c])<0))
									{ if ((sid_tone_power[x][c]=(sid_tone_power[x][c]*SID_STEP_SLOW_ADSR)>>9)<v) sid_tone_power[x][c]=v; }
								//else if (UNLIKELY(u>0))
									//{ if ((sid_tone_power[x][c]+=(SID_MAX_VOICE+64)/SID_STEP_FAST_ADSR)>v) sid_tone_power[x][c]=v; } // rise!?
								else
									sid_tone_cycle[x][c]=1<<9;
								break;
							case 2: // RELEASE
								sid_tone_cycle[x][c]=sid_tone_adsr[x][2][c];
								if ((sid_tone_power[x][c]=(sid_tone_power[x][c]*SID_STEP_SLOW_ADSR)>>9)>0) break; // fall is NOT linear either
								sid_tone_stage[x][c]=3; // no `break`!
							default: // SILENCE
								sid_tone_power[x][c]=0,sid_tone_cycle[x][c]=1<<9;
						}
					if ((u=sid_tone_shape[x][c])<16) // update the channels' wave generators? TEST mode must be off!
					{
						if ((sid_tone_count[x][c]=(v=sid_tone_count[x][c])+sid_tone_limit[x][c])&~0XFFFFF) // OVERFLOW?
							*sid_tone_syncc[x][c]=sid_tone_count[x][c]&=0XFFFFF;
						if (u<4) // TRIANGLE/SAWTOOTH/HYBRID? (NONE is already set by sid_reg_update)
							{ if (u) sid_tone_value[x][c]=sid_shape_table[u][((*sid_tone_ringg[x][c]&0X80000)^sid_tone_count[x][c])>>11]*sid_tone_power[x][c]; }
						else if (u<8) // PULSE?
							sid_tone_value[x][c]=(sid_tone_count[x][c]>=sid_tone_pulse[x][c]?sid_shape_table[u][sid_tone_count[x][c]>>11]:-128)*sid_tone_power[x][c];
						else // NOISE? beware, a noisy channel pointed by "ringg" cannot do `sid_tone_count[x][c]&=0XFFFF`: "Rasputin", "Swingers"...
							{ { if ((v^sid_tone_count[x][c])&~0XFFFF) sid_tone_noisy[x][c]=(INT8)crash[x]; } sid_tone_value[x][c]=sid_tone_noisy[x][c]*sid_tone_power[x][c]; }
					}
				}
				//if (sid_mixer[x]) // skip calculations if the chip is muted // the digis must play in "PULSOID" despite the bogus filter!
				{
					static int w[3]={0,0,0}; // minimal antialiasing; it helps!
					sid_filtered[x]=(w[x]+sid_voice[x]+(w[x]<sid_voice[x]?1:0))>>1;
					if (sid_filters) // skip the calculations if filters are off
					{
						w[x]=sid_filtered[x]; // 2nd degree
						// the Chamberlin expressions merged in a single big block!
						int i=(sid_filter_flt[x][0]&sid_tone_value[x][0])+(sid_filter_flt[x][1]&sid_tone_value[x][1])+(sid_filter_flt[x][2]&sid_tone_value[x][2]);
						sid_filter_b[x]+=sid_filter_fu[x]*(sid_filter_h[x]=(sid_filter_m[x]=i-sid_filter_qu[x]*sid_filter_b[x])-(sid_filter_l[x]+=sid_filter_fu[x]*sid_filter_b[x]));
						int m=sid_filter_h[x]*sid_filter_hw[x]+sid_filter_b[x]*sid_filter_bw[x]+sid_filter_l[x]*sid_filter_lw[x]+.5;
						sid_filtered[x]+=(sid_nouveau?m+((i-m)>>8):(m+i)>>1); // 8580: source <<< filter; 6581: 50% source, 50% filter; yet another guess :-/
					}
					else w[x]=sid_voice[x]; // 1st degree
				}
			}
		}
		// mix all channels' output together!
		#define SID_MIX_CHANNEL(x,c) (sid_filter_raw[x][c]&sid_tone_value[x][c])
		for (int x=sid_chips;x--;)
		{
			int m=((SID_MIX_CHANNEL(x,0)+SID_MIX_CHANNEL(x,1)+SID_MIX_CHANNEL(x,2)+sid_filtered[x])*sid_mixer[x])>>8; // reduce signal loss, split >>16 into two >>8
			#if AUDIO_CHANNELS > 1
			o0+=(m*sid_stereo[x][0])>>8;
			o1+=(m*sid_stereo[x][1])>>8;
			#else
			o+=m>>8;
			#endif
		}
		/*
		#if AUDIO_CHANNELS > 1
		o0+=d,
		o1+=d;
		#else
		o+=d;
		#endif
		*/
		++n;
		static int b=0; if ((b-=(AUDIO_PLAYBACK*SID_TICK_STEP)>>SID_MAIN_EXTRABITS)<=0)
		{
			b+=TICKS_PER_SECOND;
			#if AUDIO_CHANNELS > 1
			int dd=n<<(24-AUDIO_BITDEPTH),qq;
			*audio_target++=(qq=o0/dd)+AUDIO_ZERO,o0-=qq*dd, // rounded average (left)
			*audio_target++=(qq=o1/dd)+AUDIO_ZERO,o1-=qq*dd; // rounded average (right)
			#else
			int dd=n<<(16-AUDIO_BITDEPTH),qq;
			*audio_target++=(qq=o /dd)+AUDIO_ZERO,o -=qq*dd; // rounded average
			#endif
			if (n=0,++audio_pos_z>=AUDIO_LENGTH_Z) r%=SID_TICK_STEP; // end of buffer!
		}
	}
	while ((r-=SID_TICK_STEP)>=0);
}

// other operations ------------------------------------------------- //

// even when the SID is inactive or disabled by the user, ports 27 and 28 still generate values;
// and unlike the SID sound playback, they need to be precisely calculated to a single-T degree.

#define sid_mute_tick() (sid_randomize=sid_randomize*2+sid_mute_i8[(sid_randomize>>16)&255])
int sid_randomize=1; // all SID chips share the same LFSR >:-)
int sid_mute_t=0,sid_mute_r=0; // major+minor counters
int sid_mute_int27[3]; BYTE sid_mute_bit27[3],sid_mute_byte27[3];
int sid_mute_int28[3]; BYTE sid_mute_bit28[3],sid_mute_byte28[3];
void sid_mute(void) // emulate ports 27 and 28 until we catch up to the SID_MUTE_TIME clock
{
	unsigned int t=SID_MUTE_TIME-sid_mute_t; sid_mute_t=SID_MUTE_TIME;
	sid_mute_r+=t*2; unsigned int tt=sid_mute_r/SID_TICK_STEP; sid_mute_r%=SID_TICK_STEP; // the "t*2" is important, calculations aren't the same as above!
	for (int x=sid_chips;--x>=0;)
	{
		// update oscillator, either noise or tone
		if (sid_mute_bit27[x]) // noise! "MAZEMANIA" checks this in $4E33 and others, "REWIND" in $1167 and $12CF, "4KRAWALL" in the final screen...
			sid_mute_byte27[x]=sid_mute_h8[(sid_randomize>>16)&255]+sid_mute_m8[(sid_randomize>>8)&255]+sid_mute_l8[sid_randomize&255]; // precalc'd tables
		else // note! "BOX CHECK TEST" relies on it in $C091; "TO NORAH" expects valid extra SID chips to return zero when BYTE +18 is completely empty!
		{
			unsigned int u=sid_mute_int27[x]+tt*sid_tone_limit[x][2]; // regular wave generator: overflows aren't important, only the lowest twenty-something bits matter.
			sid_mute_byte27[x]=(!x||SID_TABLE[x][18])? // "ENCHANTED FOREST" aka "EF BY SAMAR" expects the extra SIDs to react slightly differently to the built-in chip!
				(((t=sid_tone_shape[x][2]&7)&4)&&(u&0X1FFFFF)<sid_tone_pulse[x][2]*2)?0: // this is how "ECHOFIED" turns the audio output into an "echo"!
				sid_shape_table[t][((u-(sid_nouveau<<7))>>12)&511]+128:0; // 8580 copies the value before updating it, 6581 updates it before copying it!
			sid_mute_int27[x]=u; // the final scene of "REWIND" sets the whole SID chip to zero but still relies on the LFSR randomizer, so this must stick!
		}
		t=tt+sid_mute_int28[x]; // update envelope, as defined by the ADSR
		BYTE k; if ((k=SID_TABLE[x][18]&1)>sid_mute_bit28[x]) // attack?
		{
			int i=sid_adsr_table[SID_TABLE[x][19]>>4];
			i<<=4; while (t>=i&&sid_mute_byte28[x]<255-16) // 1<<4=16 ticks/step
				sid_mute_byte28[x]+=16,t-=i; // save cycles, if possible
			i>>=4; while (t>=i) // single-tick step
			{
				if ((t-=i),sid_mute_byte28[x]>=255)
					{ sid_mute_bit28[x]=1; break; }
				++sid_mute_byte28[x];
			}
		}
		if (k<=sid_mute_bit28[x]) // decay or release? (notice the `break` above)
		{
			BYTE j=(k?(SID_TABLE[x][20]>>4)*17:0);
			if (sid_mute_byte28[x]>j) // fall to level?
			{
				int i=(k?sid_adsr_table[SID_TABLE[x][19]&15]:sid_adsr_table[SID_TABLE[x][20]&15])*2;
				k=sid_mute_byte28[x];
				while (t>=i)
					if ((t-=i),(k-=(k<160?1:k<224?2:3))<=j) // this has an impact on the SFX of "CLOUD KINGDOMS", but I don't know the right values :-(
						{ sid_mute_byte28[x]=j; t=0; break; }
				sid_mute_byte28[x]=k;
			}
			else t=0; // we reached the limit, nothing left to do!
		}
		else t=0; // nothing to do!
		sid_mute_int28[x]=t;
	}
}
#define sid_mute_peek27(x) (sid_mute(),sid_mute_byte27[x]) // returns value of port 27 of SID chip `x`
#define sid_mute_peek28(x) (sid_mute(),sid_mute_byte28[x]) // ditto, port 28
//#define sid_mute_dumb18(x) sid_mute() // redundant, now one function updates all chips
void sid_mute_poke18(int x,int b) // traps writes to port 18: they can trigger the oscillator and the envelope!
{
	sid_mute();
	if ((b^SID_TABLE[x][18])&(128+1)) // catch when we toggle ADSR or noise!
		sid_mute_int27[x]=sid_mute_bit27[x]=b>=128?1:0; // AFAIK the wave itself is NOT reset when we toggle ADSR only :-/
	if ((b^SID_TABLE[x][18])&1) // catch when we toggle ADSR!
		sid_mute_int28[x]=sid_mute_bit28[x]=0; // not sure if we should check this more carefully...
}

void sid_frame(void) // the equivalent of dac_frame() in other machines
{
	for (int x=sid_chips;--x>=0;) // possibly redundant AFAIK: samples are always played with just the built-in SID
	{
		int z=(SID_TABLE[x][24]&15)*17; if (sid_mixer[x]<z) ++sid_mixer[x]; // the mixer takes a while to catch up
		sid_mixer[x]=(sid_mixer[x]*3+z)>>2; // f.e. the intro of "Stormlord" plays samples ($35..$3C) but the menu just clobbers the mixer ($3F)
		z=SID_TABLE[x][24]&15; // clobbering the mixer must keep the "dac_voice", f.e. noise in "International Karate 1/2/Plus" ($04 in IK, $14 in IK+)
		if (sid_delay[x]) sid_voice[x]=sid_voice[x]*63/64; else ++sid_delay[x];
	}
	sid_mute(); // update mute timers
}

// =================================== END OF MOS 6581/8580 EMULATION //
