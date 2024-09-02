 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// Tapes used in the Amstrad CPC are effectively equivalent to those
// from other home computers and thus it's convenient to split the code
// and let this module benefit from the universality of WAV, CSW, and
// TZX/CDT file formats and from its platform-independent nature.

// Supported file formats are WAV1 (uncompressed PCM audio file), CSW1
// (run-length compressed square wave) and TZX1 (minus the CSW2 block);
// TAP, CAS and PZX1 tape files are handled as special cases of TZX1.

// BEGINNING OF TAPE SUPPORT ======================================== //

char tape_path[STRMAX]="";

FILE *tape=NULL;
INT8 tape_type; // -1 RECORD, 0 WAV/NULL, 1 CSW, 2 TZX, 3 TAP/CAS, 4 PZX
INT8 tape_signal; // signal set by the tape playback if something happens: >0 STOP, <0 END!
int tape_filetell,tape_filesize,tape_filebase; // tape file offset, length and base
int tape_playback,tape_step; // tape signal frequency and unit step
char tape_rewind=0; // option: does the tape rewind after running out?
char tape_status,tape_output,tape_record; // tape input and output values: 0 or 1, nothing else
char tape_polarity=0; // do we need to invert the value of tape_status? yes (1) or no (0)
char tape_feedable; // does the current block allow tape speedup operations? y(1) / n(0)
int tape_seekcccc; // sanity check! for example BLOCK $19 may carry dummy data!

int tape_t,tape_n,tape_heads,tape_tones,tape_datas,tape_waves,tape_tails,tape_loops,tape_loop0,tape_calls,tape_call0;
#ifdef TAPE_TZX_KANSAS
int tape_kansas,tape_kansas0n,tape_kansas1n,tape_kansasin,tape_kansasi,tape_kansason,tape_kansaso,tape_kansasrl;
#endif

#ifndef TAPE_MAIN_TZX_EXP // 1<<N ticks per packet; make it too big and it will break tapes!
#define TAPE_MAIN_TZX_EXP 5 // 5 is the highest value that doesn't lose bits with 3500000>>N
#endif // the higher N, the lighter the TZX engine, but also the riskier (undersampling!)

#define tape_safetydelay() ((tape_type>=2)&&(tape_step=2000)) // safety delay in milliseconds

// tape file handling operations ------------------------------------ //

BYTE tape_buffer[1<<12]; int tape_offset,tape_length; // tape buffer

int tape_seek(int i) // moves file cursor to `i`, cancels the cache if required
{
	if (i<0) i=0; else if (i>tape_filesize) i=tape_filesize; // sanitize
	int j=i-tape_filetell+tape_offset; if (j>=0&&j<tape_length) // within cache?
		return tape_offset=j,tape_filetell=i;
	return fseek(tape,i,SEEK_SET),tape_offset=tape_length=0,tape_filetell=i;
}
int tape_skip(int i) { return tape_seek((i)+tape_filetell); } // avoid accidents with macros!
#define tape_undo() (--tape_offset,--tape_filetell) // undo tape_getc(), use carefully
int tape_getc(void) // returns the next byte in the file, or -1 on EOF
{
	if (tape_offset>=tape_length)
		if ((tape_offset=0)>=(tape_length=fread1(tape_buffer,sizeof(tape_buffer),tape)))
			return -1; // EOF
	return ++tape_filetell,tape_buffer[tape_offset++];
}
int tape_getcc(void) // returns an Intel-style WORD; see tape_getc()
	{ int i; if ((i=tape_getc())>=0) i|=tape_getc()<<8; return i; }
int tape_getccc(void) // returns an Intel-style 24-bit; see tape_getc()
	{ int i; if ((i=tape_getc())>=0) if ((i|=tape_getc()<<8)>=0) i|=tape_getc()<<16; return i; }
int tape_getcccc(void) // returns an Intel-style DWORD; mind the sign in 32bits!
	{ int i; if ((i=tape_getc())>=0) if ((i|=tape_getc()<<8)>=0) if ((i|=tape_getc()<<16)>=0) i|=tape_getc()<<24; return i; }

void tape_putc(BYTE i) // sends a byte to the buffer
{
	if (tape_buffer[tape_offset]=i,++tape_offset>=sizeof(tape_buffer))
		fwrite1(tape_buffer,sizeof(tape_buffer),tape),tape_offset=0;
}
void tape_putcccc(int i) // sends an Intel-style DWORD to the buffer
	{ tape_putc(i); tape_putc(i>>8); tape_putc(i>>16); tape_putc(i>>24); }
void tape_flush(void) // sends the final recorded samples in CSW1-style
{
	if (tape_n>=256)
		tape_putc(0),tape_putcccc(tape_n);
	else if (tape_n>0) // zero is useless, it only happens when the recording begins
		tape_putc(tape_n);
	tape_record=tape_output,tape_n=0;
}

// tape file handling operations ------------------------------------ //

char tape_header_csw1[24]="Compressed Square Wave\032\001"; // mind the end marker!
char tape_header_tzx1[9]="ZXTape!\032\001"; // the tenth byte is missing on purpose
#ifdef TAPE_CAS_FORMAT
char tape_header_cas1[8]="\037\246\336\272\314\023\175\164"; // CAS uses this string as a block separator
#endif

#define tape_setup() tape_close()
#define tape_reset() ((void)0)
int tape_close(void) // closes the tape file, if any; 0 OK, !0 ERROR
{
	if (tape)
	{
		if (tape_type<0) tape_flush(),fwrite1(tape_buffer,tape_offset,tape);
		puff_fclose(tape);
	}
	tape=NULL; return tape_status=tape_output=tape_filesize=tape_filetell=tape_offset=tape_length=
		tape_t=tape_n=tape_heads=tape_tones=tape_datas=
		#ifdef TAPE_TZX_KANSAS
		tape_kansas=
		#endif
		tape_waves=tape_tails=tape_calls=tape_loops=tape_type=tape_feedable=tape_seekcccc=0;
}

