 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The NEC 765 floppy disc controller found in the Amstrad CPC664 and
// CPC6128 (as well as the optional disc drive DDI-1 for the CPC464)
// was common in IBM PC and compatibles (sometimes the Intel 8272A took
// its place) and other home computers such as the Spectrum +3.

// Disc file support is based on the DSK format, both in its original
// "MV - CPC" definition and the later "EXTENDED" revised format.

// BEGINNING OF DISC SUPPORT ======================================== //

FILE *disc[4]={NULL,NULL,NULL,NULL}; // disc file handles
BYTE disc_change[4],disc_motor,disc_track[4],disc_flip[4],disc_canwrite[4]; // current motor status, drive tracks, sides and protections

// the general idea is that each drive can be handled independently;
// as a result, we must assign each drive a set of data structures.

BYTE disc_index_table[4][256]; // "MV - CPC" disc header - one for each drive
BYTE disc_track_table[8][512]; // current track info - one for each drive AND side, because one SEEK TRACK enables access to BOTH sides of said track!
int disc_track_offset[8]; // current tracks' file offsets - one for each drive (drives A..D: 0..3) and side (side A +0, side B +4)

// each drive can hold a disc and point at a track, but the FDC is limited to one operation at once,
// so parameters, buffers, pointers, counters, etc. are unique for the whole system.

BYTE disc_parmtr[9]; // byte 0 is the command, next bytes are parameters, most often "DRIVE,C,H,R,N,LAST SECTOR,GAP,SECTOR LENGTH"
BYTE disc_result[7]; // results are almost always arranged as "ST0,ST1,ST2,C,H,R,N" where ST0, ST1 and ST2 are the status bytes.
BYTE disc_buffer[128<<8]; // disc buffer data; real discs are actually limited to 6.4 kB/track, below the expected 8 kB/sector.
int disc_offset,disc_length,disc_lengthfull; // disc buffer parameters

// bit 0..3: DRIVE A..D BUSY (i.e. true during seek or recalibrate operations)
// bit 4..7: DRIVE A..D DONE (i.e. true after changes in the drives' status)
BYTE disc_status;

// 0 = IDLE, WAITING FOR COMMAND, 1 = TAKING PARAMETERS, 2 = WRITING BYTES ONTO DISC, 3 = READING BYTES FROM DISC, 4 = SENDING RESULTS
BYTE disc_phase; // notice that no commmand takes all stages, and some commands don't perform any actions.
BYTE disc_trueunit,disc_trueunithead; // current unit+head after flipping sides (if feasible)
int disc_delay; // several operations need a short delay between command and action.
int disc_timer; // overrun timer: if nonzero, it decreases.
int disc_overrun; // set if disc_timer dropped to zero!
int disc_filemode=1; // +1 = read-only by default instead of read-write; +2 = relaxed disc write errors instead of strict

// disc file handling operations ------------------------------------ //

char disc_path[STRMAX]="";

void disc_track_reset(int drive) // invalidate drive `d` tracks on both sides
{
	MEMZERO(disc_track_table[drive]); MEMZERO(disc_track_table[drive+4]);
}

void disc_close(int drive) // close disc file. drive = 0 (A:) or 1 (B:)
{
	if (disc[drive])
		disc_track_reset(drive),puff_fclose(disc[drive]);
	disc_change[drive]=1,disc[drive]=NULL;
}
#define disc_closeall() (disc_close(0),disc_close(1))

int disc_open(char *s,int drive,int canwrite) // open a disc file. `s` path, `drive` = 0 (A:) or 1 (B:); 0 OK, !0 ERROR
{
	disc_close(drive);
	if (!(disc[drive]=puff_fopen(s,(disc_canwrite[drive]=canwrite)?"rb+":"rb")))
		return 1; // cannot open disc!
	int q=1; // error flag
	if (fread(disc_index_table[drive],1,256,disc[drive])==256
		&&disc_index_table[drive][0x30]<110&&disc_index_table[drive][0x31]>0&&disc_index_table[drive][0x31]<3
		&&(!memcmp("MV - CPC",disc_index_table[drive],8)||!memcmp("EXTENDED",disc_index_table[drive],8)))
			q=0; // ID and fields are valid
	disc_track_reset(drive);
	if (q)
		disc_close(drive); // unknown disc format!
	else if (disc_path!=s) strcpy(disc_path,s); // valid format
	return q;
}

BYTE disc_scratch[256]; // we don't want to touch buffers that might be active
char disc_header_text[]="EXTENDED Disk-File\015\012" MY_CAPTION " " MY_VERSION "\015\012\032";
char disc_tracks_text[]="Track-Info\015\012";

