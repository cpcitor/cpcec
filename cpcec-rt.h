 //  ####  ######    ####  #######   ####    ----------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####    ----------------------- //

// A bunch of routines can be shared across emulators and operating
// systems because they either operate with standard data types or
// rely on talking to the C standard library rather than the OS.

// START OF OS-INDEPENDENT ROUTINES ================================= //

#define GPL_3_INFO "This program comes with ABSOLUTELY NO WARRANTY; for more details" "\n" \
	"please read the GNU General Public License. This is free software" "\n" \
	"and you are welcome to redistribute it under certain conditions."

#define length(x) (sizeof(x)/sizeof(*(x)))
#define MEMZERO(x) memset((x),0,sizeof(x))
#define MEMFULL(x) memset((x),~0,sizeof(x))
#define MEMSAVE(x,y) memcpy((x),(y),sizeof(y))
#define MEMLOAD(x,y) memcpy((x),(y),sizeof(x))
#define MEMNCPY(x,y,z) memcpy((x),(y),sizeof(*(x))*(z))

unsigned char session_scratch[1<<18]; // at least 256k!

INLINE int ucase(int i) { return i>='a'&&i<='z'?i-32:i; }
INLINE int lcase(int i) { return i>='A'&&i<='Z'?i+32:i; }
int fread1(void *t,int l,FILE *f) { int k=0,i; while (l>0) { if (!(i=fread(t,1,l,f))) break; t=(void*)((char*)t+i); k+=i; l-=i; } return k; } // safe fread(t,1,l,f)
int fwrite1(void *t,int l,FILE *f) { int k=0,i; while (l>0) { if (!(i=fwrite(t,1,l,f))) break; t=(void*)((char*)t+i); k+=i; l-=i; } return k; } // safe fwrite(t,1,l,f)

char *session_configread(char *t) // reads configuration file; s points to the parameter value, *s is ZERO if unhandleable
{
	unsigned char *s=t; while (*s) ++s; // go to trail
	while (*--s<=' ') *s=0; // clean trail up
	s=t;
	while (*s>' ') ++s; *s++=0; // divide name and data
	while (*s==' ') ++s; // skip spaces between both
	//if (*s) // handle common parameters, if valid
	{
		if (!strcmp(session_parmtr,"scanlines")) return video_scanline=*s&3,NULL;
		if (!strcmp(session_parmtr,"softaudio")) return audio_filter=*s&3,NULL;
		if (!strcmp(session_parmtr,"softvideo")) return video_filter=*s&7,NULL;
	}
	return s;
}
void session_configwrite(FILE *f) // save common parameters
{
	fprintf(f,"scanlines %i\nsoftvideo %i\nsoftaudio %i\n",video_scanline,video_filter,audio_filter);
}

char *strrstr(char *h,char *n) // = strrchr + strstr
{
	char *z=NULL;
	for (;;)
	{
		char *s=h,*t=n;
		while (*t&&*s==*t)
			++s,++t;
		if (!*t)
			z=h;
		if (*h)
			++h;
		else
			break;
	}
	return z;
}
int globbing(char *w,char *t,int q) // wildcard pattern *w against string *t; q = strcasecmp/strcmp; 0 on mismatch!
{
	char *ww=NULL,*tt=NULL,c,k;
	while (c=*t)
		if ((k=*w++)=='*')
		{
			while (*w==k) // skip additional wildcards
				++w;
			if (!*w)
				return 1; // end of pattern, succeed!
			tt=t,ww=w; // remember wildcard and continue
		}
		else if (k!='?'&&(q?ucase(k)!=ucase(c):k!=c)) // compare character
		{
			if (!ww)
				return 0; // no past wildcards, fail!
			t=++tt,w=ww; // return to wildcard and continue
		}
		else
			++t;
	while (*w=='*') // skip additional wildcards, if any
		++w;
	return !*w; // succeed on end of pattern
}
#define multiglob session_substr //char multiglob[STRMAX];
int multiglobbing(char *w,char *t,int q) // like globbing(), but with multiple patterns with semicolons inbetween
{
	char c,*m;
	do
	{
		m=multiglob;
		while ((c=*w++)&&c!=';')
			*m++=c;
		*m=0;
		if (globbing(multiglob,t,q))
			return 1;
	}
	while (c);
	return 0;
}

// interframe functions --------------------------------------------- //

#define VIDEO_FILTER_Y_MASK 1
#define VIDEO_FILTER_X_MASK 2
#define VIDEO_FILTER_SMUDGE 4

