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

#define GPL_3_INFO \
	"This program comes with ABSOLUTELY NO WARRANTY; for more details" "\n" \
	"please read the GNU General Public License. This is free software" "\n" \
	"and you are welcome to redistribute it under certain conditions."

#define length(x) (sizeof(x)/sizeof(*(x)))
#define MEMZERO(x) memset((x),0,sizeof(x))
#define MEMFULL(x) memset((x),~0,sizeof(x))
#define MEMSAVE(x,y) memcpy((x),(y),sizeof(y))
#define MEMLOAD(x,y) memcpy((x),(y),sizeof(x))
#define MEMNCPY(x,y,z) memcpy((x),(y),sizeof(*(x))*(z))

int fread1(void *t,int l,FILE *f) { int k=0,i; while (l&&(i=fread(t,1,l,f))) { t=(void*)((char*)t+i); k+=i; l-=i; } return k; } // safe fread(t,1,l,f)
int fwrite1(void *t,int l,FILE *f) { int k=0,i; while (l&&(i=fwrite(t,1,l,f))) { t=(void*)((char*)t+i); k+=i; l-=i; } return k; } // safe fwrite(t,1,l,f)

char *session_configread(char *t) // reads configuration file; s points to the parameter value, *s is ZERO if unhandleable
{
	unsigned char *s=t; while (*s) ++s; // go to trail
	while (*--s<=' ') *s=0; // clean trail up
	s=t;
	while (*s>' ') ++s; *s++=0; // divide name and data
	while (*s==' ') ++s; // skip spaces between both
	//if (*s) // handle common parameters, if valid
	{
		if (!strcmp(session_parmtr,"polyphony")) return audio_channels=*s&3,NULL;
		if (!strcmp(session_parmtr,"scanlines")) return video_scanline=*s&3,NULL;
		if (!strcmp(session_parmtr,"softaudio")) return audio_filter=*s&3,NULL;
		if (!strcmp(session_parmtr,"softvideo")) return video_filter=*s&7,NULL;
		if (!strcmp(session_parmtr,"zoomvideo")) return session_intzoom=*s&1,NULL;
	}
	return s;
}
void session_configwrite(FILE *f) // save common parameters
{
	fprintf(f,"polyphony %i\nscanlines %i\nsoftaudio %i\nsoftvideo %i\nzoomvideo %i\n",audio_channels,video_scanline,audio_filter,video_filter,session_intzoom);
}

