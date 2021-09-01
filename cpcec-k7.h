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

// Supported format versions are WAV1 (uncompressed PCM audio), CSW1
// (run-length compressed square wave) and TZX1 (signal blocks only).

// BEGINNING OF TAPE SUPPORT ======================================== //

FILE *tape=NULL; // tape file handle
int tape_status=0,tape_closed,tape_rewind=1; // tape signal and rewind logic
BYTE tape_buffer[1<<12]; // tape buffer data
int tape_offset,tape_length; // tape buffer parameters
int tape_filesize,tape_filebase,tape_filetell; // tape file stats
INLINE int tape_fgetc(void) // reads one byte from tape. returns <0 on error!
{
	if (tape_offset>=tape_length) // outside cache?
	{
		tape_offset=0;
		if (!(tape_length=fread(tape_buffer,1,sizeof(tape_buffer),tape)))
			return -1;
	}
	++tape_filetell;
	return tape_buffer[tape_offset++];
}
int tape_fgetcc(void) // reads two little-endian bytes; cfr. tape_fgetc()
{
	int i=tape_fgetc(); return i+(tape_fgetc()<<8); // this keeps the sign of the byte!
}
int tape_fgetcccc(void) // reads four little-endian bytes; cfr. tape_fgetc()
{
	int i=tape_fgetc(); i+=tape_fgetc()<<8; i+=tape_fgetc()<<16;
	return i+(tape_fgetc()<<24); // in 32-bit systems the sign will be lost!
}
#define tape_ungetc() (--tape_filetell,--tape_offset) // cheating the buffer!
void tape_seek(int i) // seek to byte `i`
{
	if ((tape_filetell-tape_offset<=i)&&(tape_filetell-tape_offset+tape_length>=i)) // inside cache?
	{
		tape_offset+=i-tape_filetell;
		tape_filetell=i;
	}
	else // outside cache!
	{
		fseek(tape,tape_filetell=i,SEEK_SET);
		tape_offset=tape_length=0; // reset buffer!
	}
}
INLINE void tape_skip(int i) // skip `i` bytes; using a macro led to "gotchas" (cfr. CLANG 3.7.1)
{
	tape_seek(i+tape_filetell);
}
INLINE void tape_fputc(int i) // writes one byte on tape.
{
	tape_buffer[tape_offset]=i;
	if (++tape_offset>=sizeof(tape_buffer)) // outside cache?
	{
		fwrite(tape_buffer,1,sizeof(tape_buffer),tape);
		tape_offset=0;
	}
}
void tape_fputcc(int i) // writes two little-endian bytes; cfr. tape_fputc();
{
	tape_fputc(i);
	tape_fputc(i>>8);
}
void tape_fputcccc(int i) // writes four little-endian bytes; cfr. tape_fputc();
{
	tape_fputc(i);
	tape_fputc(i>>8);
	tape_fputc(i>>16);
	tape_fputc(i>>24);
}

int tape_type,tape_playback,tape_count; // general tape parameters
int tape_pilot,tape_pilots,tape_sync,tape_syncs,tape_syncz[256],tape_bits,tape_bit0,tape_bit1,tape_byte,tape_half,tape_mask,tape_wave,tape_hold,tape_loop,tape_looptell; // TZX tape parameters
#ifdef TAPE_KANSAS_CITY
int tape_kansas,tape_kansasin,tape_kansasi,tape_kansason,tape_kansaso,tape_kansas0n,tape_kansas1n,tape_kansasrl,tape_kansas_i,tape_kansas_n,tape_kansas_b,tape_kansas_o; // TZX block $4B: Kansas City Standard
#else
#define tape_kansas tape_wave // dummy variable!
#endif
int tape_general_totp,tape_general_npp,tape_general_asp,tape_general_totd,tape_general_npd,tape_general_asd,tape_general_count,
	tape_general_mask,tape_general_step,tape_general_bits; WORD tape_general_symdef[256][128]; // TZX block $19: Generalized Data
int tape_record,tape_output; // tape recording parameters

#define tape_setup()

#define tape_reset()

// tape file handling operations ------------------------------------ //

char tape_path[STRMAX]="";

void tape_flush(void) // dump remaining samples, if any
{
	if (tape_count)
	{
		if (tape_count<256)
			tape_fputc(tape_count);
		else
			tape_fputc(0),tape_fputcccc(tape_count);
		tape_record=tape_output;
		tape_count=0;
	}
}