int disc_create(char *s) // create a clean formatted disc; 0 OK, !0 ERROR
{
	FILE *f;
	if (!(f=puff_fopen(s,"wb")))
		return 1; // cannot create disc!
	MEMZERO(disc_scratch);
	strcpy(disc_scratch,disc_header_text);
	disc_scratch[0x30]=DISC_NEW_TRACKS;
	disc_scratch[0x31]=DISC_NEW_SIDES;
	memset(&disc_scratch[0x34],((DISC_NEW_SECTORS<<DISC_NEW_SECTOR_SIZE_FDC)+3)/2,DISC_NEW_SIDES*DISC_NEW_TRACKS);
	fwrite1(disc_scratch,256,f);
	for (int i=0;i<DISC_NEW_SIDES*DISC_NEW_TRACKS;++i)
	{
		MEMZERO(disc_scratch);
		strcpy(disc_scratch,disc_tracks_text);
		disc_scratch[0x10]=i/DISC_NEW_SIDES;
		disc_scratch[0x11]=i%DISC_NEW_SIDES;
		disc_scratch[0x14]=DISC_NEW_SECTOR_SIZE_FDC;
		disc_scratch[0x15]=DISC_NEW_SECTORS;
		disc_scratch[0x16]=DISC_NEW_SECTOR_GAPS;
		disc_scratch[0x17]=DISC_NEW_SECTOR_FILL;
		int l=128<<DISC_NEW_SECTOR_SIZE_FDC;
		for (int j=0;j<DISC_NEW_SECTORS;++j)
		{
			disc_scratch[j*8+0x18]=i/DISC_NEW_SIDES;
			disc_scratch[j*8+0x19]=i%DISC_NEW_SIDES;
			disc_scratch[j*8+0x1A]=DISC_NEW_SECTOR_IDS[j];
			disc_scratch[j*8+0x1B]=DISC_NEW_SECTOR_SIZE_FDC;
			//disc_scratch[j*8+0x1C]=0;
			//disc_scratch[j*8+0x1D]=0;
			disc_scratch[j*8+0x1E]=l;
			disc_scratch[j*8+0x1F]=l>>8;
		}
		fwrite1(disc_scratch,256,f);
		memset(disc_scratch,DISC_NEW_SECTOR_FILL,256);
		l=((DISC_NEW_SECTORS<<DISC_NEW_SECTOR_SIZE_FDC)+1)/2;
		while (l--)
			fwrite1(disc_scratch,256,f);
	}
	return puff_fclose(f);
}

// disc operations -------------------------------------------------- //

#define disc_setup() MEMZERO(disc_flip)
#define disc_reset() { disc_motor=disc_phase=disc_delay=disc_timer=disc_overrun=disc_status=0; }

int disc_sector_last=0; // custom formats may have repeated CHRN IDs; this helps us tell them apart
int disc_sector_weak=0; // custom sectors may include "weak" bytes distributed across multiple copies
int disc_sector_size(int d,int j) // get size of sector `j` (0: first, 1: second...) at drive+side `d`
{
	return disc_track_table[d][j*8+0x1E]+disc_track_table[d][j*8+0x1F]*256;
}

// tracks are unique to each disc and are divided in two halves ("sides") that operate at the same time.

int disc_track_load(int d,int c) // setup track `c` from drive `d`; 0 OK, !0 ERROR
{
	disc_track_reset(d);
	disc_track_table[d][0]=1; // track may be empty, but we checked it
	if (!disc[d]||c<0||c>=disc_index_table[d][0x30]) // fail if no disc or invalid track
		return 1;
	int i=256; // disc header must be skipped
	int j=c*disc_index_table[d][0x31],k;
	if (disc_index_table[d][0]=='M') // old style
		i+=j*(k=(disc_index_table[d][0x32]+disc_index_table[d][0x33]*256)); // all tracks are the same size, in bytes
	else // new style
	{
		k=disc_index_table[d][j+0x34];
		while (j--)
			i+=disc_index_table[d][j+0x34]*256; // each track stores its own size, in 256-byte pages
	}
	fseek(disc[d],disc_track_offset[d]=i,SEEK_SET);
	if (k>1) // tracks without a body are equivalent to empty tracks!
		fread1(disc_track_table[d],512,disc[d]); // tracks with >29 sectors use 512 bytes rather than just the first 256 bytes
	if (disc_index_table[d][0x31]>1) // is it a two-sided disc?
	{
		if (disc_index_table[d][0]=='M') // old style
			i+=k; // ditto
		else // new style
		{
			k=disc_index_table[d][c*disc_index_table[d][0x31]+1+0x34];
			i+=disc_index_table[d][c*disc_index_table[d][0x31]+0x34]*256; // ditto
		}
		fseek(disc[d],disc_track_offset[d+4]=i,SEEK_SET);
		if (k>1)
			fread1(disc_track_table[d+4],512,disc[d]);
	}
	if (disc_index_table[d][0]=='M') // old style discs lack the explicit size field and thus require calculation
		for (k=d;k<8;k+=4) // check both sides
			if ((i=128<<disc_track_table[k][0x14])<(128<<7)) // sector size is defined in the track header
				for (j=0;j<disc_track_table[k][0x15];++j)
					disc_track_table[k][j*8+0x1E]=i,disc_track_table[k][j*8+0x1F]=i>>8;
	//cprintf("(%08X:%08X %d:%d) ",disc_track_offset[d],disc_track_offset[d+4],disc_track_table[d][0x15],disc_track_table[d+4][0x15]);
	return 0;
}
void disc_track_update(void) // force reload of all tracks
{
	for (int i=0;i<4;++i)
		disc_track_load(i,disc_track[i]);
}

