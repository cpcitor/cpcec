 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// Because the goal of the emulation itself is to be OS-independent,
// the interactions between the emulator and the OS are kept behind an
// interface of variables and procedures that don't require particular
// knowledge of the emulation's intrinsic properties.

#define MY_VERSION "20240505" // all emulators share the same release date

#define INLINE // 'inline' is useless in TCC and GCC4, and harmful in GCC5!
#define UNUSED // '__attribute__((unused))' may be missing outside GCC
#if __GNUC__ >= 4 // optional branch prediction hints
#define LIKELY(x) __builtin_expect(!!(x),1) // see 'likely' from Linux
#define UNLIKELY(x) __builtin_expect((x),0) // see 'unlikely'
#else // branch prediction hints are unreliable outside GCC!!
#define LIKELY(x) (x) // not ready, fall back
#define UNLIKELY(x) (x) // ditto
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1 // reduce dependencies!
#endif
#else
#ifndef SDL2
#define SDL2 // SDL2 is required in non-Win32 systems
#endif
#endif

#ifdef DEBUG // debug-mode console texts
#define cprintf(...) fprintf(stdout,__VA_ARGS__)
#define cputchar(x) fputc((x),stdout)
#define cputs(x) fputs((x),stdout)
#else // warning: parameters won't be eval'd!
#define cprintf(...) ((void)0)
#define cputchar(x) ((void)0)
#define cputs(x) ((void)0)
#endif

// several convenient shortcuts
#define length(x) (sizeof(x)/sizeof*(x))
#define MEMZERO(x) memset((x),0,sizeof(x))
#define MEMBYTE(x,z) memset((x),(z),sizeof(x))
#define MEMSAVE(x,y) memcpy((x),(y),sizeof(y))
#define MEMLOAD(x,y) memcpy((x),(y),sizeof(x))
#define MEMNCPY(x,y,z) memcpy((x),(y),sizeof(*(x))*(z))
#define STRCOPY(x,y) ((x)!=(y)?strcpy((x),(y)):(x)) // avoid accidents!
INLINE int lcase(int i) { return i>='A'&&i<='Z'?i+32:i; }
INLINE int ucase(int i) { return i>='a'&&i<='z'?i-32:i; }

//#define MAIN_FRAMESKIP_BITS 4 // 1<<4=16 < 25 < 30 < 1<<5=32
#define MAIN_FRAMESKIP_BITS 5 // 1<<5=32 < 50 < 60 < 1<<6=64
#define AUDIO_PLAYBACK 44100 // 22050, 24000, 44100, 48000
//#define AUDIO_L2BUFFER 15 // =32K samples (667..750 ms)
#define AUDIO_L2BUFFER 14 // =16K samples (333..375 ms)
#define AUDIO_CHANNELS 2 // 1 for mono, 2 for stereo

char session_caption[]=MY_CAPTION " " MY_VERSION;
unsigned char session_scratch[1<<18]; // at least 256k
#include "cpcec-a8.h" // unsigned char onscreen_chrs[];
#define ONSCREEN_SIZE 12 // (sizeof(onscreen_chrs)/95)
#define ONSCREEN_CEIL 256 // i.e. 256 glyphs (0..255)

int video_pos_x=0,video_pos_y=0,frame_pos_y=0,audio_pos_z=0; // for keeping pointers within range
int video_pos_z=0; // frame counter for timekeeping, statistics and debugging
char video_interlaced=0,video_interlaces=0; // video scanline status (on or off, odd or even)
signed char video_framelimit=0,video_framecount=0; // video frameskip counters; they must be signed!
char audio_disabled=0,audio_required=0; int audio_session=0; // audio playing/recording status and counter
// "disabled" and "required" are independent because audio recording on wave and film files must happen even if the emulator is mute or sped-up
#ifndef audio_fastmute
#define audio_fastmute 1 // is there any reason to play sound back at full speed? :-(
#endif

#define SESSION_SIGNAL_FRAME 1
#define SESSION_SIGNAL_DEBUG 2
#define SESSION_SIGNAL_PAUSE 4
int session_timer,session_event=0; // timing synchronisation and user command
char session_fast=0,session_rhythm=0,session_wait=0,session_softblit=1,session_hardblit,session_softplay=0; // software blitting enabled by default
char session_audio=1,session_stick=1,session_shift=0,session_key2joy=0; // keyboard and joystick
int session_maus_x=0,session_maus_y=0; // mouse coordinates (debugger + SDL2 UI + optional emulation)
#ifdef MAUS_EMULATION
int session_maus_z=0; // optional mouse and lightgun
#endif
char video_filter=0,audio_filter=0,video_fineblend=0; // filter flags
char session_fullblit=0,session_zoomblit=0,session_version[16]; // OS label, [8] was too short
char session_paused=0,session_signal=0,session_signal_frames=0,session_signal_scanlines=0;
char session_dirty=0; // cfr. session_clean()

#define session_getscanline(i) (&video_frame[i*VIDEO_LENGTH_X+VIDEO_OFFSET_X]) // pointer to scanline `i`
FILE *session_wavefile=NULL,*session_filmfile=NULL; // audio + video recording is done on each done frame
void session_writewave(void); // save the current sample frame. Must be defined later on!
void session_writefilm(void); int session_closefilm(void); // must be defined later on, too!
#ifndef VIDEO_PLAYBACK // variable mode?
int AUDIO_LENGTH_Z; char VIDEO_PLAYBACK=0;
int session_ntsc(int q) // sets NTSC (60 Hz) mode if `q` is nonzero, sets PAL (50 Hz) mode instead; returns `q`
{
	int c=q?60:50; if (VIDEO_PLAYBACK!=c)
	{
		session_closefilm(); session_dirty=1; // films can't run multiple framerates!
		AUDIO_LENGTH_Z=AUDIO_PLAYBACK/(VIDEO_PLAYBACK=c); // division must be exact!
	}
	return q;
}
#else // fixed mode, either PAL or NTSC!
#define AUDIO_LENGTH_Z (AUDIO_PLAYBACK/VIDEO_PLAYBACK)
#define session_ntsc(q) (q)
#endif

#define kbd_bit_set(k) (kbd_bit[(k)>>3]|=1<<((k)&7))
#define kbd_bit_res(k) (kbd_bit[(k)>>3]&=~(1<<((k)&7)))
#define kbd_bit_tst(k) ((kbd_bit[(k)>>3]|joy_bit[(k)>>3])&(1<<((k)&7)))
unsigned char kbd_bit[16],joy_bit[16]; // up to 128 keys in 16 rows of 8 bits
unsigned char txt_session_scan[]="Press a key or ESCAPE"; // cfr. redefine virtual joystick

void session_clean(void); // "clean" dirty settings; will be defined later on!
void session_user(int); // handle the user's commands; will be defined later on, too!
int debug_xlat(int); // translate debug keys into codes
void session_debug_show(int); // redraw the debugger text
int session_debug_user(int); // debug logic takes priority: 0 UNKNOWN COMMAND, !0 OK
INLINE void audio_playframe(void); // handle the sound filters

#ifdef SDL2 // optional inside Win32, required elsewhere!
#include "cpcec-ox.h"
#else // Win32 (+4.0)
#include "cpcec-os.h"
#endif

// The following routines can be shared across emulators and operating
// systems because they either operate with standard data types or
// rely on talking to the C standard library rather than the OS.

// START OF OS-INDEPENDENT ROUTINES ================================= //

#define GPL_3_INFO \
	"This program comes with ABSOLUTELY NO WARRANTY; for more details" GPL_3_LF \
	"please see the GNU General Public License. This is free software" GPL_3_LF \
	"and you are welcome to redistribute it under certain conditions."

// byte-order based operations -------------------------------------- //

// based on the hypothetical `mgetc(x) (*(x))` and `mputc(x,y) (*(x)=(y))`,
// and considering 'i' = lil-endian (Intel) and 'm' = big-endian (Motorola)
// notice that the fgetXXXX functions cannot ensure that a negative number always means EOF was met!

#if SDL_BYTEORDER == SDL_BIG_ENDIAN // big-endian: PPC, ARM...

typedef union { WORD w; struct { BYTE h,l; } b; } HLII;
typedef union { WORD w; struct { BYTE l,h; } b; } HLMM;
//typedef union { DWORD d; struct { BYTE a,r,g,b; } b; } ARGB32;

int mgetii(const unsigned char *x) { return (x[1]<<8)+*x; }
int mputii(unsigned char *x,int y) { return x[1]=y>>8,*x=y,y; }
int mgetiiii(const unsigned char *x) { return (x[3]<<24)+(x[2]<<16)+(x[1]<<8)+*x; }
int mputiiii(unsigned char *x,int y) { return x[3]=y>>24,x[2]=y>>16,x[1]=y>>8,*x=y,y; }
#define mgetmm(x) (*(WORD*)(x))
#define mgetmmmm(x) (*(DWORD*)(x))
#define mputmm(x,y) ((*(WORD*)(x))=(y))
#define mputmmmm(x,y) ((*(DWORD*)(x))=(y))
// WORD/DWORD `i` must be a constant!
#define equalsmm(x,i) (*(WORD*)(x)==(i))
#define equalsmmmm(x,i) (*(DWORD*)(x)==(i))
#define equalsii(x,i) (*(WORD*)(x)==(WORD)(((i>>8)&255)+((i&255)<<8)))
#define equalsiiii(x,i) (*(DWORD*)(x)==(DWORD)(((i>>24)&255)+((i&(255<<16))>>8)+((i&(255<<8))<<8)+((i&255)<<24)))
#define kputmm fputmm
#define kputmmmm fputmmmm

int fgetii(FILE *f) { int i=fgetc(f); return i|(fgetc(f)<<8); } // common lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { fputc(i,f); return fputc(i>>8,f); } // common lil-endian 16-bit fputc()
int fgetiii(FILE *f) { int i=fgetc(f); i|=fgetc(f)<<8; return i|(fgetc(f)<<16); } // common lil-endian 24-bit fgetc()
int fputiii(int i,FILE *f) { fputc(i,f); fputc(i>>8,f); return fputc(i>>16,f); } // common lil-endian 24-bit fputc()
int fgetiiii(FILE *f) { int i=fgetc(f); i|=fgetc(f)<<8; i|=fgetc(f)<<16; return i|(fgetc(f)<<24); } // common lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { fputc(i,f); fputc(i>>8,f); fputc(i>>16,f); return fputc(i>>24,f); } // common lil-endian 32-bit fputc()

#else // lil-endian: i86, x64... following Z80, MOS 6502, etc.

typedef union { WORD w; struct { BYTE l,h; } b; } HLII;
typedef union { WORD w; struct { BYTE h,l; } b; } HLMM;
//typedef union { DWORD d; struct { BYTE b,g,r,a; } b; } ARGB32;

#define mgetii(x) (*(WORD*)(x))
#define mgetiiii(x) (*(DWORD*)(x))
#define mputii(x,y) ((*(WORD*)(x))=(y))
#define mputiiii(x,y) ((*(DWORD*)(x))=(y))
int mgetmm(const unsigned char *x) { return (*x<<8)+x[1]; }
int mputmm(unsigned char *x,int y) { return *x=y>>8,x[1]=y,y; }
int mgetmmmm(const unsigned char *x) { return (*x<<24)+(x[1]<<16)+(x[2]<<8)+x[3]; }
int mputmmmm(unsigned char *x,int y) { return *x=y>>24,x[1]=y>>16,x[2]=y>>8,x[3]=y,y; }
// WORD/DWORD `i` must be a constant!
#define equalsii(x,i) (*(WORD*)(x)==(i))
#define equalsiiii(x,i) (*(DWORD*)(x)==(i))
#define equalsmm(x,i) (*(WORD*)(x)==(WORD)(((i>>8)&255)+((i&255)<<8)))
#define equalsmmmm(x,i) (*(DWORD*)(x)==(DWORD)(((i>>24)&255)+((i&(255<<16))>>8)+((i&(255<<8))<<8)+((i&255)<<24)))
#define kputmm(i,f) fputii((((i)>>8)&255)+(((i)&255)<<8),f)
#define kputmmmm(i,f) fputiiii((((i)>>24)&255)+(((i)>>8)&65280)+(((i)&65280)<<8)+(((i)&255)<<24),f)

