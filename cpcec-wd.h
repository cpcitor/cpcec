 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The Western Digital diskette chip family can be found inside the MSX
// disc drive (MSX-DOS) and the ZX Spectrum extension BETA128 (TR-DOS),
// but unlike the NEC 765 it had a comparatively simple interface.

// Disc files are simple dumps of all their sectors:
// - MSX-DOS discs (".DSK") are either 360k (N=1) or 720k (N=2) long;
// the layout is 80 tracks x N sides x 9 sectors x 512 bytes.
// - TR-DOS discs (".TRD") are 640k long; ".SCL" is a packed variation;
// both feature 80 tracks x 2 sides x 16 sectors x 256 bytes.

// In practice the discs are handled as pre-loaded sector dumps that
// get specially tagged if modified; hence the use of allocated memory
// instead of file handles. This also means that the emulator must do
// the memory (alloc+free) and file (open+read+write+close) work.
// Think of all this stuff here as more like a Flash card.

// Ports STATUS/COMMAND (0), TRACK (1), SECTOR (2) and DATA (3) always
// behave the same way, but the BUSY port is different on each machine
// and so are the write-only MOTOR, DRIVE and SIDE ports; they must be
// either received (BUSY) or sent (MOTOR, DRIVE, SIDE) by the caller.

// BEGINNING OF DISKETTE SUPPORT ==================================== //

#define diskette_setup() ((void)0)

// these variables must be manually set by the caller; there's also "diskette_drives" that can be a variable or a constant!

int diskette_size[4]={0,0,0,0}; char diskette_canwrite[4];
BYTE *diskette_mem[4]={NULL,NULL,NULL,NULL},diskette_tracks[4],diskette_motor,diskette_drive,diskette_side,diskette_sides[4];

// the following variables are internally used by this module

int diskette_offset,diskette_target,diskette_length;
BYTE diskette_buffer[DISKETTE_PAGE],diskette_cursor[4],diskette_status,diskette_sector,diskette_track,diskette_command;

void diskette_reset(void)
	{ diskette_target=-1; diskette_status=0X04,diskette_sector=diskette_track=diskette_command=diskette_length=diskette_motor=diskette_drive=diskette_side=0; MEMZERO(diskette_cursor); }

#define diskette_recv_busy() (diskette_length>0?1:0) // TODO: return -1 if the operation suffered an error!
#define diskette_recv_data() (diskette_length?--diskette_length,diskette_buffer[diskette_offset++]:255)
#define diskette_recv_sector() (diskette_sector)
#define diskette_recv_track() (diskette_track)
#define diskette_recv_status() ((diskette_command>=0X08&&diskette_command<=0X0F)?diskette_command=0,0X02:diskette_length?0X02:diskette_status)
#define diskette_send_busy(b) (diskette_status=diskette_cursor[diskette_drive&3]?0X00:0X04) // TRACK 0, OK // MSX-DOS relies on this