void tape_close(void) // close tape file
{
	if (tape)
	{
		if (tape_type<0) // recording?
			tape_flush(),fwrite(tape_buffer,1,tape_offset,tape); // finish and record last sample
		puff_fclose(tape);
	}
	tape=NULL;
	tape_filesize=tape_filetell=tape_mask=tape_type=tape_status=0;
}

int tape_endoftape(void) // rewind tape, if possible; 0 OK, !0 ERROR
{
	if (tape_rewind&&!tape_closed++) // close anyway if two loops happen at once (i.e. corrupt tape)
		if (tape_type>=0) // only rewind on INPUT tapes!
		{
			fseek(tape,tape_filetell=tape_filebase,SEEK_SET);
			return tape_offset=tape_length=0; // reset buffer!
		}
	return tape_close(),++tape_closed; // cannot rewind!
}

int tape_open(char *s) // open a tape file. `s` path; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"rb")))
		return 1; // cannot open tape!
	int l=0,q=1; // temporary integer and error flag
	tape_closed=tape_offset=tape_length=tape_bits=tape_playback=0; // reset buffer!
	if (fread(tape_buffer,1,8,tape)==8) // guess the format
	{
		if (!memcmp("RIFF",tape_buffer,4)) // WAV?
		{
			if (fgetmmmm(tape)==0x57415645) // "WAVE" ID
				while ((q=fgetmmmm(tape),l=fgetiiii(tape))>=0&&q>0&&q!=0x64617461) // read chunks until "data"
				{
					if (q==0x666D7420) // "fmt " is the only chunk that we must process!
					{
						l-=fread(tape_buffer,1,16,tape); // read the first part of the "fmt " chunk
						tape_playback=tape_buffer[4]+(tape_buffer[5]<<8)+(tape_buffer[6]<<16);
						tape_sync=tape_buffer[12]; // handle any bit depth and channel amount: only the highest bit matters to us
					}
					fseek(tape,l,SEEK_CUR); // skip unknown chunk!
				}
			if (tape_playback&&l>0)
				tape_type=q=0; // accept valid WAVs only
		}
		else if (!memcmp("Compress",tape_buffer,8)) // CSW?
		{
			fseek(tape,0x17,SEEK_SET);
			if (fgetc(tape)==1) // major version 1 only!
			{
				fgetc(tape); // skip minor version
				tape_playback=fgetii(tape);
				if (fgetc(tape)==2) // skip compression type (always 1)
					tape_playback+=1<<16; // allow up to 120 KHz!
				tape_status=(~fgetiiii(tape))&1; // the first read will toggle it!
				tape_type=1; tape_count=q=0;
			}
		}
		#ifdef TAPE_OPEN_TAP_FORMAT
		else if (!memcmp("\023\000\000\000",tape_buffer,4)||!memcmp("ZXTape!\032",tape_buffer,8)) // TAP + TZX/CDT?
		#else
		else if (!memcmp("ZXTape!\032",tape_buffer,8)) // TZX/CDT?
		#endif
		{
			#ifdef TAPE_OPEN_TAP_FORMAT
			if (!tape_buffer[1])
				tape_length=8,tape_type=3; // TAP: header is actually valid data
			else
			#endif
				fgetii(tape),tape_type=2; // TZX/CDT: major.minor version
			tape_status=tape_count=q=tape_pilots=tape_syncs/*=tape_bits*/=tape_kansas=tape_wave=tape_general_totp=tape_general_totd=tape_loop=0; // force HOLD
			tape_playback=3500000/TAPE_MAIN_TZX_STEP; tape_hold=2*1000; // 2000 ms -> 3500 T = 1 ms
		}
	}
	if (q)
		return tape_close(),1; // unknown tape format!
	tape_filetell=ftell(tape); fseek(tape,0,SEEK_END); tape_filesize=ftell(tape); fseek(tape,tape_filetell,SEEK_SET);
	tape_filebase=tape_filetell+=tape_offset-tape_length;
	if (tape_path!=s)
		strcpy(tape_path,s); // valid format
	return 0;
}
int tape_create(char *s) // create a tape file. `s` path; 0 OK, !0 ERROR
{
	tape_close();
	if (!(tape=puff_fopen(s,"wb")))
		return 1; // cannot create tape!
	fwrite("Compressed Square Wave\032\001\001\104\254\001\000\000\000\000",1,32,tape); // 44100Hz CSW1 format
	tape_playback=44100; tape_type=-1;
	if (tape_path!=s)
		strcpy(tape_path,s); // why not?
	return tape_filesize=tape_filetell=tape_offset=tape_count=tape_record=tape_output=0; // reset buffer and format
}

