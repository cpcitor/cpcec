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
	int i=tape_fgetc();
	return i+(tape_fgetc()<<8); // this keeps the sign of the byte!
}
int tape_fgetcccc(void) // reads four little-endian bytes; cfr. tape_fgetc()
{
	int i=tape_fgetc();
	i+=tape_fgetc()<<8;
	i+=tape_fgetc()<<16;
	return i+(tape_fgetc()<<24); // in 32-bit systems the sign will be lost!
}
#define tape_ungetc() (--tape_filetell,--tape_offset)
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
int tape_pilot,tape_pilots,tape_sync,tape_syncs,tape_syncz[256],tape_bits,tape_bit0,tape_bit1,tape_byte,tape_half,tape_mask,tape_wave,tape_hold,tape_loop,tape_looptell; // special TZX/CDT tape parameters
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
	int q=1,l; // temporary integer and error flag
	tape_closed=tape_offset=tape_length=tape_bits=tape_playback=0; // reset buffer!
	if (fread(tape_buffer,1,8,tape)==8) // guess the format
	{
		if (!memcmp("RIFF",tape_buffer,4)) // WAV?
		{
			if (fgetmmmm(tape)==0x57415645) // "WAVE" ID
				while ((q=fgetmmmm(tape))>0&&(l=fgetiiii(tape))>=0&&q!=0x64617461) // read chunks until "data"
				{
					//logprintf("[%08X:%08X]",q,l);
					if (q==0x666D7420) // "fmt " is the only chunk that we must process!
					{
						l-=fread(tape_buffer,1,16,tape); // read the first part of the "fmt " chunk
						tape_playback=tape_buffer[0x08]+(tape_buffer[0x09]<<8)+(tape_buffer[0x0A]<<16); // to handle any bit depth and channel amount (even if it's obvious that only mono signal is reliable)
						tape_wave=(tape_buffer[0x0E]-1)/8;
						//logprintf("<%i>",tape_playback);
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
			tape_status=tape_count=q=tape_pilots=tape_syncs/*=tape_bits*/=tape_wave=tape_loop=0; // force HOLD
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
			while (p--)
			{
				int z; for (z=0;z<tape_wave;++z)
					tape_fgetc(); // clip 16-BIT lsb
				if ((tape_status=tape_fgetc())<0) // EOF?
					if (tape_endoftape())
						return; // quit!
				tape_status=tape_status>128; // tape_status>>=7
			}
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
					else if (tape_hold) // HOLD?
					{
						tape_count+=3500;
						if (tape_hold>0)
							tape_status^=1,tape_hold=1-tape_hold;
						else
							tape_status=0,++tape_hold;
					}
					// fetch new blocks if required
					#ifdef TAPE_OPEN_TAP_FORMAT
					if (tape_type==3) // TAP
					{
						if (!(tape_pilots|tape_syncs|tape_bits|tape_hold)) //
						{
							tape_half=tape_mask=0;
							tape_hold=2*1000; // 2-second gap
							tape_bits=tape_fgetcc()<<3;
							//logprintf("<%04X> ",tape_bits>>3);
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
						while (!(tape_pilots|tape_syncs|tape_bits|tape_wave|tape_hold))
						{
							tape_half=tape_mask=0;
							//logprintf("%08X:",tape_filetell);
							int i=tape_fgetc();
							//logprintf("TZX[%02X] ",i);
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
									//logprintf("%08X!\n",tape_wave);
									break;
								case 0x20: // HOLD (or STOP)
									tape_hold=tape_fgetcc();
									break;
								case 0x2A: // STOP if 48K
									tape_fgetcccc();
									break;
								// the following blocks are only moderately useful when we edit the tape
								case 0x21: // !GROUP START
									tape_skip(tape_fgetc());
									break;
								case 0x22: // !GROUP END
									break;
								// the following blocks are never used in tapes: they do more harm than good!
								case 0x23: // !JUMP TO GROUP
									tape_fgetcc();
									logprintf("UNSUPPORTED TZX BLOCK 0x23!\n");
									break;
								case 0x24: // !LOOP START
									tape_loop=tape_fgetcc();
									tape_looptell=tape_filetell;
									break;
								case 0x25: // !LOOP END
									if (tape_loop&&--tape_loop)
										tape_seek(tape_looptell);
									break;
								case 0x26: // !CALL SEQUENCE
									tape_skip(tape_fgetc()*2);
									logprintf("UNSUPPORTED TZX BLOCK 0x26!\n");
									break;
								case 0x27: // !RETURN FROM CALL
									logprintf("UNSUPPORTED TZX BLOCK 0x27!\n");
									break;
								case 0x28: // !SELECT BLOCK
									tape_skip(tape_fgetcc());
									logprintf("UNSUPPORTED TZX BLOCK 0x28!\n");
									break;
								// the following blocks are informative, but don't play a role in playback
								case 0x31: // *MESSAGE BLOCK
									tape_fgetc(); // no `break`!
								case 0x30: // *TEXT DESCRIPTION
									#ifdef CONSOLE_DEBUGGER
									{
										i=tape_fgetc();
										char blah[STRMAX];
										int j; for (j=0;j<i;++j)
											blah[j]=tape_fgetc();
										blah[j]=0;
										logprintf("TZX: '%s'\n",blah);
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
								case 0x5A: // *GLUE
									tape_skip(9);
									break;
								default: // *UNKNOWN!
									tape_skip(tape_fgetcccc()); // all unknown blocks are defined this way
									break;
							}
						}
					//logprintf("TONE %i SYNC %i BITS %i WAVE %i HOLD %i\n",tape_pilots,tape_syncs,tape_bits,tape_wave,tape_hold);
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
	if (i)
	{
		while (i--)
			*(*t)++=tape_fgetc();
		if (!(*t)[-1]) // purge trailing zero (f.e. SPEEDKING.CDT)
			--(*t);
	}
	*(*t)++=0;
}
#define TAPE_CATALOG_HEAD "%010i --"
int tape_catalog(char *t,int x)
{
	if (!tape||tape_type<0)
		return *t=0,-1;
	int i,j,p=-1;
	if (tape_type<2) // sample?
	{
		for (i=0;i<100;i+=5)
		{
			j=tape_filebase+(i*(tape_filesize-tape_filebase)/100);
			if (j<=tape_filetell)
				++p;
			t+=1+sprintf(t,TAPE_CATALOG_HEAD " sample block %02i%%",j,i);
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
				t+=1+sprintf(t,TAPE_CATALOG_HEAD " %i bytes",j=tape_filetell-2,i);
				if (j<=z)
					++p;
				tape_skip(i);
			}
		else // TZX/CDT
		#endif
			while (t<s&&(i=tape_fgetc())>=0)
			{
				t+=sprintf(t,TAPE_CATALOG_HEAD " ",j=tape_filetell-1);
				if (j<=z&&!u)
					++p;
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
					case 0x20: t+=1+sprintf(t,"HOLD");
						tape_fgetcc();
						break;
					case 0x2A: t+=1+sprintf(t,"STOP on 48K");
						tape_fgetcccc();
						break;
					case 0x21: t+=sprintf(t,"GROUP: "); // !GROUP START
						tape_catalog_text(&t,tape_fgetc());
						u=t;
						break;
					case 0x22: if (u) t=u,u=NULL; // t+=1+sprintf(t,"!GROUP END");
						break;
					case 0x23: t+=1+sprintf(t,"!JUMP TO GROUP");
						tape_fgetcc();
						break;
					case 0x24: t+=1+sprintf(t,"!LOOP START");
						tape_fgetcc();
						break;
					case 0x25: t+=1+sprintf(t,"!LOOP END");
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
					case 0x5A: t+=1+sprintf(t,"*GLUE");
						k=9;
						break;
					default: t+=1+sprintf(t,"*UNKNOWN!");
						k=tape_fgetcccc(); // all unknown blocks are defined this way
						break;
				}
				tape_skip(k+l);
				if (l)
					--t,t+=1+sprintf(t,", %i bytes",l);
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
	tape_count=tape_pilots=tape_syncs=tape_bits=tape_wave=0,tape_hold=2*1000; // reset counter and playback // 2-second gap
}

// tape speed-up hacks ---------------------------------------------- //

int fasttape_test(const BYTE *s,WORD p) // compares a chunk of memory against a pattern; see below for pattern format
{
	WORD a,z;
	for (;;)
		if (*s==0x80) // relative word
		{
			a=p+(signed char)(*++s);
			z=PEEK(p); ++p; z+=PEEK(p)<<8;
			if (a!=z)
				return 0; // word isn't the same
			++s; ++p;
		}
		else // offset+length memcmp
		{
			a=p+(signed char)(*s++);
			z=a+(*s++);
			if (a==z)
				return 1; // end of pattern, OK
			if (z<a||memcmp(&(PEEK(a)),s,z-a))
				return 0; // pattern doesn't match
			p=z; s+=z-a;
		}
}
int fasttape_add8(int q,int tape_step,BYTE *counter8,int step) // scan tape signal until it changes (or timeouts!) and update counter
{
	q&=1; int n=0;
	while ((q==tape_status)&&(*counter8<(256-step)))
		*counter8+=step,tape_main(tape_step),++n;
	return n;
}
int fasttape_sub8(int q,int tape_step,BYTE *counter8,int step) // idem, but negative
{
	q&=1; int n=0;
	while ((q==tape_status)&&(*counter8>step))
		*counter8-=step,tape_main(tape_step),++n;
	return n;
}
void fasttape_skip(int q,int x) // skip the signal in steps of `x` until it changes
{
	q&=1;
	while (q==tape_status&&(tape||tape_hold))
		tape_main(x);
}

BYTE fasttape_dump(void) // get a byte from the stream, aligned or not, and leave no edges at all
{
	tape_bits-=8;
	int b=tape_mask,i=tape_byte<<8;
	i+=tape_byte=tape_fgetc();
	while ((b<<=1)<=128)
		i<<=1;
	return i>>=8;
}
BYTE fasttape_feed(void) // get a byte from the stream, aligned or not, and leave the last edge in
{
	tape_bits-=7;
	if (tape_mask==128)
		return tape_mask=1,tape_byte;
	int b=tape_mask,i=tape_byte<<8;
	i+=tape_byte=tape_fgetc();
	do
		i<<=1;
	while ((b<<=1)<128);
	return tape_mask<<=1,i>>=8;
}
#define FASTTAPE_CAN_FEED() (tape_mask&&!(tape_pilots|tape_syncs|tape_half)&&tape_bits>7) // general purpose loader handling
#define FASTTAPE_CAN_FEEDX() (tape_mask&&!(tape_pilots|tape_syncs)&&tape_bits>7) // polarity-dependent loaders may need this
#define FASTTAPE_FEED_END(x,y) fasttape_skip((x),(y))

void fasttape_skipbits(void) // skips the current block if the PILOT and SYNCS are already over
{
	if (/*tape_mask&&*/!(tape_pilots|tape_syncs|tape_half)&&(tape_bits|tape_wave|tape_hold))
	{
		if (tape_bits>8)
		{
			tape_skip(tape_bits/8);
			tape_bits-=(tape_bits/8)*8;
		}
		else if (tape_wave>8)
		{
			tape_skip(tape_wave/8);
			tape_wave-=(tape_wave/8)*8;
		}
		else if (tape_hold!=1&&tape_hold&&!(tape_bits|tape_wave))
			tape_hold=1;
	}
}

// ============================================== END OF TAPE SUPPORT //