int tape_open(char *s) // opens a tape file `s` for input; 0 OK, !0 ERROR
{
	tape_close(); if (!(tape=puff_fopen(s,"rb")))
		return -1; // cannot open tape!
	fseek(tape,0,SEEK_END); tape_filesize=ftell(tape); fseek(tape,tape_filetell=0,SEEK_SET);
	if (tape_getc()<0)
		return tape_close(),-1; // tape is empty!
	if (tape_undo(),equalsiiii(tape_buffer,0X46464952)&&equalsiiii(&tape_buffer[8],0X45564157)) // WAVE: "RIFF" + "WAVE" IDs
	{
		tape_seek(12); tape_playback=tape_type=0;
		int k,l; while ((k=tape_getcccc())>=0&&(l=tape_getcccc())>0)
		{
			if (k==0X20746D66&&l>=16) // "fmt " ID and valid size
			{
				tape_getcccc();
				tape_step=tape_getcccc();
				tape_step/=((tape_playback=tape_getcccc())+1);
				tape_call0=(tape_getcccc()&0X100000)?128:0; // signed 16-bit versus unsigned 8-bit
				tape_skip(((l+1)&~1)-16); // RIFF even-padding
			}
			else if (k==0X61746164) // "data" ID; audio starts here
				break;
			else
				tape_skip((l+1)&~1); // RIFF even-padding
		}
		if (tape_playback<1)
			return tape_close(),1; // improper WAVE!
	}
	else if (!memcmp(tape_buffer,tape_header_csw1,24)) // CSW: CSW1 header
	{
		tape_seek(24),tape_type=1,tape_playback=(tape_getcccc()>>8)-65536;
		tape_step=tape_call0=~tape_getcccc()&1; // remember first signal, we'll need it later
	}
	else if (!memcmp(tape_buffer,tape_header_tzx1,9)) // TZX: TZX1 header
		tape_type=2,tape_playback=3500000>>TAPE_MAIN_TZX_EXP,tape_seek(10);
	#ifdef TAPE_TAP_FORMAT
	else if (equalsiiii(tape_buffer,0X00000013)||equalsiiii(tape_buffer,0X03000013)) // TAP: BASIC/binary headers
		tape_type=3,tape_playback=3500000>>TAPE_MAIN_TZX_EXP;
	#else
	#ifdef TAPE_CAS_FORMAT
	else if (!memcmp(tape_buffer,tape_header_cas1,8)) // CAS: CALL $00E1 separator
		tape_type=3,tape_playback=3500000>>TAPE_MAIN_TZX_EXP;
	#endif
	#endif
	else if (equalsiiii(tape_buffer,0X54585A50)&&!tape_buffer[7]) // PZX: "PZXT" ID + valid length
		tape_type=4,tape_playback=3500000>>TAPE_MAIN_TZX_EXP;
	else
		return tape_close(),1; // unknown file!
	STRCOPY(tape_path,s); // valid format
	return tape_filebase=tape_filetell,tape_safetydelay(),0;
}
int tape_create(char *s) // creates a tape file `s` for output; 0 OK, !0 ERROR
{
	tape_close(); if (!(tape=puff_fopen(s,"wb")))
		return 1; // cannot create tape!
	fwrite(tape_header_csw1,1,24,tape); // CSW1 format
	tape_offset=0; tape_putcccc(((tape_playback=44100)<<8)+0x1000001); tape_putcccc(tape_record=0); // 44100Hz, starting status = 0
	STRCOPY(tape_path,s); // why not?
	return tape_type=-1,tape_filesize=tape_filetell=tape_n=tape_output=0; // reset buffer and format
}

int tape_tzx1size(int i) // reads some bytes, if required, then returns its expected size; `i` is the current TZX block ID
{
	switch (i)
	{
		case 0x10: // NORMAL DATA
			return tape_getcc(),tape_getcc();
		case 0x11: // CUSTOM DATA
			return tape_skip(2+2+2+2+2+2+1+2),tape_getccc();
		case 0x12: // PURE TONE
			return tape_skip(2+2),0;
		case 0x13: // PURE SYNC
			return tape_getc()*2;
		case 0x14: // PURE DATA
			return tape_skip(2+2+1+2),tape_getccc();
		case 0x15: // SAMPLES
			return tape_skip(2+2+1),tape_getccc();
		//case 0x19: // GENERAL DATA
			//return tape_getcccc();
		case 0x20: // HOLD
			return tape_getcc(),0;
		case 0x2A: // STOP ON 48K
			return tape_getcccc(),0;
		case 0x21: // GROUP START
			return tape_getc();
		case 0x22: // GROUP END
			return 0;
		case 0x23: // !JUMP TO BLOCK
			return tape_getcc(),0;
		case 0x24: // LOOP START
			return tape_getcc(),0;
		case 0x25: // LOOP END
			return 0;
		case 0x26: // !CALL SEQUENCE
			return tape_getcc()*2;
		case 0x27: // !RETURN FROM CALL
			return 0;
		case 0x28: // !SELECT BLOCK
			return tape_getcc();
		case 0x31: // *MESSAGE BLOCK
			tape_getc(); // no `break`!
		case 0x30: // TEXT
			return tape_getc();
		case 0x32: // *ARCHIVE INFO
			return tape_getcc();
		case 0x33: // *HARDWARE TYPE
			return tape_getc()*3;
		case 0x34: // *EMULATION INFO
			return tape_skip(8),0;
		case 0x35: // *CUSTOM INFO
			return tape_skip(16),tape_getcccc();
		case 0x40: // *SNAPSHOT INFO
			return tape_getcccc()>>8;
		case 0x5A: // *GLUE
			return tape_skip(9),0;
		default: // *UNKNOWN
			return tape_getcccc();
	}
}
int tape_tzx1tell(void) // tells which TZX block in the tape we're in, 0 first
{
	int z=tape_filetell; tape_seek(tape_filebase);
	int p=-1,i; while (tape_filetell<z&&(i=tape_getc())>=0) ++p,tape_skip(tape_tzx1size(i));
	tape_seek(z); return p;
}
int tape_tzx1seek(int p) // seeks the TZX block specified by `p` (0: first)
{
	int i; tape_seek(tape_filebase);
	while (p>0&&(i=tape_getc())>=0) --p,tape_skip(tape_tzx1size(i));
	return tape_filetell;
}