char *strrstr(char *h,char *n) // = strrchr + strstr
{
	char *z=h;
	while (*h)
		++h; // go to end
	{
		char *y=n;
		while (*y)
			--h,++y; // skip last bytes that cannot match
	}
	while (h>=z)
	{
		char *s=h,*t=n;
		while (*t&&*s==*t)
			++s,++t;
		if (!*t)
			return h;
		--h;
	}
	return NULL;
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
		else if (k!='?'&&(q?lcase(k)!=lcase(c):k!=c)) // compare character
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
int multiglobbing(char *w,char *t,int q) // like globbing(), but with multiple patterns with semicolons inbetween; 1..n tells which pattern matches
{
	char c,*m; int n=1;
	do
	{
		m=multiglob;
		while ((c=*w++)&&c!=';')
			*m++=c;
		*m=0;
		if (globbing(multiglob,t,q))
			return n;
	}
	while (++n,c);
	return 0;
}

// the following algorithms are weak, but proper binary search is difficult to perform on packed lists; fortunately, items are likely to be partially sorted beforehand
int sortedinsert(char *t,int z,char *s) // insert string 's' in its alphabetical order within the packed list of strings 't' of length 'l'; returns the list's new length
{
	int m=z,n; while (m>0)
	{
		n=m;
		do --n; while (n>0&&t[n-1]); // backwards search is more convenient on partially sorted lists
		if (strcasecmp(s,&t[n])>=0)
			break;
		m=n;
	}
	n=strlen(s)+1; if (z>m)
		memmove(&t[m+n],&t[m],z-m);
	return memcpy(&t[m],s,n),z+n;
}
int sortedsearch(char *t,int z,char *s) // look for string 's' in an alphabetically ordered packed list of strings 't' of length 'l'; returns index (not offset!) in list
{
	char *r=&t[z]; int i=0; while (t<r)
	{
		if (!strcasecmp(s,t))
			return i;
		++i; while (*t++)
			;
	}
	return -1; // 's' was not found!
}

// interframe functions --------------------------------------------- //

#define VIDEO_FILTER_X_MASK 1
#define VIDEO_FILTER_Y_MASK 2
#define VIDEO_FILTER_SMUDGE 4

INLINE void video_newscanlines(int x,int y)
{
	video_target=video_frame+(video_pos_y=y)*VIDEO_LENGTH_X+(video_pos_x=x); // *!* video_pos_y=((VIDEO_LENGTH_Y-video_pos_y)/2-(video_pos_y<VIDEO_LENGTH_Y))&-2
	if (video_interlacez=(video_interlaces&&(video_scanline&2)))
		++video_pos_y,video_target+=VIDEO_LENGTH_X;
}
// do not manually unroll the following operations, GCC is smart enough to do a better job on its own: 1820% > 1780%!
INLINE void video_drawscanline(void) // call between scanlines; memory caching makes this more convenient than gathering all operations in video_endscanlines()
{
	if (video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y)
	{
		VIDEO_UNIT va,vt,vz,*vi=video_target-video_pos_x+VIDEO_OFFSET_X,*vl=vi+VIDEO_PIXELS_X,*vo=video_target-video_pos_x+VIDEO_OFFSET_X+VIDEO_LENGTH_X,*vp;
		switch ((video_filter&7)+(video_scanlinez?0:8))
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
				if (!video_interlacez)
				{
					if (video_pos_y&2) vi+=2;
					while (vi<vl)
						vt=*vi,*vi=VIDEO_FILTER_X1(vt),
						vi+=4;
					break;
				}
				// no `break`!
			case 0+VIDEO_FILTER_X_MASK:
				while (vi<vl)
					vt=*vi,
					*vi=VIDEO_FILTER_X1(vt),
					vi+=2;
				break;
			case 8+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_X_MASK:
				if (video_pos_y&2)
					while (vi<vl)
					{
						vt=*vi++,*vo++=VIDEO_FILTER_X1(vt);
						*vo++=*vi++;
						vt=*vi,*vi++=*vo++=VIDEO_FILTER_X1(vt);
						*vo++=*vi++;
					}
				else
					while (vi<vl)
					{
						vt=*vi,*vi++=*vo++=VIDEO_FILTER_X1(vt);
						*vo++=*vi++;
						vt=*vi++,*vo++=VIDEO_FILTER_X1(vt);
						*vo++=*vi++;
					}
				break;
			case 8+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=*vi++=vt;
				/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vo++=*vi++=vt;
				break;
			case 0+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_SMUDGE:
				if (video_interlacez)
				{
					vz=*(vp=(vi+2));
					while (vp<vl)
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=VIDEO_FILTER_X1(vt);
					VIDEO_FILTER_STEP(vt,vz,vz),*vi++=VIDEO_FILTER_X1(vt);
					/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vi++=VIDEO_FILTER_X1(vt);
					break;
				}
				// no `break` here!
			case 0+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vi++=vt;
				/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vi++=vt;
				break;
			case 8+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				*vo++=VIDEO_FILTER_X1(vz);
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=VIDEO_FILTER_X1(vt),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=VIDEO_FILTER_X1(vt),*vi++=vt;
				/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vo++=VIDEO_FILTER_X1(vt),*vi++=vt;
				break;
			case 8+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=*vi++=VIDEO_FILTER_X1(vt),
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vo++=*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vo++=*vi++=VIDEO_FILTER_X1(vt);
				/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vo++=*vi++=vt;
				break;
			case 0+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				if (!video_interlacez)
				{
					vz=*(vp=(vi+2));
					if (video_pos_y&2)
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt,
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt;
					while (vp<vl)
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=VIDEO_FILTER_X1(vt),
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt,
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt,
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt;
					VIDEO_FILTER_STEP(vt,vz,vz),*vi++=VIDEO_FILTER_X1(vt);
					/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vi++=vt;
					break;
				}
				// no `break` here!
			case 0+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				while (vp<vl)
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=VIDEO_FILTER_X1(vt),
					va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt;
				VIDEO_FILTER_STEP(vt,vz,vz),*vi++=VIDEO_FILTER_X1(vt);
				/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vi++=vt;
				break;
			case 8+VIDEO_FILTER_Y_MASK+VIDEO_FILTER_X_MASK+VIDEO_FILTER_SMUDGE:
				vz=*(vp=(vi+2));
				if (video_pos_y&2)
				{
					while (vp<vl)
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt,*vo++=VIDEO_FILTER_X1(vt),
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=*vo++=vt,
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=*vo++=VIDEO_FILTER_X1(vt),
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=*vo++=vt;
					VIDEO_FILTER_STEP(vt,vz,vz),*vo++=vt,*vi++=VIDEO_FILTER_X1(vt);
					/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vi++=vt,*vo++=VIDEO_FILTER_X1(vt);
				}
				else
				{
					while (vp<vl)
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=*vo++=VIDEO_FILTER_X1(vt),
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=*vo++=vt,
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=vt,*vo++=VIDEO_FILTER_X1(vt),
						va=*vp++,VIDEO_FILTER_STEP(vt,vz,va),*vi++=*vo++=vt;
					VIDEO_FILTER_STEP(vt,vz,vz),*vo++=vt,*vi++=VIDEO_FILTER_X1(vt);
					/*VIDEO_FILTER_STEP(vt,vz,vz),*/*vi++=vt,*vo++=VIDEO_FILTER_X1(vt);
				}
				break;
		}
	}
}
INLINE void video_endscanlines(VIDEO_UNIT z) // call between frames
{
	VIDEO_UNIT zz=(video_filter&VIDEO_FILTER_Y_MASK)?VIDEO_FILTER_X1(z):z; // minor video interpolation: weak scanlines
	if (video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y) // empty bottom lines?
	{
		VIDEO_UNIT *p=video_target-video_pos_x+VIDEO_OFFSET_X;
		if (video_scanlinez==0)
			for (int y=video_pos_y;y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;y+=2,p+=VIDEO_LENGTH_X-VIDEO_PIXELS_X)
			{
				for (int x=0;x<VIDEO_PIXELS_X;++x)
					*p++=z; // render primary scanlines
				p+=VIDEO_LENGTH_X-VIDEO_PIXELS_X;
				for (int x=0;x<VIDEO_PIXELS_X;++x)
					*p++=zz; // filter secondary scanlines
			}
		else
			for (int y=video_pos_y;y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;y+=2,p+=VIDEO_LENGTH_X*2-VIDEO_PIXELS_X)
				for (int x=0;x<VIDEO_PIXELS_X;++x)
					*p++=z; // render primary scanlines only
	}
	static VIDEO_UNIT zzz=-1; // first value intentionally invalid!
	if (video_scanlinez!=video_scanline||zzz!=zz) // did the config change?
		if ((video_scanlinez=video_scanline)==1) // do we have to redo the secondary scanlines?
		{
			zzz=zz;
			VIDEO_UNIT *p=video_frame+VIDEO_OFFSET_X+(VIDEO_OFFSET_Y+1)*VIDEO_LENGTH_X;
			for (int y=0;y<VIDEO_PIXELS_Y;y+=2,p+=VIDEO_LENGTH_X*2-VIDEO_PIXELS_X)
				for (int x=0;x<VIDEO_PIXELS_X;++x)
					*p++=zz; // render secondary scanlines only
		}
}