// sectors, on the other hand, unlike tracks, may repeatedly show identical IDs even within the same track!
int disc_sector_timer; // looking for a sector takes time; this helps simulating such time.
int disc_sector_find(int d) // look for sector `CHRN` in disc_parmtr[2..5] in track at unit+side `d`; 0 OK, !0 ERROR
{
	disc_change[d&3]=0;
	int i=disc_track_table[d][0x15]; // number of sectors in track
	disc_sector_timer=1;
	while (i--)
	{
		if (++disc_sector_last>=disc_track_table[d][0x15])
			disc_sector_last=0; // wrap to first sector in track
		if (!memcmp(&disc_track_table[d][disc_sector_last*8+0x18],&disc_parmtr[2],4))
			return 0; // CHRN match!
		// this was originally a kludge for MOKTAR / TITUS THE FOX, similar but more demanding than PREHISTORIK 1 that was content by simply raising high DISC_TIMER_INIT!
		//#define DISC_TIMER_STEP (14<<6) // rough approximation too, to make "MOKTAR / TITUS THE FOX" run on CPC.
		//if (disc_parmtr[2]>=89&&disc_parmtr[4]==12&&disc_parmtr[5]==5&&disc_track[disc_trueunit]>=39) disc_timer+=DISC_TIMER_STEP;
		disc_sector_timer+=2;
		//cprintf("{%02X%02X%02X%02X} ",disc_track_table[d][disc_sector_last*8+0x18],disc_track_table[d][disc_sector_last*8+0x19],disc_track_table[d][disc_sector_last*8+0x1A],disc_track_table[d][disc_sector_last*8+0x1B]);
	}
	if (disc_parmtr[4]==0xFF&&disc_parmtr[5]==0x00&&disc_parmtr[6]==0xFF)
		disc_sector_last=-1; // kludge: satisfy TCOPY3 (that performs 46 00,00,00,FF,00,FF,00,80 before the first READ ID) without hurting neither DALEY TOC, 5KB DEMO 3 or DESIGN DESIGN games!
	return 1; // sector not found!
}
int disc_skew_length,disc_skew_filler; // required when READ SECTOR with N > physical size forces inserting inter-sector bytes
void disc_sector_seek(int d,int z) // seek sector `z` (usually `disc_sector_last`) in current track at unit+side `d`; 0 OK, !0 ERROR
{
	disc_offset=0; // reset buffer
	int i=disc_track_table[d][0x15]>29?512:256,j; // track header must be skipped
	for (j=0;j<z;++j)
		i+=disc_sector_size(d,j);
	disc_length=128<<(disc_track_table[d][j*8+0x1B]&15); // sector size is defined by the sector's own N, but value isn't always reliable; see below.
	if ((disc_lengthfull=disc_sector_size(d,j))>sizeof(disc_buffer))
		disc_lengthfull=sizeof(disc_buffer);
	// an inconvenient in the definition of the format is that extra copies of the sector can be stored (i.e. "weak sectors") as well as the bytes kept in the GAP between sectors ("READ TRACK" reveals them)
	// but the definition itself isn't clear on how to tell apart whether extra bytes are meant to be extra copies, GAP bytes, or even both. Hence the following heuristics:
	if (disc_length>=disc_lengthfull) // explicit size is smaller than implicit size?
	{
		if (disc_length==0x0800&&disc_lengthfull==0x0100)/*&&disc_track_table[d][j*8+0x1C]==0x20*/ // ERE SOFTWARE (is this used elsewhere?)
			disc_skew_length=32,disc_skew_filler=disc_track_table[d][0x22],cprintf("(ERE!:%02X) ",disc_skew_filler); // this is where the 0x44 comes from; it's part of a complex header but it works
		else if (disc_length==0x2000&&disc_lengthfull<0x0900) // the educational "REUSSIR..." series
			disc_skew_length=disc_length-disc_lengthfull,disc_skew_filler=disc_buffer[511]/*0xCB*/,cprintf("(REU!:%02X) ",disc_skew_filler); // this is what the strange filler byte is compared against
		else
			disc_skew_length=0;
		disc_length=disc_lengthfull; // clip the size accordingly!
	}
	else //if (disc_track_table[d][j*8+0x1B]<7) // is the size legal? when N is 7 or higher the sector is completely nonstandard and is dumped in a single piece
	{
		disc_skew_length=0;
		if (disc_track_table[d][j*8+0x1D]&32) // CRC ERROR (0x20) bit is on?
		{
			if (++disc_sector_weak>=(disc_lengthfull/disc_length)) // out of copies?
				disc_sector_weak=0; // wrap to first!
			i+=disc_length*disc_sector_weak; // pick a copy
			disc_lengthfull=disc_length; // set sector size
		}
	}
	fseek(disc[d&3],disc_track_offset[d]+i,SEEK_SET);
	cprintf("<%08X:%04X> ",disc_track_offset[d]+i,disc_length);
}

#define DISC_RESULT_LAST_CHRN() memcpy(&disc_result[3],&disc_parmtr[2],4)
#define DISC_RESULT_DISC_CHRN() memcpy(&disc_result[3],&disc_track_table[disc_trueunithead][disc_sector_last*8+0x18],4)
#define DISC_HELD_OR_MISSING(x) (!disc_motor||!disc[x])
int disc_initstate(void) // prepares for a new operation; 0 OK, !0 ERROR
{
	MEMZERO(disc_result);
	disc_trueunit=(disc_trueunithead=DISC_PARMTR_UNITHEAD)&3;
	if (disc_index_table[disc_trueunit][0x31]>1)
	{
		if (disc_flip[disc_trueunit])
			disc_trueunithead^=4; // handle user-commanded side flipping
	}
	else
		disc_trueunithead&=~4; // single-sided disc, ignore SIDE bit
	if (!disc_track_table[disc_trueunithead][0x15]&&!disc_track_table[disc_trueunit][0])
		disc_track_load(disc_trueunit,disc_track[disc_trueunit]);
	return (disc_result[0]=(disc_parmtr[1]&7)+(DISC_HELD_OR_MISSING(disc_trueunit)?0x48:0))&8; // 0x40 = Abnormal Termination; 0x08 = Not Ready
}
void disc_exitstate(void) // sets the state bits after the end of a operation
{
	#if DISC_WIRED_MODE
	#else
		disc_result[0]|=0x40; // 0x40: Abnormal Termination
		disc_result[1]|=0x80; // 0x80: End of Cylinder
	#endif
	if ((disc_result[1]|disc_result[2])&0x7F) // did any errors happen?
	{
		#if DISC_WIRED_MODE
		#else
			disc_result[1]&=~0x80;
		#endif
		if ((disc_result[1]|disc_result[2])&0x20) // stopped because of errors?
			disc_result[2]&=~0x40; // remove Deleted Data
		#if DISC_WIRED_MODE
		#else
			else if (disc_result[2]&0x40) // stopped because of the DELETED bits?
				disc_result[0]&=~0x40,disc_result[1]&=~0x80; // remove Abnormal Termination and End of Cylinder
		#endif
	}
	DISC_RESULT_LAST_CHRN();
}

