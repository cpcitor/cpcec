 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// A bunch of routines can be shared across emulators and operating
// systems because they either operate with standard data types or
// rely on talking to the C standard library rather than the OS.

// START OF OS-INDEPENDENT ROUTINES ================================= //

#define GPL_3_INFO \
	"This program comes with ABSOLUTELY NO WARRANTY; for more details" "\n" \
	"please read the GNU General Public License. This is free software" "\n" \
	"and you are welcome to redistribute it under certain conditions."

#define MEMZERO(x) memset((x),0,sizeof(x))
#define MEMFULL(x) memset((x),-1,sizeof(x))
#define MEMSAVE(x,y) memcpy((x),(y),sizeof(y))
#define MEMLOAD(x,y) memcpy((x),(y),sizeof(x))
#define MEMNCPY(x,y,z) memcpy((x),(y),sizeof(*(x))*(z))

int fread1(void *t,int l,FILE *f) { int k=0,i; while (l&&(i=fread(t,1,l,f))) { t=(void*)((char*)t+i); k+=i; l-=i; } return k; } // safe fread(t,1,l,f)
int fwrite1(void *t,int l,FILE *f) { int k=0,i; while (l&&(i=fwrite(t,1,l,f))) { t=(void*)((char*)t+i); k+=i; l-=i; } return k; } // safe fwrite(t,1,l,f)

// byte-order based operations -------------------------------------- //

// based on the hypothetical `mgetc(x) (*(x))` and `mputc(x,y) (*(x)=(y))`,
// and considering 'i' = lil-endian (Intel) and 'm' = big-endian (Motorola)
// notice that the fgetXXXX functions cannot ensure that a negative number always means EOF was met!

#if SDL_BYTEORDER == SDL_BIG_ENDIAN

typedef union { WORD w; struct { unsigned char h,l; } b; } Z80W; // big-endian: PPC, ARM...

int  mgetii(unsigned char *x) { return (x[1]<<8)+*x; }
void mputii(unsigned char *x,int y) { x[1]=y>>8; *x=y; }
int  mgetiiii(unsigned char *x) { return (x[3]<<24)+(x[2]<<16)+(x[1]<<8)+*x; }
void mputiiii(unsigned char *x,int y) { x[3]=y>>24; x[2]=y>>16; x[1]=y>>8; *x=y; }
#define mgetmm(x) (*(WORD*)(x))
#define mgetmmmm(x) (*(DWORD*)(x))
#define mputmm(x,y) ((*(WORD*)(x))=(y))
#define mputmmmm(x,y) ((*(DWORD*)(x))=(y))
// WORD/DWORD `i` must be a constant!
#define equalsmm(x,i) (*(WORD*)(x)==(i))
#define equalsmmmm(x,i) (*(DWORD*)(x)==(i))
#define equalsii(x,i) (*(WORD*)(x)==(WORD)(((i>>8)&255)+((i&255)<<8)))
#define equalsiiii(x,i) (*(DWORD*)(x)==(DWORD)(((i>>24)&255)+((i&(255<<16))>>8)+((i&(255<<8))<<8)+((i&255)<<24)))