// tape emulation --------------------------------------------------- //

int tape_main_general_size,tape_main_general_symbol,tape_main_general_subsym;
void tape_main_general_make(int n,int m) // loads a symbol definition table (cfr. TZX block type $19)
{
	MEMZERO(tape_general_symdef); tape_main_general_size=m;
	for (int i=0;i<n;++i)
	{
		tape_general_symdef[i][0]=tape_fgetc()&3; // is this ever non-zero!?
		for (int j=1;j<=m;++j)
			tape_general_symdef[i][j]=tape_fgetcc();
	}
}
void tape_main_general_load(int i) // prepare new symbol
{
	tape_main_general_subsym=1;
	switch (tape_general_symdef[tape_main_general_symbol=i][0]&3)
	{
		case 1: tape_status^=1; break; // undo last signal swap
		case 2: tape_status=0; break;
		case 3: tape_status=1; break;
	}
}
int tape_main_general_next(void) // handle subsymbols; TRUE if last subsymbol in symbol
{
	tape_count+=tape_general_symdef[tape_main_general_symbol][tape_main_general_subsym];
	return tape_main_general_subsym>=tape_main_general_size||!tape_general_symdef[tape_main_general_symbol][++tape_main_general_subsym];
}

void tape_main(int t) // handle tape signal for `t` clock ticks
{
	if (!tape)
		return;
	static int r=0; // `tape_playback` can be a very high multiplier and cause overflows without `long long`!
	int p=(r+=(t*tape_playback))/TICKS_PER_SECOND;
	r%=TICKS_PER_SECOND; // *!* a possible solution without `long long`
	if (p<0) // catch overflows caused by options changing on the fly!!
		return;
	switch (tape_type) // `while` is inside `switch` because the tape type won't change inside the loop!
	{
		case 0: // WAV
			tape_skip(tape_sync*p-1); // avoid bugs caused by `tape_wave` getting modified by tape_select()
			if ((tape_status=tape_fgetc())<0) // EOF?
				if (tape_endoftape())
					return; // quit!
			tape_status=tape_status>128;
			break;
		case 1: // CSW
			while (p--)
			{
				if (!tape_count)
				{
					if ((tape_count=tape_fgetc())<0) // EOF?
					{
						if (tape_endoftape())
							return; // quit!
						tape_count=2*tape_playback; // 2-second gap
					}
					if (!tape_count)
						tape_count=tape_fgetcccc();
					tape_status^=1;
				}
				--tape_count;
			}
			break;
		case 2: // TZX/CDT
		case 3: // TAP
			while (p--)
			{
				while (tape_count<1)
				{
					// handle current block, if any
					if (tape_pilots) // TONE?
						tape_status^=1,--tape_pilots,tape_count+=tape_pilot;
					else if (tape_syncs) // SYNC?
						tape_status^=1,--tape_syncs,tape_count+=tape_syncz[tape_sync++];
					else if (tape_bits) // BITS?
					{
						if (!tape_half) // first half, calculate length
						{
							if (!(tape_mask>>=1)) // no bits left?
								tape_mask=128,tape_byte=tape_fgetc();
							tape_count+=tape_half=tape_mask&tape_byte?tape_bit1:tape_bit0;
						}
						else // second half, same length as first half
							tape_count+=tape_half,tape_half=0,--tape_bits;
						tape_status^=1;
					}
					else if (tape_wave) // WAVE?
					{
						if (!(tape_mask>>=1))
							tape_mask=128,tape_byte=tape_fgetc();
						tape_status=!!(tape_byte&tape_mask),--tape_wave,tape_count+=tape_bit0;
					}
					#ifdef TAPE_KANSAS_CITY
					else if (tape_kansas) // KANSAS CITY STANDARD?
					{
						if (tape_kansas_i<tape_kansasin) // IN signals?
							++tape_kansas_i,tape_count+=tape_kansasi;
						else if (tape_kansas_b<tape_kansas_n) // BIT signals?
							++tape_kansas_b,tape_count+=tape_half;
						else if (tape_mask<8) // new BIT signal?
						{
							if (!tape_mask) // new byte?
								tape_byte=tape_fgetc();
							if (tape_byte&(tape_kansasrl?128>>tape_mask:1<<tape_mask))
								tape_half=tape_bit1,tape_kansas_n=tape_kansas1n;
							else
								tape_half=tape_bit0,tape_kansas_n=tape_kansas0n;
							++tape_mask,tape_kansas_b=1,tape_count+=tape_half;
						}
						else // OUT signals?
						{
							if (++tape_kansas_o>=tape_kansason)
								--tape_kansas,tape_mask=tape_kansas_i=tape_kansas_n=tape_kansas_o=0; // next byte!
							tape_count+=tape_kansaso;
						}
						tape_status^=1;
					}
					#endif
					else if (tape_general_totp) // GENERALIZED DATA PILOT?
					{
						tape_status^=1; if (tape_general_count<=0) // fetch new item?
						{
							if (tape_general_asp) // build table?
								tape_main_general_make(tape_general_asp,tape_general_npp),tape_general_asp=0;
							// can tape_general_count ever be zero!?
							tape_main_general_load(tape_byte=tape_fgetc()); tape_general_count=tape_fgetcc();
						}
						if (tape_main_general_next()) // end of symbol?
						{
							if (--tape_general_count)
								tape_main_general_load(tape_byte); // repeat item
							else
								--tape_general_totp;
						}
					}
					else if (tape_general_totd) // GENERALIZED DATA BYTES?
					{
						tape_status^=1; if (tape_general_count<=0) // fetch new item?
						{
							if (tape_general_asd) // build table?
								tape_general_mask=(1<<(tape_general_bits=tape_general_asd>2?tape_general_asd>4?tape_general_asd>16?8:4:2:1))-1,
								tape_main_general_make(tape_general_asd,tape_general_npd),tape_general_asd=0;
							// can tape_general_bits ever be zero!?
							tape_main_general_load(((tape_byte=tape_fgetc())>>(tape_general_step=(tape_general_count=8)-tape_general_bits))&tape_general_mask);
						}
						if (tape_main_general_next()) // end of symbol?
							if (--tape_general_totd)
								tape_general_count=tape_general_step, // warn if empty
								tape_main_general_load((tape_byte>>(tape_general_step-=tape_general_bits))&tape_general_mask); // decode item
					}
					else if (tape_hold) // HOLD?
					{
						tape_count+=3500;
						if (tape_hold>0) // positive = first millisecond; negative = next milliseconds
							tape_status^=1,tape_hold=1-tape_hold;
						else
							tape_status=0,++tape_hold;
					}
					// fetch new blocks if required
					#ifdef TAPE_OPEN_TAP_FORMAT
					if (tape_type==3) // TAP
					{
						if (!(tape_pilots|tape_syncs|tape_bits|tape_hold))
						{
							tape_half=tape_mask=0;
							tape_hold=2*1000; // 2-second gap
							tape_bits=tape_fgetcc()<<3;
							tape_pilots=(tape_fgetc()&128)?3223:8063;
							tape_ungetc(); // cheating! (2/2)
							if (tape_bits<0) // EOF?
							{
								tape_pilot=tape_bits=0; // fake hold
								if (tape_bit0>=0) // first EOF?
									tape_bit0=-1;
								else // second EOF!
									if (tape_endoftape())
										return; // quit!
							}
							else
							{
								tape_pilot=2168;
								tape_sync=0;
								tape_syncs=2;
								tape_syncz[0]=667;
								tape_syncz[1]=735;
								tape_bit0=855;
								tape_bit1=1710;
							}
						}
					}
					else // TZX/CDT
					#endif
						while (!(tape_pilots|tape_syncs|tape_bits|tape_hold)&&!(tape_wave|tape_general_totp|tape_general_totd|tape_kansas))
						{
							tape_half=tape_mask=tape_general_count=tape_general_bits=0;
							int i=tape_fgetc();
							if (i<0) // EOF?
							{
								i=0x22; // dummy block
								tape_hold=2*1000; // 2-second gap
								if (tape_bit0>=0) // first EOF?
									tape_bit0=-1;
								else // second EOF!
									if (tape_endoftape())
										return; // quit!
							}
							switch (i)
							{
								case 0x10: // STANDARD DATA
									tape_hold=tape_fgetcc();
									tape_pilot=2168;
									tape_sync=0;
									tape_syncs=2;
									tape_syncz[0]=667;
									tape_syncz[1]=735;
									tape_bit0=855;
									tape_bit1=1710;
									tape_bits=tape_fgetcc()<<3;
									tape_pilots=(tape_fgetc()&128)?3223:8063;
									tape_ungetc(); // cheating! (1/2)
									break;
								case 0x11: // TURBO DATA
									tape_pilot=tape_fgetcc();
									tape_sync=0;
									tape_syncs=2;
									tape_syncz[0]=tape_fgetcc();
									tape_syncz[1]=tape_fgetcc();
									tape_bit0=tape_fgetcc();
									tape_bit1=tape_fgetcc();
									tape_pilots=tape_fgetcc();
									tape_bits=tape_fgetc()-8;
									tape_hold=tape_fgetcc();
									tape_bits+=tape_fgetcc()<<3;
									tape_bits+=tape_fgetc()<<19;
									break;
								case 0x12: // PURE TONE
									tape_pilot=tape_fgetcc();
									tape_pilots=tape_fgetcc();
									break;
								case 0x13: // PURE SYNC
									tape_sync=0;
									tape_syncs=tape_fgetc();
									for (i=0;i<tape_syncs;++i)
										tape_syncz[i]=tape_fgetcc();
									break;
								case 0x14: // PURE DATA
									tape_bit0=tape_fgetcc();
									tape_bit1=tape_fgetcc();
									tape_bits=tape_fgetc()-8;
									tape_hold=tape_fgetcc();
									tape_bits+=tape_fgetcc()<<3;
									tape_bits+=tape_fgetc()<<19;
									break;
								case 0x15: // SAMPLES
									tape_bit0=tape_fgetcc();
									tape_hold=tape_fgetcc();
									tape_wave=tape_fgetc()-8; if (tape_wave==-8) tape_wave=0; // handle dodgy VALLATION TURBO tape!
									tape_wave+=tape_fgetcc()<<3;
									tape_wave+=tape_fgetc()<<19;
									break;
								case 0x19: // GENERALIZED DATA
									tape_fgetcccc();
									tape_hold=tape_fgetcc();
									tape_general_totp=tape_fgetcccc();
									tape_general_npp=tape_fgetc();
									if (!(tape_general_asp=tape_fgetc())) tape_general_asp=256;
									tape_general_totd=tape_fgetcccc();
									tape_general_npd=tape_fgetc();
									if (!(tape_general_asd=tape_fgetc())) tape_general_asd=256;
									break;
								case 0x20: // HOLD (or STOP)
									tape_hold=tape_fgetcc();
									break;
								case 0x2A: // STOP if 48K
									tape_fgetcccc();
									break;
								// the following blocks are only moderately useful when we edit the tape
								case 0x21: // GROUP START
									tape_skip(tape_fgetc());
									//break; no `break`!
								case 0x22: // GROUP END
									break;
								case 0x24: // LOOP START
									tape_loop=tape_fgetcc();
									tape_looptell=tape_filetell;
									break;
								case 0x25: // LOOP END
									if (tape_loop&&--tape_loop)
										tape_seek(tape_looptell);
									break;
								// the following blocks shouldn't be used in tapes: they do more harm than good!
								case 0x23: // !JUMP TO GROUP
									tape_fgetcc(); //x=i;
									break;
								case 0x26: // !CALL SEQUENCE
									tape_skip(tape_fgetc()*2); // no `break`!
								case 0x27: // !RETURN FROM CALL
									//x=i;
									break;
								case 0x28: // !SELECT BLOCK
									tape_skip(tape_fgetcc()); //x=i;
									break;
								// the following blocks are informative, but don't play a role in playback
								case 0x31: // *MESSAGE BLOCK
									tape_fgetc(); // no `break`!
								case 0x30: // *TEXT DESCRIPTION
									#ifdef CONSOLE_DEBUGGER
									{
										i=tape_fgetc();
										char blah[256];
										int j; for (j=0;j<i;++j)
											blah[j]=tape_fgetc();
										blah[j]=0;
										cprintf("TZX: '%s'\n",blah);
									}
									#else
									tape_skip(tape_fgetc()); // avoid macro gotcha, cfr. CLANG 3.7.1
									#endif
									break;
								case 0x32: // *ARCHIVE INFO
									tape_skip(tape_fgetcc());
									break;
								case 0x33: // *HARDWARE TYPE
									tape_skip(tape_fgetc()*3);
									break;
								case 0x34: // *EMULATION INFO
									tape_skip(8);
									break;
								case 0x35: // *CUSTOM INFO
									tape_skip(16);
									tape_skip(tape_fgetcccc());
									break;
								case 0x40: // *SNAPSHOT INFO
									tape_skip(tape_fgetcccc()>>8);
									break;
								#ifdef TAPE_KANSAS_CITY
								case 0x4B: // KANSAS CITY STANDARD
									tape_kansas=tape_fgetcccc()-12;
									tape_hold=tape_fgetcc();
									tape_pilot=tape_fgetcc();
									tape_pilots=tape_fgetcc();
									tape_bit0=tape_fgetcc();
									tape_bit1=tape_fgetcc();
									if (!(tape_kansas0n=(i=tape_fgetc())>>4)) tape_kansas0n=16;
									if (!(tape_kansas1n=i&15)) tape_kansas1n=16;
									tape_kansasrl=(i=tape_fgetc())&1;
									tape_kansasin=(i>>6)&3; tape_kansasi=(i>>5)&1;
									tape_kansason=(i>>3)&3; tape_kansaso=(i>>2)&1;
									tape_kansas_b=tape_kansas_i=tape_kansas_n=tape_kansas_o=0;
									// optimisations to avoid repeated multiplications
									if (tape_kansasi)
										tape_kansasin*=tape_kansas1n,tape_kansasi=tape_bit1;
									else
										tape_kansasin*=tape_kansas0n,tape_kansasi=tape_bit0;
									if (tape_kansaso)
										tape_kansason*=tape_kansas1n,tape_kansaso=tape_bit1;
									else
										tape_kansason*=tape_kansas0n,tape_kansaso=tape_bit0;
									break;
								#endif
								case 0x5A: // *GLUE
									tape_skip(9);
									break;
								default: // *UNKNOWN!
									tape_skip(tape_fgetcccc()); // all unknown blocks are defined this way
									break;
							}
							//if (x) cprintf("TZX: BLOCK 0x%02X!\n",x);
						}
				}
				tape_count-=TAPE_MAIN_TZX_STEP;
			}
			break;
		case -1: // recording to CSW
			if (tape_record!=tape_output)
				tape_flush();
			tape_count+=p;
			break;
	}
}