#define TAPE_CATALOG_HEAD "%010d -- "
void tape_catalog_string(char **t,int i) // send `i` chars to ASCIZ string `*t` and purge invisible chars (f.e. SPEEDKING.CDT)
{
	for (int j;i>0;--i) if ((j=tape_getc())>=32) *(*t)++=j;
	*(*t)++=0;
}
#ifdef TAPE_CAS_FORMAT
int tape_catalog_cas_t; // -1 = dummy first entry
int tape_catalog_cas_flush(char *t,int j)
{
	if (tape_catalog_cas_t<0) { tape_catalog_cas_t=j; return 0; } // first time?
	j=j-tape_catalog_cas_t; tape_catalog_cas_t+=j; // just avoiding a temporary variable
	return 1+sprintf(t,TAPE_CATALOG_HEAD "KANSAS CITY DATA, %d bytes",tape_catalog_cas_t-j,j-8); // exclude the 8-byte ID
}
#endif

int tape_catalog(char *t,int x) // fills the buffer `t` of size `x` with the tape contents as an ASCIZ list; returns <0 ERROR, >=0 CURRENT BLOCK
{
	if (!tape||tape_type<0) return *t=0,-1;
	int i,j,p=-1; // bear in mind that the lowest valid offset must be equal or higher than the first listed block
	if (tape_type<2) // WAV/CSW?
	{
		int z=tape_type?-1:(-tape_step-1); // WAV granularity depends on sample width
		for (i=0;i<100;i+=4)
		{
			j=tape_filebase+(((long long int)i*(tape_filesize-tape_filebase)/100)&z);
			if (j<=tape_filetell) ++p;
			t+=1+sprintf(t,TAPE_CATALOG_HEAD "SAMPLES %02d%%",j,i);
		}
	}
	else // TZX/TAP/CAS/PZX?
	{
		char *s=&t[x-STRMAX],*u=NULL;
		int z=tape_filetell,l; i=-1;
		tape_seek(tape_filebase);
		#ifdef TAPE_TAP_FORMAT
		if (tape_type==3) // TAP
			while (t<s&&(i=tape_getcc())>0)
			{
				t+=1+sprintf(t,TAPE_CATALOG_HEAD "NORMAL DATA, %d bytes",j=tape_filetell-2,i);
				if (j<=z) ++p; // locate current block
				tape_skip(i);
			}
		else
		#else
		#ifdef TAPE_CAS_FORMAT
		if (tape_type==3) // CAS
		{
			tape_catalog_cas_t=-1; // the CAS format lacks a true block structure :-(
			while (t<s&&(i=tape_getc())>=0)
			{
				tape_undo();
				if (!memcmp(&tape_buffer[tape_offset],tape_header_cas1,8)) // spying the cache = cheating!
				{
					t+=tape_catalog_cas_flush(t,tape_filetell);
					if (tape_filetell<=z) ++p; // locate current block
				}
				tape_skip(8);
			}
			t+=tape_catalog_cas_flush(t,tape_filetell); // print the last entry; `t<s` may be a liability in extremely big tapes
		}
		else
		#endif
		#endif
		if (tape_type==4) // PZX
		{
			while (t<s&&(i=tape_getcccc())>0&&(l=tape_getcccc())>=0)
			{
				t+=sprintf(t,TAPE_CATALOG_HEAD "'%c%c%c%c' BLOCK",j=tape_filetell-8,i,i>>8,i>>16,i>>24);
				if (i==0X54585A50||i==0X53554150) // "PZXT", "PAUS"
					++t;
				else if (i==0X41544144||i==0X534C5550) // "DATA", "PULS"
					t+=1+sprintf(t,", %d bytes",l);
				else if (i==0X53575242&&l<256) // "BRWS"
					t-=5,t+=sprintf(t,"GROUP: "),tape_catalog_string(&t,l),l=0,u=t;
				else if (i==0X42525753||i==0X504F5453) // "SWRB", "STOP" (!)
					{ if (u) t=u,u=NULL; }
				else
					++t;
				if (j<=z&&!u) ++p; // locate current block
				tape_skip(l); l=-1;
				if (u) t=u; // keep block invisible
			}
		}
		else // TZX/CDT
			while (t<s&&(i=tape_getc())>0)
			{
				t+=sprintf(t,TAPE_CATALOG_HEAD "",j=tape_filetell-1);
				if (j<=z&&!u) ++p; // locate current block
				switch (i)
				{
					case 0x10: t+=1+sprintf(t,"NORMAL DATA");
						break;
					case 0x11: t+=1+sprintf(t,"CUSTOM DATA");
						break;
					case 0x12: t+=1+sprintf(t,"PURE TONE");
						break;
					case 0x13: t+=1+sprintf(t,"PURE SYNC");
						break;
					case 0x14: t+=1+sprintf(t,"PURE DATA");
						break;
					case 0x15: t+=1+sprintf(t,"SAMPLES");
						break;
					case 0x19: t+=1+sprintf(t,"GENERAL DATA");
						break;
					case 0x20: t+=1+sprintf(t,"HOLD");
						break;
					case 0x2A: t+=1+sprintf(t,"STOP ON 48K");
						break;
					case 0x2B: t+=1+sprintf(t,"SET SIGNAL LEVEL");
						break;
					case 0x21: t+=sprintf(t,"GROUP: "); // GROUP START
						tape_catalog_string(&t,tape_getc()); i=0;
						u=t;
						break;
					case 0x22: if (u) t=u,u=NULL; // t+=1+sprintf(t,"GROUP END");
						break;
					case 0x23: t+=1+sprintf(t,"JUMP TO BLOCK");
						break;
					case 0x24: t+=1+sprintf(t,"LOOP START");
						break;
					case 0x25: t+=1+sprintf(t,"LOOP END");
						break;
					case 0x26: t+=1+sprintf(t,"CALL SEQUENCE");
						break;
					case 0x27: t+=1+sprintf(t,"RETURN FROM CALL");
						break;
					case 0x28: t+=1+sprintf(t,"SELECT BLOCK");
						break;
					case 0x31: // *MESSAGE BLOCK
						tape_getc(); // no `break`!
					case 0x30: t+=sprintf(t,"TEXT: ");
						tape_catalog_string(&t,tape_getc()); i=0;
						break;
					case 0x32: t+=1+sprintf(t,"ARCHIVE INFO");
						break;
					case 0x33: t+=1+sprintf(t,"HARDWARE TYPE");
						break;
					case 0x34: t+=1+sprintf(t,"EMULATION INFO");
						break;
					case 0x35: t+=1+sprintf(t,"CUSTOM INFO");
						break;
					case 0x40: t+=1+sprintf(t,"SNAPSHOT INFO");
						break;
					#ifdef TAPE_TZX_KANSAS
					case 0x4B: t+=1+sprintf(t,"KANSAS CITY DATA");
						break;
					#endif
					case 0x5A: t+=1+sprintf(t,"GLUE");
						break;
					default: t+=1+sprintf(t,"UNKNOWN!");
				}
				tape_skip(l=i>0?tape_tzx1size(i):0); i=-1;
				if (l) --t,t+=1+sprintf(t,", %d bytes",l);
				if (u) t=u; // keep block invisible
			}
		if (i>=0) // tape is too long :-(
		{
			t+=1+sprintf(t,TAPE_CATALOG_HEAD "&c.",j=tape_filetell);
			if (j<=z)
				++p;
		}
		tape_seek(z);
	}
	//t+=1+sprintf(t,TAPE_CATALOG_HEAD "END OF TAPE",tape_filesize);
	return *t=0,p;
}