void diskette_send_data(BYTE b) // send a byte to DATA port
{
	if (diskette_length)
	{
		diskette_buffer[diskette_offset++]=b;
		b=diskette_drive&3; if (!--diskette_length&&diskette_target>=0&&diskette_mem[b]&&diskette_canwrite[b])
		{
			if (diskette_size[b]>0) // tag disc as modified!
				diskette_size[b]=-diskette_size[b];
			if (diskette_command>=0XF0) // cheating the WRITE TRACK operation! see below!
				memset(&diskette_mem[b][diskette_target],diskette_buffer[192],DISKETTE_SECTORS*DISKETTE_PAGE*diskette_sides[b]);
			else
				MEMSAVE(&diskette_mem[b][diskette_target],diskette_buffer);
		}
	}
	else
		*diskette_buffer=b; // the parameter of the SEEK command
}
void diskette_send_sector(BYTE b) // send a byte to SECTOR port
{
	if (!diskette_length) // ignore if busy!
		diskette_sector=b;
}
void diskette_send_track(BYTE b) // send a byte to TRACK port
{
	if (!diskette_length) // ignore if busy!
		diskette_track=b>DISKETTE_TRACK99?DISKETTE_TRACK99:b; // is the maximum checked here? see also the SEEK command
}
void diskette_send_command(BYTE b) // send a byte to COMMAND/STATUS port
{
	cprintf("%08X: DISKETTE %02X %02X:%02X:%02X:%02X",DISKETTE_PC,b,diskette_track,diskette_side*16+diskette_drive,diskette_sector,*diskette_buffer);
	if (b>=13*16&&b<14*16) // command is FORCED INTERRUPTION? this is why there's no "case 13:" below!
		diskette_length=0,diskette_status=0;
	else if (!diskette_length) // ignore if busy!
	{
		char d=diskette_drive&3; // the disc side is chosen from outside the command itself, thru the system config
		char s=diskette_sides[d]>1?diskette_side&1:0; // the disc drive is chosen from outside too
		diskette_target=-1; // tag operation as non-writing
		switch (b>>4)
		{
			case  0: // RESTORE
				diskette_cursor[d]=diskette_track=0X00;
				diskette_status=diskette_drive>=diskette_drives?0X05:0X04; // used by MSX-DOS to detect whether a drive is plugged // TRACK 0
				break;
			case  1: // SEEK
				diskette_cursor[d]=diskette_track=*diskette_buffer>DISKETTE_TRACK99?DISKETTE_TRACK99:*diskette_buffer; // is the maximum checked here? see also the TRACK port
				diskette_status=diskette_track>diskette_tracks[d]?0X10:diskette_track?0X00:0X04; // SEEK ERROR, TRACK 0, OK
				break;
			case 12: // READ ADDRESS
				if (!diskette_motor||!diskette_mem[d])
					diskette_status=0X80; // DISC MISSING?
				else if (diskette_track>=diskette_tracks[d]||diskette_sector<1||diskette_sector>DISKETTE_SECTORS)
					diskette_status=0X10; // RECORD NOT FOUND
				else
				{
					diskette_buffer[0]=diskette_track;
					diskette_buffer[1]=s;
					diskette_buffer[2]=diskette_sector=(diskette_sector%DISKETTE_SECTORS)+1; // follow sectors
					diskette_buffer[4]=//~diskette_crc16>>8;
					diskette_buffer[5]=//~diskette_crc16;
					diskette_buffer[3]=//0X00; // 0X00 means 256 bytes
					diskette_status=//0X00; // OK
					diskette_offset=0; diskette_length=6;
				}
				break;
			case  8: // READ SECTOR
				if (!diskette_motor||!diskette_mem[d])
					diskette_status=0X80; // DISC MISSING?
				else if (/*diskette_cursor[d]!=diskette_track||*/diskette_track>=diskette_tracks[d]||diskette_sector<1||diskette_sector>DISKETTE_SECTORS)
					diskette_status=0X10; // RECORD NOT FOUND
				else
				{
					int i=((diskette_track*diskette_sides[d]+s)*DISKETTE_SECTORS+diskette_sector-1)*DISKETTE_PAGE;
					MEMLOAD(diskette_buffer,&diskette_mem[d][i]);
					diskette_length=DISKETTE_PAGE,
					diskette_offset=//0;
					diskette_status=0X00; // OK
					cprintf(" RECV %08X:%04X",i,diskette_length);
				}
				break;
			case 10: // WRITE SECTOR
				if (!diskette_motor||!diskette_mem[d])
					diskette_status=0X80; // DISC MISSING?
				else if (!diskette_canwrite[d]&&!(diskette_filemode&2))
					diskette_status=0X40; // WRITE PROTECT
				else if (/*diskette_cursor[d]!=diskette_track||*/diskette_track>=diskette_tracks[d]||diskette_sector<1||diskette_sector>DISKETTE_SECTORS)
					diskette_status=0X10; // RECORD NOT FOUND
				else
				{
					diskette_target=((diskette_track*diskette_sides[d]+s)*DISKETTE_SECTORS+diskette_sector-1)*DISKETTE_PAGE;
					diskette_length=DISKETTE_PAGE,
					diskette_offset=//0;
					diskette_status=0X00; // OK
					cprintf(" SEND %08X:%04X",diskette_target,diskette_length);
				}
				break;
			case 15: // WRITE TRACK
				if (!diskette_motor||!diskette_mem[d])
					diskette_status=0X80; // DISC MISSING?
				else if (!diskette_canwrite[d]&&!(diskette_filemode&2))
					diskette_status=0X40; // WRITE PROTECT
				else if (/*diskette_cursor[d]!=diskette_track||*/diskette_track>=diskette_tracks[d])
					diskette_status=0X10; // RECORD NOT FOUND
				else
				{
					diskette_target=((diskette_track*diskette_sides[d])*DISKETTE_SECTORS)*DISKETTE_PAGE;
					diskette_length=DISKETTE_PAGE, // we are cheating! see above!
					diskette_offset=//0;
					diskette_status=0X00; // OK
				}
				break;
			// TR-DOS and MSX-DOS operate a subset of the WD1793 disc controller command set:
			// neither TR-DOS 5.03 or MSX-DOS 2.2 use the following operations... AFAIK!
			case  2: // STEP
			case  3: // STEP (bis)
				if ((b=diskette_command)&32) // redo STEP-OUT?
			case  6: // STEP-OUT
			case  7: // STEP-OUT (bis)
				{
					if (diskette_track>1) diskette_cursor[d]=--diskette_track,diskette_status=0X00; // OK
					else diskette_cursor[d]=diskette_track=0,diskette_status=0X04; // TRACK 0
					break;
				}
				else // redo STEP-IN
				// no `break`!
			case  4: // STEP-IN // a version of MSX-DOS actually uses this!
			case  5: // STEP-IN (bis)
				{
					if (diskette_track<DISKETTE_TRACK99) diskette_cursor[d]=++diskette_track;
					diskette_status=0X00; // OK (never TRACK 0!)
					break;
				}
			case  9: // READ SECTORS
			case 14: // READ TRACK
			case 11: // WRITE SECTORS
			default:
				if (!diskette_motor||!diskette_mem[d])
					diskette_status=0X80; // DISC MISSING?
				else if ((b>>4)==11&&!diskette_canwrite[d]&&!(diskette_filemode&2))
					diskette_status=0X40; // WRITE PROTECT
				else //if (diskette_track>=diskette_tracks[d])
					diskette_status=0X10; // RECORD NOT FOUND
				cprintf(" *UNSUPPORTED!");
		}
	}
	cprintf(" %02X\n",diskette_status); diskette_command=b;
}

// ========================================== END OF DISKETTE SUPPORT //