// tape browsing: catalog ------------------------------------------- //

void tape_catalog_text(char **t,int i)
{
	for (int j;i>0;--i)
		if ((j=tape_fgetc())>=32)
			*(*t)++=j; // purge invisible chars (f.e. SPEEDKING.CDT)
	*(*t)++=0;
}
#define TAPE_CATALOG_HEAD "%010d --"
int tape_catalog(char *t,int x)
{
	if (!tape||tape_type<0)
		return *t=0,-1;
	int i,j,p=-1;
	if (tape_type<2) // sample?
	{
		int x=tape_type?-1:-tape_sync; // WAV granularity depends on sample width
		for (i=0;i<100;i+=5)
		{
			j=tape_filebase+(((long long)i*(tape_filesize-tape_filebase)/100)&x);
			if (j<=tape_filetell)
				++p;
			t+=1+sprintf(t,TAPE_CATALOG_HEAD " sample block %02d%%",j,i);
		}
	}
	else // TZX/TAP?
	{
		char *s=&t[x-STRMAX],*u=NULL;
		int z=tape_filetell,k,l;
		tape_seek(tape_filebase);
		#ifdef TAPE_OPEN_TAP_FORMAT
		if (tape_type==3) // TAP
			while (t<s&&(i=tape_fgetcc())>=0)
			{
				t+=1+sprintf(t,TAPE_CATALOG_HEAD " STANDARD DATA, %d bytes",j=tape_filetell-2,i);
				if (j<=z) ++p; // locate current block
				tape_skip(i);
			}
		else // TZX/CDT
		#endif
			while (t<s&&(i=tape_fgetc())>=0)
			{
				t+=sprintf(t,TAPE_CATALOG_HEAD " ",j=tape_filetell-1);
				if (j<=z&&!u) ++p; // locate current block
				k=l=0;
				switch (i)
				{
					case 0x10: t+=1+sprintf(t,"STANDARD DATA");
						tape_fgetcc();
						l=tape_fgetcc();
						break;
					case 0x11: t+=1+sprintf(t,"TURBO DATA");
						tape_skip(2+2+2+2+2+2+1+2);
						l=tape_fgetcc();
						l+=tape_fgetc()<<16;
						break;
					case 0x12: t+=1+sprintf(t,"PURE TONE");
						tape_skip(2+2);
						break;
					case 0x13: t+=1+sprintf(t,"PURE SYNC");
						l=tape_fgetc()<<1;
						break;
					case 0x14: t+=1+sprintf(t,"PURE DATA");
						tape_skip(2+2+1+2);
						l=tape_fgetcc();
						l+=tape_fgetc()<<16;
						break;
					case 0x15: t+=1+sprintf(t,"SAMPLES");
						tape_skip(2+2+1);
						l=tape_fgetcc();
						l+=tape_fgetc()<<16;
						break;
					case 0x19: t+=1+sprintf(t,"GENERAL DATA");
						l=tape_fgetcccc();
						l-=(k=2+4+1+1+4+1+1);
						break;
					case 0x20: t+=1+sprintf(t,"HOLD");
						tape_fgetcc();
						break;
					case 0x2A: t+=1+sprintf(t,"STOP on 48K");
						tape_fgetcccc();
						break;
					case 0x21: t+=sprintf(t,"GROUP: "); // GROUP START
						tape_catalog_text(&t,tape_fgetc());
						u=t;
						break;
					case 0x22: if (u) t=u,u=NULL; // t+=1+sprintf(t,"GROUP END");
						break;
					case 0x23: t+=1+sprintf(t,"!JUMP TO GROUP");
						tape_fgetcc();
						break;
					case 0x24: t+=1+sprintf(t,"LOOP START");
						tape_fgetcc();
						break;
					case 0x25: t+=1+sprintf(t,"LOOP END");
						break;
					case 0x26: t+=1+sprintf(t,"!CALL SEQUENCE");
						k=tape_fgetc()*2;
						break;
					case 0x27: t+=1+sprintf(t,"!RETURN FROM CALL");
						break;
					case 0x28: t+=1+sprintf(t,"!SELECT BLOCK");
						k=tape_fgetcc();
						break;
					case 0x31: // *MESSAGE BLOCK
						tape_fgetc(); // no `break`!
					case 0x30: t+=sprintf(t,"TEXT: ");
						tape_catalog_text(&t,tape_fgetc());
						break;
					case 0x32: t+=1+sprintf(t,"*ARCHIVE INFO");
						k=tape_fgetcc();
						break;
					case 0x33: t+=1+sprintf(t,"*HARDWARE TYPE");
						k=tape_fgetc()*3;
						break;
					case 0x34: t+=1+sprintf(t,"*EMULATION INFO");
						k=8;
						break;
					case 0x35: t+=1+sprintf(t,"*CUSTOM INFO");
						tape_skip(16);
						k=tape_fgetcccc();
						break;
					case 0x40: t+=1+sprintf(t,"*SNAPSHOT INFO");
						k=tape_fgetcccc()>>8;
						break;
					#ifdef TAPE_KANSAS_CITY
					case 0x4B: t+=1+sprintf(t,"KANSAS CITY DATA");
						k=tape_fgetcccc();
						l=k-12;
						break;
					#endif
					case 0x5A: t+=1+sprintf(t,"*GLUE");
						k=9;
						break;
					default: t+=1+sprintf(t,"*UNKNOWN!");
						k=tape_fgetcccc(); // all unknown blocks are defined this way
						break;
				}
				tape_skip(k+l);
				if (l)
					--t,t+=1+sprintf(t,", %d bytes",l);
				if (u) // keep block invisible
					t=u;
			}
		if (i>=0) // tape is too long :-(
		{
			t+=1+sprintf(t,TAPE_CATALOG_HEAD " ...",j=tape_filetell);
			if (j<=z)
				++p;
		}
		tape_seek(z);
	}
	//t+=1+sprintf(t,TAPE_CATALOG_HEAD " END OF TAPE",tape_filesize);
	return *t=0,p;
}