INLINE void audio_playframe(int q,AUDIO_UNIT *ao) // call between frames by the OS wrapper
{
	AUDIO_UNIT aa,*ai=audio_frame; // session_filter[(aa<<8)+az] is 8-bit only and isn't faster than the calculations
	#if AUDIO_STEREO
	static AUDIO_UNIT a0=AUDIO_ZERO,a1=AUDIO_ZERO; // independent channels
	switch (q)
	{
		case 1:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=a0=(aa+a0+(aa>a0))/2,
				aa=*ai++,*ao++=a1=(aa+a1+(aa>a1))/2;
			break;
		case 2:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=a0=(aa+a0*3+(aa>a0)*3)/4,
				aa=*ai++,*ao++=a1=(aa+a1*3+(aa>a1)*3)/4;
			break;
		case 3:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=a0=(aa+a0*7+(aa>a0)*7)/8,
				aa=*ai++,*ao++=a1=(aa+a1*7+(aa>a1)*7)/8;
			break;
	}
	#else
	static AUDIO_UNIT az=AUDIO_ZERO; // single channel
	switch (q)
	{
		case 1:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=az=(aa+az+(aa>az))/2;
			break;
		case 2:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=az=(aa+az*3+(aa>az)*3)/4;
			break;
		case 3:
			for (int i=0;i<AUDIO_LENGTH_Z;++i)
				aa=*ai++,*ao++=az=(aa+az*7+(aa>az)*7)/8;
			break;
	}
	#endif
}