int fgetii(FILE *f) { int i=0; return (fread(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { return (fwrite(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fputc()
int fgetiii(FILE *f) { int i=0; return (fread(&i,1,3,f)!=3)?EOF:i; } // native lil-endian 24-bit fgetc()
int fputiii(int i,FILE *f) { return (fwrite(&i,1,3,f)!=3)?EOF:i; } // native lil-endian 24-bit fputc()
int fgetiiii(FILE *f) { int i=0; return (fread(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { return (fwrite(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fputc()

#endif

// these calls behave differently on 32-bit and 64-bit big-endian systems! they cannot be native!
#define kputii fputii
#define kputiiii fputiiii
int fgetmm(FILE *f) { int i=fgetc(f)<<8; return i|fgetc(f); } // common big-endian 16-bit fgetc()
int fputmm(int i,FILE *f) { fputc(i>>8,f); return fputc(i,f); } // common big-endian 16-bit fputc()
int fgetmmm(FILE *f) { int i=fgetc(f)<<16; i|=fgetc(f)<<8; return i|fgetc(f); } // common big-endian 24-bit fgetc()
int fputmmm(int i,FILE *f) { fputc(i>>16,f); fputc(i>>8,f); return fputc(i,f); } // common big-endian 24-bit fputc()
int fgetmmmm(FILE *f) { int i=fgetc(f)<<24; i|=fgetc(f)<<16; i|=fgetc(f)<<8; return i|fgetc(f); } // common big-endian 32-bit fgetc()
int fputmmmm(int i,FILE *f) { fputc(i>>24,f); fputc(i>>16,f); fputc(i>>8,f); return fputc(i,f); } // common big-endian 32-bit fputc()

// auxiliary functions ---------------------------------------------- //

// safe fread(t,1,l,f) and fwrite(t,1,l,f)
int fread1(void *t,int l,FILE *f) { int i,j=0; while ((i=fread((char*)t+j,1,l-j,f))&&((j+=i)<l)) {} return j; }
int fwrite1(void *t,int l,FILE *f) { int i,j=0; while ((i=fwrite((char*)t+j,1,l-j,f))&&((j+=i)<l)) {} return j; }

const char hexa1[16]="0123456789ABCDEF"; // handy for `*t++=` operations
int eval_hex(int c) { return (c>='0'&&c<='9')?c-'0':((c|=32)>='a'&&c<='f')?c-'a'+10:-1; } // 0..15 OK, <0 ERROR!
int eval_dec(int c) { return (c>='0'&&c<='9')?c-'0':-1; } // 0..9 OK, <0 ERROR!
void byte2hexa(char *t,const BYTE *s,int n) { int z; while (n-->0) z=*s++,*t++=hexa1[z>>4],*t++=hexa1[z&15]; } // string-unfriendly function!
int byte2hexa0(char *t,const BYTE *s,int n) { if (n>0) { byte2hexa(t,s,n); t[n*=2]=0; } return n; } // string-friendly function (trailing NUL)
int hexa2byte(BYTE *t,const char *s,int n) { int h,l; while (n>0&&(h=eval_hex(*s++))>=0&&(l=eval_hex(*s++))>=0) *t++=(h<<4)+l,--n; return n; }

#if __GNUC__ >= 4 // GCC4+ intrinsic!
#define log2u(x) (__builtin_clz(x)^(sizeof(unsigned int)*8-1)) // `x` must be >0!
//#define sqrtu(x) ((int)(__builtin_sqrt(x)))
#else // slow but compatible
int log2u(unsigned int x) { int i=0; while (x>>=1) ++i; return i; } // same here!
#endif
int sqrtu(unsigned int x) { if (x<2) return x; unsigned int y=x>>1,z; while (y>(z=(x/y+y)>>1)) y=z; return y; } // kudos to Hero of Alexandria!

const BYTE rbits[256]= // ((i&128)>>7)+((i&64)>>5)+((i&32)>>3)+((i&16)>>1)+((i&8)<<1)+((i&4)<<3)+((i&2)<<5)+((i&1)<<7)
{
	0,128,64,192,32,160, 96,224,16,144,80,208,48,176,112,240, 8,136,72,200,40,168,104,232,24,152,88,216,56,184,120,248,
	4,132,68,196,36,164,100,228,20,148,84,212,52,180,116,244,12,140,76,204,44,172,108,236,28,156,92,220,60,188,124,252,
	2,130,66,194,34,162, 98,226,18,146,82,210,50,178,114,242,10,138,74,202,42,170,106,234,26,154,90,218,58,186,122,250,
	6,134,70,198,38,166,102,230,22,150,86,214,54,182,118,246,14,142,78,206,46,174,110,238,30,158,94,222,62,190,126,254,
	1,129,65,193,33,161, 97,225,17,145,81,209,49,177,113,241, 9,137,73,201,41,169,105,233,25,153,89,217,57,185,121,249,
	5,133,69,197,37,165,101,229,21,149,85,213,53,181,117,245,13,141,77,205,45,173,109,237,29,157,93,221,61,189,125,253,
	3,131,67,195,35,163, 99,227,19,147,83,211,51,179,115,243,11,139,75,203,43,171,107,235,27,155,91,219,59,187,123,251,
	7,135,71,199,39,167,103,231,23,151,87,215,55,183,119,247,15,143,79,207,47,175,111,239,31,159,95,223,63,191,127,255,
};
#define rbit8(i) rbits[(BYTE)(i)] // the bit reversal of a single byte can be done with a lookup table access;
WORD rbit16(const WORD i) { return (rbits[i&255]<<8)|rbits[i>>8]; } // words and dwords require additional operations.
//DWORD rbit32(const DWORD i) { return (rbits[i&255]<<24)|(rbits[(i>>8)&255]<<16)|(rbits[(i>>16)&255]<<8)|rbits[i>>24]; } // unused
//QWORD rbit64(const QWORD i) { return (rbits[i&255]<<56)|(rbits[(i>>8)&255]<<48)|(rbits[(i>>16)&255]<<40)|(rbits[(i>>24)&255]<<32)
	//|(rbits[(i>>32)&255]<<24)|(rbits[(i>>40)&255]<<16)|(rbits[(i>>48)&255]<<8)|rbits[i>>56]; } // do we need QWORD at all!?

char *strrstr(char *h,const char *n) // = backwards `strstr` (case sensitive!)
{
	for (char *z=h+strlen(h)-strlen(n);z>=h;--z) // skip last bytes that cannot match
	{
		const char *s=z,*t=n;
		while (*t&&*s==*t) ++s,++t;
		if (!*t) return z;
	}
	return NULL;
}
int globbing(char *w,char *t,int q) // wildcard pattern `*w` against string `*t`; `q` = strcasecmp/strcmp; 0 on mismatch!
{
	char *ww=NULL,*tt=NULL,c,k; // a terribly dumbed-down take on Kirk J. Krauss' algorithm
	if (q) // case insensitive
		while (c=*t)
			if (UNLIKELY((k=*w++)=='*')) // wildcard?
			{
				while (*w=='*') ++w; // skip following wildcards
				if (!*w) return 1; // end of pattern? succeed!
				ww=w,tt=t; // remember wildcard and continue
			}
			else if (LIKELY(k!='?'&&ucase(k)!=ucase(c))) // wrong character?
			{
				if (!ww) return 0; // no past wildcards? fail!
				w=ww,t=++tt; // restore wildcard and continue
			}
			else ++t; // right character, continue
	else // case sensitive
		while (c=*t)
			if (UNLIKELY((k=*w++)=='*')) // wildcard?
			{
				while (*w=='*') ++w; // skip following wildcards
				if (!*w) return 1; // end of pattern? succeed!
				ww=w,tt=t; // remember wildcard and continue
			}
			else if (LIKELY(k!='?'&&k!=c)) // wrong character?
			{
				if (!ww) return 0; // no past wildcards? fail!
				w=ww,t=++tt; // restore wildcard and continue
			}
			else ++t; // right character, continue
	while (*w=='*') ++w; // skip trailing wildcards
	return !*w; // succeed on end of pattern
}
int multiglobbing(const char *w,char *t,int q) // like globbing(), but with multiple patterns with semicolons inbetween; 0 on mismatch, 1..n tells which pattern matches
{
	int n=1,c; char *m; do
	{
		m=session_substr; // the caller must not use this variable
		while ((c=*w++)&&c!=';') *m++=c;
		*m=0; if (globbing(session_substr,t,q)) return n;
	}
	while (++n,c); return 0;
}

#if 0 // we don't need this (yet!)
// packed lists are series of ASCIZ strings with an empty string as the end marker, for example "ABC\000DEF\000XYZ\000\000" means "ABC","DEF","XYZ";
// (for practical reasons (for example ensuring that an empty list has size 0) the size of a packed list is the relative position of its end marker)
int packedappend(char *t,int z,const char *s) // append string `s` to the packed list of strings `t` of length `z`; returns the list's new length
{
	if (!s||!*s) return z; // reject invalid `s`!
	int l=strlen(s)+1; return memcpy(t+=z,s,l),t[l]=0,z+=l;
}
#endif
// case-insensitive sorted packed lists, used by the SDL2 UI and the ZIP archive browsing dialog, are a special case of the packed lists from above;
// these algorithms are weak, but proper binary search is difficult on packed lists; fortunately, items are likely to be partially sorted in advance
int sortedinsert(char *t,int z,const char *s) // insert string `s` in its alphabetical order within the packed list of strings `t` of length `z`; returns the list's new length
{
	if (!s||!*s) return z; // reject invalid `s`!
	char *m=t+z; while (m>t)
	{
		char *n=m; do --n; while (n>t&&n[-1]); // backwards search is more convenient on partially sorted lists, new items will often go last
		if (strcasecmp(s,n)>=0) break; // notice that identical strings are deemed valid; perhaps they shouldn't.
		m=n; // search more...
	}
	int l=strlen(s)+1,i=z-(m-t); if (i>0) memmove(m+l,m,i);
	return memcpy(m,s,l),z+l;
}
int sortedsearch(char *t,int z,const char *s) // look for string `s` in an alphabetically ordered packed list of strings `t` of length `z`; returns index (not offset!) in list
{
	if (!s||!*s) return -1; // invalid `s` is never within `t`!
	char *r=t+z; int i=0,l=strlen(s)+1; while (t<r)
	{
		char *u=t,j; while (*u++) {}
		if (t+l==u&&(j=strcasecmp(s,t))<=0) return j?-1:i; // minor optimisations: skip comparison if lengths don't match, abort if we look for A but we're in B
		t=u; ++i; // search more...
	}
	return -1; // 's' was not found!
}

#ifdef RUNLENGTH_OBSOLETE
// extremely simple bytewise run-length encoding:
// - a single instance of byte X stays unmodified, "X";
// - N=2..256 instances of byte X become the series "X,X,N-2";
// - the end marker is "X,X,255"; X must NOT be the last source byte!
// size checks aside, this codec has no way to catch errors in the streams!
#if 0 // this isn't allowed anymore!
int bin2rle(BYTE *t,int o,const BYTE *s,int i) // encode `i` exact bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	const BYTE *u=t,*r=s+i; BYTE k=0; while (s!=r&&o>=3) // source error!
	{ *t++=k=*s,i=0; while (++s!=r&&k==*s&&i<255) ++i; if (i) *t++=k,*t++=i-1,o-=3; else --o; }
	return o>=3?*t++=(k=~k),*t++=k,*t++=255,t-u:-1; // OK! // target error!
}
#endif
int rle2bin(BYTE *t,int o,const BYTE *s,int i) // decode up to `i` bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	const BYTE *u=t; BYTE k; while (i>=3) // source error!
	{ int r=1; if (k=*s,k!=*++s) --i; else { if ((r=s[1])==255) return t-u; i-=3,s+=2,r+=2; } if ((o-=r)<0) break; do *t++=k; while (--r); } // exit!
	return -1; // from above: target error!
}
#endif
#ifdef RUNLENGTH_ENCODING
// slightly improved bytewise run-length encoding:
// - the first byte is the prefix X, the input's rarest byte;
// - a single instance of a byte that isn't X stays unmodified;
// - 1..3 instances of a byte that is X become the pair "X,0..2";
// - 4..255 instances of any byte Y become the series "X,3..254,Y";
// - the end marker is "X,255,Y", where Y is a global 8-bit checksum.
int bin2rlf(BYTE *t,int o,const BYTE *s,int i) // encode `i` exact bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	const BYTE *u=t,*r=s+i; BYTE k=255,x=255,c; int h[256]; for (int z=0;z<256;++z) h[z]=0; while (i) ++h[s[--i]]; do if (h[x]>=h[--k]) x=k; while (k); c=*t++=x; while (s!=r&&o>=3)
	{ c+=k=*s,i=0; while (++s!=r&&k==*s&&i<254) c+=k,++i; if (i>=3) o-=3,*t++=x,*t++=i,*t++=k; else if (k==x) o-=2,*t++=k,*t++=i; else do --o,*t++=k; while (i--); }
	return o>=3?*t++=x,*t++=255,*t++=c,t-u:-1; // OK! // target error!
}
int rlf2bin(BYTE *t,int o,const BYTE *s,int i) // decode up to `i` bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	const BYTE *u=t; BYTE k,x=*s++,c=x; while (i>=3) // source error!
	{ int r=0; if (--i,(k=*s++)==x) if (--i,(r=*s++)>=3) if (--i,k=*s++,r==255) return c-k?-1:t-u; if ((o-=++r)<0) break; do c+=*t++=k; while (--r); } // exit!
	return -1; // from above: bad checksum! target error!
}
#endif

#ifndef LEMPELZIV_LEVEL
#define LEMPELZIV_LEVEL 6 // catch when LEMPELZIV_ENCODING is enabled but LEMPELZIV_LEVEL is missing!
#endif
#if LEMPELZIV_LEVEL > 0
#define LEMPELZIV_RETRY (1<<((LEMPELZIV_LEVEL)*2-1)) // a value between 2 (LEVEL 1) and 32768 (LEVEL 8); LEVEL >= 9 means RETRY >= RANGE
#else
#define LEMPELZIV_RETRY 0
#endif
#ifdef LEMPELZIV_OBSOLETE
// quick'n'dirty implementation of Emmanuel Marty's bytewise 64K-ranged Lempel-Ziv encoding LZSA1, https://github.com/emmanuel-marty/lzsa (BlockFormat_LZSA1.md)
// this implementation always stores the End of Data marker, whose otherwise unused final offset is used here as a simple 8-bit checksum.
#if 0 // this isn't allowed anymore!
#if !LEMPELZIV_RETRY
#define bin2lze(t,o,s,i) ((t),(o),(s),(i),-1) // never compress!
#else
int bin2lze(BYTE *t,int o,const BYTE *s,int i) // encode `i` exact bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	BYTE *u=t; int h=0,p=0,k=0,z,*hh,*pp; if (!(hh=(int*)malloc(sizeof(int[65536+65536])))) return -1; // memory error! maybe this could go to session_preset()...
	{ for (pp=hh+65536,z=65536+65536;z;) hh[--z]=~65536; } while (p+2<i) // ~65536 < -65536, i.e. setup hashes with values beyond the search range
		{ int g=0,m=0,r=0,n=p-65536; for (int q=hh[s[p+1]*256+s[p]],e=LEMPELZIV_RETRY;q>=n&&e;q=pp[q&65535],(LEMPELZIV_RETRY<65536&&--e)) if (s[q+m]==s[p+m])
			{ int y; if ((z=65535+p)>i) z=i; const BYTE *v=s+q+1,*w=s+p+1,*x=s+z; while (*++v==*++w&&w<x) {} // search in current minimum and maximum
			if (z=w-s-p,g<(y=z-(p-q>256?1:0)-(z<18?0:z<256?1:z<512?2:3))) if (g=y,m=z,r=p-q,w>=x) break; } // early break!
		if (m<3) ++p; else // the LZSA1 standard can't handle more than 65535 literals in a row, hence the better-safe-than-sorry 64K limit
			{ if ((z=p-k)>65535||(o-=z+2)<8) break; *t++=(r>256?128:0)+(z<7?z*16:112)+(m<18?m-3:15);
			if (z>=7) { if (--o,z<256) *t++=z-7; else if (--o,z<512) *t++=250,*t++=z; else --o,*t++=249,*t++=z,*t++=z>>8; } // literal length
			{ while (k<p) *t++=s[k++]; } if (k=p+=m,*t++=-r,r>256) --o,*t++=-r>>8; // flush literals; LZSA1 stores offsets as negative bytes
			if (m>=18) { if (--o,m<256) *t++=m-18; else if (--o,m<512) *t++=239,*t++=m; else --o,*t++=238,*t++=m,*t++=m>>8; } } // match length
		do z=s[h+1]*256+s[h],pp[h&65535]=hh[z],hh[z]=h; while (++h<p); } // update hashes and continue
	if (free(hh),(z=i-k)>65535||o-z<8) return -1; // the end marker's checksum is a byte-sized pseudo-offset, hence the 8-bit range
	if (z<7) *t++=z*16+15; else if (*t++=127,z<256) *t++=z-7; else if (z<512) *t++=250,*t++=z; else *t++=249,*t++=z,*t++=z>>8; // final literals
	{ while (k<i) *t++=s[k++]; } for (z=0;k;) z+=s[--k]; return *t++=z,*t++=238,*t++=0,*t++=0,t-u; // calculate checksum and store end marker
}
#endif
#endif
int lze2bin(BYTE *t,int o,const BYTE *s,int i) // decode up to `i` bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	BYTE c=0,*u=t,z; while (i>4) // the shortest possible end marker is five bytes long (15, checksum, 238, 0, 0)
		{ int r,m=((z=*s++)>>4)&7; if (m>=7&&(--i,(m+=*s++)>=256)) { if (--i,m>256) m=*s+++256; else if (--i,m=*s++,(m+=*s++<<8)<512) break; } // bad header!
		{ if ((o-=m)<0||(i-=m+2)<3) break; } for (;m;--m) c+=*t++=*s++; r=*s++-256; if (z&128) if (--i,(r+=(*s++-255)<<8)>=-256) break; // bad source/length!
		if ((m=(z&15)+3)>=18&&(--i,(m+=*s++)>=256)) { if (--i,m>256) m=*s+++256; else if (--i,m=*s++,(m+=*s++<<8)<512) return (m||(z&128)||r+256-c)?-1:t-u; } // exit!
		const BYTE *v=t+r; if (v<u||(o-=m)<0) break; do c+=*t++=*v++; while (--m); } // bad offset/target!
	return -1; // something went wrong!
}
#endif
#ifdef LEMPELZIV_ENCODING
// quick'n'dirty implementation of Emmanuel Marty's nibblewise 64K-ranged Lempel-Ziv encoding LZSA2, https://github.com/emmanuel-marty/lzsa (BlockFormat_LZSA2.md)
// this implementation always stores the End of Data marker, whose normally undefined final offset is used here as a simple 8-bit checksum.
#if !LEMPELZIV_RETRY
#define bin2lzf(t,o,s,i) ((t),(o),(s),(i),-1) // never compress!
#else
void bin2lzf_encode(BYTE **t,int *o,BYTE *a,BYTE **b,int n) { if (*a) *a=0,**b|=n&15; else --*o,*(*b=(*t)++)=n<<(*a=4); } // store a nibble; see below bin2lzf_n(n)
#define bin2lzf_n(n) bin2lzf_encode(&t,&o,&a,&b,(n)) // without bin2lzf_encode(): (a?a=0,*b|=(n)&15:(--o,*(b=t++)=(n)<<(a=4)))
int bin2lzf(BYTE *t,int o,const BYTE *s,int i) // encode `i` exact bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	BYTE *u=t,a=0,*b; int h=0,p=0,k=0,rr=0,z,*hh,*pp; if (!(hh=(int*)malloc(sizeof(int[65536+65536])))) return -1; // memory error! maybe this should go to session_preset()...
	{ for (pp=hh+65536,z=65536+65536;z;) hh[--z]=~65536; } while (p+1<i) // ~65536 < -65536, i.e. setup hashes with values beyond the search range
		{ int g=0,m=0,r=0,n=p-65536; for (int q=hh[s[p+1]*256+s[p]],e=LEMPELZIV_RETRY;q>=n&&e;q=pp[q&65535],(LEMPELZIV_RETRY<65536&&--e)) if (p-q==rr||s[q+m]==s[p+m])
			{ int y; if ((z=65535+p)>i) z=i; const BYTE *v=s+q+1,*w=s+p+1,*x=s+z; while (*++v==*++w&&w<x) {} // search in current minimum and maximum
			if (z=w-s-p,g<(y=z*2-(p-q==rr?1:p-q>512?p-q>8704?5:4:p-q>32?3:2)-(z<24?z<9?1:2:z<256?4:8))) if (g=y,m=z,r=p-q,w>=x) break; } // early break!
		if (m<2) ++p; else // the LZSA2 standard can't handle more than 65535 literals in a row, hence the better-safe-than-sorry 64K limit
			{ if ((z=p-k)>65535||(o-=z+1)<7) break; *t++=(r==rr?224:r>512?r>8704?192:160-((-r>>3)&32):r>32?96-((-r>>3)&32):((~r&1)<<5))+(z<3?z*8:24)+(m<9?m-2:7);
			if (z>=3) { if (z<18) bin2lzf_n(z-3); else if (bin2lzf_n(15),z<256) --o,*t++=z-18; else o-=3,*t++=239,*t++=z,*t++=z>>8; } // literal length
			{ while (k<p) *t++=s[k++]; } k=p+=m; // flush literals; LZSA2 stores offsets as negative bytes plus a bit in the header, hence the new algebra
			if (rr!=r) { if ((rr=r)>32) { if (r>512) { if (r>8704) *t++=-r>>8; else bin2lzf_n((512-r)>>9); } *t++=-r; } else bin2lzf_n(-r>>1); }
			if (m>=9) { if (m<24) bin2lzf_n(m-9); else if (bin2lzf_n(15),m<256) --o,*t++=m-24; else o-=3,*t++=233,*t++=m,*t++=m>>8; } } // match length
		do z=s[h+1]*256+s[h],pp[h&65535]=hh[z],hh[z]=h; while (++h<p); } // update hashes and continue
	if (free(hh),(z=i-k)>65535||o-z<7) return -1; // the end marker's checksum is a byte-sized pseudo-offset, hence the 9-bit range (bit 8 always set!)
	if (z<3) *t++=z*8+103; else if (*t++=127,z<18) bin2lzf_n(z-3); else if (bin2lzf_n(15),z<256) *t++=z-18; else *t++=239,*t++=z,*t++=z>>8; // final literals
	{ while (k<i) *t++=s[k++]; } for (z=0;k;) z+=s[--k]; return *t++=z,bin2lzf_n(15),*t++=232,t-u; // calculate checksum and store end marker
}
#undef bin2lzf_n
#endif
int lzf2bin_decode(BYTE *a,BYTE *b,const BYTE **s,int *i) { return *a?*a=0,*b&15:(--*i,((*b=*(*s)++)>>(*a=4))); } // fetch a nibble; see below lzf2bin_n()
#define lzf2bin_n() lzf2bin_decode(&a,&b,&s,&i) // without lzf2bin_decode(): (a?a=0,b&15:(--i,((b=*s++)>>(a=4))))
int lzf2bin(BYTE *t,int o,const BYTE *s,int i) // decode up to `i` bytes from source `s` onto up to `o` bytes at target `t`; >=0 output length, <0 ERROR!
{
	BYTE c=0,*u=t,a=0,b,z; for (int r=0;i>2;) // the shortest possible end marker is three bytes long (103, checksum, 232; the nibble after the header may have been preloaded)
		{ int m=((z=*s++)>>3)&3; if (m>=3&&((m+=lzf2bin_n())>=18)) { if (--i,(m+=*s++)>256) if (i-=2,m=*s++,(m+=*s++<<8)<256) break; } // bad header!
		{ if ((o-=m)<0||(i-=m+1)<2) break; } for (;m;--m) c+=*t++=*s++; if (z<128) if (z<64) r=lzf2bin_n()*2-((z>>5)&1)-31; else { if (--i,(r=*s++-((z&32)*8)-256)>=-32) break; }
		else if (z<192) --i,r=lzf2bin_n()*512-((z&32)*8)-8448,r+=*s++; else { if (z<224?i-=2,r=(*s++-256)<<8,(r+=*s++)>=-8704:!r) break; } // bad source/length!
		if ((m=(z&7)+2)>=9&&((m+=lzf2bin_n())>=24)) { if (--i,(m+=*s++)==256) return ((z&~24)-103||r+512-c)?-1:t-u; else if (m>256&&(i-=2,m=*s++,(m+=*s++<<8)<256)) break; } // exit!
		const BYTE *v=t+r; if (v<u||(o-=m)<0) break; do c+=*t++=*v++; while (--m); } // bad offset/target!
	return -1; // something went wrong!
}
#undef lzf2bin_n
#endif

#ifdef SHA1_CALCULATOR
// extremely suboptimal SHA-1 calculator:
// 1.- sha1_init() resets all internal vars before a fresh start;
// 2.- sha1_hash(s) processes a 64-byte page `s` from the stream;
// 3.- sha1_exit(s,0..63) processes the last bytes in the stream,
// and stores the 160-bit result in the 32-bit vars sha1_o[0..4],
// where [0] stores the top 32 bits and [4] stores the bottom 32;
// the "&0XFFFFFFFF" operations are essential on 64-bit machines!
unsigned int sha1_o[5]; long long int sha1_i;
void sha1_init(void)
	{ sha1_o[0]=0x67452301,sha1_o[1]=0xEFCDAB89,sha1_o[2]=0x98BADCFE,sha1_o[3]=0x10325476,sha1_o[4]=0xC3D2E1F0; sha1_i=0; }
void sha1_hash(const BYTE *s)
{
	static unsigned int z[80]; // don't abuse the poor stack! we don't have to allocate and release this buffer again and again!
	unsigned int a=sha1_o[0]&0XFFFFFFFF,b=sha1_o[1]&0XFFFFFFFF,c=sha1_o[2]&0XFFFFFFFF,d=sha1_o[3]&0XFFFFFFFF,e=sha1_o[4]&0XFFFFFFFF,f,i=0;
	for (;i<16;++i) // first 16 fields
		z[i]=s[i*4+0]<<24|s[i*4+1]<<16|s[i*4+2]<<8|s[i*4+3];
	for (;i<80;++i) // up to 80 fields
		f=(z[i-3]^z[i-8]^z[i-14]^z[i-16])&0XFFFFFFFF,z[i]=((f<<1)&0XFFFFFFFF)|f>>31;
	for (i=0;i<80;++i)
	{
		if (i<40)
			if (i<20)
				f=   (((c^d)&b)^d)   +0X5A827999;
			else
				f=      (b^c^d)      +0X6ED9EBA1;
		else
			if (i<60)
				f=((b&c)^(b&d)^(c&d))+0X8F1BBCDC;
			else
				f=      (b^c^d)      +0XCA62C1D6;
		f+=(a<<5)+(a>>27)+e+z[i]; e=d; d=c;
		c=(b<<30)+(b>>2); b=a; a=f&0XFFFFFFFF;
	}
	sha1_i+=512,sha1_o[0]+=a,sha1_o[1]+=b,sha1_o[2]+=c,sha1_o[3]+=d,sha1_o[4]+=e;
}
void sha1_exit(const BYTE *s,int n)
{
	static BYTE t[64]; sha1_i+=n<<3; memcpy(t,s,n); t[n++]=128;
	if (n>=56) { while (n<64) t[n++]=0; sha1_hash(t); n=0; }
	while (n<56) t[n++]=0;
	t[n++]=sha1_i>>56; t[n++]=sha1_i>>48; t[n++]=sha1_i>>40; t[n++]=sha1_i>>32;
	t[n++]=sha1_i>>24; t[n++]=sha1_i>>16; t[n++]=sha1_i>> 8; t[n++]=sha1_i    ;
	sha1_hash(t);
	sha1_o[0]&=0XFFFFFFFF,sha1_o[1]&=0XFFFFFFFF,sha1_o[2]&=0XFFFFFFFF,sha1_o[3]&=0XFFFFFFFF,sha1_o[4]&=0XFFFFFFFF;
}
#endif

// interframe functions --------------------------------------------- //

// warning: the following code assumes that VIDEO_UNIT is DWORD 0X00RRGGBB!
#define VIDEO_FILTER_MASK_Y 1
#define VIDEO_FILTER_MASK_X 2
#define VIDEO_FILTER_MASK_Z 4
#if AUDIO_CHANNELS > 1
const int audio_stereos[][3]={{0,0,0},{+64,0,-64},{+128,0,-128},{+256,0,-256}}; // left, middle and right relative to 100% = 256
#endif
BYTE video_lineblend=0,video_pageblend=0,video_microwave=0,audio_mixmode=length(audio_stereos)-1; // 0 = pure mono... n-1 = pure stereo
BYTE video_scanline=0,video_scanlinez=8; // 0 = all scanlines, 1 = avg. scanlines, 2 = full interlace, 3 = half interlace
int video_microwavez=1; // the microwave noise
#ifdef MAUS_LIGHTGUNS
VIDEO_UNIT video_litegun; // lightgun data
#endif
// *!* are there any systems where VIDEO_UNIT is NOT the DWORD 0X00RRGGBB!? *!*

BYTE video_type=2; // 0 = monochrome, 1=dark palette, 2=normal palette, 3=light palette, 4=green screen
int video_type_less(int i) { return SDL_pow(i,3.0/2.0)/16.0+.5; }
int video_type_more(int i) { return SDL_pow(i*16.0,2.0/3.0)+.5; }
#define VIDEO_RGB2Y_255 (VIDEO_RGB2Y(255,255,255))
VIDEO_UNIT video_xlat_rgb(VIDEO_UNIT i)
{
	int r=i>>16,g=(i>>8)&255,b=i&255; switch (video_type)
	{
		case 0: return (i=VIDEO_RGB2Y(r,g,b)),((i*2+1)*255/(VIDEO_RGB2Y_255*2))*0X10101; // monochrome
		case 4: return (i=VIDEO_RGB2Y(r,g,b)),((i*2+1)*65/VIDEO_RGB2Y_255)*0X10001+(((i*3+1)*65/VIDEO_RGB2Y_255)<<8)+0X003C00; // green screen; cfr. video_xlat_fix()
		case 1: return (video_type_less(r)<<16)+(video_type_less(g)<<8)+video_type_less(b); // dark palette
		case 3: return (video_type_more(r)<<16)+(video_type_more(g)<<8)+video_type_more(b); // light palette
		default: return i; // normal palette
	}
}
VIDEO_UNIT *video_xlat_all(VIDEO_UNIT *t,VIDEO_UNIT const *s,int n) // turn palette `s` of `n` entries into palette `t` thru current filter
	{ for (;n>0;--n) *t++=video_xlat_rgb(*s++); return t; }
#define video_main_xlat() (video_xlat_all(video_xlat,video_table,length(video_xlat))) // it's always the same operation

#define VIDEO_FILTER_5050(x,y) (((((x&0XFF00FF)+(y&0XFF00FF)+0X10001)&0X1FE01FE)+(((x&0XFF00)+(y&0XFF00)+0X100)&0X1FE00))>>1) // neutral: it doesn't favour `x` over `y` in case of draw
//#define VIDEO_FILTER_5050(x,y) (((((x&0XFF00FF)+(y&0XFE00FE)+0X10001)&0X1FE01FE)+(((x&0XFF00)+(y&0XFE00)+0X100)&0X1FE00))>>1) // a possible way to favour `x` over `y`
//#define VIDEO_FILTER_5050(x,y) ((((x&0XFEFEFE)+(y&0XFEFEFE))>>1)+(x&0X10101)) // fast but coarse: it favours `x` by copying its least significant bits
#define VIDEO_FILTER_7525(x,y) (((((x&0XFF00FF)*3+(y&0XFF00FF)+0X30003)&0X3FC03FC)+(((x&0XFF00)*3+(y&0XFF00)+0X300)&0X3FC00))>>2) // 75% 1st + 25% 2nd; always raw RGB!

//#define RGB2LINEAR // uncomment this for linear sRGB gamma correction
char video_gammaflag=1; // keep it even if we disable this on compile-time!
#ifndef RGB2LINEAR // working with non-linear sRGB looks better but is slow;
BYTE srgb[1<<16],crt1[256],crt2[256]; // the sRGB-to-linear-and-back operations need precalculated tables, they're too expensive otherwise :-(
void video_recalc(void) // redo the video filter tables
{
	if (video_gammaflag)
		for (int h=0;h<256;++h)
		{
			int z=h*h*2;
			crt1[h]=z<255*255?0:sqrtu(z-255*255),
			crt2[h]=z<255*255?sqrtu(  z):   255 ;
			for (int l=0;l<256;++l)
				srgb[h*256+l]=sqrtu((h*h+l*l+h+l)>>1);
		}
	else
		for (int h=0;h<256;++h)
		{
			int z=h*2;
			crt1[h]=(z<255?0:z-255),
			crt2[h]=(z<255?  z:255);
			for (int l=0;l<256;++l)
				srgb[h*256+l]=(h+l+1)>>1;
		}
}
#define VIDEO_FILTER_SRGB(x,y) (srgb[((x>>8)&0XFF00)+(y>>16)]<<16)|(srgb[(x&0XFF00)+((y>>8)&255)]<<8)|srgb[((x&255)<<8)+(y&255)] // old-new avg.
#else // working with linear RGB is faster, but the image loses brightness!
#define VIDEO_FILTER_SRGB VIDEO_FILTER_5050
#endif
#define VIDEO_FILTER_HALF(x,y) (x==y?x:VIDEO_FILTER_SRGB(x,y))

#ifdef VIDEO_LO_X_RES
// 4-pixel lo-res blur: ABBC,BBCC,BCCD,CCDD...
#define VIDEO_FILTER_BLURDATA VIDEO_UNIT v2z,v1z,v0z
#define VIDEO_FILTER_BLUR0(z) v2z=v0z=z
#define VIDEO_FILTER_BLUR1(r,z) v1z=VIDEO_FILTER_HALF(z,v0z),r=VIDEO_FILTER_HALF(v1z,v2z) // = ((A+B)/2+(B+C)/2)/2
#define VIDEO_FILTER_BLUR2(r,z) v0z=z,r=v2z=v1z // ((B+B)/2+(C+C)/2)/2 = ((B+C)/2+(B+C)/2)/2 = (B+C)/2, already calc'd above
#else
// 4-pixel hi-res blur: ABCD,BCDE,CDEF,DEFG...
#define VIDEO_FILTER_BLURDATA VIDEO_UNIT v4z,v3z,v2z,v1z,v0z
#define VIDEO_FILTER_BLUR0(z) v3z=v2z=v0z=z
#define VIDEO_FILTER_BLUR1(r,z) v1z=VIDEO_FILTER_HALF(z,v0z),r=VIDEO_FILTER_HALF(v1z,v3z),v0z=z,v4z=v2z // = ((A+B)/2+(C+D)/2)/2
#define VIDEO_FILTER_BLUR2(r,z) v2z=VIDEO_FILTER_HALF(z,v0z),r=VIDEO_FILTER_HALF(v2z,v4z),v0z=z,v3z=v1z // = ((B+C)/2+(D+E)/2)/2
#endif

//#define VIDEO_FILTER_DOT0(x) ((((x)>>1)&0X7F7F7F)+0X404040) // (4+2)/8: 00->40,FF->BF
//#define VIDEO_FILTER_DOT0(x) ((x)-(((x)>>1)&0X7F7F7F)+0X404040) // no mult: 00->40,FF->C0
//#define VIDEO_FILTER_DOT0(x) ((((x)>>2)&0X3F3F3F)*3+0X212121) // (6+1)/8: 00->21,FF->DE
#define VIDEO_FILTER_DOT0(x) ((x)-(((x)>>2)&0X3F3F3F)+0X202020) // no mult: 00->20,FF->E0
#ifndef RGB2LINEAR
//#define VIDEO_FILTER_DOT1(x) ((crt1[(x)>>16]<<16)|(crt1[((x)>>8)&255]<<8)|crt1[(x)&255])
//#define VIDEO_FILTER_DOT2(x) ((crt2[(x)>>16]<<16)|(crt2[((x)>>8)&255]<<8)|crt2[(x)&255])
#define VIDEO_FILTER_DOT1(x) ((crt1[(x)>>16]<<16)|(crt1[((x)>>8)&255]<<8)|crt1[(x)&255]) // without colour aberration (fewer artifacts)
#define VIDEO_FILTER_DOT2(x) ((crt2[(x)>>16]<<16)|(crt2[((x)>>8)&255]<<8)|crt2[(x)&255])
//#define VIDEO_FILTER_DOT1(x) ((crt1[(x)>>16]<<16)|(crt2[((x)>>8)&255]<<8)|crt1[(x)&255]) // with colour aberration (closer to true CRT)
//#define VIDEO_FILTER_DOT2(x) ((crt2[(x)>>16]<<16)|(crt1[((x)>>8)&255]<<8)|crt2[(x)&255])
#define VIDEO_FILTER_DOT3(x) ((x)-(((x)>>1)&0X7F7F7F)+0X404040) // (4+2:4+2:4+2)/8
//#define VIDEO_FILTER_DOT3(x) ((x)-(((x)>>2)&0X3F3F3F)+0X202020) // (6+1:6+1:6+1)/8
//#define VIDEO_FILTER_DOT3 // checkerboard, quick but not very visible
#else
//#define VIDEO_FILTER_DOT1(x) ((x)-(((x)>>1)&0X7F0000)+0X400000) // red
//#define VIDEO_FILTER_DOT2(x) ((x)-(((x)>>1)&0X00007F)+0X000040) // blue
//#define VIDEO_FILTER_DOT3(x) ((x)-(((x)>>1)&0X007F00)+0X004000) // green
#define VIDEO_FILTER_DOT1(x) ((x)-(((x)>>1)&0X007F00)+0X004000) // (8+0:4+2:8+0)/8
//#define VIDEO_FILTER_DOT2(x) ((x)-(((x)>>1)&0X00007F)+0X000040) // (8+0:8+0:4+2)/8
//#define VIDEO_FILTER_DOT3(x) ((x)-(((x)>>1)&0X7F0000)+0X400000) // (4+2:8+0:8+0)/8
#define VIDEO_FILTER_DOT2(x) ((x)-(((x)>>1)&0X7F007F)+0X400040) // (4+2:8+0:4+2)/8
//#define VIDEO_FILTER_DOT3(x) ((x)-(((x)>>1)&0X7F7F7F)+0X404040) // (4+2:4+2:4+2)/8
#define VIDEO_FILTER_DOT3(x) ((x)-(((x)>>2)&0X3F3F3F)+0X202020) // (6+1:6+1:6+1)/8
//#define VIDEO_FILTER_DOT3 // checkerboard, quick but not very visible
#endif

#define MAIN_FRAMESKIP_MASK ((1<<MAIN_FRAMESKIP_BITS)-1)
void video_resetscanline(void) // reset configuration values on new video options
{
	static char b=0; if (b!=video_pageblend) // do we need to reset the blending buffer?
		if (b=video_pageblend)
			for (int y=0;y<VIDEO_PIXELS_Y/2;++y)
				MEMNCPY(&video_blend[y*VIDEO_PIXELS_X],&video_frame[(VIDEO_OFFSET_Y+y*2)*VIDEO_LENGTH_X+VIDEO_OFFSET_X],VIDEO_PIXELS_X);
}
#define VIDEO_NEXT *video_target++ // "VIDEO_NEXT = VIDEO_NEXT = ..." generates invalid code on VS13 and slower code on TCC!
// do not manually unroll the following operations, GCC is smart enough to do a better job on its own!
void video_callscanline(VIDEO_UNIT *vl)
{
	VIDEO_UNIT *vi=vl-VIDEO_PIXELS_X; if (video_scanlinez<2) // all scanlines + avg. scanlines in final line
	{
		VIDEO_UNIT *vo=vi+VIDEO_LENGTH_X;
		switch (video_filter&(VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Y))
		{
			case 0:
				MEMNCPY(vo,vi,VIDEO_PIXELS_X);
				break;
			case VIDEO_FILTER_MASK_X:
				do
					*vo=*vi,++vi,++vo,
					*vi=*vo=VIDEO_FILTER_DOT0(*vi);
				while (++vo,++vi<vl);
				break;
			case VIDEO_FILTER_MASK_Y:
				do
					*vo=VIDEO_FILTER_DOT0(*vi);
				while (++vo,++vi<vl);
				break;
			case VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Y:
				do
					*vo=VIDEO_FILTER_DOT1(*vi),++vi,++vo,
					*vo=VIDEO_FILTER_DOT3(*vi),*vi=VIDEO_FILTER_DOT2(*vi);
				while (++vo,++vi<vl);
				break;
		}
	}
	else switch ((video_filter&(VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Y))|((video_pos_y&1)?VIDEO_FILTER_MASK_Z:0))
	{
		// half/single/double scanlines: bottom lines add VIDEO_FILTER_MASK_Z
		case VIDEO_FILTER_MASK_X:
		case VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Z:
			do
				++vi,
				*vi=VIDEO_FILTER_DOT0(*vi);
			while (++vi<vl);
			break;
		case VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_Z:
			do
				*vi=VIDEO_FILTER_DOT0(*vi);
			while (++vi<vl);
			break;
		case VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Y:
			do
				++vi,
				*vi=VIDEO_FILTER_DOT2(*vi);
			while (++vi<vl);
			break;
		case VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_Z:
			do
				*vi=VIDEO_FILTER_DOT1(*vi),
				++vi,
				*vi=VIDEO_FILTER_DOT3(*vi);
			while (++vi<vl);
			break;
	}
}
void video_callmicrowave(VIDEO_UNIT *vl)
{
	VIDEO_UNIT *vi=vl-VIDEO_PIXELS_X;
	if (video_microwave) // single-bit LFSR: "microwave" static
	{
		#if 0 // coarse: shake whole scanlines
		if (video_microwavez&1)
		{
			VIDEO_UNIT *vo=vi=vl-VIDEO_PIXELS_X; if (video_microwavez&2) ++vi; else ++vo;
			memmove(vo,vi,(VIDEO_PIXELS_X-1)*sizeof(VIDEO_UNIT));
		}
		video_microwavez=(video_microwavez>>2)^((video_microwavez&3)*(0X90000000U>>2));
		if (video_scanlinez<2) // second line of non-interlaced modes
		{
			if (video_microwavez&1)
			{
				VIDEO_UNIT *vo=vi=vl-VIDEO_PIXELS_X+VIDEO_LENGTH_X; if (video_microwavez&2) ++vi; else ++vo;
				memmove(vo,vi,(VIDEO_PIXELS_X-1)*sizeof(VIDEO_UNIT));
			}
			video_microwavez=(video_microwavez>>2)^((video_microwavez&3)*(0X90000000U>>2));
		}
		#else // fine: shake individual pixels
		unsigned int vv=video_microwavez; *vl=vl[-1]; vi=vl-VIDEO_PIXELS_X;
		#if 0 // harder
		static const int vu[5]={2,1,0,1,2}; if (video_scanlinez<2)
		{
			vl[VIDEO_LENGTH_X]=vl[VIDEO_LENGTH_X-1]; do
			{
				int vw=vv&3; vi[vu[vw+1]]=vi[vu[vw]],vi[vu[vw+1]+VIDEO_LENGTH_X]=vi[vu[vw]+VIDEO_LENGTH_X];
				vv=(vv>>2)^(vw*(0X90000000U>>2)); vi+=(vw&2)+2;
			}
			while (vi<vl);
		}
		else
			do
			{
				int vw=vv&3; vi[vu[vw+1]]=vi[vu[vw]];
				vv=(vv>>2)^(vw*(0X90000000U>>2)); vi+=(vw&2)+2;
			}
			while (vi<vl);
		#else // softer
		static const int vu[9]={4,3,2,1,0,1,2,3,4}; if (video_scanlinez<2)
		{
			vl[VIDEO_LENGTH_X]=vl[VIDEO_LENGTH_X-1]; do
			{
				int vw=vv&7; vi[vu[vw+1]]=vi[vu[vw]],vi[vu[vw+1]+VIDEO_LENGTH_X]=vi[vu[vw]+VIDEO_LENGTH_X];
				vv=(vv>>3)^(vw*(0X90000000U>>3)); vi+=(vw&4)+4;
			}
			while (vi<vl);
		}
		else
			do
			{
				int vw=vv&7; vi[vu[vw+1]]=vi[vu[vw]];
				vv=(vv>>3)^(vw*(0X90000000U>>3)); vi+=(vw&4)+4;
			}
			while (vi<vl);
		#endif
		video_microwavez=vv;
		#endif
	}
}
INLINE void video_drawscanline(void) // call after each drawn scanline; memory caching makes this more convenient than gathering all operations in video_endscanlines()
{
	VIDEO_UNIT vt,vs,*vi=video_target-video_pos_x+VIDEO_OFFSET_X,*vl=vi+VIDEO_PIXELS_X,*vo;
	#ifdef MAUS_LIGHTGUNS
	if (!(((session_maus_y+VIDEO_OFFSET_Y)^video_pos_y)&-2)) // does the lightgun aim at the current scanline?
		video_litegun=session_maus_x>=0&&session_maus_x<VIDEO_PIXELS_X?vi[session_maus_x]|vi[session_maus_x^1]:0; // keep the colours BEFORE any filtering happens!
	#endif
	if (video_lineblend) // blend current and previous scanline together!
	{
		static VIDEO_UNIT vz[VIDEO_PIXELS_X]; // vertical blur
		if (UNLIKELY(video_pos_y<VIDEO_OFFSET_Y+2))
			MEMLOAD(vz,vi); // 1st line, backup only
		else
		{
			vo=vz; do
			#ifdef RAWWW // quick'n'dirty
				*vo=*vi,++vo,++vi,vs=*vo,*vo=*vi,*vi=vs;
			while (++vo,++vi<vl);
			#else // standard half'n'half
			if ((vt=*vo)!=(vs=*vi))
			#ifdef VIDEO_LO_X_RES // will cause data loss in hi-res!
				*vo=vs,vi[1]=*vi=VIDEO_FILTER_SRGB(vs,vt);
			while (vo+=2,(vi+=2)<vl);
			#else
				*vo=vs,*vi=VIDEO_FILTER_SRGB(vs,vt);
			while (++vo,++vi<vl);
			#endif
			#endif
			vi=vl-VIDEO_PIXELS_X; // go back
		}
	}
	// the order is important: doing PAGEBLEND before LINEBLEND is slow :-(
	if (video_pageblend) // blend scanlines from previous and current frame!
	{
		vo=&video_blend[((video_pos_y-VIDEO_OFFSET_Y)>>1)*VIDEO_PIXELS_X]; // gigascreen
		#ifdef RAWWW // quick'n'dirty
		if (video_pos_z&1) // give each gigascreen side its own half
			do
				*vo=*vi,++vo,++vi,vs=*vo,*vo=*vi,*vi=vs;
			while (++vo,++vi<vl);
		else
			do
				vs=*vo,*vo=*vi,*vi=vs,++vo,++vi,*vo=*vi;
			while (++vo,++vi<vl);
		#else // standard half'n'half
		do
			if ((vt=*vo)!=(vs=*vi))
			#ifdef VIDEO_LO_X_RES // will cause data loss in hi-res!
				*vo=vs,vi[1]=*vi=VIDEO_FILTER_SRGB(vs,vt);
			while (vo+=2,(vi+=2)<vl);
			#else
				*vo=vs,*vi=VIDEO_FILTER_SRGB(vs,vt);
			while (++vo,++vi<vl);
			#endif
		#endif
		vi=vl-VIDEO_PIXELS_X; // go back
	}
	// last but not least, let's blur pixels together
	if (video_filter&VIDEO_FILTER_MASK_Z)
	{
		#ifdef VIDEO_FILTER_BLUR0
		if (!video_fineblend)
		{
			VIDEO_FILTER_BLURDATA;
			vt=*vi,VIDEO_FILTER_BLUR0(vt);
			do {
				#ifdef VIDEO_FILTER_BLUR2
				vs=*vi,
				VIDEO_FILTER_BLUR1(vt,vs),*vi++=vt,
				#ifdef VIDEO_LO_X_RES
				#else
				vs=*vi,
				#endif
				VIDEO_FILTER_BLUR2(vt,vs),*vi++=vt;
				#else
				vs=*vi,
				VIDEO_FILTER_BLUR(vt,vs),*vi++=vt;
				#endif
			} while (vi<vl);
		}
		else
		#endif
		{
			vt=vs=*vi; do {
				if ((vs=*vi)!=vt) *vi=VIDEO_FILTER_SRGB(vs,vt);
				#ifdef VIDEO_LO_X_RES
				++vi;
				#endif
				++vi;
				if ((vt=*vi)!=vs) *vi=VIDEO_FILTER_SRGB(vt,vs);
				#ifdef VIDEO_LO_X_RES
				++vi;
				#endif
			} while (++vi<vl);
		}
	}
	// now we can handle the actual masks and scanlines
	if (video_pos_y>=VIDEO_OFFSET_Y+2) // handle scanlines with 50:50 neighbours
	{
		vi=(vl-=VIDEO_LENGTH_X*2)-VIDEO_PIXELS_X; // we modify the previous scanline, rather than the current one!
		if (video_scanlinez==1) // avg. scanlines!
		{
			VIDEO_UNIT *vj=(vo=vi+VIDEO_LENGTH_X)+VIDEO_LENGTH_X;
			switch(video_filter&(VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Y))
			{
				case 0:
					do
						#ifdef RAWWW // quick'n'dirty
						*vo=*vi,++vj,++vo,++vi,*vo=*vj;
						#else // standard half'n'half
						*vo=VIDEO_FILTER_HALF(*vi,*vj);
						#endif
					while (++vj,++vo,++vi<vl);
					break;
				case VIDEO_FILTER_MASK_X:
					do
						#ifdef RAWWW // quick'n'dirty
						*vo=*vi,++vj,++vo,++vi,*vi=VIDEO_FILTER_DOT0(*vi),*vo=VIDEO_FILTER_DOT0(*vj);
						#else // standard half'n'half
						*vo=VIDEO_FILTER_HALF(*vi,*vj),++vj,++vo,++vi,
						vt=VIDEO_FILTER_HALF(*vi,*vj),*vo=VIDEO_FILTER_DOT0(vt),*vi=VIDEO_FILTER_DOT0(*vi);
						#endif
					while (++vj,++vo,++vi<vl);
					break;
				case VIDEO_FILTER_MASK_Y:
					do
						#ifdef RAWWW // quick'n'dirty
						*vo=VIDEO_FILTER_DOT0(*vi),++vj,++vo,++vi,*vo=VIDEO_FILTER_DOT0(*vj);
						#else // standard half'n'half
						vt=VIDEO_FILTER_HALF(*vi,*vj),*vo=VIDEO_FILTER_DOT0(vt);
						#endif
					while (++vj,++vo,++vi<vl);
					break;
				case VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Y:
					do
						#ifdef RAWWW // quick'n'dirty
						*vo=VIDEO_FILTER_DOT1(*vi),++vj,++vo,++vi,
						*vo=VIDEO_FILTER_DOT3(*vj),*vi=VIDEO_FILTER_DOT2(*vi);
						#else // standard half'n'half
						vt=VIDEO_FILTER_HALF(*vi,*vj),*vo=VIDEO_FILTER_DOT1(vt),++vj,++vo,++vi,
						vt=VIDEO_FILTER_HALF(*vi,*vj),*vo=VIDEO_FILTER_DOT3(vt),*vi=VIDEO_FILTER_DOT2(*vi);
						#endif
					while (++vj,++vo,++vi<vl);
					break;
			}
		}
		else video_callscanline(vl); // also used in video_endscanlines()
		video_callmicrowave(vl); // also used in video_endscanlines()
	}
}
INLINE void video_nextscanline(int x) // call before each new scanline: move on to next scanline, where `x` is the new horizontal position
	{ frame_pos_y+=2,video_pos_y+=2,video_target+=VIDEO_LENGTH_X*2-video_pos_x; video_target+=video_pos_x=x; session_signal|=session_signal_scanlines; }
INLINE void video_endscanlines(void) // call after each drawn frame: end the current frame and clean up
{
	video_scanlinez=video_scanline; // technically this should go at the end...
	// handle final scanline (100:0 neighbours, no next line after last!)
	VIDEO_UNIT *vl=video_frame+VIDEO_OFFSET_X+(VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-2+(video_pos_y&1))*VIDEO_LENGTH_X+VIDEO_PIXELS_X;
	video_callscanline(vl); video_callmicrowave(vl); // also used in video_drawscanline()
}
INLINE void video_newscanlines(int x,int y) // call before each new frame: reset `video_target`, `video_pos_x` and `video_pos_y`, and update `video_pos_z`
{
	if (!video_framecount) video_endscanlines(); // finish old frame if needed!
	session_signal|=SESSION_SIGNAL_FRAME+session_signal_frames; // frame event!
	#ifdef MAUS_LIGHTGUNS
	video_litegun=0; // the lightgun signal fades away between frames
	#endif
	if (video_interlaces&&(video_scanline&2)) ++y; // odd or even field according to the current scanline mode
	++video_pos_z; video_target=video_frame+(video_pos_y=y)*VIDEO_LENGTH_X+(video_pos_x=x); // new coordinates
}

INLINE void audio_playframe(void) // call between frames by the OS wrapper
{
	AUDIO_UNIT aa,*ao=audio_frame,i=AUDIO_LENGTH_Z;
	switch (audio_filter)
	{
		#if AUDIO_CHANNELS > 1
		static AUDIO_UNIT a0=AUDIO_ZERO,a1=AUDIO_ZERO; // independent channels
		case 1: do
				aa=*ao,*ao++=a0=( (a0<aa?1:0)+a0   +aa)>>1,
				aa=*ao,*ao++=a1=( (a1<aa?1:0)+a1   +aa)>>1;
			while (--i); break;
		case 2: do
				aa=*ao,*ao++=a0=(((a0<aa?1:0)+a0)*3+aa)>>2,
				aa=*ao,*ao++=a1=(((a1<aa?1:0)+a1)*3+aa)>>2;
			while (--i); break;
		case 3: do
				aa=*ao,*ao++=a0=(((a0<aa?1:0)+a0)*7+aa)>>3,
				aa=*ao,*ao++=a1=(((a1<aa?1:0)+a1)*7+aa)>>3;
			while (--i); break;
		#else
		static AUDIO_UNIT a0=AUDIO_ZERO; // single channel
		case 1: do
				aa=*ao,*ao++=a0=( (a0<aa?1:0)+a0   +aa)>>1;
			while (--i); break;
		case 2: do
				aa=*ao,*ao++=a0=(((a0<aa?1:0)+a0)*3+aa)>>2;
			while (--i); break;
		case 3: do
				aa=*ao,*ao++=a0=(((a0<aa?1:0)+a0)*7+aa)>>3;
			while (--i); break;
		#endif
	}
}

INLINE void session_update(void) // render video+audio thru OS and handle realtime logic (self-adjusting delays, automatic frameskip, etc.)
{
	session_signal&=~SESSION_SIGNAL_FRAME; // new frame!
	session_render();
	audio_required=!audio_disabled||session_filmfile||session_wavefile; // ensures that audio is saved to WAV or XRF even on a machine without sound hardware
	audio_target=audio_frame; audio_pos_z=0;
	if (video_scanline==3)
		video_interlaced|=1;
	else
		video_interlaced&=~1;
	if (--video_framecount<0||video_framecount>video_framelimit+2)
		video_framecount=video_framelimit; // catch both <0 and >N, but not automatic frameskip!
	// simplify several frameskipping operations
	frame_pos_y=video_framecount?video_pos_y+VIDEO_LENGTH_Y*2:video_pos_y;
}

// elementary ZIP archive support ----------------------------------- //

// the INFLATE method! ... or more properly a terribly simplified mess
// based on the RFC1951 standard and PUFF.C from the ZLIB project.

BYTE *puff_data; int puff_size,puff_word,puff_bits; // stream cursor
int puff_len_c[16+288],puff_off_c[16+32]; // length and offset Huffman tables
char puff_cnt_c[288+32]; // bit counts: 0..287 length codes, 288..319 offset codes

const int puff_len_k[2][32]={ // length constants; 0, 30 and 31 are reserved
	{ 0,3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258,0,0 },
	{ 0,0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0,0,0 }};
const int puff_off_k[2][32]={ // offset constants; 30 and 31 are reserved
	{ 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0 },
	{ 0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13,0,0 }};
const char puff_cnt_k[19]={ 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 }; // custom Huffman indices
void puff_default(void) // generate the default static Huffman bit counts
{
	int i=0; // lengths: 8 x144, 9 x112, 7 x24, 8 x8; offsets: 5 x32
	for (;i<144;++i) puff_cnt_c[i]=8; // 000..143 : 00110000...10111111.
	for (;i<256;++i) puff_cnt_c[i]=9; // 144..255 : 110010000..111111111
	for (;i<280;++i) puff_cnt_c[i]=7; // 256..279 : 0000000....0010111..
	for (;i<288;++i) puff_cnt_c[i]=8; // 280..287 : 11000000...11000111.
	for (;i<320;++i) puff_cnt_c[i]=5; // notice that the reserved codes take part in the calculations!
}
// Huffman-bitwise operations
int puff_recv(int n) // receive `n` bits from source; <0 ERROR, >=0 OUTPUT
{
	int w=puff_word; while (puff_bits<n) // we'll need `w` later
	{
		if (--puff_size<0) return -1; // source underrun!
		w+=*puff_data++<<puff_bits,puff_bits+=8; // read byte from source
	}
	puff_word=w>>n; puff_bits-=n; return w&((1<<n)-1); // adjust target and flush bits
}
int puff_decode(int *h) // receive code from Huffman source; <0 ERROR, >=0 OUTPUT
{
	int w=puff_word,b=puff_bits; // local copies are faster... are they?
	for (int *s=h,l=1,i=0,base=0,code=0;;) // extract bits until they fit within an interval
	{
		while (b--)
		{
			int o=*++s; if (code+=w&1,w>>=1,code<base+o) // does the code fit the interval?
				return puff_word=w,puff_bits=(puff_bits-l)&7,h[16+i+(code-base)]; // yes!
			i+=o,base=(base+o)<<1,code<<=1,++l; // calculate next interval
		}
		if (!(b=16-l)||--puff_size<0) return -1; // too many bits! source underrun!
		if (w=*puff_data++,b>8) b=8; // read byte from source and drop any bits left
	}
}
int puff_tables(int *h,char *l,int n) // generate Huffman input tables from canonical length table; 0 OK, !0 ERROR
{
	int o[16],i,a; for (i=0;i<16;++i) h[i]=0; // reset intervals
	for (i=0;i<n;++i) ++h[l[i]]; // calculate intervals
	if (h[0]==n) return 0; // empty table, nothing to do!
	for (a=i=1;i<16;++i) if ((a=(a<<1)-h[i])<0) return a; // bad value!
	for (o[i=1]=0;i<15;++i) o[i+1]=o[i]+h[i]; // calculate base for each bit count
	for (i=0;i<n;++i) if (l[i]) h[16+o[l[i]]++]=i; // assign values to all symbols
	return a; // 0 if complete, >0 otherwise!
}
int puff_main(BYTE *t,int o,BYTE *s,int i) // inflate source `s[i]` into target `t[o]`; >=0 output length, <0 ERROR!
{
	puff_data=s,puff_size=i,puff_word=puff_bits=0;
	BYTE *u=t; int j,q; do
	{
		if (q=puff_recv(1),!(i=puff_recv(2))) // stored block?
		{
			if ((puff_size-=4)<0) return -1; // source underrun!
			i=*puff_data++,i+=*puff_data++<<8; j=*puff_data++,j+=*puff_data++<<8;
			if (i+j!=0XFFFF||(puff_size-=i)<0||(o-=i)<0) return -1; // bad value! source underrun! target overflow!
			// length zero is allowed: `...This completes the current deflate block and follows it with an empty stored block that is
			// three bits plus filler bits to the next byte, followed by four bytes (00 00 ff ff)...` http://www.zlib.net/manual.html
			memcpy(t,puff_data,i),puff_data+=i,t+=i,puff_word=puff_bits=0; // copy data, update pointers and drop any bits left
		}
		else if (i>0&&i<3) // packed block?
		{
			if (i==1) // default Huffman trees?
			{
				puff_default(); // default static Huffman tables
				puff_tables(puff_len_c,puff_cnt_c,288); // THERE MUST BE EXACTLY 288 LENGTH CODES!!
				puff_tables(puff_off_c,puff_cnt_c+288,32); // DITTO: THERE MUST BE 32 OFFSET CODES!
			}
			else // custom Huffman trees!
			{
				int len,off,a;
				if ((len=puff_recv(5)+257)>286||(off=puff_recv(5)+1)>30||(j=puff_recv(4)+4)<4)
					return -1; // bad match/range/huffman values!
				for (a=0,i=0;i<j;) a|=puff_cnt_c[puff_cnt_k[i++]]=puff_recv(3); // read bit counts for the Huffman-encoded header
				while (i<19) puff_cnt_c[puff_cnt_k[i++]]=0; // padding: clear the remainder of this header
				if (a<0||puff_tables(puff_len_c,puff_cnt_c,19)) return -1; // bad values! invalid table!
				for (len+=off,i=0;i<len;)
					if ((a=puff_decode(puff_len_c))<16) // literal?
						if (a<0) return a; else puff_cnt_c[i++]=a; // bad value!
					else
					{
						if (a<17) // copy last value 3..6 times?
						{
							if (i<1) return -1; // bad value!
							j=puff_cnt_c[i-1],a=3+puff_recv(2);
						}
						else // store zero 3..10 or 11..138 times?
							j=0,a=a==17?3+puff_recv(3):11+puff_recv(7);
						if (puff_size<0||i+a>len) return -1; // source underrun! bad value!
						do puff_cnt_c[i++]=j; while (--a);
					}
				if ((len-=off,puff_tables(puff_len_c,puff_cnt_c,len)&&len-puff_len_c[0]!=1)|| // bad length table!
					(puff_tables(puff_off_c,puff_cnt_c+len,off)&&off-puff_off_c[0]!=1)) return -1; // bad offset table!
			}
			for (;;) // beware: custom trees cannot generate lenght codes >=286 or offset codes >=30, but default trees can!
			{
				if ((i=puff_decode(puff_len_c))<256) // literal?
					if (i<0||--o<0) return -1; else *t++=i; // bad value! target overflow!
				else if (i-=256) // length:offset pair?
				{
					if (i>=30||(o-=(i=puff_len_k[0][i]+puff_recv(puff_len_k[1][i])))<0|| // target overflow!
						(j=puff_decode(puff_off_c))<0||j>=30|| // bad value!
						(j=puff_off_k[0][j]+puff_recv(puff_off_k[1][j]))>t-u) return -1; // source underrun!
					s=t-j; do *t++=*s++; while (--i);
				}
				else break; // end of block!
			}
		}
		else return -1; // unsupported block!
	}
	while (!q); return t-u; // OK, output length
}
#ifdef INFLATE_RFC1950
// RFC1950 standard data decoder
DWORD puff_adler32(const BYTE *t,int o) // calculates Adler-32 checksum of a block `t` of size `o`
	{ int j=1,k=0; while (o-->0) { if ((k+=(j+=*t++))>=65521) k%=65521,j%=65521; } return (k<<16)+j; }
int puff_zlib(BYTE *t,int o,BYTE *s,int i) // decodes a RFC1950 standard ZLIB object; >=0 output length, <0 ERROR!
{
	if (!s||!t||i<6||o<0) return -1; // NULL or empty source or target!
	if (s[0]!=120||s[1]&32||(s[0]*256+s[1])%31) return -1; // no ZLIB prefix!
	return s+=2,i-=6,(o=puff_main(t,o,s,i))<0||puff_adler32(t,o)!=mgetmmmm(&s[i])?-1:o;
}
#endif

// the DEFLATE method! ... or (once again) a quick-and-dirty implementation
// based on the RFC1951 standard and reusing bits from INFLATE functions.

#if defined(DEFLATE_RFC1950)&&!defined(DEFLATE_LEVEL)
#define DEFLATE_LEVEL 6 // catch when ZLIB encoding is required but DEFLATE_LEVEL is missing!
#endif
// = 0 never performs any compression at all
// = 1 downgrades Lempel-Ziv into Run-Length
// = 2..9 go from fast-n-weak to hard-n-slow

#ifdef DEFLATE_LEVEL
#if DEFLATE_LEVEL < 1
#undef DEFLATE_NOSTORE // this tag is obviously incompatible with DEFLATE_LEVEL = 0
#endif
#ifndef DEFLATE_NOSTORE
int huff_stored(BYTE *t,int o,BYTE *s,int i) // the storage part of DEFLATE; >=0 output length, <0 ERROR!
{
	BYTE *u=t; while (i>65535) // cuts source into 64K-1B non-final blocks
	{
		if ((o-=65535+5)<0) return -1; // not enough room!
		*t++=0,*t++=~0,*t++=~0,*t++=0,*t++=0,memcpy(t,s,65535),t+=65535,s+=65535,i-=65535;
	}
	if ((o-=i+5)<0) return -1; // not enough room!
	return *t++=1,*t++=i,*t++=i>>8,*t++=~i,*t++=~i>>8,memcpy(t,s,i),t-u+i; // final block!
}
#endif
#if DEFLATE_LEVEL > 0 // if you must save DEFLATE data and perform compression on it!
// Huffman-bitwise operations
void huff_send(int n,int i) // sends the `n`-bit word `i` to target
{
	puff_word|=i<<puff_bits; puff_bits+=n; while (puff_bits>=8)
		--puff_size,*puff_data++=puff_word,puff_word>>=8,puff_bits-=8;
}
void huff_encode(int *h,int i) { huff_send(h[i]&15,h[i]>>4); } // send value to Huffman target
void huff_append(const int h[2][32],int i,int z) { huff_send(h[1][i],z-h[0][i]); }
#define huff_flush() huff_send(7,0) // flush the last bits in the bitstream!
// notice that these function ignore target overflows; the caller must know that the pathological case (longest possible code) is 48 bits long
void huff_tables(int *h,char *l,int n) // generates Huffman output table from canonical length table
{
	for (int j=1,k=0;j<16;++j,k<<=1) for (int i=0;i<n;++i)
		if (l[i]==j) // do the bit counts match?
			h[i]=((rbit16(k++)>>(16-j))<<4)+j; // store Huffman bits backwards
}
#if DEFLATE_LEVEL > 1
#define DEFLATE_RANGE 32768 // we're better off sticking to 32K: DEFLATE_RETRY provides a better power/speed ratio
//#define DEFLATE_RANGE (64<<DEFLATE_LEVEL) // this spans from 64<<2=256 to 64<<9=32768 but it's a generally bad idea
#define DEFLATE_RETRY (1<<(DEFLATE_LEVEL*2-2)) // i.e. from 4 to 16K retries for LEVELS 2..8; LEVEL >= 9 means RETRY >= RANGE
#endif
int huff_static(BYTE *t,int o,BYTE *s,int i) // the static compression of DEFLATE; >=0 output length, <0 ERROR!
{
	puff_data=t,puff_size=o,puff_word=puff_bits=0; // beware, a bitstream isn't a bytestream!
	#if DEFLATE_LEVEL > 1
	#define HUFF_HASH_MAX 65536 // =256*256, 2-byte hash
	#if SDL_BYTEORDER == SDL_BIG_ENDIAN // PPC, ARM...
		#define HUFF_HASH(x) hh[(s[x]<<8)+s[x+1]] // big-endian hash function
	#else // i86, x64... following Z80, MOS 6502, etc.
		#define HUFF_HASH(x) hh[(s[x+1]<<8)+s[x]] // lil-endian hash function
	#endif
	int *hh=(int*)malloc(sizeof(int[HUFF_HASH_MAX+DEFLATE_RANGE])); if (!hh) return -1; // no memory!
	int p=0,h=p; int *pp=hh+HUFF_HASH_MAX; // the table is too big to be local or static, but without it
	for (int q=0;q<HUFF_HASH_MAX+DEFLATE_RANGE;++q) hh[q]=~DEFLATE_RANGE; // compression would be just too slow!
	#else // run-length mode
	int p=0; // ranges 1..4 = hash table is useless!
	#endif
	// in static compression, default sizes are assumed, and no Huffman analysis is performed
	huff_send(3,3); // tag the single block as final static ("3,5" would be final dynamic)
	puff_default(); // default static Huffman tables
	huff_tables(puff_len_c,puff_cnt_c,288); // 288 codes, but the last two are reserved
	huff_tables(puff_off_c,puff_cnt_c+288,32); // ditto, 32 codes but only 30 are valid
	while (p+3<=i) // the minimum match length is 3 bytes, a match can't begin any later!
	{
		if (6>puff_size) break; // target overflow!
		#ifdef HUFF_HASH_MAX
		while (h<p) pp[h%DEFLATE_RANGE]=HUFF_HASH(h),HUFF_HASH(h)=h,++h; // walk hash table
		int len=0,off=p+258,x=p-DEFLATE_RANGE; if (off>i) off=i; const BYTE *y=s+off;
		for (int q=HUFF_HASH(p),e=DEFLATE_RETRY;q>=x&&e;q=pp[q%DEFLATE_RANGE],(DEFLATE_RETRY<32768&&--e)) // search for matches
		{
			const BYTE *cmp1=s+q,*cmp2=s+p; if (cmp1[len]==cmp2[len]) // can this match be an improvement?
				{ for (++cmp1,++cmp2;++cmp2<y&&*++cmp1==*cmp2;) {} int z=cmp2-s-p; if (len<z) if (len=z,off=p-q,cmp2>=y) break; }
		}
		if (len>=(off>32*64?4:3)) // was there a match? is it beneficial? (empirical 24*64<<<x<=32*64; one case favours 29, another 31, another one 32...)
		#else // run-length!
		int len=0,off=p+258,x; if (off>i) off=i; BYTE *y=s+off;
		for (int q=0;q<4&&q<p;++q) // ranges 1..4 = four possible types of run-length encoding!
		{
			const BYTE *cmp1=s+p-q-1,*cmp2=s+p; while (*cmp1==*cmp2&&++cmp2<y) ++cmp1;
			x=cmp2-s-p; if (len<x) if (len=x,off=q,cmp2==y) break;
		}
		if (len>=3) // was there a match at all?
		#endif
		{
			p+=len; // greedy algorithm: grab the longest match and move on
			if (len<11) x=len-2; else if (len>=258) x=29; else //for (x=log2u(len-3)*4;len<puff_len_k[0][x];--x) {}
				x=log2u(len-3)*4-3,x+=(len-puff_len_k[0][x])>>puff_len_k[1][x];
			huff_encode(puff_len_c, 256+x),huff_append(puff_len_k,x,len);
			#ifdef HUFF_HASH_MAX
			if (off<5) x=off-1; else //for (x=log2u(off-1)*2+1;off<puff_off_k[0][x];--x) {}
				x=log2u(off-1)*2,x+=(off-puff_off_k[0][x])>>puff_off_k[1][x];
			huff_encode(puff_off_c,     x),huff_append(puff_off_k,x,off);
			#else // run-length!
			huff_encode(puff_off_c,   off); // ranges 1..4 = huff_ocode[0..3] without suffix!
			#endif
		}
		else
			huff_encode(puff_len_c,s[p++]); // too short, store a literal
	}
	#if DEFLATE_LEVEL > 1
	free(hh);
	#undef HUFF_HASH_MAX
	#undef HUFF_HASH
	#endif
	if ((i-p)*2+6>puff_size) return -1; // target overflow!
	while (p<i) huff_encode(puff_len_c,s[p++]); // last bytes (if any) are always literals
	huff_encode(puff_len_c,256); huff_flush(); return puff_data-t; // store end marker
}
#ifdef DEFLATE_NOSTORE
#define huff_main huff_static // falling back is forbidden!
#else
int huff_main(BYTE *t,int o,BYTE *s,int i) // deflates source into target; >=0 output length, <0 ERROR!
	{ int z=huff_static(t,o,s,i); return z<0?huff_stored(t,o,s,i):z; } // fall back to storage!
#endif
#else // if you must save DEFLATE data but don't want to perform any compression at all
#define huff_main huff_stored
#endif
#ifdef DEFLATE_RFC1950
// RFC1950 standard data encoder
int huff_zlib(BYTE *t,int o,BYTE *s,int i) // encodes a RFC1950 standard ZLIB object; >=0 output length, <0 ERROR!
{
	if (!t||!s||(o-=6)<=0||i<=0) return -1; // NULL or empty!
	*t++=120,*t++=218; // ZLIB prefix: compression type 8, default mode
	return (o=huff_main(t,o,s,i))<0?-1:(mputmmmm(t+o,puff_adler32(s,i)),o+6);
}
#endif
#endif

// standard ZIP v2.0 archive reader that relies on
// the main directory at the end of the ZIP archive;
// it includes a RFC1952 standard GZIP file handler.

BYTE *puff_src,*puff_tgt; int puff_srcl,puff_tgtl;
FILE *puff_file=NULL;
unsigned char puff_name[256],puff_type,puff_gzip;
unsigned int puff_skip,puff_next,puff_diff,puff_hash;//,puff_time
void puff_close(void) // closes the current ZIP archive
{
	if (puff_file)
		fclose(puff_file),puff_file=NULL;
}
int puff_open(const char *s) // opens a new ZIP archive; 0 OK, !0 ERROR
{
	puff_close();
	if (!(puff_file=fopen(s,"rb"))) return -1;
	if (fread1(session_scratch,10,puff_file)==10&&!memcmp(session_scratch,"\037\213\010",3)) // GZIP header?
	{
		if (session_scratch[3]&4)
			fseek(puff_file,fgetii(puff_file),SEEK_SET); // skip infos
		if (session_scratch[3]&8)
			while (fgetc(puff_file)>0) {} // skip filename
		if (session_scratch[3]&16)
			while (fgetc(puff_file)>0) {} // skip comment
		if (session_scratch[3]&2)
			fgetii(puff_file); // skip CRC16
		puff_skip=ftell(puff_file); fseek(puff_file,-8,SEEK_END); puff_srcl=ftell(puff_file)-puff_skip;
		puff_hash=fgetiiii(puff_file); puff_tgtl=fgetiiii(puff_file); // get CRC32 and source size
		char *t=strrchr(s,PATHCHAR); // build target name from source filename: skip the path,
		strcpy(puff_name,t?t+1:s); // or copy the whole string if there's no path at all!
		*strrchr(puff_name,'.')=0; // remove ".GZ" extension to get the real filename
		return puff_next=puff_skip+(puff_type=8),puff_gzip=2,puff_diff=0;
	}
	// find the central directory tree
	fseek(puff_file,puff_gzip=0,SEEK_END);
	int l=ftell(puff_file),k=65536; if (k>l) k=l;
	fseek(puff_file,l-k,SEEK_SET);
	int i=fread1(session_scratch,k,puff_file)-22;
	while (i>=0)
	{
		if (!memcmp(&session_scratch[i],"PK\005\006\000\000\000\000",8)) // expected header
			if (!memcmp(&session_scratch[i+8],&session_scratch[i+10],2)) // file numbers match
				if (i+22+session_scratch[i+20]+session_scratch[i+21]*256<=k) // comment OK
					break;
		--i;
	}
	if (i<0)
		return puff_close(),1;
	puff_diff=l-mgetiiii(&session_scratch[i+12])-(puff_next=mgetiiii(&session_scratch[i+16]))-k+i; // actual archive header offset
	return 0;
}
int puff_head(void) // reads a ZIP file header, if any; 0 OK, !0 ERROR
{
	if (!puff_file) return -1;
	if (puff_gzip) return --puff_gzip!=1; // header is already pre-made; can be read once, but not twice
	unsigned char h[46];
	fseek(puff_file,puff_diff+puff_next,SEEK_SET);
	if (fread(h,1,sizeof(h),puff_file)!=sizeof(h)||memcmp(h,"PK\001\002",4)||h[11]||!h[28]||h[29])//||(h[8]&8)
		return puff_close(),1; // reject EOFs, unknown IDs, extended types and improperly sized filenames!
	puff_type=h[10];
	puff_srcl=mgetiiii(&h[20]);
	puff_tgtl=mgetiiii(&h[24]);
	//puff_time=mgetiiii(&h[12]); // DOS timestamp
	puff_hash=mgetiiii(&h[16]);
	puff_skip=mgetiiii(&h[42]); // the body that belongs to the current head
	puff_next+=46+h[28]+mgetii(&h[30])+mgetii(&h[32]); // next ZIP file header
	puff_name[fread1(puff_name,h[28],puff_file)]=0;
	#if PATHCHAR != '/' // ZIP archives use the UNIX style
	for (char *s=puff_name;*s;++s) if (*s=='/') *s=PATHCHAR;
	#endif
	return 0;
}
int puff_body(int q) // loads (!0) or skips (0) a ZIP file body; 0 OK, !0 ERROR
{
	if (!puff_file) return -1;
	if (puff_tgtl<1) q=0; // no data, can safely skip the source
	else if (q)
	{
		if (puff_srcl<1||puff_tgtl<0)
			return -1; // cannot get data from nothing! cannot write negative data!
		if (puff_gzip)
			return fseek(puff_file,puff_diff+puff_skip,SEEK_SET),
				puff_main(puff_tgt,puff_tgtl,puff_src,fread1(puff_src,puff_srcl,puff_file))!=puff_tgtl;
		unsigned char h[30];
		fseek(puff_file,puff_diff+puff_skip,SEEK_SET);
		fread1(h,sizeof(h),puff_file);
		if (!memcmp(h,"PK\003\004",4)&&h[8]==puff_type) // OK?
		{
			fseek(puff_file,mgetii(&h[26])+mgetii(&h[28]),SEEK_CUR); // skip name+xtra
			if (!puff_type)
				return fread1(puff_tgt,puff_srcl,puff_file)!=puff_tgtl;
			if (puff_type==8)
				return puff_main(puff_tgt,puff_tgtl,puff_src,fread1(puff_src,puff_srcl,puff_file))!=puff_tgtl;
		}
	}
	return q; // 0 skipped=OK, !0 unknown=ERROR
}
// compact CRC-32 algorithm by Karl Malbrain; the caller must manage the initial and final values, either 0 or 0XFFFFFFFF
unsigned int puff_dohash(unsigned int k,const unsigned char *s,int l) // incremental function: run it on each data chunk
{
	static const unsigned int z[16]={0,0X1DB71064,0X3B6E20C8,0X26D930AC,0X76DC4190,0X6B6B51F4,0X4DB26158,0X5005713C,
		0XEDB88320,0XF00F9344,0XD6D6A3E8,0XCB61B38C,0X9B64C2B0,0X86D3D2D4,0XA00AE278,0XBDBDF21C}; // precalc'd!
	for (int i=0;i<l;++i) k^=s[i],k=(k>>4)^z[k&15],k=(k>>4)^z[k&15]; // walk bytes as pairs of nibbles
	return k;
}
// ZIP-aware fopen()
const char puff_pattern[]="*.gz;*.zip",PUFF_STR[]={PATHCHAR,PATHCHAR,PATHCHAR,0}; char puff_path[STRMAX]; // i.e. "TREE/PATH///ARCHIVE/FILE"
FILE *puff_ffile=NULL; int puff_fshut=0; // used to know whether to keep a file that gets repeatedly open
FILE *puff_fopen(char *s,const char *m) // mimics fopen(), so NULL on error, *FILE otherwise
{
	if (!s||!m) return NULL; // wrong parameters!
	char *z; if (!(z=strrstr(s,PUFF_STR))||strchr(m,'w'))
		return fopen(s,m); // normal file activity
	if (puff_ffile&&puff_fshut) // can we recycle the last closed file?
	{
		puff_fshut=0; // closing must have happened before recycling
		if (!strcmp(puff_path,s))
			return fseek(puff_ffile,0,SEEK_SET),puff_ffile; // recycle last file!
		fclose(puff_ffile),puff_ffile=NULL; // it's a new file, delete old
	}
	strcpy(puff_path,s);
	puff_path[z-s]=0; z+=3/*strlen(PUFF_STR)*/;
	if (!multiglobbing(puff_pattern,puff_path,1)||puff_open(puff_path))
		return 0;
	while (!puff_head()) // scan archive; beware, the insides MUST BE case sensitive!
		#ifdef _WIN32
		if (!strcasecmp(puff_name,z)) // past versions were wrong here: manually typing a path on the Win32 file dialog was enough to confuse `strcmp`
		#else
		if (!strcmp(puff_name,z))
		#endif
		{
			#ifdef __MSVCRT__ // MSVCRT.DLL: tmpfile() ignores the TEMP variable and fails on read-only drives, but fopen() gives us the flags "TD"
			static char f[STRMAX]="",g[STRMAX]; if (!*f) GetTempPath(STRMAX,f); // do it just once
			if (!(puff_ffile=GetTempFileName(f,"zip",0,g)?fopen(g,"wb+TD"):NULL)) // "TD" = _O_SHORTLIVED | _O_TEMPORARY
			#else
			if (!(puff_ffile=tmpfile()))
			#endif
				return puff_close(),NULL; // file failure!
			puff_src=puff_tgt=NULL;
			if (!(!puff_type||(puff_src=malloc(puff_srcl)))||!(puff_tgt=malloc(puff_tgtl))
				||puff_body(1)||(puff_dohash(0XFFFFFFFF,puff_tgt,puff_tgtl)^puff_hash^0XFFFFFFFF))
				fclose(puff_ffile),puff_ffile=NULL; // memory or data failure!
			if (puff_ffile)
				fwrite1(puff_tgt,puff_tgtl,puff_ffile),fseek(puff_ffile,0,SEEK_SET); // fopen() expects ftell()=0!
			if (puff_src) free(puff_src); // free if allocated
			if (puff_tgt) free(puff_tgt); // free if allocated
			puff_close(); if (puff_ffile) // either *FILE or NULL
				strcpy(puff_path,s); // allow recycling!
			else
				*puff_path=puff_fshut=0; // don't recycle on failure!
			return puff_ffile;
		}
		else
			puff_body(0); // not found, skip file
	return NULL;
}
int puff_fclose(FILE *f)
	{ return (f==puff_ffile)?(puff_fshut=1),0:fclose(f); } // delay closing to allow same-file recycling
void puff_byebye(void)
	{ if (puff_ffile) fclose(puff_ffile),puff_ffile=NULL; } // cleanup of tmpfile() is done by the runtime

// ZIP-aware user interfaces
char *puff_session_subdialog(char *r,const char *s,const char *t,char *zz,int qq) // let the user pick a file within a ZIP archive ('r' ZIP path, 's' pattern, 't' title, 'zz' default file or NULL); NULL for cancel, 'r' (with full path) for OK
{
	unsigned char rr[STRMAX],*z=session_scratch; // list of files in archive
	int l=0,i=-1; // number of files, default selection
	if (puff_open(strcpy(rr,r))) // error?
		return NULL;
	while (!puff_head())
	{
		static char x[]={'*',PATHCHAR,'.','*',0}; // exclude hidden folders and files
		if (*puff_name!='.'&&puff_name[strlen(puff_name)-1]!=PATHCHAR&&!globbing(x,puff_name,0)) // visible/hidden? file/folder?
			if (multiglobbing(s,puff_name,1))
				++l,z=&session_scratch[sortedinsert(session_scratch,z-session_scratch,puff_name)];
		puff_body(0);
		if (z>=&session_scratch[length(session_scratch)-STRMAX])
			{ z+=1+sprintf(z,"..."); break; } // too many files :-(
	}
	puff_close();
	if (!l) // no matching files?
		return NULL;
	session_filedialog_set_readonly(1); // ZIP archives cannot be modified
	if (l==1) // a matching file?
		return qq?strcpy(session_parmtr,strcat(strcat(rr,PUFF_STR),session_scratch)):NULL; // 'qq' enables automatic OK
	i=sortedsearch(session_scratch,z-session_scratch,zz);
	*z++=0; //sprintf(z,"Archive %s",rr); // END OF LIST: use it to store the window caption
	return session_list(i,session_scratch,rr)<0?NULL:strcpy(session_parmtr,strcat(strcat(rr,PUFF_STR),session_parmtr));
}
#define puff_session_zipdialog(r,s,t) puff_session_subdialog(r,s,t,NULL,1)

char *puff_session_filedialog(char *r,char *s,char *t,int q,int f) // ZIP archive wrapper for session_getfile
{
	char rr[STRMAX],ss[STRMAX],*z,*zz;
	sprintf(ss,"%s;%s",puff_pattern,s); if (!r) r=session_path; strcpy(rr,r); // NULL path = default!
	for (int qq=0;;) // try either a file list or the file dialog until the user either chooses a file or quits
	{
		if (zz=strrstr(rr,PUFF_STR)) // 'rr' holds the path, does it contain the separator already?
		{
			*zz=0; zz+=3/*strlen(PUFF_STR)*/; // *zz++=0; // now it either points to the previous file name or to a zero
			if (z=puff_session_subdialog(rr,s,t,zz,qq))
				return z;
		}
		if (!(z=(q?session_getfilereadonly(rr,ss,t,f):session_getfile(rr,ss,t)))) // what did the user choose?
			return NULL; // user cancel: give up
		if (!multiglobbing(puff_pattern,z,1))
			return z; // normal file: accept it
		strcat(strcpy(rr,z),PUFF_STR); // ZIP archive: append and try again
		qq=1; // tag repetition
	}
}
#define puff_session_getfile(x,y,z) puff_session_filedialog(x,y,z,0,0)
#define puff_session_getfilereadonly(x,y,z,q) puff_session_filedialog(x,y,z,1,q)
char *puff_session_newfile(char *x,char *y,char *z) // writing within ZIP archives isn't allowed, so they must be avoided if present
{
	char xx[STRMAX],*zz;
	strcpy(xx,x); // cancelling must NOT destroy the source path
	if (zz=strrstr(xx,PUFF_STR))
		while (--zz>=xx&&*zz!=PATHCHAR)
			*zz=0; // remove ZIP archive and file within
	return session_newfile(xx,y,z);
}

// on-screen symbols and messages ----------------------------------- //

VIDEO_UNIT onscreen_ink0,onscreen_ink1; char onscreen_flag=1;
#define onscreen_inks(q0,q1) (onscreen_ink0=q0,onscreen_ink1=q1)
VIDEO_UNIT *onscreen_pxpy(int x,int y)
{
	if ((x*=8)<0) x+=VIDEO_OFFSET_X+VIDEO_PIXELS_X; else x+=VIDEO_OFFSET_X;
	if ((y*=ONSCREEN_SIZE)<0) y+=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y; else y+=VIDEO_OFFSET_Y;
	return &video_frame[y*VIDEO_LENGTH_X+x];
}
void onscreen_bool(int x,int y,int lx,int ly,int q) // draw a rectangle at `x,y` of size `lx,ly` and boolean value `q`
{
	VIDEO_UNIT *p=onscreen_pxpy(x,y); q=q?onscreen_ink1:onscreen_ink0;
	lx*=8; ly*=ONSCREEN_SIZE;
	for (;ly;p+=VIDEO_LENGTH_X-lx,--ly)
		for (x=lx;x;--x)
			*p++=q;
}
void onscreen_hgauge(int x,int y,int lx,int ly,int q) // draw a horizontal gauge, q sets its value (-lx..0..lx)
{
	if (q>lx) q=lx; else if (q<-lx) q=-lx;
	if (q<0) // negative gauge that fills from right to left
		onscreen_bool(x,y,lx+q,ly,0),onscreen_bool(x+lx+q,y,  -q,ly,1);
	else
		onscreen_bool(x,y,   q,ly,1),onscreen_bool(x   +q,y,lx-q,ly,0);
}
void onscreen_char(int x,int y,int z) // draw a 7-bit character; the eighth bit is the INVERSE flag
{
	VIDEO_UNIT *p=onscreen_pxpy(x,y); int q=z&128?-1:0;
	const unsigned char *zz=&onscreen_chrs[(z&127)*ONSCREEN_SIZE];
	for (y=ONSCREEN_SIZE;y;--y)
	{
		int bb=*zz++; bb|=bb>>1; // normal, rather than thin or bold
		bb^=q; // apply inverse if required
		for (x=2;x;p+=VIDEO_LENGTH_X-8,--x)
			for (z=128;z;z>>=1)
				*p++=(z&bb)?onscreen_ink1:onscreen_ink0;
	}
}
void onscreen_text(int x,int y,const char *s,int q) // write an ASCIZ string
{
	if (q) q=128; // inverse video mask
	for (int z;z=*s++;++x) if (z&96) onscreen_char(x,y,z+q); // first 32 ASCII chars are invisible
}
void onscreen_byte(int x,int y,int a,int q) // write two decimal digits
{
	q=q?128+'0':'0';
	onscreen_char(x,y,(a/10)+q); // beware, this will show 100 as ":0"
	onscreen_char(x+1,y,(a%10)+q);
}

// built-in general-purpose debugger -------------------------------- //

void session_backupvideo(VIDEO_UNIT *t) // make a clipped copy of the current screen; used by the debugger and the SDL2 UI
{
	for (int y=0;y<VIDEO_PIXELS_Y;++y)
		MEMNCPY(&t[y*VIDEO_PIXELS_X],&video_frame[(VIDEO_OFFSET_Y+y)*VIDEO_LENGTH_X+VIDEO_OFFSET_X],VIDEO_PIXELS_X);
}

#define KBDBG_UP     11
#define KBDBG_DOWN   10
#define KBDBG_LEFT   14
#define KBDBG_RIGHT  15
#define KBDBG_HOME   16
#define KBDBG_END    17
#define KBDBG_NEXT   18
#define KBDBG_PRIOR  19
#define KBDBG_TAB     9
#define KBDBG_TAB_S   8
#define KBDBG_RET    13
#define KBDBG_RET_S  12
#define KBDBG_SPC    32
#define KBDBG_SPC_S  31
#define KBDBG_ESCAPE 27
#define KBDBG_CLOSE  28
#define KBDBG_CLICK  29
int debug_xlat(int k) // turns non-alphanumeric keypresses (-1 for mouse click) into pseudo-ASCII codes
{ switch (k) {
	case -1: return KBDBG_CLICK;
	case KBCODE_LEFT   : return KBDBG_LEFT;
	case KBCODE_RIGHT  : return KBDBG_RIGHT;
	case KBCODE_UP     : return KBDBG_UP;
	case KBCODE_DOWN   : return KBDBG_DOWN;
	case KBCODE_HOME   : return KBDBG_HOME;
	case KBCODE_END    : return KBDBG_END;
	case KBCODE_PRIOR  : return KBDBG_PRIOR;
	case KBCODE_NEXT   : return KBDBG_NEXT;
	case KBCODE_TAB    : return session_shift?KBDBG_TAB_S:KBDBG_TAB;
	case KBCODE_SPACE  : return session_shift?KBDBG_SPC_S:KBDBG_SPC;//' '
	case KBCODE_BKSPACE: return session_shift?KBDBG_RIGHT:KBDBG_LEFT;
	case KBCODE_X_ENTER: case KBCODE_ENTER: return session_shift?KBDBG_RET_S:KBDBG_RET;
	case KBCODE_ESCAPE : return KBDBG_ESCAPE;
	default: return 0;
} }

#define DEBUG_MAGICK 64
BYTE debug_point[1<<16]; // the breakpoint table; +1..15 = log register #n, +16 user breakpoint, +64 virtual magick, +128 volatile breakpoint
#ifdef POWER_BOOST1
#define power_boosted (power_boost!=POWER_BOOST0)
BYTE power_boost=POWER_BOOST1; // power-up boost flag; tied to the debugger because it's invariably based on virtual magick breakpoints
#endif
BYTE debug_break,debug_inter=0; // do special opcodes trigger the debugger? (f.e. $EDFF on Z80 and $12 on 6502/6510)
BYTE debug_config; // store the general debugger options here; the CPU-specific ones go elsewhere
int main_t=0,stop_t=0; // the global tick counter, used by the debugger, and optionally by the emulator

FILE *debug_logfile; int debug_logsize; BYTE debug_logtemp[1<<9]; // the byte recorder datas

#define debug_setup() (debug_dirty=session_signal&SESSION_SIGNAL_DEBUG,MEMZERO(debug_point),debug_logfile=NULL)
#define debug_configread(i) (debug_config=(i)/4,debug_break=(i)&2,debug_mode=(i)&1)
#define debug_configwrite() (debug_config*4+(debug_break?2:0)+(debug_mode?1:0))

int debug_logbyte(BYTE z) // log a byte, if possible; 0 OK, !0 ERROR
{
	if (!debug_logfile) // try creating a log file?
	{
		char *s; if (s=session_newfile(NULL,"*","Register log file"))
			debug_logfile=fopen(s,"wb"),debug_logsize=0;
		if (!debug_logfile) // do we have a valid file?
			return 1; // ERROR!
	}
	debug_logtemp[debug_logsize]=z;
	if (++debug_logsize>=length(debug_logtemp))
		fwrite1(debug_logtemp,sizeof(debug_logtemp),debug_logfile),debug_logsize=0;
	return 0; // OK
}
void debug_close(void) // debugger cleanup: flush logfile if open
{
	if (debug_logfile)
	{
		fwrite1(debug_logtemp,debug_logsize,debug_logfile);
		fclose(debug_logfile); debug_logfile=NULL;
	}
}

// the debugger's user interface ------------------------------------ //

#define DEBUG_LENGTH_X 64
#define DEBUG_LENGTH_Y 32
BYTE debug_buffer[DEBUG_LENGTH_X*DEBUG_LENGTH_Y+1];
#define DEBUG_LOCATE(x,y) (&debug_buffer[(y)*DEBUG_LENGTH_X+(x)])
#define debug_posx() ((VIDEO_PIXELS_X-DEBUG_LENGTH_X*8)/2)
#define debug_posy() ((VIDEO_PIXELS_Y-DEBUG_LENGTH_Y*ONSCREEN_SIZE)/2)

// these functions must be provided by the CPU code
void debug_reset(void); // clean temporary breakpoints, do `debug_dirty=1` and update PC and SP coordinates
void debug_clean(void); // perform any cleanup or state change when the user requests to exit the debugger
char *debug_list(void); // return a string of letters of registers that can be logged; first char must be SPACE!
BYTE debug_peek(WORD); // receive a byte from reading address WORD
BYTE debug_pook(int,WORD); // receive a byte from READING (!) or WRITING (0) address WORD
void debug_poke(WORD,BYTE); // send the value BYTE to address WORD
void debug_regs(char*,int); // fill a string with the printable info of register #int
void debug_regz(BYTE); // send nibble BYTE to the register under the current register table coordinates
WORD debug_dasm(char*,WORD); // fill a string with the disassembly of address WORD
WORD debug_pull(WORD); // receive a word from the stack address WORD
void debug_push(WORD,WORD); // send a word to the the stack address WORD (1st)
void debug_jump(WORD); // set the current program counter as WORD
void debug_step(void); // run just one operation (i.e. STEP INTO)
void debug_fall(void); // set a RETURN trap and exit the debugger
void debug_drop(WORD); // set a volatile breakpoint in the address WORD and exit the debugger
void debug_leap(void); // run until the operation after the current one (i.e. STEP OVER)
WORD debug_this(void); // get the program counter
WORD debug_that(void); // get the stack pointer

// these functions must be provided by the hardware code
int grafx_mask(void); // VRAM address mask, usually 64K-1
int grafx_size(int); // how many pixels wide is one byte?
BYTE grafx_peek(int); // receive the value BYTE from address `int`
void grafx_poke(int,BYTE); // send the value BYTE to address `int`
int grafx_show(VIDEO_UNIT*,int,int,int,int); // blit a set of pixels
void debug_info(int); // write a page of hardware information between the disassembly and the register table
void grafx_info(VIDEO_UNIT*,int,int); // draw additional infos on the top right corner
#define debug_peekpook(x) debug_pook(debug_mode,(x))

// these functions provide services for the CPU and hardware debugging code
#define DEBUG_INFOZ(n) (DEBUG_LOCATE(DEBUG_LENGTH_X-DEBUG_INFOX-10,(n))) // shortcut for debug_info to print each text line
#define debug_hilight1(t) (*(t)|=128) // set inverse video on exactly onecharacters
void debug_hilight2(char *t) { *t|=128,t[1]|=128; } // set inverse video on exactly two characters
void debug_hilight(char *t,int n) { while (n-->0) *t++|=128; } // set inverse video on a N-character string

int debug_grafx_m=0XFFFF,debug_grafx_i=0; // VRAM address binary mask: MSX1 VRAM is 16K, MSX2 VRAM is either 64K or 128K, etc.
BYTE debug_panel=0,debug_page=0,debug_grafx,debug_mode=0,debug_match,debug_grafx_l=1; // debugger options: visual style, R/W mode, disasm flags...
WORD debug_panel0_w,debug_panel1_w=1,debug_panel2_w=0,debug_panel3_w; // must be unsigned!
INT8 debug_panel0_x,debug_panel1_x=0,debug_panel2_x=0,debug_panel3_x,debug_grafx_v=0; // must be signed!
WORD debug_longdasm(char *t,WORD p) // disassemble code and include its hexadecimal dump
{
	WORD q=p; memset(t,' ',9); p=debug_dasm(t+9,p);
	for (BYTE n=4,b;q!=p&&n>0;++q,--n)
		b=debug_peek(q),*t++=hexa1[b>>4],*t++=hexa1[b&15];
	if (q!=p) t[-2]='\177',t[-1]='.'; // too long? truncate with an ellipsis! :(
	return p;
}
int session_debug_eval(char *s) // parse very simple expressions till an unknown character appears
{
	int t=0,k='+',c; do
	{
		while (*s==' ') ++s; // trim spaces
		int i=0; if (*s=='.') // decimal?
			while ((c=eval_dec(*++s))>=0)
				i=(i*10)+c;
		else
			while ((c=eval_hex(*s))>=0)
				i=(i<<4)+c,++s;
		while (*s==' ') ++s; // trim spaces
		switch (k)
		{
			case '+': t+=i; break;
			case '-': t-=i; break;
			case '&': t&=i; break;
			case '|': t|=i; break;
			case '^': t^=i; break;
			default: return t;
		}
	}
	while ((k=*s++)>' ');
	return t;
}
void session_debug_print(char *t,int x,int y,int n) // print a line of `n` characters of debug text
{
	static BYTE debug_chrs[128*ONSCREEN_SIZE],config=16; if (config!=debug_config) // redo font?
		switch (((config=debug_config&=15)>>2)&3)
		{
			case 1: MEMLOAD(debug_chrs,onscreen_chrs); break; // THIN
			case 2: for (int i=0,j=0;i<sizeof(debug_chrs);++i) j=onscreen_chrs[i]>>1,debug_chrs[i]=(i%ONSCREEN_SIZE)>=ONSCREEN_SIZE/2?(j<<1)|j:j; break; // ITALIC
			case 0: for (int i=0,j=0;i<sizeof(debug_chrs);++i) j=onscreen_chrs[i],debug_chrs[i]=j|(j>>1); break; // NORMAL
			case 3: for (int i=0,j=0;i<sizeof(debug_chrs);++i) j=onscreen_chrs[i],debug_chrs[i]=(j<<1)|j|(j>>1); break; // BOLD
		}
	for (;n>0;++x,--n)
	{
		BYTE z=*t++,w=(z&128)?-1:0; if (!(z&=127)) z+=32;
		VIDEO_UNIT p0=(config&2)?0XFFFFFF:0,p1=0XFFFFFF^p0;
		const unsigned char *zz=&debug_chrs[z*ONSCREEN_SIZE];
		VIDEO_UNIT *p=&debug_frame[(y*ONSCREEN_SIZE+debug_posy())*VIDEO_PIXELS_X+x*8+debug_posx()];
		for (int yy=0;yy<ONSCREEN_SIZE;++yy,p+=VIDEO_PIXELS_X-8)
			if (!debug_grafx&&(config&1))
			{
				for (int xx=128,q=yy,ww=w^*zz++;xx;++q,++p,xx>>=1)
					if (xx&ww)
						*p=p1,p[1]=p[VIDEO_PIXELS_X]=p[VIDEO_PIXELS_X+1]=p0; // dot and shadows
					else
						#ifdef RAWWW // quick'n'dirty
						if (q&1) *p=p0; // chequered background
						#else // standard half'n'half
						if (*p!=p0) *p=VIDEO_FILTER_SRGB(*p,p0); // translucent background
						#endif
			}
			else
				for (int xx=128,ww=w^*zz++;xx;++p,xx>>=1)
					*p=(xx&ww)?p1:p0;
	}
}

void session_debug_show(int redo) // shows the current debugger
{
	static int videox,videoy,videoz; int i; BYTE b; char *t;
	memset(debug_buffer,0,sizeof(debug_buffer)); // clear buffer
	if (redo||((videox^video_pos_x)|(videoy^video_pos_y)|(videoz^video_pos_z)))
		debug_reset(); // reset debugger and background!
	videoz=video_pos_z; session_backupvideo(debug_frame); // translucent debug text needs this :-/
	if ((videoy=video_pos_y)>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-1)
		for (int x=0,z=((video_pos_y&-2)-VIDEO_OFFSET_Y)*VIDEO_PIXELS_X;x<VIDEO_PIXELS_X;++x,++z)
			debug_frame[z]=debug_frame[z+VIDEO_PIXELS_X]^=x&16?0XFF0000:0X00FFFF;
	if ((videox=video_pos_x)>=VIDEO_OFFSET_X&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X-1)
		for (int y=0,z=(video_pos_x&-2)-VIDEO_OFFSET_X;y<VIDEO_PIXELS_Y;++y,z+=VIDEO_PIXELS_X)
			debug_frame[z]=debug_frame[z+1]^=y&2?0XFF0000:0X00FFFF;
	if (debug_grafx)
	{
		debug_grafx_m=grafx_mask(); int z=0,j=debug_grafx_i; if (debug_grafx_v&1) // VERTICAL FIRST, HORIZONTAL LAST?
		{
			for (;z+debug_grafx_l<=DEBUG_LENGTH_Y*ONSCREEN_SIZE;z+=debug_grafx_l)
				for (int x=0;x<DEBUG_LENGTH_X*8;x+=grafx_size(1))
					j=grafx_show(&debug_frame[(debug_posy()+z)*VIDEO_PIXELS_X+debug_posx()+x],VIDEO_PIXELS_X,debug_grafx_l,j,debug_grafx_v>>1);
			for (;z<DEBUG_LENGTH_Y*ONSCREEN_SIZE;++z)
			{
				i=(debug_posy()+z)*VIDEO_PIXELS_X+debug_posx();
				for (int x=0;x<DEBUG_LENGTH_X*8;++x)
					debug_frame[i+x]=0X808080; // padding
			}
		}
		else // HORIZONTAL FIRST, VERTICAL LAST!
		{
			for (;z+grafx_size(debug_grafx_l)<=DEBUG_LENGTH_X*8;z+=grafx_size(debug_grafx_l))
				for (int y=0;y<DEBUG_LENGTH_Y*ONSCREEN_SIZE;++y)
					j=grafx_show(&debug_frame[(debug_posy()+y)*VIDEO_PIXELS_X+debug_posx()+z],grafx_size(1),debug_grafx_l,j,debug_grafx_v>>1);
			for (int y=0;y<DEBUG_LENGTH_Y*ONSCREEN_SIZE;++y)
			{
				i=(debug_posy()+y)*VIDEO_PIXELS_X+debug_posx();
				for (int x=z;x<DEBUG_LENGTH_X*8;++x)
					debug_frame[i+x]=0X808080; // padding
			}
		}
		grafx_info(&debug_frame[debug_posy()*VIDEO_PIXELS_X+debug_posx()+DEBUG_LENGTH_X*8],VIDEO_PIXELS_X,debug_grafx_v>>1);
		sprintf(debug_buffer,(debug_grafx_m>0XFFFF?"%05X..%05X":"$%04X..$%04X"),debug_grafx_i&debug_grafx_m,(j-1)&debug_grafx_m);
		session_debug_print(debug_buffer,DEBUG_LENGTH_X-12,DEBUG_LENGTH_Y-2,12);
		sprintf(debug_buffer,"%03d%c W:back!",debug_grafx_l,(debug_grafx_v&1)?'Y':'X');
		session_debug_print(debug_buffer,DEBUG_LENGTH_X-12,DEBUG_LENGTH_Y-1,12);
	}
	else
	{
		// the order of the printing hinges on sprintf() appending a ZERO to each string :-/
		// top right panel: the CPU registers
		WORD p; for (i=0;i<DEBUG_LENGTH_Y/2;++i)
			debug_regs(DEBUG_LOCATE(DEBUG_LENGTH_X-9,i),i);
		// bottom right panel: the stack editor
		for (i=DEBUG_LENGTH_Y/2,p=debug_panel3_w;i<DEBUG_LENGTH_Y;++i)
		{
			sprintf(DEBUG_LOCATE(DEBUG_LENGTH_X-9,i),"%04X:%04X",p,debug_pull(p)); p+=2;
			if (debug_match) // hilight active stack item
				debug_hilight(DEBUG_LOCATE(DEBUG_LENGTH_X-9,i),4);
		}
		// print the hardware info next to the CPU registers
		debug_info(debug_page);
		// top left panel: the disassembler
		for (i=0,p=debug_panel0_w;i<DEBUG_LENGTH_Y/2;++i)
		{
			sprintf(DEBUG_LOCATE(0,i),"%04X:%c",p,debug_point[p]&16?'@':debug_list()[debug_point[p]&15]);
			p=debug_longdasm(DEBUG_LOCATE(6,i),p);
			if (debug_match) // hilight current opcode
				debug_hilight(DEBUG_LOCATE(0,i),4);
		}
		// bottom left panel: the memory editor
		for (i=DEBUG_LENGTH_Y/2,p=(debug_panel2_w&-16)-(DEBUG_LENGTH_Y/2-DEBUG_LENGTH_Y/4)*16;i<DEBUG_LENGTH_Y;++i)
		{
			sprintf(DEBUG_LOCATE(0,i),"%04X:",p);
			char *tt=DEBUG_LOCATE(5+32,i); *tt++=debug_mode?'\\':'/'; // normal mode (favouring RAM over ROM) is '/'
			t=DEBUG_LOCATE(5,i); for (int j=0;j<16;++j)
				b=debug_peekpook(p),
				*tt++=(b&96)?b:('.'+(b&128)),
				*t++=hexa1[b>>4],*t++=hexa1[b&15],++p;
		}
		// the status bar (BREAK, timer, X:Y, help) and the cursors
		sprintf(DEBUG_LOCATE(4,DEBUG_LENGTH_Y/2-1),"\177. BREAK%c  Timer %010u   XY %04d,%03d  H:help!",debug_break?'*':'-',main_t-stop_t,video_pos_x,video_pos_y); // notice the ellipsis at the beginning
		switch (debug_panel&=3)
		{
			case 0: *DEBUG_LOCATE(6+(debug_panel0_x&=1),0)|=128; break;
			case 1: *DEBUG_LOCATE(DEBUG_LENGTH_X-4+debug_panel1_x,debug_panel1_w)|=128; break;
			case 2: *DEBUG_LOCATE(5+(debug_panel2_x&=1)+(debug_panel2_w&15)*2,DEBUG_LENGTH_Y/2+DEBUG_LENGTH_Y/4)|=128; break;
			case 3: *DEBUG_LOCATE(DEBUG_LENGTH_X-4+(debug_panel3_x&=3),DEBUG_LENGTH_Y/2)|=128; break;
		}
		for (int y=0;y<DEBUG_LENGTH_Y;++y)
			session_debug_print(&debug_buffer[y*DEBUG_LENGTH_X],0,y,DEBUG_LENGTH_X);
	}
}

void debug_panel0_rewind(int n)
{
	while (n-->0)
	{
		WORD p=debug_panel0_w-DEBUG_LONGEST;
		while ((debug_dasm(debug_buffer,p)-debug_panel0_w)&64) ++p; // find the best longest match
		debug_panel0_w=p;
	}
}
void debug_panel0_fastfw(int n)
{
	while (n-->0)
		debug_panel0_w=debug_dasm(debug_buffer,debug_panel0_w);
}
BYTE debug_panel_string[STRMAX]="",debug_panel_pascal[STRMAX]; // looking for things like "$2100C0" must allow zeros!
void debug_panel_search(int d) // look for a Pascal string of bytes on memory; the memory dump search is case-insensitive
{
	WORD p=debug_panel?debug_panel2_w:debug_panel0_w,o=p; d=d?-1:1;
	do
	{
		WORD q=p+=d; int i=1,j=*debug_panel_pascal; if (debug_panel) // memory dump
		{
			while (i<=j&&ucase(debug_panel_pascal[i])==ucase(debug_peekpook(q))) ++i,++q;
			if (i>j) { debug_panel2_x=0,debug_panel2_w=p; return; }
		}
		else // disassembly
		{
			while (i<=j&&debug_panel_pascal[i]==debug_peek(q)) ++i,++q;
			if (i>j) { debug_panel0_x=0,debug_panel0_w=p; return; }
		}
	}
	while (p!=o);
}
#define debug_maus_x() ((session_maus_x-debug_posx())/8)
#define debug_maus_y() ((session_maus_y-debug_posy())/ONSCREEN_SIZE)
int session_debug_user(int k) // handles the debug event `k`; !0 valid event, 0 invalid
{
	int i; if (k==KBDBG_ESCAPE) // EXIT
		session_signal&=~SESSION_SIGNAL_DEBUG,debug_clean();
	else if (k==KBDBG_SPC) // STEP INTO
		debug_step();
	else if (k==KBDBG_SPC_S) // NEXT SCANLINE
		session_signal_scanlines|=SESSION_SIGNAL_DEBUG,session_signal&=~SESSION_SIGNAL_DEBUG;
	else if (k==KBDBG_RET) // STEP OVER
		debug_leap();
	else if (k==KBDBG_RET_S) // NEXT FRAME
		session_signal_frames|=SESSION_SIGNAL_DEBUG,session_signal&=~SESSION_SIGNAL_DEBUG;
	else if ((k=ucase(k))=='H') // HELP
		session_message(
			"Cursors\tNavigate panel\n"
			"Tab\tNext panel (shift: prev. panel)\n"
			"0-9,A-F\tEdit hexadecimal value\n"
			"G\tGo to ADDRESS ('.'+number: decimal)\n"
			"H\tThis help\n"
			"I\tInput bytes from FILE\n"
			"J\tJump to cursor\n"
			"K\tClose log file (see L)\n"
			"L\tLog 8-bit REGISTER into FILE\n"
			"M\tToggle memory dump R/W mode\n"
			"N\tNext search (shift: prev. search; see S)\n"
			"O\tOutput LENGTH bytes into FILE\n"
			"P\tPrint disassembly of LENGTH bytes into FILE\n"
			"Q\tRun to interrupt\n"
			"R\tRun to cursor\n"
			"S\tSearch for STRING ('$'+string: hexadecimal)\n"
			"T\tReset timer\n"
			"U\tRun to return\n"
			"V\tToggle appearance\n"
			"W\tToggle debug/graphics mode\n"
			"X\tToggle hardware info panels\n"
			"Y\tFill LENGTH bytes with BYTE\n"
			"Z\tDelete all breakpoints\n"
			",\tToggle BREAK opcode\n"
			".\tToggle breakpoint\n"
			"Space\tStep into (shift: skip scanline)\n"
			"Return\tStep over (shift: skip frame)\n"
			"Escape\tExit debugger\n","Debugger help");
	else if (k=='M') // TOGGLE MEMORY DUMP R/W MODE
		debug_mode=!debug_mode;
	else if (k=='Q') // RUN TO INTERRUPT
		debug_inter=1,session_signal&=~SESSION_SIGNAL_DEBUG;
	else if (k=='R') // RUN TO CURSOR
		debug_drop(debug_panel0_w);
	else if (k=='U') // RUN TO RETURN
		debug_fall();
	else if (k=='V') // TOGGLE APPEARANCE
		debug_config+=session_shift?-1:1;
	else if (k=='W') // TOGGLE DEBUG/GRAPHICS MODE
		debug_grafx=!debug_grafx;
	else if (debug_grafx)
	{
		if (k==KBDBG_TAB)
			++debug_grafx_v;
		else if (k==KBDBG_TAB_S)
			--debug_grafx_v;
		else if (k==KBDBG_LEFT)
			--debug_grafx_i;
		else if (k==KBDBG_RIGHT)
			++debug_grafx_i;
		else if (k==KBDBG_UP)
			debug_grafx_i-=debug_grafx_l;
		else if (k==KBDBG_DOWN)
			debug_grafx_i+=debug_grafx_l;
		else if (k==KBDBG_HOME)
			{ if (debug_grafx_l>1) --debug_grafx_l; }
		else if (k==KBDBG_END)
			{ if (debug_grafx_l<DEBUG_LENGTH_X*8/grafx_size(1)) ++debug_grafx_l; }
		else if (k==KBDBG_PRIOR)
			debug_grafx_i-=debug_grafx_l*16;
		else if (k==KBDBG_NEXT)
			debug_grafx_i+=debug_grafx_l*16;
		else if (k=='G') // GO TO..
		{
			sprintf(session_parmtr,(debug_grafx_m>0XFFFF)?"%05X":"%04X",debug_grafx_i&debug_grafx_m);
			if (session_input("Go to")>0)
				debug_grafx_i=session_debug_eval(session_parmtr)&debug_grafx_m;
		}
		else if (k=='I') // INPUT BYTES FROM FILE..
		{
			char *s; FILE *f; int w=debug_grafx_i;
			if (s=puff_session_getfile(NULL,"*","Input file"))
				if (f=puff_fopen(s,"rb"))
				{
					while (i=fread1(session_substr,256,f)) // better than fgetc()
						for (int j=0;j<i;++j)
							grafx_poke(w++,session_substr[j]);
					puff_fclose(f);
					//debug_grafx_i=w&debug_grafx_m; // auto increase; not everyone likes it
				}
		}
		else if (k=='O') // OUTPUT BYTES INTO FILE..
		{
			char *s; FILE *f; int w=debug_grafx_i;
			if (*session_parmtr=0,session_input("Output length")>=0)
				if ((i=session_debug_eval(session_parmtr))>0&&i<=(debug_grafx_m+1))
					if (s=session_newfile(NULL,"*","Output file"))
						if (f=fopen(s,"wb"))
						{
							while (i)
							{
								int j; for (j=0;j<i&&j<256;++j)
									session_substr[j]=grafx_peek(w++);
								fwrite1(session_substr,j,f); i-=j;
							}
							fclose(f);
						}
		}
		else
			k=0; // default!
	}
	else if ((k>='0'&&k<='9')||(k>='A'&&k<='F')) // HEXADECIMAL NIBBLE
	{
		if ((k-='0')>=16) k-=7; // get actual nibble
		switch (debug_panel)
		{
			case 0: i=debug_peek(debug_panel0_w);
				debug_poke(debug_panel0_w,debug_panel0_x?(i&240)+k:(i&15)+(k<<4));
				if (++debug_panel0_x>1) ++debug_panel0_w,debug_panel0_x=0;
				break;
			case 1: debug_regz(k);
				break;
			case 2: i=debug_peekpook(debug_panel2_w);
				debug_poke(debug_panel2_w,debug_panel2_x?(i&240)+k:(i&15)+(k<<4));
				if (++debug_panel2_x>1) ++debug_panel2_w,debug_panel2_x=0;
				break;
			case 3: debug_push(debug_panel3_w,(debug_pull(debug_panel3_w)&(-1^(0XF000>>(debug_panel3_x*4))))+(k<<(12-debug_panel3_x*4)));
				if (++debug_panel3_x>3) debug_panel3_x=0,debug_panel3_w+=2;
				break;
		}
		k=1; // nonzero!
	}
	else switch (k)
	{
		case KBDBG_CLICK: // CLICK!
			{
				int x,y; if ((x=debug_maus_x())>=0&&x<DEBUG_LENGTH_X&&(y=debug_maus_y())>=0&&y<DEBUG_LENGTH_Y)
				{
					if (y<DEBUG_LENGTH_Y/2) // top half?
					{
						if (x<33) // disassembly?
							debug_panel0_fastfw(y),debug_panel=0,debug_panel0_x=0;
						else if (x>=DEBUG_LENGTH_X-9) // registers
							debug_panel=1,debug_panel1_w=y,debug_panel1_x=x-(DEBUG_LENGTH_X-4);
						else
							k=0; // default!
					}
					else // bottom half
					{
						if (x>=5&&x<37) // memory dump?
							debug_panel=2,debug_panel2_w=(debug_panel2_w&-16)+(y-DEBUG_LENGTH_Y/4*3)*16+((x-5)/2),debug_panel2_x=x-5;
						else if (x>=DEBUG_LENGTH_X-9) // stack
							debug_panel=3,debug_panel3_w+=2*(y-(DEBUG_LENGTH_Y/2)),debug_panel3_x=x-(DEBUG_LENGTH_X-4),x=x<0?0:x;
						else
							k=0; // default!
					}
				}
				else
					k=0; // default!
			}
			break;
		case KBDBG_TAB:
			++debug_panel; break;
		case KBDBG_TAB_S:
			--debug_panel; break;
		case KBDBG_LEFT:
			switch (debug_panel)
			{
				case 0: if (--debug_panel0_x<0) debug_panel0_x=1,--debug_panel0_w; break;
				case 1: if (--debug_panel1_x<0) debug_panel1_x=0; break;
				case 2: if (--debug_panel2_x<0) debug_panel2_x=1,--debug_panel2_w; break;
				case 3: if (--debug_panel3_x<0) debug_panel3_x=3,debug_panel3_w-=2; break;
			}
			break;
		case KBDBG_RIGHT:
			switch (debug_panel)
			{
				case 0: if (++debug_panel0_x>1) debug_panel0_x=0,++debug_panel0_w; break;
				case 1: if (++debug_panel1_x>3) debug_panel1_x=3; break;
				case 2: if (++debug_panel2_x>1) debug_panel2_x=0,++debug_panel2_w; break;
				case 3: if (++debug_panel3_x>3) debug_panel3_x=0,debug_panel3_w+=2; break;
			}
			break;
		case KBDBG_UP:
			switch (debug_panel)
			{
				case 0: debug_panel0_rewind(1); break;
				case 1: if (debug_panel1_w>1) --debug_panel1_w; break;
				case 2: debug_panel2_w-=16; break;
				case 3: debug_panel3_w-=2; break;
			}
			break;
		case KBDBG_DOWN:
			switch (debug_panel)
			{
				case 0: debug_panel0_fastfw(1); break;
				case 1: if (debug_panel1_w<DEBUG_LENGTH_Y/2-1) ++debug_panel1_w; break;
				case 2: debug_panel2_w+=16; break;
				case 3: debug_panel3_w+=2; break;
			}
			break;
		case KBDBG_HOME:
			switch (debug_panel)
			{
				case 0: debug_panel0_x=debug_panel0_w=0; break;
				case 1: debug_panel1_x=0; break;
				case 2: debug_panel2_x=debug_panel2_w=0; break;
				case 3: debug_panel3_x=debug_panel3_w=0; break;
			}
			break;
		case KBDBG_END:
			switch (debug_panel)
			{
				case 0: debug_panel0_x=0,debug_panel0_w=debug_this(); break;
				case 1: debug_panel1_x=3; break;
				case 2: debug_panel2_x=0,debug_panel2_w=debug_this(); break;
				case 3: debug_panel3_x=0,debug_panel3_w=debug_that(); break;
			}
			break;
		case KBDBG_PRIOR:
			switch (debug_panel)
			{
				case 0: debug_panel0_rewind(DEBUG_LENGTH_Y/2-1); break;
				case 1: debug_panel1_w=1; break;
				case 2: debug_panel2_w-=DEBUG_LENGTH_Y/2*16; break;
				case 3: debug_panel3_w-=DEBUG_LENGTH_Y/2*2; break;
			}
			break;
		case KBDBG_NEXT:
			switch (debug_panel)
			{
				case 0: debug_panel0_fastfw(DEBUG_LENGTH_Y/2-1); break;
				case 1: debug_panel1_w=DEBUG_LENGTH_Y/2-1; break;
				case 2: debug_panel2_w+=DEBUG_LENGTH_Y/2*16; break;
				case 3: debug_panel3_w+=DEBUG_LENGTH_Y/2*2; break;
			}
			break;
		case 'G': // GO TO..
			if (debug_panel!=1)
			{
				sprintf(session_parmtr,"%04X",debug_panel>2?debug_panel3_w:debug_panel?debug_panel2_w:debug_panel0_w);
				if (session_input("Go to")>0)
					switch (i=session_debug_eval(session_parmtr),debug_panel)
					{
						case 0: debug_panel0_x=0; debug_panel0_w=(WORD)i; break;
						case 2: debug_panel2_x=0; debug_panel2_w=(WORD)i; break;
						case 3: debug_panel3_x=0; debug_panel3_w=(WORD)i; break;
					}
			}
			break;
		case 'I': // INPUT BYTES FROM FILE..
			if (!(debug_panel&1))
			{
				char *s; FILE *f; WORD w=debug_panel?debug_panel2_w:debug_panel0_w;
				if (s=puff_session_getfile(NULL,"*","Input file"))
					if (f=puff_fopen(s,"rb"))
					{
						while (i=fread1(session_substr,256,f)) // better than fgetc()
							for (int j=0;j<i;++j)
								debug_poke(w++,session_substr[j]);
						puff_fclose(f);
						//if (debug_panel) debug_panel2_w=w; else debug_panel0_w=w; // auto increase; not everyone likes it
					}
			}
			break;
		case 'J': // JUMP TO CURSOR
			debug_jump(debug_panel0_w); break;
		case 'K': // CLOSE LOG
			debug_close(); break;
		case 'L': // LOG REGISTER.. (overwrites BREAKPOINT!)
			if (*session_parmtr=0,sprintf(session_parmtr+1,"Log register%s",debug_list()),session_input(session_parmtr+1)==1)
			{
				char *t=strchr(debug_list(),ucase(session_parmtr[0])); if (t&&(i=t-debug_list()))
					debug_point[debug_panel0_w]=(debug_point[debug_panel0_w]&~31)+i;
			}
			break;
		case 'N': // NEXT SEARCH
			{ if (!(debug_panel&1)) if (*debug_panel_pascal) debug_panel_search(session_shift); } break;
		case 'O': // OUTPUT BYTES INTO FILE..
			if (!(debug_panel&1))
			{
				char *s; FILE *f; WORD w=debug_panel?debug_panel2_w:debug_panel0_w;
				if (*session_parmtr=0,session_input("Output length")>=0)
					if ((i=session_debug_eval(session_parmtr))>0&&i<=65536)
						if (s=session_newfile(NULL,"*","Output file"))
							if (f=fopen(s,"wb"))
							{
								while (i)
								{
									int j; if (debug_panel)
										for (j=0;j<i&&j<256;++j)
											session_substr[j]=debug_peekpook(w++);
									else
										for (j=0;j<i&&j<256;++j)
											session_substr[j]=debug_peek(w++);
									fwrite1(session_substr,j,f); i-=j;
								}
								fclose(f);
							}
			}
			break;
		case 'P': // PRINT DISASSEMBLY/HEXDUMP INTO FILE..
			if (!(debug_panel&1))
				if (*session_parmtr=0,session_input(debug_panel?"Hexa dump length":"Disassembly length")>=0)
				{
					char *s,*t; FILE *f; WORD w=debug_panel?debug_panel2_w:debug_panel0_w,u;
					if ((i=session_debug_eval(session_parmtr))>0&&i<=65536)
						if (s=session_newfile(NULL,"*.TXT",debug_panel?"Print hexa dump":"Print disassembly"))
							if (f=fopen(s,"w"))
							{
								if (debug_panel) // HEXDUMP?
								{
									k=0; do // WRAP!
									{
										if (!k)
											t=session_substr+sprintf(session_substr,"$%04X: ",w);
										else
											*t++=',';//t+=sprintf(t,",");
										t+=sprintf(t,"$%02X",debug_peekpook(w++));
										if (++k>=16)
											k=0,fprintf(f,"%s\n",session_substr);
									}
									while (--i>0);
									if (k)
										fprintf(f,"%s\n",session_substr);
									k=1; // nonzero!
								}
								else // DISASSEMBLY
									do // WRAP!
									{
										u=debug_longdasm(session_substr,w);
										fprintf(f,"$%04X: %s\n",w,session_substr);
										i-=(WORD)(u-w); w=u;
									}
									while (i>0);
								fclose(f);
							}
				}
			break;
		case 'S': // SEARCH FOR STRING..
			if (!(debug_panel&1))
				if (strcpy(session_parmtr,debug_panel_string),session_input("Search string")>=0)
				{
					strcpy(debug_panel_string,session_parmtr);
					if (*debug_panel_string=='$') // HEXA STRING?
						*debug_panel_pascal=256-hexa2byte(debug_panel_pascal+1,debug_panel_string+1,256); // STRMAX>=256!
					else // NORMAL STRING
						memcpy(debug_panel_pascal+1,debug_panel_string,*debug_panel_pascal=strlen(debug_panel_string));
					if (*debug_panel_pascal) debug_panel_search(0);
				}
			break;
		case 'T': // RESET CLOCK
			stop_t=main_t; break; // avoid trouble if the emulation needs a stable `main_t`
		case 'X': // SHOW MORE HARDWARE INFO
			++debug_page; break;
		case 'Y': // FILL BYTES WITH BYTE..
			if (!(debug_panel&1))
			{
				WORD w=debug_panel?debug_panel2_w:debug_panel0_w; BYTE b;
				if (*session_parmtr=0,session_input("Fill length")>=0)
					if (i=(WORD)session_debug_eval(session_parmtr))
						if (*session_parmtr=0,session_input("Filler byte")>=0)
						{
							b=session_debug_eval(session_parmtr);
							while (i--) debug_poke(w++,b);
							if (debug_panel) debug_panel2_w=w; else debug_panel0_w=w; // auto increase!
						}
			}
			break;
		case 'Z': // DELETE ALL BREAKPOINTS
			{ for (i=0;i<length(debug_point);++i) debug_point[i]&=64; } break; // respect virtual magick!
		case '.': // TOGGLE BREAKPOINT (erases REGISTER LOG!)
			if (debug_panel==0) { if (debug_point[debug_panel0_w]&31) debug_point[debug_panel0_w]&=~31; else debug_point[debug_panel0_w]|=16; } break;
		case ',': // TOGGLE `BRK` BREAKPOINT
			debug_break=!debug_break; break;
		default: k=0;
	}
	return k?(debug_dirty=1):0;
}

// multimedia file output ------------------------------------------- //

unsigned int session_savenext(char *z,unsigned int i) // scans for available filenames; `z` is "%s%u.ext" and `i` is the current index; returns 0 on error, or a new index
{
	while (i) // can this ever be false? for starters, that'd require four billion files in a directory!
	{
		sprintf(session_parmtr,z,session_path,i);
		if (getftype(session_parmtr)<0) // does the file exist?
			break; // no? excellent!
		++i; // try again
	}
	return i;
}

// multimedia: audio file output ------------------------------------ //

unsigned int session_nextwave=1,session_wavesize;
#if AUDIO_BITDEPTH > 8
BYTE session_wavedepth=0; // let the user reduce 16-bit audio to 8-bit
#else
#define session_wavedepth 0 // audio bitrate never changes
#endif
int session_createwave(void) // create a wave file; 0 OK, !0 ERROR
{
	if (session_wavefile)
		return 1; // file already open!
	if (!(session_nextwave=session_savenext("%s%08u.wav",session_nextwave)))
		return 1; // too many files!
	if (!(session_wavefile=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	static char h[44]="RIFF\000\000\000\000WAVEfmt \020\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000data"; // zero-padded
	h[22]=AUDIO_CHANNELS; // channels
	mputii(&h[24],AUDIO_PLAYBACK); // samples per second
	mputiiii(&h[28],(AUDIO_PLAYBACK*AUDIO_BYTESTEP)>>session_wavedepth); // bytes per second
	h[32]=AUDIO_BYTESTEP>>session_wavedepth; // bytes per sample
	h[34]=AUDIO_BITDEPTH>>session_wavedepth;
	fwrite(h,1,sizeof(h),session_wavefile);
	return session_wavesize=0;
}
void session_writewave(void)
{
	if (session_wavefile)
	{
		if (!session_wavedepth)
		{
			#if SDL_BYTEORDER == SDL_BIG_ENDIAN && AUDIO_BITDEPTH > 8 // swap HI-LO bytes, WAVE files follow the LO-HI style
			for (int i=0,j;i<AUDIO_LENGTH_Z*AUDIO_CHANNELS;++i) session_scratch[i*2]=j=audio_frame[i],session_scratch[i*2+1]=j>>8;
			session_wavesize+=fwrite(session_scratch,1,(AUDIO_LENGTH_Z*AUDIO_BYTESTEP)>>session_wavedepth,session_wavefile);
			#else
			session_wavesize+=fwrite(audio_frame,1,(AUDIO_LENGTH_Z*AUDIO_BYTESTEP)>>session_wavedepth,session_wavefile);
			#endif
		}
		else
		{
			for (int i=0;i<AUDIO_LENGTH_Z*AUDIO_CHANNELS;++i) session_scratch[i]=(audio_frame[i]>>8)+128;
			session_wavesize+=fwrite(session_scratch,1,(AUDIO_LENGTH_Z*AUDIO_BYTESTEP)>>session_wavedepth,session_wavefile);
		}
	}
}
int session_closewave(void) // close a wave file; 0 OK, !0 ERROR
{
	if (!session_wavefile) return 1;
	#if (AUDIO_CHANNELS*AUDIO_BITDEPTH) <= 8
	if (session_wavesize&1) fputc(0,session_wavefile); // RIFF even-padding
	#endif
	fseek(session_wavefile,0X28,SEEK_SET);
	fputiiii(session_wavesize,session_wavefile);
	fseek(session_wavefile,0X04,SEEK_SET);
	fputiiii(session_wavesize+36,session_wavefile); // file=head+data
	return fclose(session_wavefile),session_wavefile=NULL,0;
}

// multimedia: extremely primitive video+audio output! -------------- //
// warning: the following code assumes that VIDEO_UNIT is DWORD 0X00RRGGBB!

unsigned int session_nextfilm=1,session_filmfreq,session_filmcount;
BYTE session_filmflag,session_filmscale=1,session_filmtimer=1,session_filmalign; // format options
#define SESSION_FILMVIDEO_LENGTH (VIDEO_PIXELS_X*VIDEO_PIXELS_Y) // copy of the previous video frame
#define SESSION_FILMAUDIO_LENGTH (AUDIO_LENGTH_Z*AUDIO_CHANNELS*2) // copies of TWO audio frames
VIDEO_UNIT *session_filmvideo=NULL; AUDIO_UNIT *session_filmaudio=NULL;
BYTE *xrf_chunk=NULL; // this buffer contains one video frame and two audio frames AFTER encoding
BYTE session_filmdirt; // used to detect skipped frames

//#define xrf_encode1(n) (((a>>=1)||(*(b=t++)=0,a=128)),((n)&&(*b|=a,*t++=(n)&255))) // write "0" (zero) or "1nnnnnnnn" (nonzero)
#define xrf_encode1(n) xrf_encode_n(&t,&a,&b,(n))
void xrf_encode_n(BYTE **t,BYTE *a,BYTE **b,int n) // write "0" (zero) or "1nnnnnnnn" (nonzero)
{
	if (!(*a>>=1)) *(*b=(*t)++)=0,*a=128;
	if (n) **b|=*a,*(*t)++=n; // this will handle 256 as nonzero: 100000000
}
int xrf_encode(BYTE *t,BYTE *s,int l,int x) // terribly hacky encoder based on an 8-bit RLE and an interleaved pseudo Huffman filter!
{
	if (l<=0) return *t++=128,*t++=0,2; // quick END MARKER!
	BYTE *u=t,a=0,*b,q=0; if (x<0) x=-x,q=128; // x<0 = perform XOR 128 on bytes, used when turning 16s audio into 8u
	do
	{
		int n=l; BYTE c=*s; do s+=x; while (--l&&c==*s); n-=l; c^=q;
		while (n>66047) // i.e. 512+65535; this happens when the screen is blank, or when nothing moves
			xrf_encode1(c),xrf_encode1(c),xrf_encode1(255),xrf_encode1(255),xrf_encode1(255),n-=66047;
		xrf_encode1(c); if (n>1)
		{
			xrf_encode1(c); if (n>=256)
			{
				if (n>=512)
					xrf_encode1(255),n-=512,xrf_encode1(n>>8); // 512..66047 -> 0XFF 0X0000..0XFFFF (0..65535)
				else
					xrf_encode1(254); // 256..511 -> 0XFE 0X00..0XFF (0..255)
				n&=255;
			}
			else
				n-=2; // 2..255 -> 0X00..0XFD
			xrf_encode1(n);
		}
	}
	while (l);
	return xrf_encode1(256),t-u; // END MARKER is the special case "100000000"!
}

int session_createfilm(void) // start recording video and audio; 0 OK, !0 ERROR
{
	if (session_filmfile) return 1; // file already open!
	if (!session_filmvideo&&!(session_filmvideo=malloc(sizeof(VIDEO_UNIT[SESSION_FILMVIDEO_LENGTH]))))
		return 1; // cannot allocate buffer!
	if (!session_filmaudio&&!(session_filmaudio=malloc(sizeof(AUDIO_UNIT[SESSION_FILMAUDIO_LENGTH]))))
		return 1; // cannot allocate buffer!
	if (!xrf_chunk&&!(xrf_chunk=malloc((sizeof(VIDEO_UNIT[SESSION_FILMVIDEO_LENGTH])+sizeof(AUDIO_UNIT[SESSION_FILMAUDIO_LENGTH]))*9/8+4*8))) // maximum pathological length!
		return 1; // cannot allocate memory!
	if (!(session_nextfilm=session_savenext("%s%08u.xrf",session_nextfilm))) // "Xor-Rle Film"
		return 1; // too many files!
	if (!(session_filmfile=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	fwrite1("XRF1!\015\012\032",8,session_filmfile);
	fputmm(VIDEO_PIXELS_X>>session_filmscale,session_filmfile);
	fputmm(VIDEO_PIXELS_Y>>session_filmscale,session_filmfile);
	fputmm(session_filmfreq=audio_disabled?0:AUDIO_LENGTH_Z<<session_filmtimer,session_filmfile);
	fputc(VIDEO_PLAYBACK>>session_filmtimer,session_filmfile);
	fputc(session_filmflag=((AUDIO_BITDEPTH>>session_wavedepth)>8?1:0)+(AUDIO_CHANNELS>1?2:0),session_filmfile); // +16BITS(1)+STEREO(2)
	kputmmmm(0,session_filmfile); // frame count, will be filled later
	session_filmalign=video_pos_y; // catch scanline mode, if any
	return memset(session_filmvideo,0,sizeof(VIDEO_UNIT[SESSION_FILMVIDEO_LENGTH])),
		memset(session_filmaudio,0,sizeof(AUDIO_UNIT[SESSION_FILMAUDIO_LENGTH])),
		session_filmdirt=session_filmcount=0;
}
void session_writefilm(void) // record one frame of video and audio
{
	if (!session_filmfile) return; // file not open!
	// ignore first frame if the video is interleaved and we're on the wrong half frame
	if (!session_filmcount&&video_interlaced&&!video_interlaces) return;
	BYTE *z=xrf_chunk; if (!video_framecount) session_filmdirt=1; // frameskipping?
	if (!(++session_filmcount&session_filmtimer))
	{
		if (!session_filmdirt) // skipped frame?
		{
			z+=xrf_encode(z,NULL,0,0); // B
			z+=xrf_encode(z,NULL,0,0); // G
			z+=xrf_encode(z,NULL,0,0); // R
			//z+=xrf_encode(z,NULL,0,0); // A!
		}
		else
		{
			session_filmdirt=0; // to avoid encoding multiple times a frameskipped image
			VIDEO_UNIT *s,*t=session_filmvideo; // notice that this backup doesn't include secondary scanlines
			if (session_filmscale)
				for (int i=VIDEO_OFFSET_Y+(session_filmalign&1);s=session_getscanline(i),i<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;i+=2)
					for (int j=0;j<VIDEO_PIXELS_X/2;++j)
						*t++^=*s++,++s; // bitwise delta against last frame
			else
				for (int i=VIDEO_OFFSET_Y;s=session_getscanline(i),i<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;++i)
					for (int j=0;j<VIDEO_PIXELS_X;++j)
						*t++^=*s++; // bitwise delta against last frame
			// the compression relies on perfectly standard arrays, i.e. the stride of an array of DWORDs is always 4 bytes wide;
			// are there any exotic platforms where the byte length of WORD/Uint16 and DWORD/Uint32 isn't strictly enforced?
			#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			z+=xrf_encode(z,&((BYTE*)session_filmvideo)[3],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // B
			z+=xrf_encode(z,&((BYTE*)session_filmvideo)[2],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // G
			z+=xrf_encode(z,&((BYTE*)session_filmvideo)[1],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // R
			//z+=xrf_encode(z,&((BYTE*)session_filmvideo)[0],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // A!
			#else
			z+=xrf_encode(z,&((BYTE*)session_filmvideo)[0],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // B
			z+=xrf_encode(z,&((BYTE*)session_filmvideo)[1],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // G
			z+=xrf_encode(z,&((BYTE*)session_filmvideo)[2],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // R
			//z+=xrf_encode(z,&((BYTE*)session_filmvideo)[3],(VIDEO_PIXELS_X*VIDEO_PIXELS_Y)>>(2*session_filmscale),4); // A!
			#endif
			t=session_filmvideo;
			if (session_filmscale)
				for (int i=VIDEO_OFFSET_Y+(session_filmalign&1);s=session_getscanline(i),i<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;i+=2)
					for (int j=0;j<VIDEO_PIXELS_X/2;++j)
						*t++=*s++,++s; // keep this frame for later
			else // no scaling, copy
				for (int i=VIDEO_OFFSET_Y;i<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;++i)
					MEMNCPY(t,session_getscanline(i),VIDEO_PIXELS_X),t+=VIDEO_PIXELS_X;
		}
		if (session_filmfreq) // audio?
		{
			BYTE *s=(BYTE*)audio_frame; int l=AUDIO_LENGTH_Z;
			if (session_filmtimer)
				s=(BYTE*)session_filmaudio,l*=2,memcpy(&session_filmaudio[AUDIO_LENGTH_Z*AUDIO_CHANNELS],audio_frame,AUDIO_LENGTH_Z*AUDIO_BYTESTEP); // glue both blocks!
			#if AUDIO_CHANNELS > 1
			#if AUDIO_BITDEPTH > 8
				#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				if (session_wavedepth)
					z+=xrf_encode(z,&s[0],l,-4), // L
					z+=xrf_encode(z,&s[2],l,-4); // R
				else
					z+=xrf_encode(z,&s[1],l,4), // l
					z+=xrf_encode(z,&s[0],l,4), // L
					z+=xrf_encode(z,&s[3],l,4), // r
					z+=xrf_encode(z,&s[2],l,4); // R
				#else
				if (session_wavedepth)
					z+=xrf_encode(z,&s[1],l,-4), // L
					z+=xrf_encode(z,&s[3],l,-4); // R
				else
					z+=xrf_encode(z,&s[0],l,4), // l
					z+=xrf_encode(z,&s[1],l,4), // L
					z+=xrf_encode(z,&s[2],l,4), // r
					z+=xrf_encode(z,&s[3],l,4); // R
				#endif
			#else
			z+=xrf_encode(z,&s[0],l,2), // L
			z+=xrf_encode(z,&s[1],l,2); // R
			#endif
			#else
			#if AUDIO_BITDEPTH > 8
				#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				if (session_wavedepth)
					z+=xrf_encode(z,&s[0],l,-2); // M
				else
					z+=xrf_encode(z,&s[1],l,2), // m
					z+=xrf_encode(z,&s[0],l,2); // M
				#else
				if (session_wavedepth)
					z+=xrf_encode(z,&s[1],l,-2); // M
				else
					z+=xrf_encode(z,&s[0],l,2), // m
					z+=xrf_encode(z,&s[1],l,2); // M
				#endif
			#else
			z+=xrf_encode(z,&s[0],l,1); // M
			#endif
			#endif
		}
		fputmmmm(z-xrf_chunk,session_filmfile); // chunk size!
		fwrite1(xrf_chunk,z-xrf_chunk,session_filmfile);
	}
	else if (session_filmfreq) // audio?
		memcpy(session_filmaudio,audio_frame,AUDIO_LENGTH_Z*AUDIO_BYTESTEP); // keep audio block for later!
	session_filmalign=video_pos_y;
}
int session_closefilm(void) // stop recording video and audio; 0 OK, !0 ERROR
{
	if (xrf_chunk) free(xrf_chunk),xrf_chunk=NULL;
	if (session_filmvideo) free(session_filmvideo),session_filmvideo=NULL;
	if (session_filmaudio) free(session_filmaudio),session_filmaudio=NULL;
	if (!session_filmfile) return 1; // file not open!
	fseek(session_filmfile,16,SEEK_SET);
	fputmmmm(session_filmcount>>session_filmtimer,session_filmfile); // number of frames
	return fclose(session_filmfile),session_filmfile=NULL,0;
}

// multimedia: screenshot output ------------------------------------ //
// warning: the following code assumes that VIDEO_UNIT is DWORD 0X00RRGGBB!

unsigned int session_nextbitmap=1;

// Optional QOI 24-bit output, based on three operations that return the amount of bytes they stored in session_qoi3_temp[]
// 1.- session_qoi3_init(x,y) prepares the encoding procedure and generates the appropriate QOI file header;
// 2.- session_qoi3_main(s,l,d) encodes `l` pixels in steps of `d` from VIDEO_UNIT* `s` into QOI data;
// 3.- session_qoi3_exit() flushes any pending data and generates the footer of the QOI file.
// For the interested, https://qoiformat.org/ (format specification) + https://github.com/phoboslab/qoi (reference codec)
#define session_qoi3_temp (session_scratch+VIDEO_LENGTH_X*sizeof(VIDEO_UNIT)) // this should never overflow :-X
#define session_qoi3_calc(x) ((((x)>>16)*3+((x)>>8)*5+(x)*7+53)&63) // Alpha is always 255, so (255*11)&63= 53
VIDEO_UNIT session_qoi3_hash[64],session_qoi3_prev; char session_qoi3_size;
int session_qoi3_init(int x,int y)
{
	BYTE *t=session_qoi3_temp;
	memset(session_qoi3_hash,-1,sizeof(session_qoi3_hash)),session_qoi3_prev=session_qoi3_size=0; // session_qoi3_hash[] must be invalid!
	mputmmmm(t,0X716F6966),mputmmmm((t+4),x),mputmmmm((t+8),y),mputmm((t+12),0X0300),t+=14; // header: "qoif", width, height, RGB minus A
	return t-session_qoi3_temp;
}
int session_qoi3_main(VIDEO_UNIT *s,int l,int d) // where `d` is 1 to encode all pixels, 2 to encode one half
{
	BYTE *t=session_qoi3_temp;
	VIDEO_UNIT p; while (l>0)
	{
		p=*s; s+=d,l-=d; // remember, VIDEO_UNIT is 0X00RRGGBB
		if (session_qoi3_prev==p) { if (++session_qoi3_size==62) *t++=253,session_qoi3_size=0; continue; } // ++QOI_OP_RUN
		if (session_qoi3_size) *t++=191+session_qoi3_size,session_qoi3_size=0; // flush QOI_OP_RUN if it still holds data!
		int h=session_qoi3_calc(p); if (session_qoi3_hash[h]==p) { *t++=h,session_qoi3_prev=p; continue; } // QOI_OP_INDEX
		BYTE r=((p>>16)-(session_qoi3_prev>>16))+2,g=((p>>8)-(session_qoi3_prev>>8))+2,b=(p-session_qoi3_prev)+2; // diffs
		session_qoi3_hash[h]=session_qoi3_prev=p; if (g<4&&r<4&&b<4) { *t++=64+(r<<4)+(g<<2)+b; } // one byte: QOI_OP_DIFF
		else if ((g+=32-2)<64&&(r+=8-2+32-g)<16&&(b+=8-2+32-g)<16) { *t++=128+g,*t++=(r<<4)+b; } // two bytes: QOI_OP_LUMA
		else { *t++=254,*t++=p>>16,*t++=p>>8,*t++=p; } // three bytes: QOI_OP_RGB; we don't store Alpha, so no QOI_OP_RGBA
	}
	return t-session_qoi3_temp;
}
int session_qoi3_exit(void)
{
	BYTE *t=session_qoi3_temp;
	{ if (session_qoi3_size) *t++=191+session_qoi3_size; } mputmmmm(t,0),mputmmmm(t+4,1),t+=8; // flush QOI_OP_RUN, put footer
	return t-session_qoi3_temp;
}

char session_qoi3_flag=0;
INLINE int session_savebitmap(void) // save a RGB888 BMP/QOI file; 0 OK, !0 ERROR
{
	if (!(session_nextbitmap=session_savenext(session_qoi3_flag?"%s%08u.qoi":"%s%08u.bmp",session_nextbitmap)))
		return 1; // too many files!
	FILE *f; if (!(f=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	int i; if (session_qoi3_flag)
	{
		fwrite(session_qoi3_temp,1,session_qoi3_init(VIDEO_PIXELS_X>>session_filmscale,VIDEO_PIXELS_Y>>session_filmscale),f);
		for (i=VIDEO_OFFSET_Y;i<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;i+=session_filmscale+1)
			fwrite(session_qoi3_temp,1,session_qoi3_main(session_getscanline(i),VIDEO_PIXELS_X,session_filmscale+1),f);
		fwrite(session_qoi3_temp,1,session_qoi3_exit(),f);
	}
	else
	{
		static char h[54]="BM\000\000\000\000\000\000\000\000\066\000\000\000\050\000\000\000\000\000\000\000\000\000\000\000\001\000\030\000"; // zero-padded
		i=(VIDEO_PIXELS_X*VIDEO_PIXELS_Y*3)>>(2*session_filmscale);
		mputiiii(&h[2],sizeof(h)+i); mputiiii(&h[34],i);
		mputii(&h[18],VIDEO_PIXELS_X>>session_filmscale);
		mputii(&h[22],VIDEO_PIXELS_Y>>session_filmscale);
		fwrite(h,1,sizeof(h),f);
		for (i=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-session_filmscale-1;i>=VIDEO_OFFSET_Y;i-=session_filmscale+1)
		{
			VIDEO_UNIT *s=session_getscanline(i),v;
			BYTE *t=session_scratch; if (session_filmscale)
				for (int j=0;j<VIDEO_PIXELS_X;j+=2) // turn two ARGB into one RGB
				{
					v=*s++; //if (v!=*s) v=VIDEO_FILTER_SRGB(v,*s); // not worth it
					++s,*t++=v,*t++=v>>8,*t++=v>>16; // B, G, R
				}
			else
				for (int j=0;j<VIDEO_PIXELS_X;++j) // turn each ARGB into one RGB
					*t++=v=*s++,*t++=v>>8,*t++=v>>16; // B, G, R
			fwrite(session_scratch,1,t-session_scratch,f);
		}
	}
	return fclose(f),0;
}

#if 0 // right now we don't need a QOI reader :-/
// This QOI 24-bit decoder is straightforward: each operation receives a source `s` of `i` bytes;
// each succesful operation returns the amount of processed bytes N so the caller can do s+=N,i-=N.
int session_qoi3_head(int *w,int *h,BYTE *s,int i) // process the header: NEGATIVE if bad, header size if OK
{
	if (i<14+8||!equalsmmmm(s,0X716F6966)) return -1;
	memset(session_qoi3_hash,-1,sizeof(session_qoi3_hash)),session_qoi3_prev=0;
	session_qoi3_size=(*w=mgetmmmm(s+4))*(*h=mgetmmmm(s+8));
	return *w>0&&*h>0?14:-1;
}
int session_qoi3_body(VIDEO_UNIT *t,BYTE *s,int i) // process the body: NEGATIVE if bad, body size if OK
{
	BYTE *u=s,r,g,b; DWORD p; int o=session_qoi3_size;
	while (i>8&&o>0)
	{
		--i,b=*s++; if (b<128)
			if (b<64) // QOI_OP_INDEX?
				{ --o,*t++=session_qoi3_prev=session_qoi3_hash[b]; continue; }
			else // QOI_OP_DIFF
			{
				r=(b>>4)-4,g=(b>>2)&3,b&=3;
				r+=(session_qoi3_prev>>16)-2,g+=(session_qoi3_prev>>8)-2,b+=session_qoi3_prev-2;
				p=(r<<16)+(g<<8)+b;
			}
		else if (b<254)
			if (b<192) // QOI_OP_LUMA?
			{
				--i,g=b-128-32,r=(b=*s++)>>4,b&=15; r+=g-8,b+=g-8;
				r+=session_qoi3_prev>>16,g+=session_qoi3_prev>>8,b+=session_qoi3_prev;
				p=(r<<16)+(g<<8)+b;
			}
			else // QOI_OP_RUN?
			{
				if ((o-=b-=191)<0) break;
				p=session_qoi3_prev; do *t++=p; while (--b);
				session_qoi3_hash[session_qoi3_calc(p)]=p; continue;
			}
		else // QOI_OP_RGB/QOI_OP_RGBA
			{ i-=3,p=*s++<<16,p+=*s++<<8,p+=*s++; if (b&1) --i,++s; } // skip A
		--o,*t++=session_qoi3_hash[session_qoi3_calc(p)]=session_qoi3_prev=p;
	}
	return o?-1:s-u;
}
int session_qoi3_foot(BYTE *s,int i) // process the footer: NEGATIVE if bad, footer size if OK
	{ return i>7&&equalsmmmm(s+0,0)&&equalsmmmm(s+4,1)?8:-1; }
#endif

// configuration functions ------------------------------------------ //

char *UTF8_BOM(char *s) // skip UTF8 BOM if present
	{ return (239==(BYTE)*s&&187==(BYTE)s[1]&&191==(BYTE)s[2])?s+3:s; }
FILE *session_configfile(int q) // returns handle to configuration file, `q`?read:write
{
	return fopen(strcat(strcpy(session_parmtr,session_path),
	#ifdef _WIN32
	my_caption ".ini" // "filename.ini"
	#else
	"." my_caption "rc" // ".filenamerc"
	#endif
	),q?"r":"w");
}
char *session_configread(void) // reads configuration file; session_parmtr holds the current line; returns NULL if handled, the unknown label otherwise
{
	char *t=UTF8_BOM(session_parmtr),*s=t; while (*s) ++s; // go to trail
	while (s!=t&&(BYTE)*--s<=' ') *s=0; // clean trail up
	s=t; while ((BYTE)*s>' ') ++s; *s++=0; // divide name and data
	while (*s==' ') ++s; // skip spaces between both
	if (*s) // handle common parameters, unless empty
	{
		if (!strcasecmp(t,"hardaudio")) return audio_mixmode=*s&3,NULL;
		if (!strcasecmp(t,"softaudio")) return audio_filter=*s&3,NULL;
		if (!strcasecmp(t,"hardvideo")) return video_scanline=(*s>>1)&3,video_pageblend=*s&1,NULL;
		if (!strcasecmp(t,"softvideo")) return video_filter=*s&7,NULL;
		if (!strcasecmp(t,"zoomvideo")) return session_zoomblit=(*s>>1)&7,session_zoomblit=session_zoomblit>4?4:session_zoomblit,video_lineblend=*s&1,NULL;
		if (!strcasecmp(t,"safevideo")) return session_softblit=*s&1,video_gammaflag=!(*s&2),video_microwave=(*s>>2)&1,NULL;
		if (!strcasecmp(t,"safeaudio")) return session_softplay=(~*s)&3,NULL; // stay compatible with old configs (ZERO was accelerated, NONZERO wasn't)
		if (!strcasecmp(t,"film")) return session_filmscale=*s&1,session_filmtimer=(*s>>1)&1,session_wavedepth=(*s>>2)&1,NULL;
		if (!strcasecmp(t,"info")) return onscreen_flag=*s&1,session_qoi3_flag=(*s>>1)&1,video_fineblend=(*s>>2)&1,NULL;
	}
	return s;
}
void session_configwrite(FILE *f) // save common parameters
{
	fprintf(f,"film %d\ninfo %d\n"
		"hardaudio %d\nsoftaudio %d\nhardvideo %d\nsoftvideo %X\n"
		"zoomvideo %d\nsafevideo %d\nsafeaudio %d\n"
		,session_filmscale+session_filmtimer*2+session_wavedepth*4,onscreen_flag+session_qoi3_flag*2+video_fineblend*4
		,audio_mixmode,audio_filter,video_scanline*2+video_pageblend,video_filter,
		video_lineblend+session_zoomblit*2,session_softblit+(video_gammaflag?0:2)+video_microwave*4,session_softplay^3);
}

void session_configreadmore(char*); // must be defined by the emulator!
int session_prae(char *s) // load configuration and set stuff up; `s` is argv[0]
{
	if (s=strrchr(strcpy(session_path,s),PATHCHAR)) // detect session path using argv[0] as reference
		s[1]=0; // keep separator
	else
		*session_path=0; // no path (?)
	FILE *f; if ((f=session_configfile(1)))
	{
		while (fgets(session_parmtr,STRMAX-1,f)) session_configreadmore(session_configread());
		fclose(f);
	}
	#ifndef RGB2LINEAR
	video_recalc();
	#endif
	return debug_setup(),0; // set debugger up as soon as possible
}
void session_configwritemore(FILE*); // ditto!
int session_post(void) // save configuration and shut stuff down; always 0 OK
{
	puff_byebye();
	session_closefilm();
	session_closewave();
	FILE *f; if ((f=session_configfile(0)))
	{
		session_configwritemore(f),session_configwrite(f);
		fclose(f);
	}
	return debug_close(),0; // shut debugger down as late as possible
}

char txt_error[]="Error!";
#if defined(DEBUG) || defined(SDL_MAIN_HANDLED)
void printferror(char *s) { printf("error: %s\n",s); }
#define printfusage(s) printf("%s " MY_LICENSE "\n\n" s,session_caption) // the console help shows the authorship
#else
void printferror(char *s) { session_message(s,txt_error); }
#define printfusage(s) session_message(s,session_caption) // authorship is in the ABOUT box instead of the HELP box
#endif

// =================================== END OF OS-INDEPENDENT ROUTINES //