void disc_sector_loadgaps(void) // used by READ TRACK
{
	if (disc_sector_last>=disc_track_table[disc_trueunithead][0x15])
		disc_sector_last=0; // wrap to first sector in track
	disc_sector_seek(disc_trueunithead,disc_sector_last);
	fread1(disc_buffer,disc_lengthfull,disc[disc_trueunit]); // normal case
	// UBI SOFT's discs need padding, but Batman the Movie (Spectrum +3) rejects it (?): parameter 2 is nonzero when padding must be skipped (!)
	if (disc_length==disc_lengthfull&&!disc_parmtr[2]) // pad the length with inter-sector bytes?
	{
		int i=2+disc_track_table[disc_trueunithead][0x16]+12+3+7+(disc_sector_last+1==disc_track_table[disc_trueunithead][0x15]?374:22); // size of padding
		disc_length=disc_lengthfull;
		cprintf("(GAP!:%04X) ",i);
		memset(&disc_buffer[disc_length],0x4E,i); // first layer: enough to satisfy "Conspiration de l'an III"
		memset(&disc_buffer[disc_length+i-12-3-7-22],0,12); // second layer: required by "E.X.I.T." and "Le Necromancien"
		//memset(&disc_buffer[disc_length+i-3-7-22],0xA1,3); // third layer, it happens always, but not sure if necessary
		disc_lengthfull=disc_length+=i;
	}
	else
		disc_length=disc_lengthfull; // clip! Batman the Movie for Spectrum doesn't want any inter-sector data!
	disc_delay=1,disc_phase=3;
	disc_timer=DISC_TIMER_INIT;
}
void disc_sector_load(void) // read a sector and set the status flags accordingly
{
	do
	{
		if (disc_sector_find(disc_trueunithead)) // sector not found?
		{
			disc_result[0]|=0x40; // 0x40: Abnormal Termination
			disc_result[1]|=0x04; // 0x04: No Data
			DISC_RESULT_LAST_CHRN();
			disc_length=7;
			disc_phase=4;
		}
		else
		{
			// possible errors: 0x20: CRC error; 0x04: cannot find sector ID; 0x01: cannot find ID mark
			disc_result[1]=disc_track_table[disc_trueunithead][disc_sector_last*8+0x1C]&0x25;
			// possible errors: 0x40: deleted data; 0x20: CRC error in data; 0x01: missing address mark
			disc_result[2]=disc_track_table[disc_trueunithead][disc_sector_last*8+0x1D]&0x61;
			if ((disc_parmtr[0]&31)==0x0C) // command is READ DELETED DATA?
				disc_result[2]^=0x40; // toggle `deleted` bit
			if ((disc_parmtr[0]&32)&&(disc_result[2]&0x40)) // command includes SKIP DELETED DATA bit and the data is deleted?
			{
				cprintf("<SKIP> ");
				if (disc_parmtr[4]==disc_parmtr[6]) // is it the last requested sector?
				{
					cprintf("<STOP> ");
					disc_exitstate();
					disc_length=7;
					disc_phase=4;
				}
				else
					disc_length=-1,++disc_parmtr[4]; // ignore this sector and move on
			}
			else // read sector proper
			{
				if (disc_result[2]&0x40) // if DELETED data and operations don't match the operation stops
					disc_parmtr[6]=disc_parmtr[4];
				disc_sector_seek(disc_trueunithead,disc_sector_last);
				cprintf("[RD %04X] ",disc_length);
				fread1(disc_buffer,disc_length,disc[disc_trueunit]);
				disc_delay=1,disc_phase=3;
				disc_timer=DISC_TIMER_INIT*disc_sector_timer;
			}
		}
	}
	while (disc_length<0); // repeat until either success or failure!
}