int video_pos_z=0; // for statistics and debugging
INLINE void session_update(void) // render video+audio thru OS and handle realtime logic (self-adjusting delays, automatic frameskip, etc.)
{
	session_render();
	session_signal&=~SESSION_SIGNAL_FRAME; // new frame!
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
	//bitmapheader[0x1C]=24; // cfr. VIDEO_UNIT
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
	waveheader[0x16]=AUDIO_STEREO+1; // channels
	mputiiii(&waveheader[0x18],AUDIO_PLAYBACK); // samples per second
	mputiiii(&waveheader[0x1C],AUDIO_PLAYBACK*sizeof(AUDIO_UNIT)*(AUDIO_STEREO+1)); // bytes per second
	waveheader[0x20]=sizeof(AUDIO_UNIT)*(AUDIO_STEREO+1); // bytes per sample
	waveheader[0x22]=AUDIO_BITDEPTH;
	fwrite(waveheader,1,sizeof(waveheader),session_wavefile);
	return session_wavesize=0;
}
INLINE void session_writewave(AUDIO_UNIT *t) { session_wavesize+=fwrite(t,1,sizeof(audio_buffer),session_wavefile); }
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
			do puff_tgt[puff_tgto++]=puff_tgt[o++]; while (--l);
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
	do puff_tgt[puff_tgto++]=puff_src[puff_srco++]; while (--l); // copy source to target and update pointers
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
	puff_tables(&puff_lcode,t,288); // THERE MUST BE EXACTLY 288 LENGTH CODES!!
	puff_tables(&puff_ocode,t+288,32); // CAN THERE BE AS FEW AS 30 OFFSET CODES??
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
	if (fread(h,1,sizeof(h),puff_file)!=sizeof(h)||memcmp(h,"PK\001\002",4)||h[11]||/*!h[28]||*/h[29])//||(h[8]&8)
		return puff_close(),1; // reject EOFs, unknown IDs, extended types and improperly sized filenames!
	puff_type=h[10];
	puff_srcl=mgetiiii(&h[20]);
	puff_tgtl=mgetiiii(&h[24]);
	//puff_time=mgetiiii(&h[12]); // DOS timestamp
	puff_hash=mgetiiii(&h[16]);
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
		fread1(h,sizeof(h),puff_file);
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
char PUFFCHAR[]="|",puff_path[STRMAX];
FILE *puff_ffile=NULL;
FILE *puff_fopen(char *s,char *m) // mimics fopen(), so NULL on error, *FILE otherwise
{
	if (!s||!m)
		return NULL; // wrong parameters!
	if (puff_ffile&&!strcmp(puff_path,s))
		return fseek(puff_ffile,0,SEEK_SET),puff_ffile; // recycle last file!
	// TODO: handle more than one puff_ffile at once so the file caching is smart
	//if (puff_ffile) fclose(puff_ffile),puff_ffile=NULL; // it's a new file, destroy old one and start anew
	char *z;
	if (!(z=strrstr(s,PUFFCHAR))||!strchr(m,'r'))
		return fopen(s,m); // normal file logic
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
		if (zz=strrstr(rr,PUFFCHAR)) // 'rr' holds the path, does it contain the separator already?
		{
			*zz=0; zz+=strlen(PUFFCHAR);  // *zz++=0; // now it either points to the previous file name or to a zero
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
							//if (!strcasecmp(zz,puff_name))
								//i=l; // select file
							++l;
							z=&session_scratch[sortedinsert(session_scratch,z-session_scratch,puff_name)];//z+=1+sprintf(z,"%s",puff_name);
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
					return session_filedialog_readonly1(),strcpy(session_parmtr,strcat(strcat(rr,PUFFCHAR),session_scratch));
			}
			else if (l) // at least one file?
			{
				i=sortedsearch(session_scratch,z-session_scratch,zz);
				*z++=0; // END OF LIST
				sprintf(z,"Browse archive %s",rr);
				if (session_list(i,session_scratch,z)>=0) // default to latest file if possible
					return session_filedialog_readonly1(),strcpy(session_parmtr,strcat(strcat(rr,PUFFCHAR),session_parmtr));
			}
		}
		if (!(z=(q?session_getfilereadonly(rr,ss,t,f):session_getfile(rr,ss,t)))) // what did the user choose?
			return NULL; // user cancel: give up
		if (strlen(z)<4||strcasecmp(z+strlen(z)-4,&puff_pattern[1]))
			return z; // normal file: accept it
		strcat(strcpy(rr,z),PUFFCHAR); // ZIP archive: append and try again
	}
}
#define puff_session_getfile(x,y,z) puff_session_filedialog(x,y,z,0,0)
#define puff_session_getfilereadonly(x,y,z,q) puff_session_filedialog(x,y,z,1,q)
char *puff_session_newfile(char *x,char *y,char *z) // writing within ZIP archives isn't allowed, so they must be avoided if present
{
	char xx[STRMAX],*zz;
	strcpy(xx,x); // cancelling must NOT destroy the source path
	if (zz=strrstr(xx,PUFFCHAR))
		while (--zz>=xx&&*zz!=PATHCHAR)
			*zz=0; // remove ZIP archive and file within
	return session_newfile(xx,y,z);
}