// tape playback/record operations ---------------------------------- //

int tape_headcode[256*2]; // "loop+size" pairs, f.e. "8064,2168"
int tape_head,tape_tail,tape_time; // head+tail item+loop counts
int tape_tonecodes,tape_toneitems,tape_datacodes,tape_dataitems;
short tape_codeitem[256][32]; // are there tapes THIS complex!?
short *tape_code; int tape_item,tape_mask,tape_bits,tape_byte;

// TZX config values and calls
int tape_tzxpilot,tape_tzxpilots,tape_tzxsync1,tape_tzxsync2,tape_tzxbit0,tape_tzxbit1,tape_tzxhold;
void tape_tzx20(void) // TZX BLOCK $20 et al. sets the "tails"
	{ tape_tail=0; tape_tails=3500*tape_tzxhold; } // 1 ms = 3500 T
void tape_tzx14(int l) // TZX BLOCK $14 et al. sets the "datas" and the "tails"
{
	tape_datacodes=tape_time=tape_item=0;
	tape_mask=tape_bits=tape_feedable=1; // eight 1-bit codes per byte
	if (tape_datas) tape_datas-=8; // dodgy tapes may have zero final bits!
	tape_datas+=l*8;
	tape_codeitem[0][0]=tape_codeitem[1][0]=0;
	tape_codeitem[0][1]=tape_codeitem[0][2]=tape_tzxbit0;
	tape_codeitem[1][1]=tape_codeitem[1][2]=tape_tzxbit1;
	tape_codeitem[0][3]=tape_codeitem[1][3]=-1;
	tape_tzx20();
}
void tape_tzx12(void) // TZX BLOCK $12 et al. sets the "heads"
{
	tape_time=tape_head=tape_heads=0;
	if (tape_tzxpilot&&tape_tzxpilots)
		tape_headcode[0]=tape_tzxpilots,tape_headcode[1]=tape_tzxpilot,tape_heads=1;
	if (tape_tzxsync1&&tape_tzxsync2) // can SYNC2 be zero when SYNC1 isn't!?
		tape_headcode[tape_heads*2]=1,tape_headcode[tape_heads*2+1]=tape_tzxsync1,++tape_heads,
		tape_headcode[tape_heads*2]=1,tape_headcode[tape_heads*2+1]=tape_tzxsync2,++tape_heads;
}
void tape_tzx11(int l) // TZX BLOCK $11 et al. sets "heads, datas and tails"
	{ tape_tzx12(); tape_tzx14(l); }
