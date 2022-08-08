 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// The YM3 file format can naturally record the contents of a YM2149
// audio chip, that happens to be fully compatible with the AY-3-8910,
// but also a handy "virtual" format for other three-voice sound chips.

// This module includes functions to export the current chip registers
// into a YM3 file that can be read by platform-independent players.

// BEGINNING OF YM3 FILE FORMAT OUTPUT =============================== //

// YM3 file logging ------------------------------------------------- //

char ym3_tmpname[STRMAX]="";
FILE *ym3_file=NULL,*ym3_dump; int ym3_nextfile=1,ym3_count;
unsigned char ym3_tmp[14<<9],ym3_log[1<<9]; // `ym3_tmp` must be 14 times as big as `ym3_log`!
int ym3_close(void)
{
	if (!ym3_file)
		return 1; // nothing to do!
	fwrite("YM3!",1,4,ym3_file);
	fwrite1(ym3_tmp,ym3_count,ym3_dump);
	fclose(ym3_dump);
	if (ym3_dump=fopen(ym3_tmpname,"rb"))
	{
		for (int c=0,l,o;c<14;++c) // arrange byte dumps into long byte channels
		{
			fseek(ym3_dump,0,SEEK_SET);
			while (l=fread1(ym3_tmp,sizeof(ym3_log)*14,ym3_dump))
			{
				o=0; for (int i=c;i<l;i+=14)
					ym3_log[o++]=ym3_tmp[i];
				fwrite1(ym3_log,o,ym3_file);
			}
		}
		fclose(ym3_dump);
	}
	remove(ym3_tmpname); // destroy temporary file
	fclose(ym3_file);
	ym3_file=NULL;
	return 0;
}
int ym3_create(char *s)
{
	if (ym3_file)
		return 1; // already busy!
	if (!(ym3_dump=fopen(strcat(strcpy(ym3_tmpname,s),"$"),"wb")))
		return 1; // cannot create temporary file!
	if (!(ym3_file=fopen(s,"wb")))
		return fclose(ym3_dump),1; // cannot create file!
	return ym3_count=0;
}
void ym3_write(void); // must be defined later on!
void ym3_flush(void) { if (ym3_count>=sizeof(ym3_tmp)) fwrite1(ym3_tmp,sizeof(ym3_tmp),ym3_dump),ym3_count=0; }
void ym3_write_ay(BYTE *s,BYTE *t,int k) // default operation, where `s`, `t` and `k` are normally &psg_table[0], &psg_hard_log and PSG_KHZ_CLOCK
{
	int i; // we must adjust YM values to a 2 MHz clock.
	i=((s[0]+s[1]*256)*2000+k/2)/k; // channel 1 wavelength
	ym3_tmp[ym3_count++]=i; ym3_tmp[ym3_count++]=i>>8;
	i=((s[2]+s[3]*256)*2000+k/2)/k; // channel 2 wavelength
	ym3_tmp[ym3_count++]=i; ym3_tmp[ym3_count++]=i>>8;
	i=((s[4]+s[5]*256)*2000+k/2)/k; // channel 3 wavelength
	ym3_tmp[ym3_count++]=i; ym3_tmp[ym3_count++]=i>>8;
	ym3_tmp[ym3_count++]=s[6]; // noise wavelength
	ym3_tmp[ym3_count++]=s[7]; // mixer
	ym3_tmp[ym3_count++]=s[8]; // channel 1 amplitude
	ym3_tmp[ym3_count++]=s[9]; // channel 2 amplitude
	ym3_tmp[ym3_count++]=s[10]; // channel 3 amplitude
	i=((s[11]+s[12]*256)*2000+k/2)/k; // hard envelope wavelength
	ym3_tmp[ym3_count++]=i; ym3_tmp[ym3_count++]=i>>8;
	ym3_tmp[ym3_count++]=*t; // hard envelope type
	*t=0xFF; // 0xFF means the hard envelope doesn't change
}

// ===================================== END OF YM3 FILE FORMAT OUTPUT //