INLINE void video_newscanlines(int x,int y)
{
	video_target=video_frame+(video_pos_y=y)*VIDEO_LENGTH_X+(video_pos_x=x); // *!* video_pos_y=((VIDEO_LENGTH_Y-video_pos_y)/2-(video_pos_y<VIDEO_LENGTH_Y))&-2
	if (video_interlacez=(video_interlaces&&(video_scanline&2)))
		++video_pos_y,video_target+=VIDEO_LENGTH_X;
}
// do not manually unroll the following operations, GCC is smart enough to do a better job on its own: 1820% > 1780%!
INLINE void video_drawscanline(void) // call between scanlines
{
	if (video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
	{
		VIDEO_DATATYPE va,vt,vz,*vi=video_target-video_pos_x+VIDEO_OFFSET_X,*vl=vi+VIDEO_PIXELS_X,*vo=video_target-video_pos_x+VIDEO_OFFSET_X+VIDEO_LENGTH_X,*vp;
		switch ((video_filter&7)+(video_scanlinez==0?8:0))
		{
			case 8+0: // nothing (full)
				MEMNCPY(vo,vi,VIDEO_PIXELS_X);
				// no `break` here!
			case 0+0: // nothing (half)
				break;
			case 0+VIDEO_FILTER_Y_MASK:
				if (video_interlacez)
					while (vi<vl)
						vt=*vi,*vi++=VIDEO_FILTER_X1(vt);
				break;
			case 8+VIDEO_FILTER_Y_MASK:
				while (vi<vl)
					vt=*vi++,*vo++=VIDEO_FILTER_X1(vt);
				break;
			case 8+VIDEO_FILTER_X_MASK:
				while (vi<vl)
					vt=*vi,*vo++=*vi++=VIDEO_FILTER_X1(vt),
					*vo++=*vi++;
				break;
			case 0+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_X_MASK:
				if (video_interlacez) ++vi;
				// no `break` here!
			case 0+VIDEO_FILTER_X_MASK:
				while (vi<vl)
					vt=*vi,
					*vi=VIDEO_FILTER_X1(vt),
					vi+=2;
				break;
			case 8+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_X_MASK:
				while (vi<vl)
					*vo++=vt=*vi,*vi++=VIDEO_FILTER_X1(vt),
					vt=*vi++,*vo++=VIDEO_FILTER_X1(vt);
				break;
			case 8+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=*vi++=vt;
				break;
			case 0+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_SMUDGE:
				if (video_interlacez)
				{
					vz=*(vp=(vi+2));
					while (vp<vl)
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=VIDEO_FILTER_X1(vt);
					break;
				}
				// no `break` here!
			case 0+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vi++=vt;
				break;
			case 8+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				*vo++=VIDEO_FILTER_X1(vz);
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=VIDEO_FILTER_X1(vt),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=VIDEO_FILTER_X1(vt),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=VIDEO_FILTER_X1(vt),*vi++=vt;
				break;
			case 8+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=*vi++=VIDEO_FILTER_X1(vt),
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=*vi++=VIDEO_FILTER_X1(vt);
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=*vi++=vt;
				break;
			case 0+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				if (video_interlacez) ++vi;
				// no `break` here!
			case 0+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=VIDEO_FILTER_X1(vt),
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vi++=VIDEO_FILTER_X1(vt);
				VIDEO_FILTER_STEP(vt,vz,vz),*vi++=vt;
				break;
			case 8+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=vt,*vi++=VIDEO_FILTER_X1(vt),
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt,*vo++=VIDEO_FILTER_X1(vt);
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=vt,*vi++=VIDEO_FILTER_X1(vt);
				VIDEO_FILTER_STEP(vt,vz,vz),*vi++=vt,*vo++=VIDEO_FILTER_X1(vt);
				break;
		}
	}
}
INLINE void video_endscanlines(VIDEO_DATATYPE z) // call between frames
{
	#if 0
		static int video_rasterscan;
		for (int i=8;i>0;--i)
		{
			video_rasterscan=(video_rasterscan+2)%VIDEO_LENGTH_Y;
			if (video_rasterscan>=VIDEO_OFFSET_Y&&video_rasterscan<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
			{
				VIDEO_DATATYPE *p=video_frame+VIDEO_OFFSET_X+video_rasterscan*VIDEO_LENGTH_X;
				for (int x=0;x<VIDEO_PIXELS_X;x+=i)
					*p=VIDEO_FILTER_AVG(*p,VIDEO1(0xE0E0E0)),p+=i;
				if (video_scanlinez==0)
				{
					p+=VIDEO_LENGTH_X-VIDEO_PIXELS_X;
					for (int x=0;x<VIDEO_PIXELS_X;x+=i)
					*p=VIDEO_FILTER_AVG(*p,VIDEO1(0xC0C0C0)),p+=i;
				}
			}
		}
	#endif
	static VIDEO_DATATYPE zzz=-1; // first value intentionally invalid!
	VIDEO_DATATYPE zz=(video_filter&VIDEO_FILTER_Y_MASK)?VIDEO_FILTER_X1(z):z; // minor video interpolation: weak scanlines
	if (video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y) // empty bottom lines?
	{
		VIDEO_DATATYPE *p=video_target-video_pos_x+VIDEO_OFFSET_X;
		int x,y;
		if (video_scanlinez==0)
			for (y=video_pos_y;y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;y+=2,p+=VIDEO_LENGTH_X-VIDEO_PIXELS_X)
			{
				for (x=0;x<VIDEO_PIXELS_X;++x)
					*p++=z; // render primary scanlines
				p+=VIDEO_LENGTH_X-VIDEO_PIXELS_X;
				for (x=0;x<VIDEO_PIXELS_X;++x)
					*p++=zz; // filter secondary scanlines
			}
		else
			for (y=video_pos_y;y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;y+=2,p+=VIDEO_LENGTH_X*2-VIDEO_PIXELS_X)
				for (x=0;x<VIDEO_PIXELS_X;++x)
					*p++=z; // render primary scanlines only
	}
	if (video_scanlinez!=video_scanline||zzz!=zz) // did the config change?
	{
		if ((video_scanlinez=video_scanline)==1) // do we have to redo the secondary scanlines?
		{
			zzz=zz;
			int x,y; VIDEO_DATATYPE *p=video_frame+VIDEO_OFFSET_X+(VIDEO_OFFSET_Y+1)*VIDEO_LENGTH_X;
			for (y=0;y<VIDEO_PIXELS_Y;y+=2,p+=VIDEO_LENGTH_X*2-VIDEO_PIXELS_X)
				for (x=0;x<VIDEO_PIXELS_X;++x)
					*p++=zz; // render secondary scanlines only
		}
	}
}

INLINE void audio_playframe(int q,AUDIO_DATATYPE *ao) // call between frames by the OS wrapper
{
	static AUDIO_DATATYPE az=AUDIO_ZERO;
	AUDIO_DATATYPE aa,*ai=audio_frame; // session_filter[(aa<<8)+az] is 8-bit only and isn't faster than the calculations
	int i; switch (q)
	{
		case 1:
			for (i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,
				*ao++=az=(aa+az+(aa>az))/2; // hard filter
				//*ao++=(aa+az+(aa>az))/2,az=aa; // soft filter
			break;
		case 2:
			for (i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,
				*ao++=az=(aa+az*3+(aa>az)*3)/4; // hard filter
				//*ao++=(aa+az*3+(aa>az)*2)/4,az=aa; // soft filter
			break;
		case 3:
			for (i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,
				*ao++=az=(aa+az*7+(aa>az)*7)/8; // hard filter
				//*ao++=(aa+az*7+(aa>az)*4)/8,az=aa; // soft filter
			break;
	}
}

INLINE void session_update(void) // render video+audio thru OS and handle realtime logic (self-adjusting delays, automatic frameskip, etc.)
{
	session_render();
	session_signal&=~SESSION_SIGNAL_FRAME;
	audio_target=audio_frame;
	if (video_scanline==3)
		video_interlaced|=1;
	else
		video_interlaced&=~1;
	if (++video_framecount>video_framelimit)
		video_framecount=0;
}

// multimedia file output ------------------------------------------- //

unsigned int session_savenext(char *z,unsigned int i) // scans for available filenames; `z` is "%s%i.ext" and `i` is the current index; returns 0 on error, or a new index
{
	FILE *f;
	while (i)
	{
		sprintf(session_parmtr,z,session_path,i);
		if (!(f=fopen(session_parmtr,"rb"))) // does the file exist?
			break; // no? excellent!
		fclose(f); // try again
		++i;
	}
	return i;
}

unsigned char bitmapheader[54]="BM\000\000\000\000\000\000\000\000\066\000\000\000\050\000\000\000\000\000\000\000\000\000\000\000\001\000\030\000";
unsigned int session_nextbitmap=1;
INLINE int session_savebitmap(void) // save a bitmap file; !0 ERROR
{
	if (!(session_nextbitmap=session_savenext("%s%08i.bmp",session_nextbitmap)))
		return 1; // too many files!
	FILE *f;
	if (!(f=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	mputiiii(&bitmapheader[0x02],sizeof(bitmapheader)+VIDEO_PIXELS_X*VIDEO_PIXELS_Y*3);
	mputii(&bitmapheader[0x12],VIDEO_PIXELS_X);
	mputii(&bitmapheader[0x16],VIDEO_PIXELS_Y);
	//bitmapheader[0x1A]=1;
	//bitmapheader[0x1C]=24; // cfr. VIDEO_DATATYPE
	mputiiii(&bitmapheader[0x22],VIDEO_PIXELS_X*VIDEO_PIXELS_Y*3);
	fwrite(bitmapheader,1,sizeof(bitmapheader),f);
	session_writebitmap(f); // conversion can be OS-dependent!
	fclose(f);
	return 0;
}

unsigned char waveheader[44]="RIFF\000\000\000\000WAVEfmt \020\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000data";
unsigned int session_nextwave=1,session_wavesize;
INLINE int session_createwave(void) // create a wave file; !0 ERROR
{
	if (session_wavefile||!session_audio)
		return 1; // file already open, or no audio to save!
	if (!(session_nextwave=session_savenext("%s%08i.wav",session_nextwave)))
		return 1; // too many files!
	if (!(session_wavefile=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	waveheader[0x16]=AUDIO_CHANNELS; // channels
	mputiiii(&waveheader[0x18],AUDIO_PLAYBACK); // samples per second
	mputiiii(&waveheader[0x1C],AUDIO_PLAYBACK*sizeof(AUDIO_DATATYPE)); // bytes per second
	waveheader[0x20]=sizeof(AUDIO_DATATYPE); // bytes per sample
	waveheader[0x22]=AUDIO_BITDEPTH;
	fwrite(waveheader,1,sizeof(waveheader),session_wavefile);
	return session_wavesize=0;
}
INLINE void session_writewave(AUDIO_DATATYPE *t) { session_wavesize+=fwrite(t,1,sizeof(audio_buffer),session_wavefile); }
INLINE int session_closewave(void) // close a wave file; !0 ERROR
{
	if (session_wavefile)
	{
		fseek(session_wavefile,0x28,SEEK_SET);
		fputiiii(session_wavesize,session_wavefile);
		fseek(session_wavefile,0x04,SEEK_SET);
		fputiiii(session_wavesize+36,session_wavefile); // file=head+data
		fclose(session_wavefile);
		session_wavefile=NULL;
		return 0;
	}
	return 1;
}

// elementary ZIP archive support ----------------------------------- //

// the INFLATE method! ... or more properly a terribly simplified mess
// based on the RFC1951 standard and PUFF.C from the ZLIB project.

// temporary variables
unsigned char *puff_src,*puff_tgt; // source and target buffers
int puff_srcl,puff_tgtl,puff_srco,puff_tgto,puff_buff,puff_bits;
struct puff_huff { short *cnt,*sym; }; // Huffman table element
// auxiliary functions
int puff_read(int n) // reads N bits from source; <0 ERROR
{
	int a=puff_buff;
	while (puff_bits<n)
	{
		if (puff_srco>=puff_srcl)
			return -1; // source overrun!
		a+=puff_src[puff_srco++]<<puff_bits; // copy byte from source
		puff_bits+=8; // skip byte from source
	}
	puff_buff=a>>n; puff_bits-=n; // adjust target and flush bits
	return a&((1<<n)-1); // result!
}
int puff_decode(struct puff_huff *h) // decodes Huffman code from source; <0 ERROR
{
	int l=1;
	short *next=h->cnt;
	int i=0,base=0,code=0;
	int buff=puff_buff,bits=puff_bits; // local copies are faster!
	for (;;)
	{
		while (bits--)
		{
			code+=buff&1;
			buff>>=1;
			int o=*++next;
			if (code<base+o) // does the code fit the interval?
			{
				puff_buff=buff; // adjust target
				puff_bits=(puff_bits-l)&7; // flush bits
				return h->sym[i+(code-base)]; // result!
			}
			i+=o,base=(base+o)<<1,code<<=1,++l; // calculate next interval
		}
		if (!(bits=(15+1)-l))
			return -1; // invalid value!
		if (puff_srco>=puff_srcl)
			return -1; // source overrun!
		buff=puff_src[puff_srco++]; // copy byte from source
		if (bits>8)
			bits=8; // flush source bits
	}
}
int puff_tables(struct puff_huff *h,short *l,int n) // generates Huffman tables from canonical table; !0 ERROR
{
	int i,a;
	short o[15+1];
	for (i=0;i<=15;++i)
		h->cnt[i]=0; // reset all bit counts
	for (i=0;i<n;++i)
		++(h->cnt[l[i]]); // increase relevant bit counts
	if (h->cnt[0]==n)
		return 0; // already done!
	for (a=i=1;i<=15;++i)
		if ((a=(a<<1)-(h->cnt[i]))<0)
			return a; // invalid value!
	for (o[i=1]=0;i<15;++i)
		o[i+1]=o[i]+h->cnt[i]; // reset all bit offsets
	for (i=0;i<n;++i)
		if (l[i])
			h->sym[o[l[i]]++]=i; // define symbols from bit offsets
	return a; // 0 if complete, >0 otherwise!
}
int puff_expand(struct puff_huff *puff_lcode,struct puff_huff *puff_ocode) // expands DEFLATE source into target; !0 ERROR
{
	static const short lcode[2][29]={ // length constants
		{ 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 },
		{ 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 }};
	static const short ocode[2][30]={ // offset constants
		{ 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 },
		{ 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 }};
	int a,l,o;
	for (;;)
	{
		if ((a=puff_decode(puff_lcode))<0)
			return a; // invalid value!
		if (a<256) // literal?
		{
			if (puff_tgto>=puff_tgtl)
				return -1; // target overrun!
			puff_tgt[puff_tgto++]=a; // copy literal
		}
		else if (a>256) // length:offset pair?
		{
			if ((a-=257)>=29)
				return -1; // invalid value!
			if (puff_tgto+(l=lcode[0][a]+puff_read(lcode[1][a]))>puff_tgtl)
				return -1; // target overrun!
			if ((a=puff_decode(puff_ocode))<0)
				return a; // invalid value!
			if ((o=puff_tgto-ocode[0][a]-puff_read(ocode[1][a]))<0)
				return -1; // source underrun!
			while (l--)
				puff_tgt[puff_tgto++]=puff_tgt[o++];
		}
		else return 0; // end of block
	}
}
// block type handling
INLINE int puff_stored(void) // copies raw uncompressed byte block from source to target; !0 ERROR
{
	if (puff_srco+4>puff_srcl)
		return -1; // source overrun!
	puff_buff=puff_bits=0; // ignore remaining bits
	int l=puff_src[puff_srco++];
	l+=puff_src[puff_srco++]<<8;
	int k=puff_src[puff_srco++];
	k+=puff_src[puff_srco++]<<8;
	if (!l||l+k!=0xFFFF)
		return -1; // invalid value!
	if (puff_srco+l>puff_srcl||puff_tgto+l>puff_tgtl)
		return -1; // source/target overrun!
	while (l--)
		puff_tgt[puff_tgto++]=puff_src[puff_srco++]; // copy source to target and update pointers
	return 0;
}
short puff_lencnt[15+1],puff_lensym[288]; // 286 and 287 are reserved
short puff_offcnt[15+1],puff_offsym[32]; // 30 and 31 are reserved
struct puff_huff puff_lcode={ puff_lencnt,puff_lensym };
struct puff_huff puff_ocode={ puff_offcnt,puff_offsym };
INLINE int puff_static(void) // generates default Huffman codes and expands block from source to target; !0 ERROR
{
	short t[288+32];
	int i=0;
	for (;i<144;++i) t[i]=8;
	for (;i<256;++i) t[i]=9;
	for (;i<280;++i) t[i]=7;
	for (;i<288;++i) t[i]=8;
	for (;i<288+32;++i) t[i]=5;
	puff_tables(&puff_lcode,t,288); // MUST BE 288!!
	puff_tables(&puff_ocode,t+288,32); // CAN BE 30?
	return puff_expand(&puff_lcode,&puff_ocode);
}
INLINE int puff_dynamic(void) // generates custom Huffman codes and expands block from source to target; !0 ERROR
{
	int l,o,h;
	if ((l=puff_read(5)+257)<257||l>286||(o=puff_read(5)+1)<1||o>30||(h=puff_read(4)+4)<4)
		return -1; // invalid value!
	static const short hcode[19]={ 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 }; // Huffman constants
	short t[288+32];
	int i=0;
	while (i<h) t[hcode[i++]]=puff_read(3); // read bit sizes for the Huffman-encoded header
	while (i<19) t[hcode[i++]]=0; // padding
	if (puff_tables(&puff_lcode,t,19)) // clear the remainder of this header
		return -1; // invalid table!
	i=0;
	while (i<l+o)
	{
		int a;
		if ((a=puff_decode(&puff_lcode))<16) // literal?
			t[i++]=a; // copy literal
		else
		{
			int z;
			if (a==16)
			{
				if (!i)
					return -1; // invalid value!
				z=t[i-1],a=3+puff_read(2); // copy last value 3+0..3 times
			}
			else if (a==17)
				z=0,a=3+puff_read(3); // store zero 3+0..7 times
			else // (a==18)
				z=0,a=11+puff_read(7); // store zero 11+0..127 times
			if (i+a>l+o)
				return -1; // overflow!
			while (a--)
				t[i++]=z; // copy or store value
		}
	}
	if (puff_tables(&puff_lcode,t,l)&&(l-puff_lcode.cnt[0]!=1))
		return -1; // invalid length tables!
	if (puff_tables(&puff_ocode,t+l,o)&&(o-puff_ocode.cnt[0]!=1))
		return -1; // invalid offset tables!
	return puff_expand(&puff_lcode,&puff_ocode);
}
INLINE int puff_main(void) // inflates compressed source into target; !0 ERROR
{
	int q,e;
	puff_buff=puff_bits=0; // puff_srcX and puff_tgtX are set by caller
	do
	{
		q=puff_read(1); // is this the last block?
		switch (puff_read(2)) // which type of block it is?
		{
			case 0: e=puff_stored(); break; // uncompressed raw bytes
			case 1: e=puff_static(); break; // default Huffman coding
			case 2: e=puff_dynamic(); break; // custom Huffman coding
			default: e=1; break; // invalid value!
		}
	}
	while (!(q|e));
	return e;
}

// standard ZIP v2.0 archive reader that relies on
// the main directory at the end of the ZIP archive

FILE *puff_file=NULL;
unsigned char puff_name[256],puff_type;
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
	// find the central directory tree
	fseek(puff_file,0,SEEK_END);
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
	if (!puff_file)
		return -1;
	unsigned char h[46];
	fseek(puff_file,puff_diff+puff_next,SEEK_SET);
	if (fread(h,1,sizeof(h),puff_file)!=sizeof(h)||memcmp(h,"PK\001\002",4)||h[11]||h[29])//||(h[8]&8)
		return puff_close(),1; // reject EOFs, unknown IDs, extended types and very long filenames!
	puff_type=h[10];
	//puff_time=mgetiiii(&h[12]); // DOS timestamp
	puff_hash=mgetiiii(&h[16]);
	puff_srcl=mgetiiii(&h[20]);
	puff_tgtl=mgetiiii(&h[24]);
	puff_skip=mgetiiii(&h[42]); // the body that belongs to the current head
	puff_next+=46+h[28]+mgetii(&h[30])+mgetii(&h[32]); // next ZIP file header
	return puff_name[h[28]]=0,fread(puff_name,1,h[28],puff_file)!=h[28];
}
int puff_body(int q) // loads (!0) or skips (0) a ZIP file body; !0 ERROR
{
	puff_tgto=puff_srco=0;
	if (!puff_file)
		return -1;
	if (puff_tgtl<1)
		q=0; // no data, can safely skip the source
	if (q)
	{
		if (puff_srcl<1||puff_tgtl<0)
			return -1; // cannot get data from nothing! cannot write negative data!
		unsigned char h[30];
		fseek(puff_file,puff_diff+puff_skip,SEEK_SET);
		fread(h,1,sizeof(h),puff_file);
		if (!memcmp(h,"PK\003\004",4)&&h[8]==puff_type) // OK?
		{
			fseek(puff_file,mgetii(&h[26])+mgetii(&h[28]),SEEK_CUR); // skip name+xtra
			if (!puff_type)
				return fread1(puff_tgt,puff_tgto=puff_srco=puff_srcl,puff_file),puff_tgtl!=puff_srcl;
			if (puff_type==8)
				return fread1(puff_src,puff_srcl,puff_file),puff_main();
		}
	}
	return q; // 0 skipped=OK, !0 unknown=ERROR
}
// simple ANSI X3.66
unsigned int puff_dohash(unsigned int k,unsigned char *s,int l)
{
	static unsigned int z[]=
		{0,0x01DB71064,0x03B6E20C8,0x026D930AC,0x076DC4190,0x06B6B51F4,0x04DB26158,0x05005713C,
		0x0EDB88320,0x0F00F9344,0x0D6D6A3E8,0x0CB61B38C,0x09B64C2B0,0x086D3D2D4,0x0A00AE278,0x0BDBDF21C};
	for (int i=0;i<l;++i)
		k^=s[i],k=(k>>4)^z[k&15],k=(k>>4)^z[k&15];
	return k;
}
// ZIP-aware fopen()
char PUFF_SEPARATOR[]="|",puff_path[STRMAX];
FILE *puff_ffile=NULL;
FILE *puff_fopen(char *s,char *m) // mimics fopen(), so NULL on error, *FILE otherwise
{
	if (!s||!m)
		return NULL; // wrong parameters!
	char *z;
	if (!(z=strrstr(s,PUFF_SEPARATOR))||!strchr(m,'r'))
		return fopen(s,m); // normal file logic
	if (!strcmp(puff_path,s))
		return fseek(puff_ffile,0,SEEK_SET),puff_ffile; // recycle last file!
	strcpy(puff_path,s);
	puff_path[z++-s]=0;
	if (puff_open(puff_path))
		return 0;
	while (!puff_head()) // scan archive
		if (!strcasecmp(puff_name,z)) // found?
		{
			if (!(puff_ffile=tmpfile()))
				return puff_close(),NULL; // file failure!
			puff_src=puff_tgt=NULL;
			if (!(!puff_type||(puff_src=malloc(puff_srcl)))||!(puff_tgt=malloc(puff_tgtl))
				||puff_body(1)||puff_hash!=(~puff_dohash(~0,puff_tgt,puff_tgto)))
				fclose(puff_ffile),puff_ffile=NULL; // memory or data failure!
			if (puff_ffile)
				fwrite1(puff_tgt,puff_tgto,puff_ffile),fseek(puff_ffile,0,SEEK_SET); // fopen() expects ftell()=0!
			if (puff_src) free(puff_src); if (puff_tgt) free(puff_tgt);
			puff_close();
			if (puff_ffile) // either *FILE or NULL
				strcpy(puff_path,s); // allow recycling!
			else
				*puff_path=0; // don't recycle on failure!
			return puff_ffile;
		}
		else
			puff_body(0); // not found, skip file
	return NULL;
}
int puff_fclose(FILE *f)
{
	return (f!=puff_ffile)?fclose(f):0; // delay closing to allow recycling
}
void puff_byebye(void)
{
	if (puff_ffile) fclose(puff_ffile),puff_ffile=NULL; // cleanup is done by the runtime thanks to tmpfile()
}

// ZIP-aware user interfaces
char puff_pattern[]="*.zip";
char *puff_session_filedialog(char *r,char *s,char *t,int q,int f) // ZIP archive wrapper for session_getfile
{
	unsigned char rr[STRMAX],ss[STRMAX],*z,*zz;
	sprintf(ss,"%s;%s",puff_pattern,s);
	strcpy(rr,r);
	for (;;) // try either a file list or the file dialog until the user either chooses a file or quits
	{
		if (zz=strrstr(rr,PUFF_SEPARATOR)) // 'rr' holds the path, does it contain the separator already?
		{
			*zz=0; zz+=strlen(PUFF_SEPARATOR);  // *zz++=0; // now it either points to the previous file name or to a zero
			z=session_scratch; // generate list of files in archive
			int l=0,i=-1; // number of files, default selection
			if (!puff_open(rr))
			{
				while (!puff_head())
				{
					if (puff_name[strlen(puff_name)-1]!='/') // file or folder?
					{
						if (multiglobbing(s,puff_name,1))
						{
							if (!strcasecmp(zz,puff_name))
								i=l; // select file
							++l;
							z+=1+sprintf(z,"%s",puff_name);
						}
					}
					puff_body(0);
					if (z>=&session_scratch[length(session_scratch)-STRMAX])
					{
						z+=1+sprintf(z,"...");
						break; // too many files :-(
					}
				}
				puff_close();
			}
			if (l==1) // exactly one file?
			{
				if (!*zz) // new archive? (old archive would still show a file here)
					return session_filedialog_readonly1(),strcpy(session_parmtr,strcat(strcat(rr,PUFF_SEPARATOR),session_scratch));
			}
			else if (l) // at least one file?
			{
				*z++=0; // END OF LIST
				sprintf(z,"Browse archive %s",rr);
				if (session_list(i,session_scratch,z)>=0) // default to latest file if possible
					return session_filedialog_readonly1(),strcpy(session_parmtr,strcat(strcat(rr,PUFF_SEPARATOR),session_parmtr));
			}
		}
		if (!(z=(q?session_getfilereadonly(rr,ss,t,f):session_getfile(rr,ss,t)))) // what did the user choose?
			return NULL; // user cancel: give up
		if (strlen(z)<4||strcasecmp(z+strlen(z)-4,&puff_pattern[1]))
			return z; // normal file: accept it
		strcat(strcpy(rr,z),PUFF_SEPARATOR); // ZIP archive: append and try again
	}
}
#define puff_session_getfile(x,y,z) puff_session_filedialog(x,y,z,0,0)
#define puff_session_getfilereadonly(x,y,z,q) puff_session_filedialog(x,y,z,1,q)
char *puff_session_newfile(char *x,char *y,char *z) // writing within ZIP archives isn't allowed, so they must be leave them out if present
{
	char xx[STRMAX],*zz;
	strcpy(xx,x); // cancelling must NOT destroy the source path
	if (zz=strrstr(xx,PUFF_SEPARATOR))
		while (--zz>=xx&&*zz!=PATH_SEPARATOR)
			*zz=0; // remove ZIP archive and file within
	return session_newfile(xx,y,z);
}

// on-screen and debug text printing -------------------------------- //

VIDEO_DATATYPE onscreen_ink0,onscreen_ink1;
#include "cpcec-a7.h" //unsigned char *onscreen_chrs;
#define onscreen_inks(q0,q1) onscreen_ink0=q0,onscreen_ink1=q1

#define ONSCREEN_XY if ((x*=8)<0) x+=VIDEO_OFFSET_X+VIDEO_PIXELS_X; else x+=VIDEO_OFFSET_X; \
	if ((y*=8)<0) y+=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y; else y+=VIDEO_OFFSET_Y; \
	int p=y*VIDEO_LENGTH_X+x,a=video_scanline==1?2:1; \
	VIDEO_DATATYPE q0=q?onscreen_ink1:onscreen_ink0
void onscreen_char(int x,int y,int z,int q) // draw a character
{
	if ((z=(z&127)-32)<0)//if (z=='\t')
		return;
	ONSCREEN_XY,q1=q?onscreen_ink0:onscreen_ink1;
	unsigned const char *zz=&onscreen_chrs[z*8];
	for (y=0;y<8;++y)
	{
		unsigned char bb=*zz++,ww;
		bb|=bb>>1;
		for (ww=0;ww<2;ww+=a)
		{
			for (z=128;z;z>>=1)
				video_frame[p++]=(z&bb)?q1:q0;
			p+=VIDEO_LENGTH_X*a-8;
		}
	}
}
void onscreen_text(int x, int y, char *s, int q) // write a string
{
	while (*s)
		onscreen_char(x++, y, *s++, q);
}
void onscreen_byte(int x, int y, int a, int q) // write two digits
{
	onscreen_char(x,y,'0'+(a/10),q);
	onscreen_char(x+1,y,'0'+(a%10),q);
}
void onscreen_bool(int x,int y,int lx,int ly,int q) // draw dots
{
	ONSCREEN_XY;
	lx<<=3; ly<<=3;
	for (y=0;y<ly;y+=a)
	{
		for (x=0;x<lx;++x)
			video_frame[p++]=q0;
		p+=VIDEO_LENGTH_X*a-lx;
	}
}

#ifdef DEBUG
#else

char *debug_output,debug_search[STRMAX]=""; // output offset, string buffer, search buffer
void debug_locate(int x,int y) // move debug output to (X,Y)
{
	if (x<0)
		x+=DEBUG_LENGTH_X;
	if (y<0)
		y+=DEBUG_LENGTH_Y;
	debug_output=&debug_buffer[DEBUG_LENGTH_X*y+x];
}
void debug_prints(char *s) // print string onto debug output
{
	char c;
	while (c=*s++)
		*debug_output++=c;
}
void debug_printi(char *s,int i) // idem, string and integer
{
	sprintf(session_tmpstr,s,i);
	debug_prints(session_tmpstr);
}
void debug_dumpxy(int x,int y,char *s) // dump text block
{
	char c;
	do
	{
		debug_locate(x,y++);
		while ((c=*s++)&&c!='\n')
			*debug_output++=c;
	}
	while (c);
}

char onscreen_debug_mask=0,onscreen_debug_mask_=-1;
unsigned char onscreen_debug_chrs[sizeof(onscreen_chrs)];
void onscreen_debug(void) // rewrite debug texts
{
	if ((onscreen_debug_mask_^onscreen_debug_mask)&~1)
		switch((onscreen_debug_mask_=(onscreen_debug_mask&=7))&6) // font styles
		{
			default:
				for (int i=0,j;i<sizeof(onscreen_debug_chrs);++i)
					j=onscreen_chrs[i],onscreen_debug_chrs[i]=j|(j>>1); // normal
				break;
			case 2:
				memcpy(onscreen_debug_chrs,onscreen_chrs,sizeof(onscreen_chrs)); // thin
				break;
			case 6:
				for (int i=0,j;i<sizeof(onscreen_debug_chrs);++i)
					j=onscreen_chrs[i],onscreen_debug_chrs[i]=j|(j>>1)|(j<<1); // bold
				break;
		}
	unsigned char *t=debug_buffer;
	VIDEO_DATATYPE qq1,qq0;
	if (onscreen_debug_mask&1) // normal or inverse?
		qq1=VIDEO1(0xFFFFFF),qq0=VIDEO1(0x000000);
	else
		qq0=VIDEO1(0xFFFFFF),qq1=VIDEO1(0x000000);
	for (int y=0;y<DEBUG_LENGTH_Y;++y)
		for (int x=0;x<DEBUG_LENGTH_X;++x)
		{
			int z=*t++;
			const unsigned char *zz=&onscreen_debug_chrs[((z&127)-32)*8];
			VIDEO_DATATYPE q1,q0;
			if (z&128)
				q1=qq1,q0=qq0;
			else
				q1=qq0,q0=qq1;
			z=(y*DEBUG_LENGTH_X*DEBUG_LENGTH_Z+x)*8;
			int yy=(8-DEBUG_LENGTH_Z-1)/2;
			for (;yy<0;++yy)
			{
				for (int w=0;w<8;++w)
					debug_frame[z++]=q0;
				z+=(DEBUG_LENGTH_X-1)*8;
			}
			for (;yy<8;++yy)
			{
				unsigned char bb=*zz++;
				for (int w=128;w;w>>=1)
					debug_frame[z++]=(w&bb)?q1:q0;
				z+=(DEBUG_LENGTH_X-1)*8;
			}
			for (;yy<(8+DEBUG_LENGTH_Z)/2;++yy)
			{
				for (int w=0;w<8;++w)
					debug_frame[z++]=q0;
				z+=(DEBUG_LENGTH_X-1)*8;
			}
		}
}
#endif

// =================================== END OF OS-INDEPENDENT ROUTINES //