// tape browsing: select -------------------------------------------- //

void tape_select(int i) // caller must provide a valid offset
{
	if (!tape||tape_type<0)
		return;
	tape_seek(i);
	tape_count=tape_pilots=tape_syncs=tape_bits=tape_kansas=tape_wave=tape_general_totp=tape_general_totd=0,tape_hold=2*1000; // reset counter and playback // 2-second gap
}

// tape speed-up hacks ---------------------------------------------- //

int fasttape_test(const BYTE *s,WORD p) // compares a chunk of memory against a pattern; see below for pattern format
{
	for (WORD a,z;;)
		if (*s==0x80) // relative word, f.e. 0x80 0xFE (-2) points back to itself
		{
			z=PEEK(p); ++p; z+=PEEK(p)*256; ++p;
			a=p+(signed char)(*++s);
			if (a-z) return 0; // word doesn't match
			++s;
		}
		else // offset+length memcmp
		{
			a=p+(signed char)(*s++); z=a+(*s++);
			if (a==z) return 1; // any offset, zero length: end of pattern, OK
			if (z<a||memcmp(&(PEEK(a)),s,z-a)) return 0; // pattern doesn't match
			p=z; s+=z-a;
		}
}

void fasttape_skip(int q,int x) // skip the signal in steps of `x` until it changes
{
	q&=1; while (q==tape_status&&(tape_hold|(size_t)tape)) tape_main(x);
}
int fasttape_add8(int q,int tape_step,BYTE *counter8,int step) // idem, but increasing a counter
{
	int n=0;
	q&=1; while ((q==tape_status)&&(*counter8<(256-step))) *counter8+=step,tape_main(tape_step),++n;
	return n;
}
int fasttape_sub8(int q,int tape_step,BYTE *counter8,int step) // idem, but decreasing a counter
{
	int n=0;
	q&=1; while ((q==tape_status)&&(*counter8>step)) *counter8-=step,tape_main(tape_step),++n;
	return n;
}