void tape_tzx10(int q,int l) // TZX BLOCK $10 is a special case of BLOCK $11
{
	tape_tzxpilot=2168; tape_tzxpilots=q?3222:8062;
	tape_tzxsync1=667; tape_tzxsync2=735;
	tape_tzxbit0=855; tape_tzxbit1=1710;
	tape_datas=8; tape_tzx11(l);
}
void tape_tzx19(int n,int m) // build the code-item table
{
	for (int i=0,j;i<n;++i)
	{
		tape_codeitem[i][0]=tape_getc();
		for (j=1;j<=m;++j)
			if ((tape_codeitem[i][j]=tape_getcc())<1)
				tape_codeitem[i][j]=-1; // ZERO is an early end marker in TZX
		tape_codeitem[i][j]=-1; // normal end marker
	}
}
void tape_firstbit(int m) // set the status according to BLOCK $19 flags
	{ if (m&2) tape_status=(m&1)^tape_polarity; else if (!(m&1)) tape_status^=1; }
void tape_eofmet(void) // rewinds the tape or stops it depending on options
{
	if (tape_type==1) tape_step=tape_call0; // keep CSW playback safe
	tape_safetydelay(); tape_signal=-1; // signal that we met EOF
	if (tape_rewind) tape_seek(tape_filebase);
}
void tape_select(int i) // seeks the position `i` in the tape input
{
	if (tape_type==1) tape_step=(i^tape_call0)&1; // keep CSW playback safe
	tape_status=tape_polarity;
	tape_seek(i); tape_t=tape_n=tape_heads=tape_tones=tape_datas=
	#ifdef TAPE_TZX_KANSAS
	tape_kansas=
	#endif
	tape_waves=tape_tails=tape_calls=tape_loops=tape_seekcccc=0; tape_safetydelay();
}