// formatting a track requires rearranging the whole disc both on file and on memory
// 1.- prepare a backup of the data after the current track
// 2.- modify the disc header to include the new track size
// 3.- store the new track in place of the current track
// 4.- dump the backup after the current track
// 5.- discard the backup
// difficult enough? if the disc is old style we also have to convert it to new style on the fly!
// 1.- prepare a backup of the data after the current track
// 2.- turn the disc header into "EXTENDED" while including the new track size
// 3.- modify all the tracks before the current one to fit the new style
// 4.- store the new track in place of the current track
// 5.- dump the backup after the current track while modifying the tracks into new style
// 6.- discard the backup
void disc_track_format_old2new(BYTE *t) // converts a single track from MV - CPC (old) to EXTENDED (new) style; `t` is the header
{
	int i,j;
	if ((i=128<<t[0x14])<(128<<7)) // sector size is defined in the track header
		for (j=0;j<t[0x15];++j)
			t[j*8+0x1E]=i,t[j*8+0x1F]=i>>8;
}
void disc_track_format(void)
{
	int j=0,l128=0; // length of new track (header and data) in 128-byte chunks
	for (int i=0;i<disc_parmtr[3];++i) // disc_parmtr[2] is useless to calculate the REAL length of the track!
	{
		cprintf("%02X%02X%02X%02X ",disc_buffer[0+i*4],disc_buffer[1+i*4],disc_buffer[2+i*4],disc_buffer[3+i*4]);
		//if (disc_buffer[0+i*4]|disc_buffer[1+i*4]|disc_buffer[2+i*4]|disc_buffer[3+i*4]) // skip dummy sectors? (Rubi's self-copier for "The Demo")
		{
			memmove(&disc_buffer[j*4],&disc_buffer[i*4],4),++j; // keep non-dummy sector
			l128+=1<<disc_buffer[3+i*4]; // layout is "CHRN", as usual; we use N here
		}
	}
	disc_parmtr[3]=j; // all dummy sectors are gone now
	j=disc_track[disc_trueunit]*disc_index_table[disc_trueunit][0x31]+((disc_trueunithead&4)?1:0); // location on file: (track*heads)+head
	if (j<0||j>=84*disc_index_table[disc_trueunit][0x31]||(!disc_canwrite[disc_trueunit])) // invalid track? (up to 84) write protected?
	{
		disc_result[0]=0x40|(disc_parmtr[1]&7); // 0x40: Command Aborted
		disc_result[1]=0x02; // 0x02: Not Writeable
		DISC_RESULT_LAST_CHRN();
		disc_length=7;
		disc_phase=4;
		return;
	}

	// adjust the header, as formatting can lead to differently sized tracks that don't fit in "MV - CPC" discs
	int m; if (m=(disc_index_table[disc_trueunit][0]=='M')) // old style
	{
		strcpy(disc_index_table[disc_trueunit],disc_header_text);
		memset(&disc_index_table[disc_trueunit][0x34],disc_index_table[disc_trueunit][0x33],disc_index_table[disc_trueunit][0x30]*disc_index_table[disc_trueunit][0x31]);
		disc_index_table[disc_trueunit][0x33]=0;
	}

	// is the new track formatted or unformatted?
	BYTE q=disc_index_table[disc_trueunit][j+0x34];
	MEMZERO(disc_track_table[disc_trueunithead]);
	cprintf("[FR %02X:%02X:%04X] ",disc_parmtr[2],disc_parmtr[3],l128);
	if (disc_parmtr[2]>=7||disc_parmtr[3]>29||l128>49)
		disc_index_table[disc_trueunit][j+0x34]=l128=0; // track is unformatted, no header or body
	else
	{
		if (disc_index_table[disc_trueunit][0x30]<=disc_track[disc_trueunit])
			disc_index_table[disc_trueunit][0x30]=1+disc_track[disc_trueunit]; // augment tracks if required
		disc_index_table[disc_trueunit][j+0x34]=((l128+3)/2); // header + 256-byte-chunks body
		strcpy(disc_track_table[disc_trueunithead],disc_tracks_text);
		disc_track_table[disc_trueunithead][0x10]=disc_track[disc_trueunit];
		disc_track_table[disc_trueunithead][0x11]=(DISC_PARMTR_UNITHEAD&4)/4; // not disc_trueunithead!
		memcpy(&disc_track_table[disc_trueunithead][0x14],&disc_parmtr[2],4); // formatting parameters
		for (int i=0;i<disc_parmtr[3];++i)
		{
			memcpy(&disc_track_table[disc_trueunithead][i*8+0x18],&disc_buffer[0+i*4],4); // CHRN
			int l=128<<disc_buffer[3+i*4]; //disc_parmtr[2] is useless, again; we use N here
			disc_track_table[disc_trueunithead][i*8+0x1E]=l;
			disc_track_table[disc_trueunithead][i*8+0x1F]=l>>8;
		}
	}

	int i=0,new_offset=256,old_offset=0,old_length=0;
	while (i<j)
		new_offset+=disc_index_table[disc_trueunit][0x34+i++]<<8;
	old_offset=new_offset+(q<<8);
	if (q-=disc_index_table[disc_trueunit][j+0x34]||m) // will the file size change? did the style change? backup if required!
		while (++i<disc_index_table[disc_trueunit][0x30]*disc_index_table[disc_trueunit][0x31])
			old_length+=disc_index_table[disc_trueunit][0x34+i]<<8;

	char *disc_backup=NULL;
	if (old_length)
	{
		disc_backup=malloc(old_length);
		fseek(disc[disc_trueunit],old_offset,SEEK_SET);
		fread1(disc_backup,old_length,disc[disc_trueunit]);
	}
	fseek(disc[disc_trueunit],0,SEEK_SET);
	fwrite1(disc_index_table[disc_trueunit],256,disc[disc_trueunit]);
	if (m)
		for (int i=0;i<j;++i)
		{
			fread1(disc_scratch,256,disc[disc_trueunit]); // load the header
			fseek(disc[disc_trueunit],-256,SEEK_CUR); // rewind
			disc_track_format_old2new(disc_scratch); // update header
			fwrite1(disc_scratch,256,disc[disc_trueunit]); // save the header
			fseek(disc[disc_trueunit],(disc_index_table[disc_trueunit][0x34+i]-1)<<8,SEEK_CUR); // next track
		}
	else
		fseek(disc[disc_trueunit],new_offset,SEEK_SET);
	if (l128)
	{
		fwrite1(disc_track_table[disc_trueunithead],256,disc[disc_trueunit]);
		memset(disc_buffer,disc_parmtr[5],sizeof(disc_buffer)); // filler!
		fwrite1(disc_buffer,((l128+1)/2)<<8,disc[disc_trueunit]);
	}
	if (old_length)
	{
		if (m)
		{
			int i=j,k=0;
			while (++i<disc_index_table[disc_trueunit][0x30]*disc_index_table[disc_trueunit][0x31])
			{
				disc_track_format_old2new(&disc_backup[k]); // update header
				k+=disc_index_table[disc_trueunit][0x34+i]<<8; // next track
			}
		}
		fwrite1(disc_backup,old_length,disc[disc_trueunit]);
		free(disc_backup);
	}
	if (q) // did the file size change? truncate!
		fsetsize(disc[disc_trueunit],new_offset+(disc_index_table[disc_trueunit][j+0x34]<<8)+old_length);

	// and it's done!
	disc_exitstate();
	disc_length=7;
	disc_phase=4;
}