int fgetii(FILE *f) { int i=fgetc(f); return i|(fgetc(f)<<8); } // common lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { fputc(i,f); return fputc(i>>8,f); } // common lil-endian 16-bit fputc()
int fgetiiii(FILE *f) { int i=fgetc(f); i|=fgetc(f)<<8; i|=fgetc(f)<<16; return i|(fgetc(f)<<24); } // common lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { fputc(i,f); fputc(i>>8,f); fputc(i>>16,f); return fputc(i>>24,f); } // common lil-endian 32-bit fputc()
int fgetmm(FILE *f) { int i=0; return (fread(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fgetc()
int fputmm(int i,FILE *f) { return (fwrite(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fputc()
int fgetmmmm(FILE *f) { int i=0; return (fread(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fgetc()
int fputmmmm(int i,FILE *f) { return (fwrite(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fputc()

#else

typedef union { WORD w; struct { unsigned char l,h; } b; } Z80W; // lil-endian: i86, x64...

#define mgetii(x) (*(WORD*)(x))
#define mgetiiii(x) (*(DWORD*)(x))
#define mputii(x,y) ((*(WORD*)(x))=(y))
#define mputiiii(x,y) ((*(DWORD*)(x))=(y))
int  mgetmm(unsigned char *x) { return (*x<<8)+x[1]; }
void mputmm(unsigned char *x,int y) { *x=y>>8; x[1]=y; }
int  mgetmmmm(unsigned char *x) { return (*x<<24)+(x[1]<<16)+(x[2]<<8)+x[3]; }
void mputmmmm(unsigned char *x,int y) { *x=y>>24; x[1]=y>>16; x[2]=y>>8; x[3]=y; }
// WORD/DWORD `i` must be a constant!
#define equalsii(x,i) (*(WORD*)(x)==(i))
#define equalsiiii(x,i) (*(DWORD*)(x)==(i))
#define equalsmm(x,i) (*(WORD*)(x)==(WORD)(((i>>8)&255)+((i&255)<<8)))
#define equalsmmmm(x,i) (*(DWORD*)(x)==(DWORD)(((i>>24)&255)+((i&(255<<16))>>8)+((i&(255<<8))<<8)+((i&255)<<24)))

int fgetii(FILE *f) { int i=0; return (fread(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { return (fwrite(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fputc()
int fgetiiii(FILE *f) { int i=0; return (fread(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { return (fwrite(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fputc()
int fgetmm(FILE *f) { int i=fgetc(f)<<8; return i|fgetc(f); } // common big-endian 16-bit fgetc()
int fputmm(int i,FILE *f) { fputc(i>>8,f); return fputc(i,f); } // common big-endian 16-bit fputc()
int fgetmmmm(FILE *f) { int i=fgetc(f)<<24; i|=fgetc(f)<<16; i|=fgetc(f)<<8; return i|fgetc(f); } // common big-endian 32-bit fgetc()
int fputmmmm(int i,FILE *f) { fputc(i>>24,f); fputc(i>>16,f); fputc(i>>8,f); return fputc(i,f); } // common big-endian 32-bit fputc()

#endif

// auxiliary functions ---------------------------------------------- //

const char hexa1[16]="0123456789ABCDEF"; // handy for `*t++=` operations
int eval_hex(int c) // 0..15 OK, <0 ERROR!
	{ return (c>='0'&&c<='9')?c-'0':(c>='A'&&c<='F')?c-'A'+10:(c>='a'&&c<='f')?c-'a'+10:-1; }
int eval_dec(int c) // 0..9 OK, <0 ERROR!
	{ return (c>='0'&&c<='9')?c-'0':-1; }
void byte2hexa(char *t,BYTE *s,int n) { while (n-->0) { int z=*s++; *t++=hexa1[z>>4],*t++=hexa1[z&15]; } }
int hexa2byte(BYTE *t,char *s,int n) { int h,l; while (n>0&&(h=eval_hex(*s++))>=0&&(l=eval_hex(*s++))>=0) *t++=(h<<4)+l,--n; *t=0; return n; }

char *strrstr(char *h,char *n) // = strrchr + strstr (case sensitive!)
{
	char *z=h+strlen(h)-strlen(n); // skip last bytes that cannot match
	while (z>=h)
	{
		char *s=z,*t=n;
		while (*t&&*s==*t)
			++s,++t;
		if (!*t)
			return z;
		--z;
	}
	return NULL;
}
int globbing(char *w,char *t,int q) // wildcard pattern `*w` against string `*t`; `q` = strcasecmp/strcmp; 0 on mismatch!
{
	char *ww=NULL,*tt=NULL,c,k; // a terribly dumbed-down take on Kirk J. Krauss' algorithm
	if (q) // case insensitive
		while (c=*t)
			if ((k=*w++)=='*') // wildcard?
			{
				while (*w=='*') ++w; // skip trailing wildcards
				if (!*w) return 1; // end of pattern? succeed!
				tt=t,ww=w; // remember wildcard and continue
			}
			else if (k!='?'&&ucase(k)!=ucase(c)) // wrong character?
			{
				if (!ww) return 0; // no past wildcards? fail!
				t=++tt,w=ww; // return to wildcard and continue
			}
			else ++t; // right character, continue
	else // case sensitive
		while (c=*t)
			if ((k=*w++)=='*') // wildcard?
			{
				while (*w=='*') ++w; // skip trailing wildcards
				if (!*w) return 1; // end of pattern? succeed!
				tt=t,ww=w; // remember wildcard and continue
			}
			else if (k!='?'&&k!=c) // wrong character?
			{
				if (!ww) return 0; // no past wildcards? fail!
				t=++tt,w=ww; // return to wildcard and continue
			}
			else ++t; // right character, continue
	while (*w=='*') ++w; // skip trailing wildcards
	return !*w; // succeed on end of pattern
}
int multiglobbing(char *w,char *t,int q) // like globbing(), but with multiple patterns with semicolons inbetween; 0 on mismatch, 1..n tells which pattern matches
{
	int n=1,c; char *m; do
	{
		m=session_substr; // the caller must not use this variable
		while ((c=*w++)&&c!=';') *m++=c; *m=0;
		if (globbing(session_substr,t,q))
			return n;
	}
	while (++n,c);
	return 0;
}

// the following case-insensitive algorithms are weak, but proper binary search is difficult on packed lists; fortunately, items are more likely to be partially sorted in advance
int sortedinsert(char *t,int z,char *s) // insert string `s` in its alphabetical order within the packed list of strings `t` of length `z`; returns the list's new length
{
	char *m=t+z; while (m>t)
	{
		char *n=m; do --n; while (n>t&&n[-1]); // backwards search is more convenient on partially sorted lists
		if (strcasecmp(s,n)>=0)
			break;
		m=n;
	}
	int l=strlen(s)+1,i=z-(m-t); if (i>0) memmove(m+l,m,i);
	return memcpy(m,s,l),z+l;
}
int sortedsearch(char *t,int z,char *s) // look for string `s` in an alphabetically ordered packed list of strings `t` of length `z`; returns index (not offset!) in list
{
	if (!s||!*s) return -1; // invalid `s` is never within `t`!
	char *r=&t[z]; int i=0,l=1+strlen(s); while (t<r)
	{
		char *u=t; while (*u++) ;
		if (u-t==l&&!strcasecmp(s,t)) // minor optimisation: don't compare if lengths don't match
			return i;
		t=u; ++i;
	}
	return -1; // 's' was not found!
}

// interframe functions --------------------------------------------- //
// warning: the following code assumes that VIDEO_UNIT is DWORD 0X00RRGGBB!

#define VIDEO_FILTER_MASK_X 1
#define VIDEO_FILTER_MASK_Y 2
#define VIDEO_FILTER_MASK_Z 4
int video_pos_z=0; // for timekeeping, statistics and debugging
BYTE video_lineblend=0,video_pageblend=0,audio_mixmode=1; // 0 = pure mono, 1 = pure stereo, 2 = 50%, 3 = 25%
BYTE video_scanline=0,video_scanlinez=8; // 0 = all scanlines, 1 = half scanlines, 2 = full interlace, 3 = half interlace, 4 = average scanlines
VIDEO_UNIT video_lastscanline,video_halfscanline; // the fillers used in video_endscanlines() and the lightgun buffer
#ifdef MAUS_LIGHTGUNS
VIDEO_UNIT video_litegun; // lightgun data
#endif

// *!* are there any systems where VIDEO_UNIT is NOT the DWORD 0X00RRGGBB!? *!*
//#define VIDEO_FILTER_HALF(x,y) (x==y?x:x<y?((((x&0XFF00FF)+(y&0XFF00FF)+0X10001)&0X1FE01FE)+(((x&0XFF00)+(y&0XFF00)+0X100)&0X1FE00))>>1:((((x&0XFF00FF)+(y&0XFF00FF))&0X1FE01FE)+(((x&0XFF00)+(y&0XFF00))&0X1FE00))>>1) // 50:50, better
#define VIDEO_FILTER_HALF(x,y) (x==y?x:((((x&0XFF00FF)+(y&0XFF00FF)+0X10001)&0X1FE01FE)+(((x&0XFF00)+(y&0XFF00)+0X100)&0X1FE00))>>1) // 50:50, faster bur coarser: f(old,new) becomes f(0,0)=0, f(0,1)=1, f(1,0)=1 instead of 0, f(1,1)=1
//#define VIDEO_FILTER_BLURDATA vzz
//#define VIDEO_FILTER_BLUR0(z) vzz=z
//#define VIDEO_FILTER_BLUR(r,z) (r=VIDEO_FILTER_HALF(vzz,z),vzz=z) // 50:50
//#define VIDEO_FILTER_BLURDATA v0h,v0l,vzh,vzl
//#define VIDEO_FILTER_BLUR0(z) v0h=z&0XFF00FF,v0l=z&0XFF00
//#define VIDEO_FILTER_BLUR(r,z) r=(((v0h+(vzh=z&0XFF00FF)+0X10001)&0X1FE01FE)+((v0l+(vzl=z&0XFF00)+0X100)&0x1FE00))>>1,v0h=vzh,v0l=vzl // 50:50
//#define VIDEO_FILTER_BLURDATA v1h,v1l,v0h,v0l,vzh,vzl
//#define VIDEO_FILTER_BLUR0(z) v1h=v0h=z&0XFF00FF,v1l=v0l=z&0XFF00
//#define VIDEO_FILTER_BLUR(r,z) r=(((v1h+v0h+(vzh=z&0XFF00FF)*2+0X20002)&0X3FC03FC)+((v1l+v0l+(vzl=z&0XFF00)*2+0X200)&0x3FC00))>>2,v1h=v0h,v1l=v0l,v0h=vzh,v0l=vzl // 25:25:50
#define VIDEO_FILTER_BLURDATA v2h,v2l,v1h,v1l,v0h,v0l,vzh,vzl
#define VIDEO_FILTER_BLUR0(z) v2h=v1h=v0h=z&0XFF00FF,v2l=v1l=v0l=z&0XFF00
#define VIDEO_FILTER_BLUR(r,z) r=(((v2h+v1h+v0h+(vzh=z&0XFF00FF)+0X20002)&0X3FC03FC)+((v2l+v1l+v0l+(vzl=z&0XFF00)+0X200)&0x3FC00))>>2,v2h=v1h,v1h=v0h,v0h=vzh,v2l=v1l,v1l=v0l,v0l=vzl // 25:25:25:25
//#define VIDEO_FILTER_BLURDATA v0,v1
//#define VIDEO_FILTER_BLUR0(z) v0=v1=z
//#define VIDEO_FILTER_BLUR(r,z) r=(v0&0XFF0000)+(v1&0XFF00)+(z&0XFF),v0=v1,v1=z // chromatic aberration, 100:0
//#define VIDEO_FILTER_BLUR(r,z) r=((((v0&0XFF0000)*3+(z&0XFF0000)+0X20000)>>2)&0XFF0000)+((((v1&0XFF00)*3+(z&0XFF00)+0X200)>>2)&0XFF00)+(z&0XFF),v0=v1,v1=z // chromatic aberration, 75:25
#define VIDEO_FILTER_X1(x) (((x>>1)&0X7F7F7F)+0X2B2B2B) // average
//#define VIDEO_FILTER_X1(x) (((x>>2)&0X3F3F3F)+0X404040) // heavier
//#define VIDEO_FILTER_X1(x) (((x>>2)&0X3F3F3F)*3+0X161616) // lighter
//#define VIDEO_FILTER_X1(x) (((x&0XF8F8F8)>>3)*5+0X323232) // practical
//#define VIDEO_FILTER_X1(x) ((((((x&0XFF0000)*54)>>16)+(((x&0XFF00)*183)>>8)+(((x&0XFF)*19)))>>8)*0X10101) // monochrome (canonical)
//#define VIDEO_FILTER_X1(x) ((((((x&0XFF0000)*7)>>13)+(((x&0XFF00)*45)>>6)+(((x&0XFF)*20)))>>8)*0X10101) // monochrome (56:180:20)
//#define VIDEO_FILTER_SCAN(w,b) (((b>>1)&0X7F7F7F)+((w>>1)&0X7F7F7F)+0X010101) // 50:50
//#define VIDEO_FILTER_SCAN(w,b) (((b>>2)&0X3F3F3F)*3+((w>>2)&0X3F3F3F)+0X020202) // 25:75
#define VIDEO_FILTER_SCAN(w,b) (((b>>3)&0X1F1F1F)*7+((w>>3)&0X1F1F1F)+0X040404) // 13:87
//#define VIDEO_FILTER_LINE(x,y) (((((x&0XFF00FF)*3+(y&0XFF00FF)+0X20002)&0X3FC03FC)+(((x&0XFF00)*3+(y&0XFF00)+0X200)&0X3FC00))>>2) // 75:25
#define VIDEO_FILTER_LINE(x,y) (((((x&0XFF00FF)+(y&0XFF00FF)+0X10001)&0X1FE01FE)+(((x&0XFF00)+(y&0XFF00)+0X100)&0X1FE00))>>1) // 50:50

#define MAIN_FRAMESKIP_MASK ((1<<MAIN_FRAMESKIP_BITS)-1)
void video_resetscanline(void) // reset scanline filler values on new video options
{
	static int b=-1; if (b!=video_pageblend) // do we need to reset the blending buffer?
		if (b=video_pageblend)
			for (int y=0;y<VIDEO_PIXELS_Y/2;++y)
				MEMNCPY(&video_blend[y*VIDEO_PIXELS_X],&video_frame[(VIDEO_OFFSET_Y+y*2)*VIDEO_LENGTH_X+VIDEO_OFFSET_X],VIDEO_PIXELS_X);
}

#define VIDEO_NEXT *video_target++ // "VIDEO_NEXT = VIDEO_NEXT = ..." generates invalid code on VS13 and slower code on TCC
INLINE void video_newscanlines(int x,int y) // call before each new frame: reset `video_target`, `video_pos_x` and `video_pos_y`, and update `video_pos_z`
{
	#ifdef MAUS_LIGHTGUNS
	video_litegun=0; // the lightgun signal fades away between frames
	#endif
	++video_pos_z; video_target=video_frame+(video_pos_y=y)*VIDEO_LENGTH_X+(video_pos_x=x); // new coordinates
	if (video_interlaces&&(video_scanline&2))
		++video_pos_y,video_target+=VIDEO_LENGTH_X; // current scanline mode
}
// do not manually unroll the following operations, GCC is smart enough to do a better job on its own: 1820% > 1780%!
INLINE void video_drawscanline(void) // call after each drawn scanline; memory caching makes this more convenient than gathering all operations in video_endscanlines()
{
	VIDEO_UNIT vt,vs,VIDEO_FILTER_BLURDATA,*vi=video_target-video_pos_x+VIDEO_OFFSET_X,*vl=vi+VIDEO_PIXELS_X,*vo,*vp;
	#ifdef MAUS_LIGHTGUNS
	if (!(((session_maus_y+VIDEO_OFFSET_Y)^video_pos_y)&-2)) // does the lightgun aim at the current scanline?
		video_litegun=session_maus_x>=0&&session_maus_x<VIDEO_PIXELS_X?vi[session_maus_x]|vi[session_maus_x^1]:0; // keep the colours BEFORE any filtering happens!
	#endif
	if (video_lineblend) // blend current and previous scanline together!
	{
		static VIDEO_UNIT zzz[VIDEO_PIXELS_X];
		if (video_pos_y<VIDEO_OFFSET_Y+2)
			MEMLOAD(zzz,vi); // 1st line, backup only
		else
		{
			vo=zzz;
			do
				if ((vs=*vi)!=(vt=*vo)) *vo=vs,*vi=VIDEO_FILTER_LINE(vs,vt); // vertical blur
			while (++vo,++vi<vl);
			vi-=VIDEO_PIXELS_X;
		}
	}
	if (video_pageblend) // blend scanlines from previous and current frame!
	{
		vo=&video_blend[(video_pos_y-VIDEO_OFFSET_Y)/2*VIDEO_PIXELS_X];
		do
			if ((vs=*vi)!=(vt=*vo)) *vo=vs,*vi=VIDEO_FILTER_LINE(vs,vt); // non-accumulative gigascreen
		while (++vo,++vi<vl);
		vi-=VIDEO_PIXELS_X;
	}
	// the order is important: doing PAGEBLEND before LINEBLEND is slow :-(
	vo=video_target-video_pos_x+VIDEO_OFFSET_X+VIDEO_LENGTH_X;
	switch (video_filter+(video_scanlinez?0:8))
	{
		case 8: // nothing (full)
			MEMNCPY(vo,vi,VIDEO_PIXELS_X);
			// no `break` here!
		case 0: // nothing (half)
			break;
		case 0+VIDEO_FILTER_MASK_Y:
			if (video_pos_y&1)
				do
					vt=*vi,*vi++=VIDEO_FILTER_X1(vt);
				while (vi<vl);
			break;
		case 8+VIDEO_FILTER_MASK_Y:
			do
				vt=*vi++,*vo++=VIDEO_FILTER_X1(vt);
			while (vi<vl);
			break;
		case 8+VIDEO_FILTER_MASK_X:
			do
				*vo++=*vi++,
				vt=*vi,*vo++=*vi++=VIDEO_FILTER_X1(vt);
			while (vi<vl);
			break;
		case 0+VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_X:
			if (video_pos_y&1)
			{
				if (video_pos_y&2)
					++vi;
				do
					vt=*vi,*vi++=VIDEO_FILTER_X1(vt),++vi;
				while (vi<vl);
			}
			break;
		case 0+VIDEO_FILTER_MASK_X:
			do
				vt=*++vi,*vi++=VIDEO_FILTER_X1(vt);
			while (vi<vl);
			break;
		case 8+VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_X:
			if (video_pos_y&2)
				*vo++=*vi++;
			do
				vt=*vi++,*vo++=VIDEO_FILTER_X1(vt),*vo++=*vi++;
			while (vi<vl);
			break;
		case 8+VIDEO_FILTER_MASK_Z:
			vt=*vo++=*vi++,VIDEO_FILTER_BLUR0(vt);
			do
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vo++=*vi++=vt;
			while (vi<vl);
			break;
		case 0+VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_Z:
			if (video_pos_y&1)
			{
				vt=*vi,*vi++=VIDEO_FILTER_X1(vt),VIDEO_FILTER_BLUR0(vt);
				do
					vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=VIDEO_FILTER_X1(vt);
				while (vi<vl);
				break;
			}
			// no `break` here!
		case 0+VIDEO_FILTER_MASK_Z:
			vt=*vi++,VIDEO_FILTER_BLUR0(vt);
			do
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=vt;
			while (vi<vl);
			break;
		case 8+VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_Z:
			vt=*vi++,*vo++=VIDEO_FILTER_X1(vt),VIDEO_FILTER_BLUR0(vt);
			do
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=vt,*vo++=VIDEO_FILTER_X1(vt);
			while (vi<vl);
			break;
		case 8+VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Z:
			vt=*vi,VIDEO_FILTER_BLUR0(vt);
			do
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vo++=*vi++=vt,
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vo++=*vi++=VIDEO_FILTER_X1(vt);
			while (vi<vl);
			break;
		case 0+VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Z:
			vt=*vi,VIDEO_FILTER_BLUR0(vt);
			if (video_pos_y&1)
			{
				if (video_pos_y&2)
					vi++;
				do
					vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=VIDEO_FILTER_X1(vt),
					vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=vt;
				while (vi<vl);
				break;
			}
			do
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=vt;
			while (vi<vl);
			break;
		case 0+VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Z:
			vt=*vi,VIDEO_FILTER_BLUR0(vt);
			do
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=vt,
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=VIDEO_FILTER_X1(vt);
			while (vi<vl);
			break;
		case 8+VIDEO_FILTER_MASK_Y+VIDEO_FILTER_MASK_X+VIDEO_FILTER_MASK_Z:
			vt=*vi,VIDEO_FILTER_BLUR0(vt);
			if (video_pos_y&2)
				*vo++=*vi++;
			do
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vi++=vt,*vo++=VIDEO_FILTER_X1(vt),
				vs=*vi,VIDEO_FILTER_BLUR(vt,vs),*vo++=*vi++=vt;
			while (vi<vl);
			break;
	}
	if (video_scanline>=4&&video_pos_y>VIDEO_OFFSET_Y+1) // fill all scanlines but one with 50:50 neighbours
	{
		vo=(vp=(vi=vl-VIDEO_PIXELS_X)-VIDEO_LENGTH_X)-VIDEO_LENGTH_X;
		if (video_filter&VIDEO_FILTER_MASK_Y) // we skipped the Y-mask above, so we must perform it here
			if (video_filter&VIDEO_FILTER_MASK_X) // if both masks are enabled, we must replicate the pattern
				if (video_pos_y&2)
					do
						vt=VIDEO_FILTER_HALF(*vi,*vo),*vp++=VIDEO_FILTER_X1(vt),++vi,++vo,
						*vp++=VIDEO_FILTER_HALF(*vi,*vo),++vi,++vo; // yes no yes no ...
					while (vi<vl);
				else
					do
						*vp++=VIDEO_FILTER_HALF(*vi,*vo),++vi,++vo, // no yes no yes ...
						vt=VIDEO_FILTER_HALF(*vi,*vo),*vp++=VIDEO_FILTER_X1(vt),++vi,++vo;
					while (vi<vl);
			else // no X-mask, just perform the Y-mask
				do
					vt=VIDEO_FILTER_HALF(*vi,*vo),*vp++=VIDEO_FILTER_X1(vt),++vi,++vo;
				while (vi<vl);
		else // no masks, just blur the scanlines
			do
				*vp++=VIDEO_FILTER_HALF(*vi,*vo),++vi,++vo;
			while (vi<vl);
	}
}
INLINE void video_nextscanline(int x) // call before each new scanline: move on to next scanline, where `x` is the new horizontal position
	{ frame_pos_y+=2,video_pos_y+=2,video_target+=VIDEO_LENGTH_X*2-video_pos_x; video_target+=video_pos_x=x; session_signal|=session_signal_scanlines; }
INLINE void video_endscanlines(void) // call after each drawn frame: end the current frame and clean up
{
	static int f=-128; static VIDEO_UNIT z=~0; // first value intentionally invalid!
	if (video_scanlinez!=video_scanline||f!=video_filter||z!=video_halfscanline) // did the config change?
		if ((video_scanlinez=video_scanline)==1) // do we have to redo the secondary scanlines?
		{
			z=video_halfscanline; f=video_filter;
			VIDEO_UNIT v0=f&VIDEO_FILTER_MASK_Y?VIDEO_FILTER_X1(z):z,
				v1=f&VIDEO_FILTER_MASK_X?z:v0;
			VIDEO_UNIT *p=video_frame+VIDEO_OFFSET_X+(VIDEO_OFFSET_Y+1)*VIDEO_LENGTH_X;
			for (int y=0;y<VIDEO_PIXELS_Y/2;++y,p+=VIDEO_LENGTH_X*2-VIDEO_PIXELS_X)
				for (int x=0;x<VIDEO_PIXELS_X/2;++x)
					*p++=v0,*p++=v1; // render secondary scanlines only
		}
	if (video_scanline>=4) // fill last scanline with 100:0 neighbours (no next line after last!)
	{
		VIDEO_UNIT *vi=video_frame+VIDEO_OFFSET_X+(VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-2)*VIDEO_LENGTH_X,
			*vo=video_frame+VIDEO_OFFSET_X+(VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-1)*VIDEO_LENGTH_X;
		if (video_filter&VIDEO_FILTER_MASK_X) // if X-mask is enabled, we must replicate the pattern
			for (int i=0;i<VIDEO_PIXELS_X/2;++i)
				*vo++=*vi++,*vo++=VIDEO_FILTER_X1(*vi),++vi;
		else if (video_filter&VIDEO_FILTER_MASK_Y) // no X-mask, just perform the Y-mask
			for (int i=0;i<VIDEO_PIXELS_X;++i)
				*vo++=VIDEO_FILTER_X1(*vi),++vi;
		else // no masks, just copy the scanline
			MEMNCPY(vo,vi,VIDEO_PIXELS_X);
	}
}

INLINE void audio_playframe(int q,AUDIO_UNIT *ao) // call between frames by the OS wrapper
{
	AUDIO_UNIT aa,*ai=audio_frame;
	#if AUDIO_CHANNELS > 1
	static AUDIO_UNIT a0=AUDIO_ZERO,a1=AUDIO_ZERO; // independent channels
	switch (q)
	{
		case 1:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=a0=(aa+a0+(aa>a0))>>1,
				aa=*ai++,*ao++=a1=(aa+a1+(aa>a1))>>1;
			break;
		case 2:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=a0=(aa+(a0+(aa>a0))*3)>>2,
				aa=*ai++,*ao++=a1=(aa+(a1+(aa>a1))*3)>>2;
			break;
		case 3:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=a0=(aa+(a0+(aa>a0))*7)>>3,
				aa=*ai++,*ao++=a1=(aa+(a1+(aa>a1))*7)>>3;
			break;
	}
	#else
	static AUDIO_UNIT az=AUDIO_ZERO; // single channel
	switch (q)
	{
		case 1:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=az=(aa+az+(aa>az))>>1;
			break;
		case 2:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=az=(aa+(az+(aa>az))*3)>>2;
			break;
		case 3:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=az=(aa+(az+(aa>az))*7)>>3;
			break;
	}
	#endif
}

INLINE void session_update(void) // render video+audio thru OS and handle realtime logic (self-adjusting delays, automatic frameskip, etc.)
{
	session_signal&=~SESSION_SIGNAL_FRAME; // new frame!
	session_render();
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

unsigned char *puff_src,*puff_tgt; // source and target buffers
int puff_srcl,puff_tgtl,puff_srco,puff_tgto,puff_buff,puff_bits;
struct puff_huff { short *cnt,*sym; }; // Huffman table element
// Huffman-bitwise operations
int puff_read(int n) // reads N bits from source; <0 ERROR
{
	int buff=puff_buff,bits=puff_bits; // local copies are faster
	while (bits<n)
	{
		if (puff_srco>=puff_srcl) return -1; // source overrun!
		buff+=puff_src[puff_srco++]<<bits; // copy byte from source
		bits+=8; // skip byte from source
	}
	puff_buff=buff>>n; puff_bits=bits-n; // adjust target and flush bits
	return buff&((1<<n)-1); // result!
}
int puff_decode(struct puff_huff *h) // decodes Huffman code from source; <0 ERROR
{
	int buff=puff_buff,bits=puff_bits; // local copies are faster
	short *next=h->cnt; int l=1,i=0,base=0,code=0;
	for (;;) // extract bits until they fit within an interval
	{
		while (bits--)
		{
			code+=buff&1; buff>>=1;
			int o=*++next; if (code<base+o) // does the code fit the interval?
			{
				puff_buff=buff; // adjust target
				puff_bits=(puff_bits-l)&7; // flush bits
				return h->sym[i+(code-base)]; // result!
			}
			i+=o,base=(base+o)<<1,code<<=1,++l; // calculate next interval
		}
		if (!(bits=(15+1)-l)) return -1; // invalid value!
		if (puff_srco>=puff_srcl) return -1; // source overrun!
		buff=puff_src[puff_srco++]; // copy byte from source
		if (bits>8) bits=8; // flush source bits
	}
}
int puff_tables(struct puff_huff *h,short *l,int n) // generates Huffman input tables from canonical length table; !0 ERROR
{
	short o[15+1]; int i,a;
	for (i=0;i<=15;++i) h->cnt[i]=0; // reset all bit counts
	for (i=0;i<n;++i) ++(h->cnt[l[i]]); // increase relevant bit counts
	if (h->cnt[0]==n) return 0; // already done!
	for (a=i=1;i<=15;++i) if ((a=(a<<1)-(h->cnt[i]))<0) return a; // invalid value!
	for (o[i=1]=0;i<15;++i) o[i+1]=o[i]+h->cnt[i]; // reset all bit offsets
	for (i=0;i<n;++i) if (l[i]) h->sym[o[l[i]]++]=i; // define symbols from bit offsets
	return a; // 0 if complete, >0 otherwise!
}
const short puff_lcode0[2][32]={ // length constants; 0, 30 and 31 are reserved
	{ 0,3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 },
	{ 0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 }};
const short puff_ocode0[2][32]={ // offset constants; 30 and 31 are reserved
	{ 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 },
	{ 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 }};
int puff_expand(struct puff_huff *puff_lcode,struct puff_huff *puff_ocode) // decodes source into target; !0 ERROR
{
	for (int a,l,o;;)
	{
		if ((a=puff_decode(puff_lcode))<0) return a; // invalid value!
		if (a<256) // literal?
		{
			if (puff_tgto>=puff_tgtl) return -1; // target overrun!
			puff_tgt[puff_tgto++]=a; // copy literal
		}
		else if (a-=256) // length:offset pair?
		{
			if (a>29) return -1; // invalid value!
			if (puff_tgto+(l=puff_lcode0[0][a]+puff_read(puff_lcode0[1][a]))>puff_tgtl) return -1; // target overrun!
			if ((a=puff_decode(puff_ocode))<0) return a; // invalid value!
			if ((o=puff_tgto-puff_ocode0[0][a]-puff_read(puff_ocode0[1][a]))<0) return -1; // source underrun!
			do puff_tgt[puff_tgto++]=puff_tgt[o++]; while (--l);
		}
		else return 0; // end of block
	}
}
// block type handling
INLINE int puff_stored(void) // copies raw uncompressed byte block from source to target; !0 ERROR
{
	if (puff_srco+4>puff_srcl) return -1; // source overrun!
	int l=puff_src[puff_srco++]; l+=puff_src[puff_srco++]<<8;
	int k=puff_src[puff_srco++]; k+=puff_src[puff_srco++]<<8;
	if (!l||l+k!=0xFFFF) return -1; // invalid value!
	if (puff_srco+l>puff_srcl||puff_tgto+l>puff_tgtl) return -1; // source/target overrun!
	do puff_tgt[puff_tgto++]=puff_src[puff_srco++]; while (--l); // copy source to target and update pointers
	return puff_buff=puff_bits=0; // ignore remaining bits
}
short puff_lencnt[15+1],puff_lensym[288]; // 286 and 287 are reserved
short puff_offcnt[15+1],puff_offsym[32]; // 30 and 31 are reserved
struct puff_huff puff_lcode={ puff_lencnt,puff_lensym };
struct puff_huff puff_ocode={ puff_offcnt,puff_offsym };
short puff_tablez[288+32]; // 0..287 lengths, 288..319 offsets
void puff_table0(void) // generates static Huffman bit counts
{
	int i=0; // lengths: 8 x144, 9 x112, 7 x24, 8 x8; offsets: 5 x32
	for (;i<144;++i) puff_tablez[i]=8; // 000..143 : 00110000...10111111.
	for (;i<256;++i) puff_tablez[i]=9; // 144..255 : 110010000..111111111
	for (;i<280;++i) puff_tablez[i]=7; // 256..279 : 0000000....0010111..
	for (;i<288;++i) puff_tablez[i]=8; // 280..287 : 11000000...11000111.
	for (;i<288+32;++i) puff_tablez[i]=5;
}
INLINE int puff_static(void) // generates default Huffman codes and expands block from source to target; !0 ERROR
{
	puff_table0();
	puff_tables(&puff_lcode,puff_tablez,288); // THERE MUST BE EXACTLY 288 LENGTH CODES!!
	puff_tables(&puff_ocode,puff_tablez+288,32); // CAN THERE BE AS FEW AS 30 OFFSET CODES??
	return puff_expand(&puff_lcode,&puff_ocode);
}
INLINE int puff_dynamic(void) // generates custom Huffman codes and expands block from source to target; !0 ERROR
{
	short t[288+32]; int l,o,h,i=0;
	if ((l=puff_read(5)+257)<257||l>286||(o=puff_read(5)+1)<1||o>30||(h=puff_read(4)+4)<4)
		return -1; // invalid value!
	static const short hh[19]={ 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 }; // Huffman constants
	while (i<h) t[hh[i++]]=puff_read(3); // read bit counts for the Huffman-encoded header
	while (i<19) t[hh[i++]]=0; // padding: clear the remainder of this header
	if (puff_tables(&puff_lcode,t,19)) return -1; // invalid table!
	for (i=0;i<l+o;)
	{
		int a; if ((a=puff_decode(&puff_lcode))<16) // literal?
			t[i++]=a; // copy literal
		else
		{
			int z; if (a==16)
			{
				if (i<1) return -1; // invalid value!
				z=t[i-1],a=3+puff_read(2); // copy last value 3..6 times
			}
			else if (a==17)
				z=0,a=3+puff_read(3); // store zero 3..10 times
			else // (a==18)
				z=0,a=11+puff_read(7); // store zero 11..138 times
			if (i+a>l+o) return -1; // overflow!
			while (a--) t[i++]=z; // copy or store value
		}
	}
	if (puff_tables(&puff_lcode,t,l)&&(l-puff_lcode.cnt[0]!=1)) return -1; // invalid length tables!
	if (puff_tables(&puff_ocode,t+l,o)&&(o-puff_ocode.cnt[0]!=1)) return -1; // invalid offset tables!
	return puff_expand(&puff_lcode,&puff_ocode);
}
int puff_main(void) // inflates deflated source into target; !0 ERROR
{
	int q,e; puff_buff=puff_bits=0; // puff_srcX and puff_tgtX are set by caller
	do
		switch (q=puff_read(1),puff_read(2)) // which type of block it is?
		{
			case 0: e=puff_stored(); break; // uncompressed raw bytes
			case 1: e=puff_static(); break; // default Huffman coding
			case 2: e=puff_dynamic(); break; // custom Huffman coding
			default: e=1; // invalid value, either -1 or 3!
		}
	while (!(q|e)); // is this the last block? did something go wrong?
	return e;
}
#ifdef INFLATE_RFC1950
// RFC1950 standard data decoder
DWORD puff_adler32(BYTE *t,int o) // calculates Adler32 checksum of a block `t` of size `o`
	{ int j=1,k=0; do if ((k+=(j+=*t++))>=65521) j%=65521,k%=65521; while (--o); return (k<<16)+j; }
int puff_zlib(BYTE *t,int o,BYTE *s,int i) // decodes a RFC1950 standard ZLIB object; !0 ERROR
{
	if (!s||!t||o<=0||i<=6) return -1; // NULL or empty source or target!
	if (s[0]!=0X78||s[1]&32||(s[0]*256+s[1])%31) return -1; // wrong ZLIB prefix!
	puff_tgt=t; puff_tgtl=o; puff_tgto=0; puff_src=s; puff_srcl=i-4; puff_srco=2;
	return puff_main()||puff_adler32(t,o)!=mgetmmmm(&s[i-4]);
}
#endif

// the DEFLATE method! ... or (once again) a quick-and-dirty implementation
// based on the RFC1951 standard and reusing bits from INFLATE functions.

#if defined(DEFLATE_RFC1950)&&!defined(DEFLATE_LEVEL)
#define DEFLATE_LEVEL 0 // catch when ZLIB encoding is required but DEFLATE_LEVEL is missing
#endif

#ifdef DEFLATE_LEVEL
int huff_stored(BYTE *t,int o,BYTE *s,int i) // the storage part of DEFLATE
{
	if (o<=i+(i>>12)) return -1; // not enough room!
	BYTE *r=t; while (i>32768) // cuts source into 32K blocks
	{
		*t++=0; *t++=0; *t++=128; *t++=255; *t++=127; // non-final block
		memcpy(t,s,32768); t+=32768; s+=32768; i-=32768;
	}
	*t++=1; *t++=i; *t++=i>>8; *t++=~i; *t++=~i>>8; // final block
	if (i) memcpy(t,s,i); return puff_tgtl=(t-r)+i;
}
#if DEFLATE_LEVEL > 0 // if you must save DEFLATE data and perform compression on it
#if DEFLATE_LEVEL >= 9
#define DEFLATE_SHL 32768 // strongest compression and slowest performance!!
#else
#define DEFLATE_SHL (64<<DEFLATE_LEVEL) // better than 32768*DEFLATE_LEVEL/9
#endif
// Huffman-bitwise operations
void huff_write(int n,int i) // sends `n`-bit word `i` to bitstream
{
	puff_buff|=i<<puff_bits; puff_bits+=n; while (puff_bits>=8)
		puff_tgt[puff_tgtl++]=puff_buff,puff_buff>>=8,puff_bits-=8;
}
struct puff_huff huff_lcode={ &puff_tablez[  0],puff_lensym }; // precalc'd length count+value tables, different to puff_lcode
struct puff_huff huff_ocode={ &puff_tablez[288],puff_offsym }; // precalc'd offset count+value tables, different to puff_ocode
void huff_tables(struct puff_huff *h,short *l,int n) // generates Huffman output tables from canonical length table
{
	for (int j=1,k=0;j<16;++j,k<<=1) for (int i=0;i<n;++i)
		if (l[i]==j) // do the bit counts match?
		{
			int t=0,s=k++; // select Huffman item
			for (int m=h->cnt[i]=j;m>0;--m) t=(t<<1)+(s&1),s>>=1;
			h->sym[i]=t; // Huffman bits must be stored upside down
		}
}
void huff_encode(struct puff_huff *h,int i) { huff_write(h->cnt[i],h->sym[i]); }
void huff_append(const short h[2][32],int i,int z) { huff_write(h[1][i],z-h[0][i]); }
// block type handling
INLINE void huff_static(BYTE *t,BYTE *s,int i) // the compression part of DEFLATE
{
	int p=0,hash2s=0; static int hash2[256*256]; // too big, must be "static" :-(
	#define HUFF_HASH2(x) hash2[s[x+1]*256+s[x]] // cfr. "hash2[*(WORD*)(&s[x])]"
	memset(hash2,-1,sizeof(hash2)); // reset the hash table to -1, right below zero
	huff_write(3,3); puff_table0(); // tag the single block as "final" and "static"
	huff_tables(&huff_lcode,puff_tablez,288); // 288 codes, but the last two are reserved
	huff_tables(&huff_ocode,puff_tablez+288,32); // ditto, 32 codes but only 30 are valid
	huff_encode(&huff_lcode,s[p++]); // first byte is always a literal
	while (p+2<i)
	{
		while (hash2s<p) HUFF_HASH2(hash2s)=hash2s,++hash2s; // update hash table
		int len=p+1,off=0,x=p-DEFLATE_SHL,y=p+258; if (x<0) x=0; if (y>i) y=i;
		for (int o=HUFF_HASH2(p);o>=x;--o) // search: the slowest part of the function
			if (s[o]==s[p]&&s[o+1]==s[p+1]&&s[o+2]==s[p+2]) // is this a match?
			{
				int cmp1=o+2,cmp2=p+2; while (++cmp2<y&&s[cmp2]==s[++cmp1]);
				if (len<cmp2) if (off=p-o,(len=cmp2)>=y) break; // maximum?
			}
		if ((len-=p)>2) // was there a match after all?
		{
			p+=len; // greedy algorithm always chooses the longest match
			for (x=len/   9;x<29;++x) if (len<puff_lcode0[0][x+1]) break; //    9=(  258/30)+1
			huff_encode(&huff_lcode,256+x); huff_append(puff_lcode0,x,len);
			for (y=off/1093;y<29;++y) if (off<puff_ocode0[0][y+1]) break; // 1093=(32768/30)+1
			huff_encode(&huff_ocode,    y); huff_append(puff_ocode0,y,off);
		}
		else
			huff_encode(&huff_lcode,s[p++]); // too short, store a literal
	}
	while (p<i) huff_encode(&huff_lcode,s[p++]); // last bytes (if any) are always literals
	huff_encode(&huff_lcode,256); // store end marker
}
int huff_main(BYTE *t,int o,BYTE *s,int i) // deflates inflated source into target; <0 ERROR, >=0 LENGTH
{
	puff_tgt=t; puff_tgtl=puff_buff=puff_bits=0; // beware, a bitstream isn't a bytestream!
	if (i>3&&o>i+(i>>3)) { huff_static(t,s,i),huff_write(7,0); if (puff_tgtl<i) return puff_tgtl; }
	return huff_stored(t,o,s,i); // the actual compression failed, fall back to storage
}
#else // if you must save DEFLATE data but don't want to perform any compression at all
#define huff_main(t,o,s,i) huff_stored(t,o,s,i)
#endif
#ifdef DEFLATE_RFC1950
// RFC1950 standard data encoder
int huff_zlib(BYTE *t,int o,BYTE *s,int i) // encodes a RFC1950 standard ZLIB object; <0 ERROR, >=0 LENGTH
{
	if (!t||!s||(o-=6)<=0||i<=0) return -1; // NULL or empty!
	BYTE *r=t; *t++=0X78; *t++=0XDA; // compatible ZLIB prefix
	if ((o=huff_main(t,o,s,i))<0) return -1; // out of memory!
	mputmmmm(t+=o,puff_adler32(s,i)); return t+4-r;
}
#endif
#endif

// standard ZIP v2.0 archive reader that relies on
// the main directory at the end of the ZIP archive;
// it includes a RFC1952 standard GZIP file handler.

FILE *puff_file=NULL;
unsigned char puff_name[256],puff_type,puff_gzip;
unsigned int puff_skip,puff_next,puff_diff,puff_hash;//,puff_time
void puff_close(void) // closes the current ZIP archive
{
	if (puff_file)
		fclose(puff_file),puff_file=NULL;
}
int puff_open(char *s) // opens a new ZIP archive; !0 ERROR
{
	puff_close();
	if (!(puff_file=fopen(s,"rb")))
		return -1;
	if (fread1(session_scratch,10,puff_file)==10&&!memcmp(session_scratch,"\037\213\010",3)) // GZIP header?
	{
		if (session_scratch[3]&4)
			fseek(puff_file,fgetii(puff_file),SEEK_SET); // skip infos
		if (session_scratch[3]&8)
			while (fgetc(puff_file)>0) ; // skip filename
		if (session_scratch[3]&16)
			while (fgetc(puff_file)>0) ; // skip comment
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
	int l=ftell(puff_file),k=65536;
	if (k>l)
		k=l;
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
int puff_head(void) // reads a ZIP file header, if any; !0 ERROR
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
	char *s=puff_name; // this will never be blank
	do
		if (*s=='/') *s=PATHCHAR;
	while (*++s);
	#endif
	return 0;
}
int puff_body(int q) // loads (!0) or skips (0) a ZIP file body; !0 ERROR
{
	if (!puff_file) return -1;
	if (puff_tgtl<1) q=0; // no data, can safely skip the source
	else if (q)
	{
		if (puff_srcl<1||puff_tgtl<0)
			return -1; // cannot get data from nothing! cannot write negative data!
		if (puff_gzip)
			return fseek(puff_file,puff_diff+puff_skip,SEEK_SET),
				fread1(puff_src,puff_srcl,puff_file),puff_tgto=puff_srco=0,puff_main();
		unsigned char h[30];
		fseek(puff_file,puff_diff+puff_skip,SEEK_SET);
		fread1(h,sizeof(h),puff_file);
		if (!memcmp(h,"PK\003\004",4)&&h[8]==puff_type) // OK?
		{
			fseek(puff_file,mgetii(&h[26])+mgetii(&h[28]),SEEK_CUR); // skip name+xtra
			if (!puff_type)
				return fread1(puff_tgt,puff_tgto=puff_srco=puff_srcl,puff_file),puff_tgtl!=puff_srcl;
			if (puff_type==8)
				return fread1(puff_src,puff_srcl,puff_file),puff_tgto=puff_srco=0,puff_main();
		}
	}
	return q; // 0 skipped=OK, !0 unknown=ERROR
}
// simple ANSI X3.66
unsigned int puff_dohash(unsigned int k,unsigned char *s,int l)
{
	static const unsigned int z[]= {0,0x1DB71064,0x3B6E20C8,0x26D930AC,0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,
		0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C}; // precalc'd!
	for (int i=0;i<l;++i) k^=s[i],k=(k>>4)^z[k&15],k=(k>>4)^z[k&15];
	return k;
}
// ZIP-aware fopen()
char puff_pattern[]="*.zip;*.gz",PUFF_STR[]={PATHCHAR,PATHCHAR,PATHCHAR,0},puff_path[STRMAX]; // i.e. "TREE/PATH///ARCHIVE/FILE"
FILE *puff_ffile=NULL; int puff_fshut=0; // used to know whether to keep a file that gets repeatedly open
FILE *puff_fopen(char *s,char *m) // mimics fopen(), so NULL on error, *FILE otherwise
{
	if (!s||!m)
		return NULL; // wrong parameters!
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
	puff_path[z-s]=0;
	z+=strlen(PUFF_STR);
	if (!multiglobbing(puff_pattern,puff_path,1)||puff_open(puff_path))
		return 0;
	while (!puff_head()) // scan archive; beware, the insides MUST BE case sensitive!
		//#ifdef _WIN32
		//if (!strcasecmp(puff_name,z)) // `strcasecmp` cannot be here, not even on _WIN32: this isn't a general purpose ZIP reader
		//#else
		if (!strcmp(puff_name,z))
		//#endif
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
				||puff_body(1)||(puff_srco^puff_srcl)||(puff_tgto^puff_tgtl)
				||(DWORD)~(puff_hash^puff_dohash(-1,puff_tgt,puff_tgtl)))
				fclose(puff_ffile),puff_ffile=NULL; // memory or data failure!
			if (puff_ffile)
				fwrite1(puff_tgt,puff_tgtl,puff_ffile),fseek(puff_ffile,0,SEEK_SET); // fopen() expects ftell()=0!
			if (puff_src) free(puff_src); if (puff_tgt) free(puff_tgt);
			puff_close();
			if (puff_ffile) // either *FILE or NULL
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
char *puff_session_subdialog(char *r,char *s,char *t,char *zz,int qq) // let the user pick a file within a ZIP archive ('r' ZIP path, 's' pattern, 't' title, 'zz' default file or NULL); NULL for cancel, 'r' (with full path) for OK
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
	unsigned char rr[STRMAX],ss[STRMAX],*z,*zz;
	sprintf(ss,"%s;%s",puff_pattern,s); if (!r) r=session_path; strcpy(rr,r); // NULL path = default!
	for (int qq=0;;) // try either a file list or the file dialog until the user either chooses a file or quits
	{
		if (zz=strrstr(rr,PUFF_STR)) // 'rr' holds the path, does it contain the separator already?
		{
			*zz=0; zz+=strlen(PUFF_STR); // *zz++=0; // now it either points to the previous file name or to a zero
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

VIDEO_UNIT onscreen_ink0,onscreen_ink1; BYTE onscreen_flag=1;
#define onscreen_inks(q0,q1) (onscreen_ink0=q0,onscreen_ink1=q1)
#define ONSCREEN_XY if ((x*=8)<0) x+=VIDEO_OFFSET_X+VIDEO_PIXELS_X; else x+=VIDEO_OFFSET_X; \
	if ((y*=ONSCREEN_SIZE)<0) y+=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y; else y+=VIDEO_OFFSET_Y; \
	VIDEO_UNIT *p=&video_frame[y*VIDEO_LENGTH_X+x],a=video_scanline==1?2:1
void onscreen_bool(int x,int y,int lx,int ly,int q) // draw a rectangle at `x,y` of size `lx,ly` and boolean value `q`
{
	ONSCREEN_XY; q=q?onscreen_ink1:onscreen_ink0;
	lx*=8; ly*=ONSCREEN_SIZE;
	for (y=0;y<ly;p+=VIDEO_LENGTH_X*a-lx,y+=a)
		for (x=0;x<lx;++x)
			*p++=q;
}
void onscreen_char(int x,int y,int z) // draw a 7-bit character; the eighth bit is the INVERSE flag
{
	ONSCREEN_XY; int q=z&128?-1:0;
	const unsigned char *zz=&onscreen_chrs[(z&127)*ONSCREEN_SIZE];
	for (y=0;y<ONSCREEN_SIZE;++y)
	{
		int bb=*zz++; bb|=bb>>1; // normal, rather than thin or bold
		bb^=q; // apply inverse if required
		for (x=0;x<2;p+=VIDEO_LENGTH_X*a-8,x+=a)
			for (z=128;z;z>>=1)
				*p++=(z&bb)?onscreen_ink1:onscreen_ink0;
	}
}
void onscreen_text(int x,int y,char *s,int q) // write a string
{
	if (q) q=128; int z; while (z=*s++)
	{
		if (z&96) // first 32 characters are invisible
			onscreen_char(x,y,z+q);
		++x;
	}
}
void onscreen_byte(int x,int y,int a,int q) // write two digits
{
	q=q?128+'0':'0';
	onscreen_char(x,y,(a/10)+q);
	onscreen_char(x+1,y,(a%10)+q);
}

// built-in general-purpose debugger -------------------------------- //

void session_backupvideo(VIDEO_UNIT *t) // make a clipped copy of the current screen; used by the debugger and the SDL2 UI
{
	for (int y=0;y<VIDEO_PIXELS_Y;++y)
		MEMNCPY(&t[y*VIDEO_PIXELS_X],&video_frame[(VIDEO_OFFSET_Y+y)*VIDEO_LENGTH_X+VIDEO_OFFSET_X],VIDEO_PIXELS_X);
}

#define KBDBG_UP	11
#define KBDBG_DOWN	10
#define KBDBG_LEFT	14
#define KBDBG_RIGHT	15
#define KBDBG_HOME	16
#define KBDBG_END	17
#define KBDBG_NEXT	18
#define KBDBG_PRIOR	19
#define KBDBG_TAB	9
#define KBDBG_TAB_S	8
#define KBDBG_RET	13
#define KBDBG_RET_S	12
#define KBDBG_SPC	32
#define KBDBG_SPC_S	160 // inspired by the ANSI non-breaking space
#define KBDBG_ESCAPE	27
#define KBDBG_CLOSE	128
#define KBDBG_CLICK	129
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

BYTE debug_point[1<<16]; // the breakpoint table; 0 = nothing, <8 = stop execution, >=8 = record a byte
BYTE debug_break,debug_inter=0; // do special opcodes trigger the debugger? (f.e. $EDFF on Z80 and $12 on 6502/6510)
BYTE debug_config; // store the general debugger options here; the CPU-specific ones go elsewhere
DWORD main_t=0; // the global tick counter, used by the debugger, and optionally by the emulator

FILE *debug_logfile; int debug_logsize; BYTE debug_logtemp[1<<9]; // the byte recorder datas

#define debug_setup() (MEMZERO(debug_point),debug_logfile=NULL,debug_dirty=session_signal&SESSION_SIGNAL_DEBUG)
#define debug_configread(i) (debug_config=(i)/4,debug_break=!!((i)&2),debug_mode=(i)&1)
#define debug_configwrite() (debug_config*4+(debug_break?2:0)+(debug_mode?1:0))

int debug_logbyte(BYTE z) // log a byte, if possible; !0 ERROR, 0 OK
{
	if (!debug_logfile) // try creating a log file?
	{
		char *s; if (s=session_newfile(NULL,"*","Register log file"))
			debug_logfile=fopen(s,"wb"),debug_logsize=0;
	}
	if (!debug_logfile) // do we have a valid file?
		return 1; // ERROR!
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
void debug_clear(void); // clean temporary breakpoints
void debug_reset(void); // do `debug_dirty=1` and set the disassembly and stack coordinates
char *debug_list(void); // return a string of letters of registers that can be logged
BYTE debug_peek(BYTE,WORD); // receive a byte from address WORD and R/W flag BYTE
void debug_poke(WORD,BYTE); // send the byte BYTE to address WORD
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
int grafx_size(int); // how many pixels wide is one byte?
WORD grafx_show(VIDEO_UNIT*,int,int,BYTE,WORD,int); // blit a set of pixels
void debug_info(int); // write a page of hardware information between the disassembly and the register table
void grafx_info(VIDEO_UNIT*,int,int); // draw additional infos on the top right corner

// these functions provide services for the CPU and hardware debugging code
#define DEBUG_INFOZ(n) (DEBUG_LOCATE(DEBUG_LENGTH_X-DEBUG_INFOX-10,(n))) // shortcut for debug_info to print each text line
void debug_hilight(char *t,int n) { while (n-->0) *t++|=128; } // set inverse video on a N-character string
void debug_hilight2(char *t) { *t|=128,t[1]|=128; } // set inverse video on exactly two characters

BYTE debug_panel=0,debug_page=0,debug_grafx,debug_mode=0,debug_match,debug_grafx_l=1; // debugger options: visual style, R/W mode, disasm flags...
WORD debug_panel0_w,debug_panel1_w=1,debug_panel2_w=0,debug_panel3_w,debug_grafx_w=0; // must be unsigned!
char debug_panel0_x,debug_panel1_x=0,debug_panel2_x=0,debug_panel3_x,debug_grafx_v=0; // must be signed!

WORD debug_longdasm(char *t,WORD p) // disassemble code and include its hexadecimal dump
{
	WORD q=p; memset(t,' ',9); p=debug_dasm(t+9,p);
	for (BYTE n=4,b;q!=p&&n>0;++q,--n)
		b=debug_peek(0,q),*t++=hexa1[b>>4],*t++=hexa1[b&15];
	if (q!=p) t[-2]='\177',t[-1]='.'; // too long? truncate! :(
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
			case 0: for (int i=0,j=0;i<sizeof(debug_chrs);++i) j=onscreen_chrs[i],debug_chrs[i]=j|(j>>1); break; // NORMAL
			case 1: MEMLOAD(debug_chrs,onscreen_chrs); break; // THIN
			case 2: for (int i=0,j=0;i<sizeof(debug_chrs);++i) j=onscreen_chrs[i],debug_chrs[i]=j|(((i/(ONSCREEN_SIZE/2))&1)?j<<1:j>>1); break; // ITALIC
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
				for (int xx=128,ww=w^*zz++;xx;xx>>=1,++p)
					if (xx&ww)
						*p=p1/*,p[1]=p0,p[VIDEO_PIXELS_X]=p0*/,p[VIDEO_PIXELS_X+1]=p0;
					else
						if (*p!=p0) *p=VIDEO_FILTER_LINE(*p,p0);
			}
			else
				for (int xx=128,ww=w^*zz++;xx;xx>>=1,++p)
					*p=(xx&ww)?p1:p0;
	}
}

void session_debug_show(int redo) // shows the current debugger
{
	static int videox,videoy,videoz; int i; WORD p; BYTE b; char *t;
	memset(debug_buffer,' ',sizeof(debug_buffer)); // clear buffer
	if (redo||((videox^video_pos_x)|(videoy^video_pos_y)|(videoz^video_pos_z)))
		debug_clear(),debug_reset(); // reset debugger and background!
	videoz=video_pos_z; session_backupvideo(debug_frame); // translucent debug text needs this :-/
	if ((videoy=video_pos_y)>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-1)
		for (int x=0,z=((video_pos_y&-2)-VIDEO_OFFSET_Y)*VIDEO_PIXELS_X;x<VIDEO_PIXELS_X;++x,++z)
			debug_frame[z]=debug_frame[z+VIDEO_PIXELS_X]^=x&16?0x00FFFF:0xFF0000;
	if ((videox=video_pos_x)>=VIDEO_OFFSET_X&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X-1)
		for (int y=0,z=(video_pos_x&-2)-VIDEO_OFFSET_X;y<VIDEO_PIXELS_Y;++y,z+=VIDEO_PIXELS_X)
			debug_frame[z]=debug_frame[z+1]^=y&2?0x00FFFF:0xFF0000;
	if (debug_grafx)
	{
		int z=0; p=debug_grafx_w; if (debug_grafx_v&1) // VERTICAL FIRST, HORIZONTAL LAST?
		{
			for (;z+debug_grafx_l<=DEBUG_LENGTH_Y*ONSCREEN_SIZE;z+=debug_grafx_l)
				for (int x=0;x<DEBUG_LENGTH_X*8;x+=grafx_size(1))
					p=grafx_show(&debug_frame[(debug_posy()+z)*VIDEO_PIXELS_X+debug_posx()+x],VIDEO_PIXELS_X,debug_grafx_l,debug_mode,p,debug_grafx_v>>1);
			for (;z<DEBUG_LENGTH_Y*ONSCREEN_SIZE;++z)
			{
				i=(debug_posy()+z)*VIDEO_PIXELS_X+debug_posx();
				for (int x=0;x<DEBUG_LENGTH_X*8;++x)
					debug_frame[i+x]=0X808080;
			}
		}
		else // HORIZONTAL FIRST, VERTICAL LAST!
		{
			for (;z+grafx_size(debug_grafx_l)<=DEBUG_LENGTH_X*8;z+=grafx_size(debug_grafx_l))
				for (int y=0;y<DEBUG_LENGTH_Y*ONSCREEN_SIZE;++y)
					p=grafx_show(&debug_frame[(debug_posy()+y)*VIDEO_PIXELS_X+debug_posx()+z],grafx_size(1),debug_grafx_l,debug_mode,p,debug_grafx_v>>1);
			for (int y=0;y<DEBUG_LENGTH_Y*ONSCREEN_SIZE;++y)
			{
				i=(debug_posy()+y)*VIDEO_PIXELS_X+debug_posx();
				for (int x=z;x<DEBUG_LENGTH_X*8;++x)
					debug_frame[i+x]=0X808080;
			}
		}
		grafx_info(&debug_frame[debug_posy()*VIDEO_PIXELS_X+debug_posx()+DEBUG_LENGTH_X*8],VIDEO_PIXELS_X,debug_grafx_v>>1);
		sprintf(debug_buffer,"$%04X..$%04X",debug_grafx_w,p-1);
		session_debug_print(debug_buffer,DEBUG_LENGTH_X-12,DEBUG_LENGTH_Y-2,12);
		sprintf(debug_buffer,"%03d%c  W:exit",debug_grafx_l,(debug_grafx_v&1)?'Y':'X');
		session_debug_print(debug_buffer,DEBUG_LENGTH_X-12,DEBUG_LENGTH_Y-1,12);
	}
	else
	{
		// the order of the printing hinges on sprintf() appending a ZERO to each string :-/
		// top right panel: the CPU registers
		for (i=0;i<DEBUG_LENGTH_Y/2;++i)
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
			sprintf(DEBUG_LOCATE(0,i),"%04X:%c",p,(debug_point[p]&63)?(debug_point[p]&8)?debug_list()[debug_point[p]&7]:'@':' ');
			p=debug_longdasm(DEBUG_LOCATE(6,i),p);
			if (debug_match) // hilight current opcode
				debug_hilight(DEBUG_LOCATE(0,i),4);
		}
		// bottom left panel: the memory editor
		for (i=DEBUG_LENGTH_Y/2,p=(debug_panel2_w&-16)-(DEBUG_LENGTH_Y/2-DEBUG_LENGTH_Y/4)*16;i<DEBUG_LENGTH_Y;++i)
		{
			sprintf(DEBUG_LOCATE(0,i),"%04X:",p);
			char *tt=DEBUG_LOCATE(5+32,i); *tt++=debug_mode?'\\':'/';
			t=DEBUG_LOCATE(5,i); for (int j=0;j<16;++j)
				b=debug_peek(debug_mode,p),
				*tt++=(b&96)?b:('.'+(b&128)),
				*t++=hexa1[b>>4],*t++=hexa1[b&15],++p;
		}
		// the status bar (BREAK, timer, X:Y, help) and the cursors
		sprintf(DEBUG_LOCATE(4,DEBUG_LENGTH_Y/2-1),"\177. BREAK%c  Timer: %010d  (%04d,%03d) -- H:help",debug_break?'*':'-',main_t,video_pos_x,video_pos_y);
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
		WORD p=debug_panel0_w-4;
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
void debug_panel_search(int backwards) // look for a Pascal string of bytes on memory; the memory dump search is case-insensitive
{
	WORD p=debug_panel?debug_panel2_w:debug_panel0_w,o=p; backwards=backwards?-1:1;
	do
	{
		WORD q=p+=backwards; int i=1,j=*debug_panel_pascal; if (debug_panel)
		{
			while (i<=j&&ucase(debug_panel_pascal[i])==ucase(debug_peek(debug_mode,q))) ++i,++q;
			if (i>j) { debug_panel2_x=0,debug_panel2_w=p; return; }
		}
		else
		{
			while (i<=j&&debug_panel_pascal[i]==debug_peek(0,q)) ++i,++q;
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
		session_signal&=~SESSION_SIGNAL_DEBUG;
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
			"Q\tRun until interrupt\n"
			"R\tRun to cursor\n"
			"S\tSearch for STRING ('$'+string: hexadecimal)\n"
			"T\tReset timer\n"
			"U\tRun until return\n"
			"V\tToggle appearance\n"
			"W\tToggle debug/graphics mode\n"
			"X\tShow extended hardware info\n"
			"Y\tFill LENGTH bytes with BYTE\n"
			"Z\tDelete breakpoints\n"
			".\tToggle breakpoint\n"
			",\tToggle BREAK opcode\n"
			"Space\tStep into (shift: skip scanline)\n"
			"Return\tStep over (shift: skip frame)\n"
			"Escape\tExit\n","Debugger help");
	else if (k=='M') // TOGGLE MEMORY DUMP R/W MODE
		debug_mode=!debug_mode;
	else if (k=='Q') // RUN UNTIL INTERRUPT
		debug_inter=1,session_signal&=~SESSION_SIGNAL_DEBUG;
	else if (k=='R') // RUN TO...
		debug_drop(debug_panel0_w);
	else if (k=='U') // RUN UNTIL RETURN
		debug_fall();
	else if (k=='V') // TOGGLE APPEARANCE
		debug_config+=session_shift?-1:1;
	else if (k=='W') // TOGGLE DEBUG/GRAPHICS MODE
		debug_grafx=!debug_grafx;
	else if (k==',') // TOGGLE `BRK` BREAKPOINT
		debug_break=!debug_break;
	else if (debug_grafx)
	{
		if (k==KBDBG_TAB)
			++debug_grafx_v;
		else if (k==KBDBG_TAB_S)
			--debug_grafx_v;
		else if (k==KBDBG_LEFT)
			--debug_grafx_w;
		else if (k==KBDBG_RIGHT)
			++debug_grafx_w;
		else if (k==KBDBG_UP)
			debug_grafx_w-=debug_grafx_l;
		else if (k==KBDBG_DOWN)
			debug_grafx_w+=debug_grafx_l;
		else if (k==KBDBG_HOME)
			{ if (debug_grafx_l>1) --debug_grafx_l; }
		else if (k==KBDBG_END)
			{ if (debug_grafx_l<DEBUG_LENGTH_X*8/grafx_size(1)) ++debug_grafx_l; }
		else if (k==KBDBG_PRIOR)
			debug_grafx_w-=debug_grafx_l*16;
		else if (k==KBDBG_NEXT)
			debug_grafx_w+=debug_grafx_l*16;
		else if (k=='G') // GO TO..
			{
				sprintf(session_parmtr,"%04X",debug_grafx_w);
				if (session_input("Go to")>0)
					debug_grafx_w=session_debug_eval(session_parmtr);
			}
		else
			k=0; // default!
	}
	else if (((k=ucase(k))>='0'&&k<='9')||(k>='A'&&k<='F')) // HEXADECIMAL NIBBLE
	{
		if ((k-='0')>=16) k-=7; // get actual nibble
		switch (debug_panel)
		{
			case 0: i=debug_peek(0,debug_panel0_w);
				debug_poke(debug_panel0_w,debug_panel0_x?(i&240)+k:(i&15)+(k<<4));
				if (++debug_panel0_x>1) ++debug_panel0_w,debug_panel0_x=0;
				break;
			case 1: debug_regz(k);
				break;
			case 2: i=debug_peek(debug_mode,debug_panel2_w);
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
				case 3: if (--debug_panel3_x<0) debug_panel3_x=3,debug_panel3_w-=2;
			}
			break;
		case KBDBG_RIGHT:
			switch (debug_panel)
			{
				case 0: if (++debug_panel0_x>1) debug_panel0_x=0,++debug_panel0_w; break;
				case 1: if (++debug_panel1_x>3) debug_panel1_x=3; break;
				case 2: if (++debug_panel2_x>1) debug_panel2_x=0,++debug_panel2_w; break;
				case 3: if (++debug_panel3_x>3) debug_panel3_x=0,debug_panel3_w+=2;
			}
			break;
		case KBDBG_UP:
			switch (debug_panel)
			{
				case 0: debug_panel0_rewind(1); break;
				case 1: if (debug_panel1_w>1) --debug_panel1_w; break;
				case 2: debug_panel2_w-=16; break;
				case 3: debug_panel3_w-=2;
			}
			break;
		case KBDBG_DOWN:
			switch (debug_panel)
			{
				case 0: debug_panel0_fastfw(1); break;
				case 1: if (debug_panel1_w<DEBUG_LENGTH_Y/2-1) ++debug_panel1_w; break;
				case 2: debug_panel2_w+=16; break;
				case 3: debug_panel3_w+=2;
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
		case 'G': // GO TO...
			if (debug_panel!=1)
			{
				sprintf(session_parmtr,"%04X",debug_panel>2?debug_panel3_w:debug_panel?debug_panel2_w:debug_panel0_w);
				if (session_input("Go to")>0)
				{
					i=session_debug_eval(session_parmtr);
					switch (debug_panel)
					{
						case 0: debug_panel0_x=0; debug_panel0_w=(WORD)i; break;
						case 2: debug_panel2_x=0; debug_panel2_w=(WORD)i; break;
						case 3: debug_panel3_x=0; debug_panel3_w=(WORD)i; break;
					}
				}
			}
			break;
		case 'I': // INPUT BYTES FROM FILE
			if (!(debug_panel&1))
			{
				char *s; FILE *f; WORD w=i=debug_panel?debug_panel2_w:debug_panel0_w;
				if (s=puff_session_getfile(NULL,"*","Input file"))
					if (f=puff_fopen(s,"rb"))
					{
						while (i=fread1(session_substr,256,f)) // better than fgetc()
							for (int j=0;j<i;++j)
								debug_poke(w++,session_substr[j]);
						puff_fclose(f);
					}
			}
			break;
		case 'J': // JUMP TO...
			debug_jump(debug_panel0_w); break;
		case 'K': // CLOSE LOG
			debug_close(); break;
		case 'L': // LOG REGISTER
			if (*session_parmtr=0,sprintf(session_parmtr+1,"Log register (%s)",debug_list()),session_input(session_parmtr+1)==1)
			{
				char *t=strchr(debug_list(),ucase(session_parmtr[0])); if (t)
					debug_point[debug_panel0_w]=8+(t-debug_list());
			}
			break;
		case 'N': // NEXT SEARCH
			if (!(debug_panel&1)) if (*debug_panel_pascal) debug_panel_search(session_shift); break;
		case 'O': // OUTPUT BYTES INTO FILE
			if (!(debug_panel&1))
			{
				char *s; FILE *f; WORD w=i=debug_panel?debug_panel2_w:debug_panel0_w;
				if (*session_parmtr=0,session_input("Output length")>=0)
					if (i=(WORD)session_debug_eval(session_parmtr))
						if (s=session_newfile(NULL,"*","Output file"))
							if (f=fopen(s,"wb"))
							{
								while (i)
								{
									int j; for (j=0;j<i&&j<256;++j)
										session_substr[j]=debug_peek(debug_panel&&debug_mode,w++);
									fwrite1(session_substr,j,f); i-=j;
								}
								fclose(f);
							}
			}
			break;
		case 'P': // PRINT DISASSEMBLY/HEXDUMP INTO FILE
			if (!(debug_panel&1))
				if (*session_parmtr=0,session_input(debug_panel?"Hex dump length":"Disassembly length")>=0)
				{
					char *s,*t; FILE *f; WORD w=debug_panel?debug_panel2_w:debug_panel0_w,u;
					if (i=(WORD)session_debug_eval(session_parmtr))
						if (s=session_newfile(NULL,"*.TXT",debug_panel?"Print hex dump":"Print disassembly"))
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
										t+=sprintf(t,"$%02X",debug_peek(debug_mode,w++));
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
		case 'S': // SEARCH FOR STRING
			if (!(debug_panel&1))
				if (strcpy(session_parmtr,debug_panel_string),session_input("Search string")>=0)
				{
					strcpy(debug_panel_string,session_parmtr);
					if (*debug_panel_string=='$') // HEXA STRING?
					{
						for (i=1;debug_panel_string[i*2-1]*debug_panel_string[i*2];++i)
							debug_panel_pascal[i]=(eval_hex(debug_panel_string[i*2-1])<<4)+eval_hex(debug_panel_string[i*2]);
						*debug_panel_pascal=i-1;
					}
					else // NORMAL STRING
						memcpy(debug_panel_pascal+1,debug_panel_string,*debug_panel_pascal=strlen(debug_panel_string));
					if (*debug_panel_pascal) debug_panel_search(0);
				}
			break;
		case 'T': // RESET CLOCK
			main_t=0; break;
		case 'X': // SHOW MORE HARDWARE INFO
			++debug_page; break;
		case 'Y': // FILL BYTES WITH BYTE
			if (!(debug_panel&1))
			{
				WORD w=i=debug_panel?debug_panel2_w:debug_panel0_w; BYTE b;
				if (*session_parmtr=0,session_input("Fill length")>=0)
					if (i=(WORD)session_debug_eval(session_parmtr))
						if (*session_parmtr=0,session_input("Filler byte")>=0)
						{
							b=session_debug_eval(session_parmtr);
							while (i--) debug_poke(w++,b);
						}
			}
			break;
		case 'Z': // DELETE BREAKPOINTS
			MEMZERO(debug_point); break;
		case '.': // TOGGLE BREAKPOINT
			if (debug_panel==0) { debug_point[debug_panel0_w]=!debug_point[debug_panel0_w]; break; }
		default: k=0;
	}
	return k&&(debug_dirty=1);
}

// multimedia file output ------------------------------------------- //

unsigned int session_savenext(char *z,unsigned int i) // scans for available filenames; `z` is "%s%u.ext" and `i` is the current index; returns 0 on error, or a new index
{
	FILE *f; while (i)
	{
		sprintf(session_parmtr,z,session_path,i);
		if (!(f=fopen(session_parmtr,"rb"))) // does the file exist?
			break; // no? excellent!
		fclose(f),++i; // try again
	}
	return i;
}

// multimedia: audio file output ------------------------------------ //

unsigned char waveheader[44]="RIFF\000\000\000\000WAVEfmt \020\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000data"; // zero-padded
unsigned int session_nextwave=1,session_wavesize;
int session_createwave(void) // create a wave file; !0 ERROR
{
	if (session_wavefile||!session_audio)
		return 1; // file already open, or no audio to save!
	if (!(session_nextwave=session_savenext("%s%08u.wav",session_nextwave)))
		return 1; // too many files!
	if (!(session_wavefile=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	waveheader[0x16]=AUDIO_CHANNELS; // channels
	mputiiii(&waveheader[0x18],AUDIO_PLAYBACK); // samples per second
	mputiiii(&waveheader[0x1C],AUDIO_PLAYBACK*AUDIO_BITDEPTH/8*AUDIO_CHANNELS); // bytes per second
	waveheader[0x20]=AUDIO_BITDEPTH/8*AUDIO_CHANNELS; // bytes per sample
	waveheader[0x22]=AUDIO_BITDEPTH;
	fwrite(waveheader,1,sizeof(waveheader),session_wavefile);
	return session_wavesize=0;
}
#if SDL_BYTEORDER == SDL_BIG_ENDIAN && AUDIO_BITDEPTH > 8
void session_writewave(AUDIO_UNIT *t) // swap HI-LO bytes, WAVE files follow the INTEL lil endian style
{
	AUDIO_UNIT s[sizeof(audio_buffer)];
	for (int i=0;i<sizeof(s);++i) s[i]=((t[i]>>8)&255)+(t[i]<<8);
	session_wavesize+=fwrite(s,1,sizeof(s),session_wavefile);
}
#else
void session_writewave(AUDIO_UNIT *t) { session_wavesize+=fwrite(t,1,sizeof(audio_buffer),session_wavefile); }
#endif
int session_closewave(void) // close a wave file; !0 ERROR
{
	if (session_wavefile)
	{
		#if (AUDIO_CHANNELS*AUDIO_BITDEPTH<=8)&&(AUDIO_LENGTH_Z&1)
		if (session_wavesize&1) fputc(0,session_wavefile); // RIFF even-padding
		#endif
		fseek(session_wavefile,0x28,SEEK_SET);
		fputiiii(session_wavesize,session_wavefile);
		fseek(session_wavefile,0x04,SEEK_SET);
		fputiiii(session_wavesize+36,session_wavefile); // file=head+data
		return fclose(session_wavefile),session_wavefile=NULL,0;
	}
	return 1;
}

// multimedia: extremely primitive video+audio output! -------------- //
// warning: the following code assumes that VIDEO_UNIT is DWORD 0X00RRGGBB!

unsigned int session_nextfilm=1,session_filmfreq,session_filmcount;
BYTE session_filmflag,session_filmscale=1,session_filmtimer=1,session_filmalign; // format options
#define SESSION_FILMVIDEO_LENGTH (VIDEO_PIXELS_X*VIDEO_PIXELS_Y) // copy of the previous video frame
#define SESSION_FILMAUDIO_LENGTH (AUDIO_LENGTH_Z*2*AUDIO_CHANNELS) // copies of TWO audio frames
VIDEO_UNIT *session_filmvideo=NULL; AUDIO_UNIT session_filmaudio[SESSION_FILMAUDIO_LENGTH];
BYTE *xrf_chunk=NULL; // this buffer contains one video frame and two audio frames AFTER encoding

int xrf_encode(BYTE *t,BYTE *s,int l,int x) // terribly hacky encoder based on an 8-bit RLE and an interleaved pseudo Huffman filter!
{
	if (l<=0) return *t++=128,*t++=0,2; // quick EOF!
	#define xrf_encode1(n) ((n)&&(*z++=(n),a+=b),(b>>=1)||(*y=a,y=z++,a=0,b=128)) // write "0" (zero) or "1nnnnnnnn" (nonzero)
	BYTE *z=t,*y=z++,a=0,b=128,q=0; if (x<0) x=-x,q=128; // x<0 = perform XOR 128 on bytes, used when turning 16s audio into 8u
	do
	{
		BYTE *r=s,c=*r;
		do
			s+=x;
		while (--l&&c==*s);
		int n=(s-r)/x; c^=q;
		while (n>=1+66047) // i.e. 0x101FF = 512+65535
			xrf_encode1(c),xrf_encode1(c),xrf_encode1(255),xrf_encode1(255),xrf_encode1(255),n-=66047;
		xrf_encode1(c); if (n>1)
		{
			xrf_encode1(c); if (n>=512)
				xrf_encode1(255),n-=512,xrf_encode1(n>>8),xrf_encode1(n&255); // 512..66047 -> 0xFF 0x0000..0xFFFF
			else if (n>=256)
				xrf_encode1(254),xrf_encode1(n&255); // 256..511 -> 0xFE 0x00..0xFF
			else
				xrf_encode1(n-2); // 2..256 -> 0x00..0xFE
		}
	}
	while (l);
	return *y=a+b,*z++=0,z-t; // EOF: "100000000"
}

int session_createfilm(void) // start recording video and audio; !0 ERROR
{
	if (session_filmfile) return 1; // file already open!

	if (!session_filmvideo&&!(session_filmvideo=malloc(sizeof(VIDEO_UNIT)*SESSION_FILMVIDEO_LENGTH)))
		return 1; // cannot allocate buffer!
	if (!xrf_chunk&&!(xrf_chunk=malloc((sizeof(VIDEO_UNIT)*SESSION_FILMVIDEO_LENGTH+SESSION_FILMAUDIO_LENGTH*AUDIO_BITDEPTH/8)*9/8+4*8))) // maximum pathological length!
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
	fputc(session_filmflag=((AUDIO_BITDEPTH>>session_filmscale)>8?1:0)+(AUDIO_CHANNELS>1?2:0),session_filmfile); // +16BITS(1)+STEREO(2)
	fputmmmm(-1,session_filmfile); // frame count, will be filled later
	session_filmalign=video_pos_y; // catch scanline mode, if any
	return memset(session_filmvideo,0,sizeof(VIDEO_UNIT)*SESSION_FILMVIDEO_LENGTH),session_filmcount=0;
}
void session_writefilm(void) // record one frame of video and audio
{
	if (!session_filmfile) return; // file not open!
	// ignore first frame if the video is interleaved and we're on the wrong half frame
	if (!session_filmcount&&video_interlaced&&!video_interlaces) return;
	BYTE *z=xrf_chunk; static BYTE dirty=0;
	if (!video_framecount) dirty=1; // frameskipping?
	if (!(++session_filmcount&session_filmtimer))
	{
		if (!dirty)
		{
			z+=xrf_encode(z,NULL,0,0); // B
			z+=xrf_encode(z,NULL,0,0); // G
			z+=xrf_encode(z,NULL,0,0); // R
			//z+=xrf_encode(z,NULL,0,0); // A!
		}
		else
		{
			dirty=0; // to avoid encoding multiple times a frameskipped image
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
				s=(BYTE*)session_filmaudio,l*=2,memcpy(&session_filmaudio[AUDIO_LENGTH_Z*AUDIO_CHANNELS],audio_frame,AUDIO_BITDEPTH/8*AUDIO_LENGTH_Z*AUDIO_CHANNELS); // glue both blocks!
			#if AUDIO_CHANNELS > 1
			#if AUDIO_BITDEPTH > 8
				#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				if (session_filmscale)
					z+=xrf_encode(z,&s[0],l,-4), // L
					z+=xrf_encode(z,&s[2],l,-4); // R
				else
					z+=xrf_encode(z,&s[1],l,4), // l
					z+=xrf_encode(z,&s[0],l,4), // L
					z+=xrf_encode(z,&s[3],l,4), // r
					z+=xrf_encode(z,&s[2],l,4); // R
				#else
				if (session_filmscale)
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
				if (session_filmscale)
					z+=xrf_encode(z,&s[0],l,-2); // M
				else
					z+=xrf_encode(z,&s[1],l,2), // m
					z+=xrf_encode(z,&s[0],l,2); // M
				#else
				if (session_filmscale)
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
		memcpy(session_filmaudio,audio_frame,AUDIO_BITDEPTH/8*AUDIO_LENGTH_Z*AUDIO_CHANNELS); // keep audio block for later!
	session_filmalign=video_pos_y;
}
int session_closefilm(void) // stop recording video and audio; !0 ERROR
{
	if (session_filmvideo) free(session_filmvideo),session_filmvideo=NULL;
	if (xrf_chunk) free(xrf_chunk),xrf_chunk=NULL;
	if (!session_filmfile) return 1;
	fseek(session_filmfile,16,SEEK_SET);
	fputmmmm(session_filmcount>>session_filmtimer,session_filmfile); // number of frames
	return fclose(session_filmfile),session_filmfile=NULL,0;
}

// multimedia: screenshot output ------------------------------------ //
// warning: the following code assumes that VIDEO_UNIT is DWORD 0X00RRGGBB!

unsigned char bitmapheader[54]="BM\000\000\000\000\000\000\000\000\066\000\000\000\050\000\000\000\000\000\000\000\000\000\000\000\001\000\030\000"; // zero-padded
unsigned int session_nextbitmap=1;
INLINE int session_savebitmap(void) // save a RGB888 bitmap file; !0 ERROR
{
	if (!(session_nextbitmap=session_savenext("%s%08u.bmp",session_nextbitmap)))
		return 1; // too many files!
	FILE *f; if (!(f=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	int i=VIDEO_PIXELS_X*VIDEO_PIXELS_Y*3>>(2*session_filmscale);
	mputiiii(&bitmapheader[0x02],sizeof(bitmapheader)+i);
	mputii(&bitmapheader[0x12],VIDEO_PIXELS_X>>session_filmscale);
	mputii(&bitmapheader[0x16],VIDEO_PIXELS_Y>>session_filmscale);
	mputiiii(&bitmapheader[0x22],i);
	fwrite(bitmapheader,1,sizeof(bitmapheader),f);
	static BYTE r[VIDEO_PIXELS_X*3]; // target scanline buffer
	for (i=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-session_filmscale-1;i>=VIDEO_OFFSET_Y;fwrite(r,1,VIDEO_PIXELS_X*3>>session_filmscale,f),i-=session_filmscale+1)
		if (session_filmscale)
		{
			BYTE *t=r; VIDEO_UNIT *s=session_getscanline(i);
			for (int j=0;j<VIDEO_PIXELS_X;j+=2) // turn two ARGB into one RGB
			{
				VIDEO_UNIT v=VIDEO_FILTER_HALF(s[0],s[1]);
				*t++=v, // copy B
				*t++=v>>8, // copy G
				*t++=v>>16; // copy R
				s+=2;
			}
		}
		else
		{
			BYTE *t=r; VIDEO_UNIT *s=session_getscanline(i);
			for (int j=0;j<VIDEO_PIXELS_X;++j) // turn each ARGB into one RGB
			{
				VIDEO_UNIT v=*s++;
				*t++=v, // copy B
				*t++=v>>8, // copy G
				*t++=v>>16; // copy R
			}
		}
	return fclose(f),0;
}

// configuration functions ------------------------------------------ //

#define UTF8_BOM(s) ((-17==(char)*s&&-69==(char)s[1]&&-65==(char)s[2])?&s[3]:s) // skip UTF8 BOM if present
void session_detectpath(char *s) // detects session path using argv[0] as reference
{
	if (s=strrchr(strcpy(session_path,s),PATHCHAR))
		s[1]=0; // keep separator
	else
		*session_path=0; // no path (?)
}
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
char *session_configread(unsigned char *t) // reads configuration file; t points to the current line; returns NULL if handled, a string otherwise
{
	unsigned char *s=t; while (*s) ++s; // go to trail
	while (*--s<=' ') *s=0; // clean trail up
	s=t=UTF8_BOM(t);
	while (*s>' ') ++s; *s++=0; // divide name and data
	while (*s==' ') ++s; // skip spaces between both
	if (*s) // handle common parameters, unless empty
	{
		if (!strcasecmp(t,"polyphony")) return audio_mixmode=*s&3,NULL;
		if (!strcasecmp(t,"softaudio")) return audio_filter=*s&3,NULL;
		if (!strcasecmp(t,"scanlines")) return video_scanline=(*s>>1)&7,video_scanline=video_scanline>4?4:video_scanline,video_pageblend=*s&1,NULL;
		if (!strcasecmp(t,"softvideo")) return video_filter=*s&7,NULL;
		if (!strcasecmp(t,"zoomvideo")) return session_zoomblit=(*s>>1)&7,session_zoomblit=session_zoomblit>4?4:session_zoomblit,video_lineblend=*s&1,NULL;
		if (!strcasecmp(t,"safevideo")) return session_softblit=*s&1,NULL;
		if (!strcasecmp(t,"safeaudio")) return session_softplay=*s&1,NULL;
		if (!strcasecmp(t,"film")) return session_filmscale=*s&1,session_filmtimer=(*s>>1)&1,NULL;
		if (!strcasecmp(t,"info")) return onscreen_flag=*s&1,NULL;
	}
	return s;
}
void session_configwrite(FILE *f) // save common parameters
{
	fprintf(f,
		"film %d\ninfo %d\n"
		"polyphony %d\nsoftaudio %d\nscanlines %d\nsoftvideo %X\nzoomvideo %d\nsafevideo %d\nsafeaudio %d\n"
		,(session_filmscale?1:0)+(session_filmtimer?2:0),onscreen_flag
		,audio_mixmode,audio_filter,(video_scanline<<1)+(video_pageblend?1:0),video_filter,(video_lineblend?1:0)+(session_zoomblit<<1),session_softblit,session_softplay
		);
}

void session_wrapup(void) // final operations before `session_byebye`
{
	puff_byebye();
	debug_close();
	session_closefilm();
	session_closewave();
}

// =================================== END OF OS-INDEPENDENT ROUTINES //