void tape_main(int t) // plays tape back for `t` ticks; t must be >0!
{
	// *!* work around overflows without `long long`
	int p=(tape_t+=(t*tape_playback))/TICKS_PER_SECOND;
	if (p>0) switch (tape_t%=TICKS_PER_SECOND,tape_type)
	{
		case -1: // RECORD to CSW
			if (tape_record!=tape_output) tape_flush();
			tape_n+=p;
			break;
		case 0: // WAV
			tape_skip((p-1)*(tape_step+1)+tape_step);
			if ((p=tape_getc())<0) // EOF?
				{ tape_eofmet(); return; }
			tape_status=tape_polarity?(p^tape_call0)<128:(p^tape_call0)>128;
			break;
		case 1: // CSW
			do
				if (--tape_n<=0) // this only falls below zero after init and seek!
					if ((tape_status=(tape_step^=1)^tape_polarity),(tape_n=tape_getc())<=0)
						if ((tape_n=tape_getcccc())<=0) // EOF or ZERO!
							{ tape_eofmet(); return; }
			while (--p);
			break;
		case 2: // TZX
		#ifdef TAPE_TAP_FORMAT
		case 3: // TAP
		#else
		#ifdef TAPE_CAS_FORMAT
		case 3: // CAS
		#endif
		#endif
		case 4: // PZX
			tape_n-=p<<TAPE_MAIN_TZX_EXP; // "flush" the "bucket"
			int watchdog=99; // the watchdog catches corrupted tapes!!
			while (tape_n<=0)
				if (tape_heads) // predef'd head
				{
					if (!tape_time)
					{
						if (tape_type==4) // PZX
						{
							if (!tape_head) // cfr. BASIL.PZX
								tape_status=!tape_polarity;
							tape_head=tape_time=1;
							if ((p=tape_getcc())>32768) // not ">="!!
								--tape_heads,tape_time=p-32768,p=tape_getcc();
							if (p>=32768)
								--tape_heads,p=((p-32768)<<16)+tape_getcc();
							tape_headcode[1]=p;
						}
						else // TZX/TAP/CAS
							tape_time=tape_headcode[tape_head++];
					}
					tape_status^=1; tape_n+=tape_headcode[tape_head];
					if (!--tape_time)
						++tape_head,--tape_heads;
				}
				else if (tape_tones) // encoded tones
				{
					if (tape_tonecodes) // build table?
					{
						tape_tzx19(tape_tonecodes,tape_toneitems);
						tape_tonecodes=tape_time=0;
					}
					if (!tape_time) // new tone?
						tape_code=tape_codeitem[tape_byte=tape_getc()],tape_time=tape_getcc(),tape_item=0;
					if (!tape_item) // start loop?
						tape_firstbit(*tape_code);
					if ((t=*++tape_code)>=0)
						{ if (tape_n+=t,tape_item++) tape_status^=1; } // toggle signal
					else if (tape_item=0,--tape_time)
						tape_code=tape_codeitem[tape_byte]; // next loop
					else
						--tape_tones; // end loop
				}
				else if (tape_datas) // encoded datas
				{
					if (tape_datacodes) // build table?
					{
						if (tape_type==4) // PZX?
							tape_status=!(tape_datacodes&0X80000000)^tape_polarity; // cfr. BASIL.PZX
						else // TZX/TAP/CAS
						{
							tape_tzx19(tape_datacodes,tape_dataitems);
							tape_mask=(1<<(tape_bits=tape_datacodes<=2?1:tape_datacodes<=4?2:tape_datacodes<=16?4:8))-1;
							tape_feedable=tape_bits==1&&!tape_codeitem[0][0]&&!tape_codeitem[1][0]&&
								tape_codeitem[0][2]>0&&tape_codeitem[1][2]>0&&tape_codeitem[0][3]<0&&tape_codeitem[1][3]<0;
						}
						tape_datacodes=tape_time=tape_item=0;
					}
					if (!tape_time) // new byte?
						tape_byte=tape_getc(),tape_time=8;
					if (!tape_item) // start loop?
						tape_firstbit(*(tape_code=tape_codeitem[(tape_byte>>(tape_time-tape_bits))&tape_mask]));
					if ((t=*++tape_code)>=0)
						{ if (tape_n+=t,tape_item++) tape_status^=1; } // toggle signal
					else
						tape_item=0,tape_time-=tape_bits,--tape_datas; // end loop
				}
				#ifdef TAPE_TZX_KANSAS
				else if (tape_kansas) // support for the Kansas City Standard extended block in TSX files!
				{
					if (!tape_time)	// new byte?
						tape_byte=tape_getc(),tape_time=10,tape_item=0;
					if (!tape_item) // new bit?
					{
						if (tape_time>9) // IN signal
							tape_mask=tape_kansasi,tape_item=tape_kansasin;
						else if (tape_time>1) // DATA
							if ((tape_byte>>(tape_kansasrl?tape_time-2:9-tape_time))&1)
								tape_mask=tape_tzxbit1,tape_item=tape_kansas1n;
							else
								tape_mask=tape_tzxbit0,tape_item=tape_kansas0n;
						else // OUT signal at the end
							tape_mask=tape_kansaso,tape_item=tape_kansason;
					}
					if (tape_item) // it's zero if `tape_kansasin` or `tape_kansason` are zero!
						tape_status^=1,tape_n+=tape_mask,--tape_item;
					if (!tape_item)
						if (!--tape_time) --tape_kansas;
				}
				#endif
				else if (tape_waves) // 1-bit samples
				{
					if (!tape_time)
						tape_time=8,tape_byte=tape_getc();
					tape_status=((tape_byte>>--tape_time)^tape_polarity)&1;
					tape_n+=tape_mask,--tape_waves;
				}
				else if (tape_tails) // predef'd tail
				{
					if (tape_tails>3500) tape_n+=3500,tape_tails-=3500; // 3500 T (1 ms) in TZX; 70000 T in PZX?
						else tape_n+=tape_tails,tape_tails=0;
					if (tape_tail) tape_status=tape_polarity; else tape_status^=tape_tail=1; // quiet after 1 ms
				}
				else do
				{
					if (!--watchdog) { tape_close(); tape_signal=-1; return; } // it's a corrupted tape!!
					if (tape_step) // safety delay when opening, rewinding or browsing tapes
						tape_tzxhold=tape_step,tape_tzx20(),tape_step=0,cprintf("TAPE:ZZZ... ");
					#ifdef TAPE_TAP_FORMAT
					else if (tape_type==3) // TAP
					{
						if ((p=tape_getcc())<=0)
							{ tape_eofmet(); return; }
						t=tape_getc()&128; tape_undo();
						tape_tzxhold=960;
						tape_tzx10(t,p);
					}
					#else
					#ifdef TAPE_CAS_FORMAT
					else if (tape_type==3) // CAS
					{
						if ((p=tape_getc())<0)
							{ tape_eofmet(); return; }
						tape_undo();
						tape_time=tape_head=0;
						if (!memcmp(&tape_buffer[tape_offset],tape_header_cas1,8)) // separator?
						{
							tape_skip(8); tape_heads=2; tape_status=tape_polarity;
							tape_headcode[0]=5,tape_headcode[1]=700000; // "fake" 1-s pause
							tape_headcode[2]=9999,tape_headcode[3]=729; // valid pilot tone
						}
						else // a chunk of eight bytes
						{
							tape_kansas=8; tape_kansasrl=0;
							tape_kansasin=(tape_kansas0n=2)  ,tape_kansasi=tape_tzxbit0=1458;
							tape_kansason=(tape_kansas1n=4)*2,tape_kansaso=tape_tzxbit1= 729;
						}
					}
					#endif
					#endif
					else if (tape_type==4) // PZX
					{
						if ((p=tape_getcccc())<=(tape_feedable=0)||(t=tape_getcccc())<0)
							{ tape_eofmet(); return; }
						cprintf("PZX:%08X-%04X,%04X ",tape_filetell-4,p,t);
						if (p==0X534C5550&&!(t&1)) // "PULS"
							tape_time=tape_head=0,tape_heads=t/2;
						else if (p==0X41544144) // "DATA"
						{
							tape_time=tape_tail=0;
							tape_mask=tape_bits=1;
							tape_datacodes=tape_getcccc();
							if (!(tape_datas=tape_datacodes&7)) tape_datas=8;
							tape_datas+=(t-9)*8; // adjust (1/2)
							tape_tails=tape_getcc();
							int n0=tape_getc(),n1=tape_getc();
							tape_feedable=n0==2&&n1==2;
							tape_codeitem[0][0]=tape_codeitem[1][0]=0; // default behaviors
							tape_codeitem[0][n0+1]=tape_codeitem[1][n1+1]=-1; // end markers
							for (p=1;p<=n0;++p)
								tape_codeitem[0][p]=tape_getcc(); // ZERO is a legal value in PZX
							for (p=1;p<=n1;++p)
								tape_codeitem[1][p]=tape_getcc();
							tape_datas-=(n0+n1)*16; // adjust (2/2)
						}
						else if (p==0X53554150) // "PAUS"
						{
							tape_time=tape_tail=0;
							tape_tails=tape_getcccc()&0X7FFFFFFF;
							tape_skip(t-4);
						}
						else if (p==0X504F5453) // "STOP"
						{
							tape_signal|=(tape_getcccc()?2:1); // 1 all systems, 2 = <128K
							tape_skip(t-4);
						}
						else // unknown, ignore
							tape_skip(t);
					}
					else // TZX
					{
						if (tape_seekcccc) // sanity check?
							tape_seek(tape_seekcccc),tape_seekcccc=0;
						if ((p=tape_getc())<=(tape_feedable=0))
							{ tape_eofmet(); return; }
						cprintf("TZX:%08X-%02X ",tape_filetell-1,p);
						switch (p)
						{
							case 0x10: // NORMAL DATA
								tape_tzxhold=tape_getcc();
								p=tape_getcc();
								tape_tzx10(tape_getc()&128,p); tape_undo();
								break;
							case 0x11: // CUSTOM DATA
								tape_tzxpilot=tape_getcc();
								tape_tzxsync1=tape_getcc();
								tape_tzxsync2=tape_getcc();
								tape_tzxbit0=tape_getcc();
								tape_tzxbit1=tape_getcc();
								tape_tzxpilots=tape_getcc();
								tape_datas=tape_getc();
								tape_tzxhold=tape_getcc();
								tape_tzx11(tape_getccc());
								break;
							case 0x12: // PURE TONE
								tape_tzxpilot=tape_getcc();
								tape_tzxpilots=tape_getcc();
								tape_tzxsync1=tape_tzxsync2=0; tape_tzx12();
								break;
							case 0x13: // PURE SYNC
								tape_heads=tape_getc(); // does 0 mean 256!?
								for (p=0;p<tape_heads;++p)
									tape_headcode[p*2]=1,tape_headcode[p*2+1]=tape_getcc();
								tape_time=tape_head=0;
								break;
							case 0x14: // PURE DATA
								tape_tzxbit0=tape_getcc();
								tape_tzxbit1=tape_getcc();
								tape_datas=tape_getc();
								tape_tzxhold=tape_getcc();
								tape_tzx14(tape_getccc());
								break;
							case 0x15: // SAMPLES
								tape_time=0; tape_mask=tape_getcc();
								tape_tzxhold=tape_getcc(); tape_tzx20();
								if (tape_waves=tape_getc()) tape_waves-=8; // dodgy VALLATION turbo tape!
								tape_waves+=tape_getccc()<<3;
								break;
							case 0x19: // GENERALIZED DATA
								tape_seekcccc=tape_getcccc(); tape_seekcccc+=tape_filetell; // sanity check!
								tape_tzxhold=tape_getcc(); tape_tzx20();
								tape_tones=tape_getcccc();
								tape_toneitems=tape_getc();
								if (!(tape_tonecodes=tape_getc())) tape_tonecodes=256;
								tape_datas=tape_getcccc();
								tape_dataitems=tape_getc();
								if (!(tape_datacodes=tape_getc())) tape_datacodes=256;
								break;
							case 0x20: // HOLD (or STOP)
								if (tape_tzxhold=tape_getcc()) tape_tzx20(); else tape_signal|=1;
								break;
							case 0x2A: // STOP ON 48K
								tape_getcccc(); tape_signal|=2; // 1 = all systems, 2 = <128K
								break;
							case 0x2B: // SET SIGNAL LEVEL
								p=tape_getcccc()-1; tape_status=(tape_getc()&1)^tape_polarity; tape_skip(p);
								break;
							#ifdef TAPE_TZX_KANSAS
							case 0x4B: // KANSAS CITY DATA, used in TSX files (MSX tapes)
								tape_kansas=tape_getcccc()-12;
								tape_tzxhold=tape_getcc(); tape_tzx20();
								tape_tzxpilot=tape_getcc();
								tape_tzxpilots=tape_getcc();
								tape_tzxsync1=tape_tzxsync2=0; tape_tzx12();
								tape_tzxbit0=tape_getcc(); tape_tzxbit1=tape_getcc();
								p=tape_getc(); // default $24:
								if (!(tape_kansas0n=p>>4)) tape_kansas0n=16; // default 2
								if (!(tape_kansas1n=p&15)) tape_kansas1n=16; // default 4
								tape_kansasrl=(p=tape_getc())&1; // code flags, default $54
								tape_kansasin=(p>>6)&3; tape_kansason=(p>>3)&3; // defaults 1 and 2
								if ((p>>5)&1) // precalc'd IN: default 0
									tape_kansasin*=tape_kansas1n,tape_kansasi=tape_tzxbit1;
								else
									tape_kansasin*=tape_kansas0n,tape_kansasi=tape_tzxbit0;
								if ((p>>2)&1) // precalc'd OUT: default 1
									tape_kansason*=tape_kansas1n,tape_kansaso=tape_tzxbit1;
								else
									tape_kansason*=tape_kansas0n,tape_kansaso=tape_tzxbit0;
								break;
							#endif
							// the following blocks are moderately useful but don't carry any signals
							case 0x21: // GROUP START
								tape_skip(tape_getc());
								//break; no `break`!
							case 0x22: // GROUP END
								break;
							case 0x24: // LOOP START
								tape_loops=tape_getcc();
								tape_loop0=tape_filetell;
								break;
							case 0x25: // LOOP END
								if (tape_loops&&--tape_loops)
									tape_seek(tape_loop0);
								break;
							// the following blocks should never appear, they do more harm than good!
							case 0x23: // !JUMP TO BLOCK
								p=tape_tzx1tell(),tape_tzx1seek(p+(signed short)tape_getcc()); // the watchdog will catch loops!
								break;
							case 0x26: // !CALL SEQUENCE
								if (tape_calls=tape_getcc())
									tape_call0=tape_filetell+2,p=tape_tzx1tell(),tape_tzx1seek(p+(signed short)tape_getcc());
								break;
							case 0x27: // !RETURN FROM CALL
								if (tape_calls) if (tape_seek(tape_call0),--tape_calls)
									tape_call0+=2,p=tape_tzx1tell(),tape_tzx1seek(p+(signed short)tape_getcc());
								break;
							case 0x28: // !SELECT BLOCK
								tape_skip(tape_getcc()); // what's this list for!?
								break;
							// the following blocks are informative but don't play a role in playback
							case 0x31: // *MESSAGE BLOCK
								tape_getc(); // no `break`!
							case 0x30: // *TEXT DESCRIPTION
								tape_skip(tape_getc());
								break;
							case 0x32: // *ARCHIVE INFO
								tape_skip(tape_getcc());
								break;
							case 0x33: // *HARDWARE TYPE
								tape_skip(tape_getc()*3);
								break;
							case 0x34: // *EMULATION INFO
								tape_skip(8);
								break;
							case 0x35: // *CUSTOM INFO
								tape_skip(16); // no `break`!
								tape_skip(tape_getcccc());
								break;
							case 0x40: // *SNAPSHOT INFO
								tape_skip(tape_getcccc()>>8);
								break;
							case 0x5A: // *GLUE
								tape_skip(9);
								break;
							default: // *UNKNOWN!
								tape_skip(tape_getcccc()); // all future blocks will be like this
						}
					}
				}
				while (!(tape_heads|tape_tones|tape_datas|
					#ifdef TAPE_TZX_KANSAS
					tape_kansas|
					#endif
					tape_waves|tape_tails));
			break;
	}
}