char fasttape_log2[256]= // precalc'd bitlength; [0] stands for 1<<8= 256
{
	8,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5, 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
};

int FASTTAPE_CAN_FEED(void) { return tape_general_bits?!tape_general_totp&&tape_general_totd>16&&tape_general_count>=0&&tape_main_general_subsym==1:
	(!(tape_pilots+tape_syncs)&&tape_bits>16&&tape_mask>0&&!tape_half); } // check for a single full byte (not the last one!) from its first pulse
BYTE fasttape_feed(int q,int x) // get a whole byte from the stream, but leave the last signal in
{
	int i=tape_byte; // keep for later
	if (tape_general_bits)
	{
		tape_general_totd-=8-tape_general_bits; if (tape_general_count!=8-tape_general_bits)
			i=(((i<<8)+(tape_byte=tape_fgetc()))>>(tape_general_count+=tape_general_bits));
		else
			tape_general_count=0;
	}
	else
	{
		tape_bits-=7; if (tape_mask!=128)
			i=(((i<<8)+(tape_byte=tape_fgetc()))>>fasttape_log2[tape_mask<<=1]);
		else
			tape_mask=1;
	}
	fasttape_skip(q,x); return i;
}
int FASTTAPE_CAN_DUMP(void) { return tape_general_bits?tape_general_totd>=24:tape_bits>=24; } // all but the final bytes!
BYTE fasttape_dump(void) // get a whole byte from the stream and consume all signals
{
	int i=tape_byte; tape_byte=tape_fgetc();
	if (tape_general_bits)
	{
		tape_general_totd-=8; if (tape_general_count!=8-tape_general_bits)
			i=(((i<<8)+tape_byte)>>(tape_general_count+tape_general_bits));
	}
	else
	{
		tape_bits-=8; if (tape_mask!=128)
			i=(((i<<8)+tape_byte)>>(fasttape_log2[tape_mask]+1));
	}
	return i; // no `fasttape_skip` here
}

void fasttape_gotonext(void) // skips the current block (minus the last bits) if the PILOT and SYNCS are already over
{
	if (!(tape_pilots|tape_syncs|tape_general_totp)&&(tape_bits|tape_wave|tape_kansas|tape_general_totd|tape_hold))
	{
		int i; if (tape_bits>8)
			tape_skip(i=(tape_bits-1)/8),tape_bits-=i*8;
		else if (tape_wave>8)
			tape_skip(i=(tape_wave-1)/8),tape_wave-=i*8;
		#ifdef TAPE_KANSAS_CITY
		else if (tape_kansas>1)
			tape_skip(tape_kansas-1),tape_kansas=1;
		#endif
		else if ((tape_general_totd*tape_general_bits)&&(tape_general_totd>8/tape_general_bits))
		{
			int j=8/tape_general_bits;
			tape_skip(i=(tape_general_totd-1)/j);
			tape_general_totd-=i*j;
		}
		else if (tape_hold!=1&&tape_hold&&!(tape_bits|tape_wave|tape_kansas|tape_general_totd))
			tape_hold=1;
	}
	tape_loop=0; // just in case a tape uses this!
}

// ============================================== END OF TAPE SUPPORT //