// disc port handling ----------------------------------------------- //

void disc_motor_set(BYTE b)
{
	disc_motor=b;
	disc_status|=0x30; // raise OKEY status on drives!
}

void disc_data_send(BYTE b) // DATA I/O
{
	if (disc_phase==0) // IDLE?
	{
		cprintf("FDC %08X: %02X ",DISC_CURRENT_PC,b);
		disc_offset=disc_phase=1;
		switch ((disc_parmtr[0]=b)&31)
		{
			case 0x06: // READ DATA
			case 0x0C: // READ DELETED DATA
			case 0x05: // WRITE DATA
			case 0x09: // WRITE DELETED DATA
			case 0x11: // SCAN EQUAL
			case 0x19: // SCAN LOW OR EQUAL
			case 0x1D: // SCAN HIGH OR EQUAL
			case 0x02: // READ TRACK
				disc_length=9;
				break;
			case 0x0A: // READ ID
				disc_length=2;
				break;
			case 0x0D: // FORMAT TRACK
				disc_length=6;
				break;
			case 0x07: // RECALIBRATE
				disc_length=2;
				break;
			case 0x08: // SENSE INTERRUPT STATUS
				++disc_sector_last; // the protected versions of "RENEGADE" and "ARKANOID 2" lose time between SEEK and READ ID
				disc_result[0]&=~7;
				disc_length=2;
				int i;
				for (i=0;i<4;++i)
				{
					if (disc_status&(0x01<<i)) // BUSY DRIVE A?
					{
						disc_result[0]|=0x20+i; // 0x20: Seek End; Drive ID
						disc_result[1]=disc_track[i];
						disc_status&=~(0x11<<i); // reset DRIVE A bits
						break; // no more tests
					}
					if (disc_status&(0x10<<i)) // DONE DRIVE A?
					{
						disc_result[0]|=0xC0+i; // 0xC0: Status Change; Drive ID
						disc_result[1]=disc_track[i];
						if (DISC_HELD_OR_MISSING(i))
							disc_result[0]|=0x08; // 0x08: Not Ready
						disc_status&=~(0x10<<i); // reset DRIVE A bit4
						break; // no more tests
					}
				}
				if (i>=4)
				{
					disc_result[0]=0x80; // 0x80: Bad Command!
					disc_length=1; // result is just one byte!
				}
				disc_phase=4;
				disc_offset=0;
				break;
			case 0x03: // SPECIFY STEP AND HEAD LOAD
				disc_length=3;
				break;
			case 0x04: // SENSE DRIVE STATUS
				disc_length=2;
				break;
			case 0x0F: // SEEK
				disc_length=3;
				break;
			default: // INVALID COMMAND
				disc_result[0]=0x80; // ERROR CODE!
				disc_length=1;
				disc_phase=4;
				disc_offset=0;
				break;
		}
	}
	else if (disc_phase==1) // PARAMETERS?
	{
		cprintf("%02X",b);
		disc_parmtr[disc_offset]=b;
		if (++disc_offset>=disc_length)
		{
			disc_offset=0;
			cprintf(" ");
			switch (disc_parmtr[0]&31)
			{
				case 0x05: // WRITE DATA
				case 0x09: // WRITE DELETED DATA
					if (disc_initstate()) // is the disc not ready?
					{
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else if (!disc_track_table[disc_trueunithead][0x15]) // invalid track?
					{
						disc_result[0]|=0x40; // 0x40: Abnormal Termination
						disc_result[1]|=0x01; // 0x01: missing address mark
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else // disc is ready, track is valid
					{
						if (disc_length=disc_parmtr[5]>=7?0:(128<<disc_parmtr[5])) // reject invalid sizes
						{
							disc_delay=1,disc_phase=2;
							disc_timer=DISC_TIMER_INIT;
						}
						else // invalid size
						{
							disc_exitstate(); // nothing to write
							disc_length=7;
							disc_phase=4;
						}
					}
					break;
				case 0x0D: // FORMAT TRACK
					if (disc_initstate()) // is the disc not ready?
					{
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else // disc is ready
					{
						if ((disc_length=disc_parmtr[3]*4)&&disc_length<=224)
						{
							disc_delay=1,disc_phase=2;
							disc_timer=DISC_TIMER_INIT;
						}
						else
							disc_track_format();
					}
					break;
				case 0x11: // *!* SCAN EQUAL
				case 0x19: // *!* SCAN LOW OR EQUAL
				case 0x1D: // *!* SCAN HIGH OR EQUAL
					disc_length=0;
					disc_phase=2;
					break;
				case 0x02: // READ TRACK
					if (disc_initstate()) // is the disc not ready?
					{
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else if (!disc_track_table[disc_trueunithead][0x15]) // invalid track?
					{
						disc_result[0]|=0x40; // 0x40: Abnormal Termination
						disc_result[1]|=0x01; // 0x01: missing address mark
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else // disc is ready, track is valid
					{
						disc_sector_last=equalsii(&disc_track_table[disc_trueunithead][0x20],0x0401),disc_sector_loadgaps(); // kludge: "Le Necromancien" expects READ TRACK to begin at the second sector in track 0
					}
					break;
				case 0x06: // READ DATA
				case 0x0C: // READ DELETED DATA
					if (disc_initstate()) // is the disc not ready?
					{
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else if (!disc_track_table[disc_trueunithead][0x15]) // invalid track?
					{
						disc_result[0]|=0x40; // 0x40: Abnormal Termination
						disc_result[1]|=0x01; // 0x01: missing address mark
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else // disc is ready, track is valid
					{
						disc_sector_load(); // read the first requested sector
						if (!disc_length)
						{
							disc_exitstate(); // nothing to read
							disc_length=7;
							disc_phase=4;
						}
					}
					break;
				case 0x0A: // READ ID
					if (!disc_initstate()) // is the disc ready?
					{
						if (disc_track_table[disc_trueunithead][0x15]) // is the track valid?
						{
							disc_change[disc_trueunit]=0;
							if (++disc_sector_last>=disc_track_table[disc_trueunithead][0x15])
								disc_sector_last=0; // wrap to first sector in track
							DISC_RESULT_DISC_CHRN();
						}
						else // no sectors!
						{
							disc_result[0]|=0x40; // 0x40: Abnormal Termination
							disc_result[1]|=0x01; // 0x01: missing address mark
							DISC_RESULT_LAST_CHRN();
						}
					}
					disc_length=7;
					disc_delay=mgetii(&disc_track_table[disc_trueunithead][0x1E])*2,disc_phase=4; // crude delay that satisfies Ubi Soft's "La Chose de Grotemburg"
					break;
				case 0x04: // SENSE DRIVE STATUS
					disc_result[0]=(disc_parmtr[1]&7)|(disc_initstate()?0:0x20)|(disc_track[disc_trueunit]?0:0x10); // 0x20: Ready; 0x10: Track 0
					if (disc_change[disc_trueunit])
						disc_result[0]|=0x40,disc_change[disc_trueunit]=0; // disc changed!
					if (!disc_canwrite[disc_trueunit])
						if (!(disc_filemode&2)) disc_result[0]|=0x40; // 0x40: Write protected
					if (!(disc_index_table[disc_trueunit][0x31]&1))
						disc_result[0]|=0x08; // 0x08: Two-sided drive
					disc_length=1;
					disc_phase=4;
					break;
				case 0x07: // RECALIBRATE
					disc_parmtr[2]=0; // no `break`! `recalibrate` equals to `seek track 0`
				case 0x0F: // SEEK
					disc_status|=1<<DISC_PARMTR_UNIT; // tag drive as busy!
					disc_initstate(); disc_result[0]&=~0x48; // remove 0x48 because it doesn't matter here (fixes "Basun")
					disc_track_load(disc_trueunit,disc_track[disc_trueunit]=disc_parmtr[2]); // even if there's trouble, the head must move anyway (fixes "Dark Century")
					// kludge: -1 fixes DALEY TOC and DESIGN DESIGN games but breaks 5KB DEMO 3 (that expects 0) while TCOPY3 expects -1 rather than 0!
					disc_sector_last=(disc_parmtr[2]==0&&disc_track_table[disc_trueunithead][0x1A]==0x41&&disc_track_table[disc_trueunithead][0x22]==0xC1&&disc_track_table[disc_trueunithead][0x2A]==0xC6)?0:-1;
					// no `break`!
				case 0x03: // SPECIFY STEP AND HEAD LOAD
					disc_phase=0;
					cprintf("\n");
					break;
			}
		}
		else
			cprintf(",");
	}
	else if (disc_phase==2) // WRITE DATA?
	{
		disc_buffer[disc_offset]=b;
		if (++disc_offset>=disc_length)
		{
			// buffer is full, is command over?
			disc_offset=0;
			disc_overrun|=DISC_HELD_OR_MISSING(disc_trueunit); // stopping the motor or ejecting the disc is fatal!
			switch (disc_parmtr[0]&31)
			{
				case 0x05: // WRITE DATA
				case 0x09: // WRITE DELETED DATA
					if (disc_overrun||disc_sector_find(disc_trueunithead)) // timeout overrun? sector not found?
					{
						disc_overrun=0;
						disc_result[0]|=0x40; // 0x40: Abnormal Termination
						disc_result[1]|=disc_overrun?0x10:0x04; // 0x10: Overrun; 0x04: No Data
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else // write sector proper
					{
						disc_sector_seek(disc_trueunithead,disc_sector_last);
						cprintf("[WR %04X] ",disc_length);
						if (disc_canwrite[disc_trueunit])
						{
							fwrite(disc_buffer,1,disc_length,disc[disc_trueunit]); // warning: this silently fails if the file mode is "rb" instead of "rb+"
							// WRITE DATA (05) and WRITE DELETED DATA (09) reset and set the DELETED flag:
							// we check whether the track header needs updating
							int oldtag=disc_track_table[disc_trueunithead][disc_sector_last*8+0x1D],
								newtag=disc_parmtr[0]&8?oldtag|0x40:oldtag&~0x40;
							if (oldtag!=newtag)
							{
								disc_track_table[disc_trueunithead][disc_sector_last*8+0x1D]=newtag;
								fseek(disc[disc_trueunit],disc_track_offset[disc_trueunithead],SEEK_SET);
								fwrite(disc_track_table[disc_trueunithead],1,disc_track_table[disc_trueunithead][0x15]>29?512:256,disc[disc_trueunit]); // warning: this silently fails if the file mode is "rb" instead of "rb+" (again!)
							}
						}
						else if (!(disc_filemode&2))
							disc_result[1]|=0x02; // 0x02: Not Writeable // warning: this is a cheap way to fool software into writing data
						if ((disc_parmtr[4]==disc_parmtr[6])||((disc_result[1]|disc_result[2])&0x3F)) // is it the last requested sector? did any fatal errors happen?
						{
							disc_exitstate();
							disc_length=7;
							disc_phase=4;
						}
						else // flags are valid
						{
							++disc_parmtr[4]; // move on
							disc_delay=1,disc_phase=2;
							disc_timer=DISC_TIMER_INIT;
						}
					}
					break;
				case 0x0D: // FORMAT TRACK
					disc_track_format();
					break;
				case 0x11: // *!* SCAN EQUAL
				case 0x19: // *!* SCAN LOW OR EQUAL
				case 0x1D: // *!* SCAN HIGH OR EQUAL
					disc_result[0]=0x80|(disc_parmtr[1]&7); // 0x80: Bad Command!
					disc_length=1;
					disc_phase=4;
					break;
			}
		}
		else
			disc_timer=DISC_TIMER_BYTE; // more bytes, more time...
	}
}

BYTE disc_data_recv(void) // DATA I/O
{
	int i=0xFF;
	if (disc_phase==3)
	{
		i=disc_buffer[disc_offset];
		if (++disc_offset>=disc_length)
		{
			// buffer is empty, is command over?
			disc_offset=0;
			disc_overrun|=DISC_HELD_OR_MISSING(disc_trueunit); // stopping the motor or ejecting the disc is fatal!
			switch (disc_parmtr[0]&31)
			{
				case 0x02: // READ TRACK
					if (disc_overrun) // timeout overrun?
					{
						disc_overrun=0;
						disc_result[0]|=0x40; // 0x40: Abnormal Termination
						disc_result[1]|=0x10; // 0x10: Overrun
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else if (!--disc_parmtr[6]) // out of sectors?
					{
						disc_exitstate();
						disc_length=7;
						disc_phase=4;
					}
					else
						++disc_sector_last,disc_sector_loadgaps(); // move on and read the next sector
					break;
				case 0x06: // READ DATA
				case 0x0C: // READ DELETED DATA
					if (disc_overrun) // timeout overrun?
					{
						disc_overrun=0;
						disc_result[0]|=0x40; // 0x40: Abnormal Termination
						disc_result[1]|=0x10; // 0x10: Overrun
						DISC_RESULT_LAST_CHRN();
						disc_length=7;
						disc_phase=4;
					}
					else if (disc_skew_length) // catch special case where inter-sector bytes must be improvised
					{
						memset(disc_buffer,disc_skew_filler,disc_length=disc_skew_length); // extremely crude inter-sector padding
						disc_overrun=disc_skew_length=0;
					}
					else if ((disc_parmtr[4]==disc_parmtr[6])||((disc_result[1]|disc_result[2])&0x3F)) // is it the last requested sector? did any fatal errors happen?
					{
						if ((disc_result[1]|disc_result[2])&0x3F)
							++disc_sector_last; // "BAD CAT" implies that errors waste enough time to miss the very next sector
						disc_exitstate();
						disc_length=7;
						disc_phase=4;
					}
					else
						++disc_parmtr[4],disc_sector_load(); // move on and read the next sector
					break;
			}
		}
		else
			disc_timer=DISC_TIMER_BYTE; // more bytes, more time...
	}
	else if (disc_phase==4)
	{
		i=disc_result[disc_offset];
		if (++disc_offset<disc_length)
			cprintf("%02X:",i);
		else
		{
			cprintf("%02X\n",i);
			disc_offset=disc_phase=disc_timer=0; // results are over, return to IDLE.
		}
	}
	return i;
}

BYTE disc_data_info(void) // STATUS
{
	BYTE i=0x80; // bit 7: Data Register Ready
	if (disc_phase) // is the FDC busy at all? (any phases but 0)
	{
		i+=0x10; //if (disc_phase&2) // bit 4: Busy; is the FDC performing reads or writes (phases 2 and 3)
		{
			if (disc_delay)
				--disc_delay,i=0x10; // bit 4 minus bit 7
			else if (disc_phase&2) // is the FDC performing reads or writes (phases 2 and 3)
				i+=0x20; // bit 5: Reading or Writing
		}
		if (disc_phase>2) // is the FDC sending bytes to the CPU? (phases 3 and 4)
			i+=0x40; // bit 6: FDC-to-CPU
	}
	return i|(disc_status&15);
}

// disc timeout logic ----------------------------------------------- //

INLINE void disc_main(int t) // handle disc drives for `t` clock ticks
{
	int i; // overrun timeouts can happen during WRITING and READING stages!
	if ((disc_phase&2)&&!(disc_parmtr[0]==0x46&&(i=disc_parmtr[4])==disc_parmtr[6]&&i==disc_parmtr[7]&&i==disc_parmtr[8])) // kludge: the second condition helps 5KB DEMO 3 and ORION PRIME work
	{
		static int r=0;
		//cprintf("%d ",t);
		t=(t*DISC_PER_FRAME)+r;
		r=t%TICKS_PER_FRAME;
		disc_timer-=t/TICKS_PER_FRAME;
		while (disc_timer<=0)
		{
			if ((disc_overrun=disc_phase)==3)
				disc_data_recv(); // dummy RECV
			if (disc_phase==2)
				disc_data_send(0xFF); // dummy SEND
			if (!(disc_phase&2))
				break;
		}
	}
}

// ============================================== END OF DISC SUPPORT //