// tape speedup operations ------------------------------------------ //

int fasttape_test(const BYTE *s,WORD p) // compares a chunk of memory against a pattern; see below for pattern format
{
	for (WORD a,z;;)
		if (*s==0x80) // relative word, f.e. the pair "-128,  -2" points at its first byte
		{
			z=PEEK(p); ++p; z+=PEEK(p)*256; ++p; a=p+(INT8)(*++s); // fetch the word
			{ if (a!=z) return 0; } ++s; // give up if they don't match
		}
		else // memory comparison, the two bytes are the offset (signed!) and the length
		{
			p+=(INT8)(*s++); if (!(z=*s++)) return 1; // zero length: end of pattern
			while (z>0) { if (PEEK(p)!=*s) return 0; ++p,++s,--z; }
		}
}

int fasttape_skip(char q,char x) // reads the tape at steps of `x` until the state isn't `q` or the tape is over; returns the amount of iterations
	{ int n=0; q&=1; while (tape_status==q&&!tape_signal) tape_main(x),++n; return n; }
int fasttape_add8(char q,char x,BYTE *u8,BYTE d) // adds `d` to `u8` until the state isn't `q` or the addition overflows
	{ int n=0; q&=1; d=-d; while (tape_status==q&&*u8<d) tape_main(x),*u8-=d,++n; return n; } // notice the unsigned "-d" abuse!