// on-screen and debug text printing -------------------------------- //

VIDEO_UNIT onscreen_ink0,onscreen_ink1;
#define onscreen_inks(q0,q1) onscreen_ink0=q0,onscreen_ink1=q1

#define ONSCREEN_XY if ((x*=8)<0) x+=VIDEO_OFFSET_X+VIDEO_PIXELS_X; else x+=VIDEO_OFFSET_X; \
	if ((y*=onscreen_size)<0) y+=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y; else y+=VIDEO_OFFSET_Y; \
	int p=y*VIDEO_LENGTH_X+x,a=video_scanline==1?2:1; \
	VIDEO_UNIT q0=q?onscreen_ink1:onscreen_ink0
void onscreen_char(int x,int y,int z,int q) // draw a character
{
	if ((z=(z&127)-32)<0)//if (z=='\t')
		return;
	ONSCREEN_XY,q1=q?onscreen_ink0:onscreen_ink1;
	unsigned const char *zz=&onscreen_chrs[z*onscreen_size];
	for (y=0;y<onscreen_size;++y)
	{
		unsigned char bb=*zz++;
		bb|=bb>>1; // normal, rather than thin or bold
		for (int ww=0;ww<2;ww+=a)
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
	lx*=8; ly*=onscreen_size;
	for (y=0;y<ly;y+=a)
	{
		for (x=0;x<lx;++x)
			video_frame[p++]=q0;
		p+=VIDEO_LENGTH_X*a-lx;
	}
}

#ifdef CONSOLE_DEBUGGER
#else

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
#define KBDBG_SPC_S	160
#define KBDBG_ESCAPE	27
int debug_xlat(int k) // turns non-alphanumeric keypresses into pseudo-ASCII codes
{ switch (k) {
	case KBCODE_LEFT : return KBDBG_LEFT;
	case KBCODE_RIGHT: return KBDBG_RIGHT;
	case KBCODE_UP   : return KBDBG_UP;
	case KBCODE_DOWN : return KBDBG_DOWN;
	case KBCODE_HOME : return KBDBG_HOME;
	case KBCODE_END  : return KBDBG_END;
	case KBCODE_PRIOR: return KBDBG_PRIOR;
	case KBCODE_NEXT : return KBDBG_NEXT;
	case KBCODE_TAB: return session_shift?KBDBG_TAB_S:KBDBG_TAB;
	case KBCODE_SPACE: return session_shift?KBDBG_SPC_S:KBDBG_SPC;//' '
	case KBCODE_BKSPACE: return session_shift?KBDBG_RIGHT:KBDBG_LEFT;
	case KBCODE_X_ENTER: case KBCODE_ENTER: return session_shift?KBDBG_RET_S:KBDBG_RET;
	case KBCODE_ESCAPE: return KBDBG_ESCAPE;
	default: return 0;
} }

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
void onscreen_clear(void) // get a copy of the visible screen
{
	for (int y=0;y<VIDEO_PIXELS_Y;++y)
		memcpy(&debug_frame[y*VIDEO_PIXELS_X],&video_frame[(VIDEO_OFFSET_Y+y)*VIDEO_LENGTH_X+VIDEO_OFFSET_X],sizeof(VIDEO_UNIT)*VIDEO_PIXELS_X);
	if (video_pos_y>=VIDEO_OFFSET_Y&&video_pos_y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-1)
		for (int x=0,z=((video_pos_y&-2)-VIDEO_OFFSET_Y)*VIDEO_PIXELS_X;x<VIDEO_PIXELS_X;++x,++z)
			debug_frame[z]=debug_frame[z+VIDEO_PIXELS_X]^=x&16?VIDEO1(0x00FFFF):VIDEO1(0xFF0000);
	if (video_pos_x>=VIDEO_OFFSET_X&&video_pos_x<VIDEO_OFFSET_X+VIDEO_PIXELS_X-1)
		for (int y=0,z=(video_pos_x&-2)-VIDEO_OFFSET_X;y<VIDEO_PIXELS_Y;++y,z+=VIDEO_PIXELS_X)
			debug_frame[z]=debug_frame[z+1]^=y&2?VIDEO1(0x00FFFF):VIDEO1(0xFF0000);
}
WORD onscreen_grafx_addr=0; BYTE onscreen_grafx_size=1;
WORD onscreen_grafx(int q,VIDEO_UNIT *v,int w,int x,int y); // defined later!
VIDEO_UNIT onscreen_ascii0,onscreen_ascii1;
void onscreen_ascii(int x,int y,int z)
{
	const unsigned char *zz=&onscreen_debug_chrs[((z&127)-32)*onscreen_size];
	VIDEO_UNIT q1,q0;
	if (z&128)
		q1=onscreen_ascii1,q0=onscreen_ascii0;
	else
		q0=onscreen_ascii1,q1=onscreen_ascii0;
	z=(y*VIDEO_PIXELS_X*DEBUG_LENGTH_Z)+x*8+((VIDEO_PIXELS_Y-DEBUG_LENGTH_Y*DEBUG_LENGTH_Z)*VIDEO_PIXELS_X+VIDEO_PIXELS_X-DEBUG_LENGTH_X*8)/2;
	int yy=(onscreen_size-DEBUG_LENGTH_Z)/2;
	for (;yy<0;++yy,z+=VIDEO_PIXELS_X-8)
		for (int w=0;w<8;++w)
			debug_frame[z++]=q0;
	for (;yy<onscreen_size;++yy,z+=VIDEO_PIXELS_X-8)
		for (int w=128,bb=*zz++;w;w>>=1)
			debug_frame[z++]=(w&bb)?q1:q0;
	for (;yy<(onscreen_size+DEBUG_LENGTH_Z)/2;++yy,z+=VIDEO_PIXELS_X-8)
		for (int w=0;w<8;++w)
			debug_frame[z++]=q0;
}
void onscreen_debug(int q) // rewrite debug texts or redraw graphics
{
	static int videox=-1,videoy=-1,videoz=-1;
	if (videox!=video_pos_x||videoy!=video_pos_y||videoz!=video_pos_z)
		videox=video_pos_x,videoy=video_pos_y,onscreen_clear(),videoz=video_pos_z; // flush background if required!
	if (onscreen_debug_mask&1) // normal or inverse?
		onscreen_ascii1=VIDEO1(0xFFFFFF),onscreen_ascii0=VIDEO1(0x000000);
	else
		onscreen_ascii0=VIDEO1(0xFFFFFF),onscreen_ascii1=VIDEO1(0x000000);
	if ((onscreen_debug_mask_^onscreen_debug_mask)&~1)
		switch ((onscreen_debug_mask_=(onscreen_debug_mask&=7))&6) // font styles
		{
			default:
				for (int i=0,j;i<sizeof(onscreen_debug_chrs);++i)
					j=onscreen_chrs[i],onscreen_debug_chrs[i]=j|(j>>1); // normal
				break;
			case 2:
				memcpy(onscreen_debug_chrs,onscreen_chrs,sizeof(onscreen_chrs)); // thin
				break;
			case 4:
				for (int i=0,j;i<sizeof(onscreen_debug_chrs);++i)
					j=onscreen_chrs[i],onscreen_debug_chrs[i]=j|((2*i/onscreen_size)&1?(j<<1):(j>>1)); // italic
				break;
			case 6:
				for (int i=0,j;i<sizeof(onscreen_debug_chrs);++i)
					j=onscreen_chrs[i],onscreen_debug_chrs[i]=j|(j>>1)|(j<<1); // bold
				break;
		}
	unsigned char *t=debug_buffer;
	if (q>=0)
	{
		int z=onscreen_grafx(q,&debug_frame[VIDEO_PIXELS_X*(VIDEO_PIXELS_Y-DEBUG_LENGTH_Y*DEBUG_LENGTH_Z)/2+(VIDEO_PIXELS_X-DEBUG_LENGTH_X*8)/2],
			VIDEO_PIXELS_X,DEBUG_LENGTH_X*8,DEBUG_LENGTH_Y*DEBUG_LENGTH_Z);
		sprintf(t,"%04X...%04X %c%03iH: help  W: exit",onscreen_grafx_addr&0xFFFF,z&0xFFFF,(q&1)?'V':'H',onscreen_grafx_size);
		int w=strlen(t)/2;
		for (int y=-2;y<0;++y)
			for (int x=-w;x<0;++x)
				onscreen_ascii(DEBUG_LENGTH_X+x,DEBUG_LENGTH_Y+y,*t++);
	}
	else
		for (int y=0;y<DEBUG_LENGTH_Y;++y)
			for (int x=0;x<DEBUG_LENGTH_X;++x)
				onscreen_ascii(x,y,*t++);
}
#endif

// extremely primitive video+audio output! -------------------------- //

FILE *session_filmfile=NULL; unsigned int session_nextfilm=1,session_filmfreq,session_filmcount;
BYTE session_filmflag,session_filmscale=2,session_filmtimer=2,session_filmalign; // format options
#define SESSION_FILMVIDEO_LENGTH (VIDEO_PIXELS_X*VIDEO_PIXELS_Y) // copy of the previous video frame
#define SESSION_FILMAUDIO_LENGTH (AUDIO_LENGTH_Z*2*(AUDIO_STEREO+1)) // copies of TWO audio frames
VIDEO_UNIT *session_filmvideo=NULL; AUDIO_UNIT session_filmaudio[SESSION_FILMAUDIO_LENGTH];
BYTE *xrf_chunk=NULL; // this buffer contains one video frame and two audio frames AFTER encoding

#define xrf_encode1(n) ((n)&&(*z++=(n),a+=b),!(b>>=1)&&(*y=a,y=z++,a=0,b=128)) // write "0" (zero) or "1nnnnnnnn" (nonzero)
int xrf_encode(BYTE *t,BYTE *s,int l,int x) // terribly hacky encoder based on an 8-bit RLE and a pseudo Huffman filter!
{
	if (l<=0) return *t++=128,*t++=0,2; // quick EOF!
	BYTE *z=t,*y=z++,a=0,b=128;
	do
	{
		BYTE *r=s,c=*r;
		do
			s+=x;
		while (--l&&c==*s);
		int n=(s-r)/x;
		while (n>0x101FF) // i.e. 66047 = 512+65535
			xrf_encode1(c),xrf_encode1(c),xrf_encode1(-1),xrf_encode1(-1),xrf_encode1(-1),n-=0x101FF;
		xrf_encode1(c); if (n>1)
		{
			xrf_encode1(c); if (n>=512)
				xrf_encode1(-1),n-=512,xrf_encode1(n>>8),xrf_encode1(n&255); // 512..66047 -> 0xFF 0x0000..0xFFFF
			else if (n>=256)
				xrf_encode1(-2),xrf_encode1(n&255); // 256..511 -> 0xFE 0x00..0xFF
			else
				xrf_encode1(n-2); // 2..256 -> 0x00..0xFE
		}
	}
	while (l);
	return *y=a+b,*z++=0,z-t; // EOF: "100000000"
}

int session_createfilm(void) // start recording video and audio; !0 ERROR
{
	if (session_filmfile) // file already open!
		return 1;
	if (session_filmscale<1||session_filmscale>2)
		return 1; // improper scale value!
	if (session_filmtimer<1||session_filmtimer>2)
		return 1; // improper timer value!

	if (!session_filmvideo&&!(session_filmvideo=malloc(sizeof(VIDEO_UNIT)*SESSION_FILMVIDEO_LENGTH)))
		return 1; // cannot allocate buffer!
	if (!xrf_chunk&&!(xrf_chunk=malloc((sizeof(VIDEO_UNIT)*SESSION_FILMVIDEO_LENGTH+sizeof(AUDIO_UNIT)*SESSION_FILMAUDIO_LENGTH)*9/8+4*8))) // maximum pathological length!
		return 1; // cannot allocate memory!

	if (!(session_nextfilm=session_savenext("%s%08i.xrf",session_nextfilm))) // "Xor-Rle Film"
		return 1; // too many files!
	if (!(session_filmfile=fopen(session_parmtr,"wb")))
		return 1; // cannot create file!
	fwrite1("XRF1!\015\012\032",8,session_filmfile); // XRF-1 was limited to 8-bit RLE lengths, and XRF+1 didn't store the amount of frames!
	fputmm(VIDEO_PIXELS_X/session_filmscale,session_filmfile);
	fputmm(VIDEO_PIXELS_Y/session_filmscale,session_filmfile);
	fputmm(session_filmfreq=audio_disabled?0:AUDIO_LENGTH_Z*session_filmtimer,session_filmfile);
	fputc(VIDEO_PLAYBACK/session_filmtimer,session_filmfile);
	fputc(session_filmflag=sizeof(AUDIO_UNIT)-1+AUDIO_STEREO*2,session_filmfile); // +16BITS(1)+STEREO(2)
	fputmmmm(-1,session_filmfile); // to be filled later
	session_filmalign=video_pos_y; // catch scanline mode, if any
	return session_recording=1,MEMZERO(session_filmvideo),session_filmcount=0;
}
void session_writefilm(void) // record one frame of video and audio
{
	if (!session_filmfile) return; // file not open!

	BYTE *z=xrf_chunk; static BYTE dirty=0;
	if (!video_framecount)
		dirty=1; // frameskipping?
	if (!(++session_filmcount%session_filmtimer))
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
			VIDEO_UNIT *t=session_filmvideo;
			for (int y=VIDEO_OFFSET_Y+(session_filmalign%session_filmscale);y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;y+=session_filmscale)
			{
				VIDEO_UNIT *s=&video_frame[y*VIDEO_LENGTH_X+VIDEO_OFFSET_X];
				for (int x=session_filmscale-1;x<VIDEO_PIXELS_X;x+=session_filmscale)
					*t++^=s[x];
			}
			#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				z+=xrf_encode(z,&((BYTE*)session_filmvideo)[3],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // B
				z+=xrf_encode(z,&((BYTE*)session_filmvideo)[2],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // G
				z+=xrf_encode(z,&((BYTE*)session_filmvideo)[1],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // R
				//z+=xrf_encode(z,&((BYTE*)session_filmvideo)[0],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // A!
			#else
				z+=xrf_encode(z,&((BYTE*)session_filmvideo)[0],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // B
				z+=xrf_encode(z,&((BYTE*)session_filmvideo)[1],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // G
				z+=xrf_encode(z,&((BYTE*)session_filmvideo)[2],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // R
				//z+=xrf_encode(z,&((BYTE*)session_filmvideo)[3],VIDEO_PIXELS_X/session_filmscale*VIDEO_PIXELS_Y/session_filmscale,4); // A!
			#endif
			t=session_filmvideo;
			for (int y=VIDEO_OFFSET_Y+(session_filmalign%session_filmscale);y<VIDEO_OFFSET_Y+VIDEO_PIXELS_Y;y+=session_filmscale)
			{
				VIDEO_UNIT *s=&video_frame[y*VIDEO_LENGTH_X+VIDEO_OFFSET_X];
				for (int x=session_filmscale-1;x<VIDEO_PIXELS_X;x+=session_filmscale)
					*t++=s[x];
			}
		}
		if (session_filmfreq) // audio?
		{
			BYTE *s=(BYTE*)audio_frame; int l=AUDIO_LENGTH_Z;
			if (session_filmtimer>1)
				s=(BYTE*)session_filmaudio,l*=2,memcpy(&session_filmaudio[AUDIO_LENGTH_Z*(AUDIO_STEREO+1)],audio_frame,sizeof(AUDIO_UNIT)*AUDIO_LENGTH_Z*(AUDIO_STEREO+1)); // glue both blocks!
			#if AUDIO_STEREO
			#if AUDIO_BITDEPTH > 8
				#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				z+=xrf_encode(z,&s[1],l,4); // l
				z+=xrf_encode(z,&s[0],l,4); // L
				z+=xrf_encode(z,&s[3],l,4); // r
				z+=xrf_encode(z,&s[2],l,4); // R
				#else
				z+=xrf_encode(z,&s[0],l,4); // l
				z+=xrf_encode(z,&s[1],l,4); // L
				z+=xrf_encode(z,&s[2],l,4); // r
				z+=xrf_encode(z,&s[3],l,4); // R
				#endif
			#else
				z+=xrf_encode(z,&s[0],l,2); // L
				z+=xrf_encode(z,&s[1],l,2); // R
			#endif
			#else
			#if AUDIO_BITDEPTH > 8
				#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				z+=xrf_encode(z,&s[1],l,2); // m
				z+=xrf_encode(z,&s[0],l,2); // M
				#else
				z+=xrf_encode(z,&s[0],l,2); // m
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
		memcpy(session_filmaudio,audio_frame,sizeof(AUDIO_UNIT)*AUDIO_LENGTH_Z*(AUDIO_STEREO+1)); // keep audio block for later!
	session_filmalign=video_pos_y;
}
int session_closefilm(void) // stop recording video and audio; !0 ERROR
{
	if (session_filmvideo)
		free(session_filmvideo),session_filmvideo=NULL;
	if (xrf_chunk)
		free(xrf_chunk),xrf_chunk=NULL;
	if (!session_filmfile)
		return 1;
	fseek(session_filmfile,16,SEEK_SET);
	fputmmmm(session_filmcount/session_filmtimer,session_filmfile); // number of frames
	fclose(session_filmfile);
	session_filmfile=NULL;
	return session_recording=0;
}

// =================================== END OF OS-INDEPENDENT ROUTINES //
