 //  ####  ######    ####  #######   ####    ----------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####    ----------------------- //

/* This notice applies to the source code of CPCEC and its binaries.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Contact information: <mailto:cngsoft@gmail.com> */

// RUNEC is a basic file runner for CPCEC, ZXSEC, CSFEC and MSXEC:
// when the type of a file can't be identified from its extension,
// magic numbers are checked to detect which emulator can read it.

#include <stdio.h>
#include <string.h> // strchr..
#include <stdlib.h> // system..

unsigned char tmp[1<<15]=""; // 32K
int endswith(char *s,char *t) // ZERO if the path "s" doesn't end in "t"; comparison is case-insensitive
{
	if (!s||!t||!*s||!*t) return 0;
	int n=0; while (*s) ++s; while (*t) ++n,++t;
	while (n--) if (((*--s)^(*--t))&~32) return 0;
	return 1;
}
int c64chr(unsigned char *s,unsigned char *t,int l) // ZERO if s[0..l[ isn't the complement of t[0..l[
{
	for (;l>0;--l) if (*s+++*t++!=255) return 0;
	return 1;
}
int main(int argc,char *argv[])
{
	const char exe[][6]={"cpcec","zxsec","csfec","msxec"}; // respectively 0, 1, 2 and 3
	FILE *f; int o=-1; for (int i=1;i<argc;++i) if (f=fopen(argv[i],"rb"))
	{
		int l=fread(tmp,1,sizeof(tmp),f); fclose(f); if (l<16) // the lowest valid filesize (empty C64 TAP, minimal Spectrum TAP)
			{}
		else if (endswith(argv[i],".cpr")||endswith(argv[i],".cdt")||endswith(argv[i],".cdp")||endswith(argv[i],".csw")||endswith(argv[i],".des"))
			o=0;
		else if (endswith(argv[i],".trd")||endswith(argv[i],".scl")||endswith(argv[i],".tzx")||endswith(argv[i],".pzx")||endswith(argv[i],".mld")
			||endswith(argv[i],".z80")||endswith(argv[i],".szx")||endswith(argv[i],".sp")/*||endswith(argv[i],".sit")||endswith(argv[i],".blk")*/)
			o=1;
		else if (endswith(argv[i],".d64")||endswith(argv[i],".t64")||endswith(argv[i],".prg")||endswith(argv[i],".s64")||endswith(argv[i],".crt"))
			o=2;
		else if (endswith(argv[i],".cas")||endswith(argv[i],".tsx")||endswith(argv[i],".stx")||endswith(argv[i],".mx1")||endswith(argv[i],".mx2"))
			o=3;
		else if (endswith(argv[i],".crt"))
			o=memcmp(tmp,"C64 CARTRIDGE   ",16)?0:2; // tell apart between cartridges: raw CPC and cooked C64
		else if (endswith(argv[i],".tap"))
			o=l<12||memcmp(tmp,"C64-TAPE-RAW",12)?1:2;
		else if (endswith(argv[i],".dsk"))
			if (memcmp(tmp,"EXTENDED",8)&&memcmp(tmp,"MV - CPC",8)) // i.e. DSK for MSX
				o=3;
			else // Spectrum bootable sectors begin with $01 and file sectors begin with "PLUS3DOS"
				o=(/*(tmp[0x11A]&0x40)&&*/memcmp(&tmp[0x0A00],"PLUS3DOS",8)&&memcmp(&tmp[0x1200],"PLUS3DOS",8))?0:1;
		else if (endswith(argv[i],".sna")) // CPC snapshots include a header that begins with an ID
			o=memcmp(tmp,"MV - SNA",8)?1:0;
		else if (endswith(argv[i],".vpl")) // VPL files are 16 colours long (C64) or 15 (MSX)
		{
			int t=0,u,n=0; while (t<l&&n<17)
			{
				u=tmp[t]; if ((u>='0'&&u<='9')||((u|=32)>='a'&&u<='f')) ++n;
				while (tmp[t]>=' '&&++t<l) {} while (tmp[t]<' '&&++t<l) {}
			}
			if (n==15) o=3; else if (n==16) o=2;
		}
		else if (endswith(argv[i],".rom"))
		{
			/**/ if (l==0X5000&&c64chr(&tmp[0X4008],&tmp[0X4408],0XD0)&&c64chr(&tmp[0X4808],&tmp[0X4C08],0XD0)) // C64 firmware
				o=2;
			else if (l>=0X2000&&(
				(!memcmp(tmp,"AB",2)&&(!tmp[3]||(tmp[3]>=64&&tmp[3]<192)))|| // MSX cartridge (99% titles) (1/3)
				(l>=0X8000&&!memcmp(&tmp[0x4000],"AB",2)&&tmp[0x4003]>=64&&tmp[0x4003]<192)|| // MSX cartridge (2/3)
				(l>=0X8000&&!memcmp(&tmp[0],"\010\200\151\215",4))|| // MSX cartridge (special case "R-TYPE"!) (3/3)
				!memcmp(&tmp[4],"\277\033\230\230",4))) // MSX firmware
				o=3;
			else // CPC Dandanator cartridges must include FDFDFD71 and FDFDFD77 somewhere in their first 4K block
			{
				o=0; for (int t=0;t<l-4;++t)
					if (tmp[t+0]==0xFD&&tmp[t+1]==0xFD&&tmp[t+2]==0xFD)
						if (tmp[t+3]==0x71) o|=2; else if (tmp[t+3]==0x77) o|=4;
				if (!(o=(o==6))) // do a slightly more complex test if the CPC Dandanator test fails
					o=(tmp[0]<<24)+(tmp[1]<<16)+(tmp[2]<<8)+tmp[3]==0x01897FED?0:1; // CPC firmware fingerprint
			}
		}
		if (o<0) break; // unknown file!
	}
	if (o<0) return puts("usage: runec file.. [option..]"),1;
	#ifdef _WIN32
	char *z; if (z=strrchr(argv[0],'\\')) z[1]=0; else *argv[0]=0; sprintf(tmp,"start \"\" \"%s%s.exe\"",
	#else
	char *z; if (z=strrchr(argv[0],'/')) z[1]=0; else *argv[0]=0; sprintf(tmp,"\"%s%s\"",
	#endif
		argv[0],exe[o]);
	for (int i=1;i<argc;++i) sprintf(tmp+strlen(tmp),strchr(argv[i],' ')?" \"%s\"":" %s",argv[i]);
	return /*puts(tmp),*/system(tmp);
}