int fasttape_sub8(char q,char x,BYTE *u8,BYTE d) // ditto, but substracting
	{ int n=0; q&=1; while (tape_status==q&&*u8>d) tape_main(x),*u8-=d,++n; return n; }

#define FASTTAPE_CAN_FEED() (tape_feedable&&fasttape_feedable())
int fasttape_feedable(void) // can I feed a *single* byte to the system?
	{ return !(tape_heads|tape_tones|tape_datacodes)&&tape_datas>8&&tape_time; } // `&&tape_code[1]<0` is redundant outside malformed tapes
BYTE fasttape_feed(char q,char x) // pull a byte minus the last signal; see fasttape_skip()
{
	int i=tape_byte; tape_datas-=7; if (tape_time<8)
		i=((i<<8)+(tape_byte=tape_getc()))>>(tape_time+(tape_code[1]<0?0:1)),++tape_time; // the rare !(tape_code[1]<0) needs balancing
	else
		tape_time=1; // already aligned, no need to shuffle bits
	fasttape_skip(q,x); return i;
}
#ifdef FASTTAPE_DUMPER
#define FASTTAPE_CAN_DUMP() (tape_datas>16) // can I dump *multiple* bytes on the system?
BYTE fasttape_dump(void) // pull a whole byte, all signals included; see fasttape_skip()
{
	int i=tape_byte; tape_byte=tape_getc(); tape_datas-=8;
	return tape_time<8?((i<<8)+tape_byte)>>tape_time:i;
}
#endif
#ifdef TAPE_TZX_KANSAS
#define FASTTAPE_CAN_KFEED() (tape_kansas&&!tape_heads&&tape_time==9)
#define fasttape_kfeed(q,x) (tape_time-=7,fasttape_skip(q,x),tape_byte)
#ifdef FASTTAPE_DUMPER
#define FASTTAPE_CAN_KDUMP() (tape_kansas>2)
BYTE fasttape_kdump(void)
	{ BYTE b=tape_byte; tape_byte=tape_getc(); --tape_kansas; return b; }
#endif
#endif

void fasttape_gotonext(void) // skips the current datas (minus the last bytes) if possible
{
	int i,k; tape_loops=0; if (!(tape_heads|tape_tones|tape_datacodes)) // playing DATAS or WAVES?
	{
		if (tape_datas>8&&tape_bits) tape_skip(i=(tape_datas-1)/(k=8/tape_bits)),tape_datas-=i*k;
		#ifdef TAPE_TZX_KANSAS
		if (tape_kansas>1) tape_skip(tape_kansas-1),tape_kansas=1;
		#endif
		if (tape_waves>8) tape_skip(i=(tape_waves-1)/8),tape_waves-=i*8;
	}
}

// ============================================== END OF TAPE SUPPORT //
