 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

#define MY_CAPTION "XRFEC"
#define my_caption "xrfec"
#define MY_VERSION "20240707"//"2555"
#define MY_LICENSE "Copyright (C) 2019-2024 Cesar Nicolas-Gonzalez"

#define GPL_3_INFO \
	"This program comes with ABSOLUTELY NO WARRANTY; for more details" "\n" \
	"please see the GNU General Public License. This is free software" "\n" \
	"and you are welcome to redistribute it under certain conditions."

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

// XRFEC is a small tool that reads the temporary XOR-RLE-FILM files
// generated by CPCEC, ZXSEC etc. and decodes them into video and audio
// that can be processed by conventional codecs such as ZMBV and XVID.
// The Windows version relies on the Video For Windows (VFW) interface;
// other systems will get RGB24+PCM AVI files to feed FFMPEG with.

#include <stdio.h> // printf...
#include <string.h> // strcmp...
#include <stdlib.h> // malloc...

#ifdef _WIN32 // "BYTE", "WORD", "DWORD" and (unused) "QWORD" are always exactly 8, 16, 32 and 64 bits long

#ifndef RAWWW // disable VFW!
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <vfw.h>
#endif

#else // "char", "short int" and "long long int" are 8, 16 and 64 bits, but "long int" can be 32 or 64 bits

#include <stdint.h>
#define BYTE uint8_t // can this be safely reduced to "unsigned char"?
#define WORD uint16_t // can this be safely reduced to "unsigned short"?
#define DWORD uint32_t // this CANNOT be safely reduced to "unsigned int"!

#endif

#if __GNUC__ >= 4 // optional branch prediction hints
#define LIKELY(x) __builtin_expect(!!(x),1) // see 'likely' from Linux
#define UNLIKELY(x) __builtin_expect((x),0) // see 'unlikely'
#else // branch prediction hints are unreliable outside GCC!!
#define LIKELY(x) (x) // not ready, fall back
#define UNLIKELY(x) (x) // ditto
#endif

// common data types -- mostly based on ARGB32 video and lLrR32 audio //

#define MAXVIDEOBYTES (32<<16) // 2 MB > one frame of video at 800x600px 32bpp
#define MAXAUDIOBYTES (1<<16) // 64 kB > 1/3 seconds of audio at 48000x2ch 16b
BYTE flags_audio[4]={1,2,2,4}; // sample size according to 16bits (bit 0) and stereo (bit1)
BYTE wave32[MAXAUDIOBYTES]; // the current audio frame

int video_x,video_y,audio_z,clock_z,flags_z,count_z; // dimensions and features of the video and audio streams

// open and read a XRF file ----------------------------------------- //

FILE *xrf_file=NULL; int xrf_length,xrf_cursor,xrf_count,xrf_dummy;
BYTE *xrf_argb32=NULL,*xrf_diff32=NULL,*xrf_chunk=NULL; // buffers
int xrf_version; // experimental support for additional encodings!!
int xrf_chunksize(void) // get the maximum chunk length in bytes
{
	return xrf_version?MAXVIDEOBYTES+MAXAUDIOBYTES+8*8: // all 100% literal streams (video R,G,B + audio l,L,r,R) plus headers and footers
	((MAXVIDEOBYTES+MAXAUDIOBYTES)*19/16+8*4); // pathological case xxxxxxxx.xxxxxxxx -> 1xxxxxxxx.1xxxxxxxx.0 plus 8 end markers
}
int xrf_decodebool(BYTE **s,BYTE *a,BYTE *b) // fetch zero or nonzero
	{ { if (UNLIKELY(!(*a>>=1))) *b=*((*s)++),*a=128; } return *b&*a; }
int xrf_decode1(BYTE *t,int o,BYTE *s,int *l,int x) // equally hacky decoder based on an 8-bit RLE and an interleaved pseudo Huffman filter!
{
	#define xrf_decode1n() (xrf_decodebool(&v,&a,&b)?*v++:0)
	BYTE *v=s,*w=t,a=0,b=0; for (int m=-1;o>=0;)
	{
		int n; if (xrf_decodebool(&v,&a,&b)) // special case of xrf_decode1n()
			{ if (!(n=*v++)) return *l=(w-t)/x,v-s; } // long zero "100000000" is EOF!
		else n=0; // short zero
		--o,*w=n,w+=x; if (n==m) // string?
		{
			n=xrf_decode1n(); if (n==255) n=510+(xrf_decode1n()<<8);
			if (n>=254) n+=xrf_decode1n();
			if ((o-=n)>=0) for (;n;--n) *w=m,w+=x; // avoid overruns; the error is handled later
			m=-1; // avoid accidental repetitions
		}
		else m=n; // new literal
	}
	#undef xrf_decode1n
	return *l=-1; // false values = automatic errors!
}
int xrf_decode2(BYTE *t,int o,BYTE *s,int *l,int x) // slightly improved version of the original hacky decoder!
{
	#define xrf_decode1n() xrf_decodebool(&v,&a,&b)
	BYTE *v=s,*w=t,a=0,b=0,r=128; for (;o>=0;)
	{
		int n=1; while (n>0&&xrf_decode1n()) if ((n<<=1)<1) goto failure; else if (xrf_decode1n()) ++n;
		if ((o-=--n)<0) break; else for (;n;--n) *w=*v++,w+=x; // avoid overruns!
		BYTE c=xrf_decode1n(); // 00: fetch byte; 01: reuse byte; 10: set 0; 11: set 255
		n=1; while (n>0&&xrf_decode1n()) if ((n<<=1)<1) goto failure; else if (xrf_decode1n()) ++n;
		if (c) c=xrf_decode1n()?255:0; else if (xrf_decode1n()) c=r; else
			if ((c=*v++)==255) break; else if (!c) return *l=(w-t)/x,v-s; else r=c;
		if ((o-=++n)<0) break; else do *w=c,w+=x; while (--n); // avoid overruns!
	}
	failure: return *l=1; // false values = automatic errors!
	#undef xrf_decode1n
}
int xrf_decode(BYTE *t,int o,BYTE *s,int *l,int x)
	{ return xrf_version?xrf_decode2(t,o,s,l,x):xrf_decode1(t,o,s,l,x); }

#define xrf_fgetc() fgetc(xrf_file)
int xrf_fgetcc(void) { int m=xrf_fgetc()<<8; return m+xrf_fgetc(); } // Motorola order, (*(WORD*))(x) is no use
int xrf_fgetcccc(void) { int m=xrf_fgetc()<<24; m+=xrf_fgetc()<<16; m+=xrf_fgetc()<<8; return m+xrf_fgetc(); }
//int xrf_fread(BYTE *t,int l) { int i=0,j; while (i<l&&(j=fread(&t[i],1,l-i,xrf_file))) i+=j; return i; }
#define xrf_fread(t,l) fread(t,1,l,xrf_file)

int xrf_open(char *s) // opens a XRF file and sets the common parameters up; !0 ERROR
{
	if (!xrf_argb32&&!(xrf_argb32=malloc(MAXVIDEOBYTES)))
		return 1; // cannot allocate video buffer!
	if (!xrf_diff32&&!(xrf_diff32=malloc(MAXVIDEOBYTES)))
		return 1; // cannot allocate audio buffer!
	if (xrf_file) return 1; // already open!
	if (!(xrf_file=fopen(s,"rb"))) return 1;
	fseek(xrf_file,0,SEEK_END);
	xrf_length=ftell(xrf_file);
	fseek(xrf_file,0,SEEK_SET);
	char id[9]; id[xrf_fread(id,8)]=0;
	video_x=xrf_fgetcc();
	video_y=xrf_fgetcc();
	audio_z=xrf_fgetcc();
	clock_z=xrf_fgetc();
	flags_z=xrf_fgetc();
	count_z=xrf_fgetcccc();
	xrf_cursor=8+8+4; // the bytes we've read so far
	xrf_count=xrf_dummy=0;
	// reject obsolete files: XRF-1 was limited to 8-bit RLE lengths, and XRF+1 didn't store the amount of frames!
	xrf_version=id[3]-'1'; id[3]='1'; // experimental support for additional encodings!!
	return video_x<=0||video_y<=0||clock_z<=0||audio_z<0||xrf_version<0||xrf_version>1||
		strcmp(id,"XRF1!\015\012\032")?fclose(xrf_file),1:0;
}
int xrf_read(void) // reads a XRF chunk and decodes the current video and audio frames; !0 ERROR/EOF
{
	if (!xrf_file) return 1; // file not open!
	if (xrf_count>=count_z) return 1; // EOF!
	//if (!xrf_chunk||!xrf_chunk2) return 1; // buffer error!
	int i,j,l=xrf_fgetcccc();
	{
		if (l<1||l>xrf_chunksize()) return 1; // improper chunk size!
		if (xrf_fread(xrf_chunk,l)!=l) return 1; // file is truncated!
	}
	xrf_cursor+=4+l;
	BYTE *s=xrf_chunk; i=0; l=video_x*video_y>>((flags_z&12)==4);
	s+=xrf_decode(&xrf_diff32[0],l,s,&j,4); i+=j; // B
	s+=xrf_decode(&xrf_diff32[1],l,s,&j,4); i+=j; // G
	s+=xrf_decode(&xrf_diff32[2],l,s,&j,4); i+=j; // R
	// s+=xrf_decode(&xrf_diff32[3],l,s,&j,4); i+=j; // A
	if (!i) ++xrf_dummy; // empty frame
	else if (i!=l*3) return 1; // damaged video frame!
	else // apply changes!
	{
		int zi=0,zo=video_y*video_x; // doing the vertical flipping here saves some overhead later
		if ((flags_z&12)==4) // vertical scaling? 00xx = normal, 01xx = vertical scaling, 1Zxx = interleaved
			for (zo-=video_x*2;zo>=0;zo-=video_x*3) for (int z=video_x;z;++zi,++zo,--z)
					((DWORD*)xrf_argb32)[zo+video_x]=((DWORD*)xrf_argb32)[zo]^=((DWORD*)xrf_diff32)[zi];
		else // no vertical scaling, copy
			for (zo-=video_x*1;zo>=0;zo-=video_x*2) for (int z=video_x;z;++zi,++zo,--z)
					((DWORD*)xrf_argb32)[zo]^=((DWORD*)xrf_diff32)[zi];
	}
	if (audio_z)
	{
		switch(flags_z&3)
		{
			case 0: // 8-bit mono
				i=audio_z;
				s+=xrf_decode(&wave32[0],audio_z,s,&j,1); i-=j; // M
				break;
			case 1: // 16-bit mono
			case 2: // 8-bit stereo: equal to 16-bit mono on the lil-endian AVI!
				i=audio_z*2;
				s+=xrf_decode(&wave32[0],audio_z,s,&j,2); i-=j; // m/L
				s+=xrf_decode(&wave32[1],audio_z,s,&j,2); i-=j; // M/R
				break;
			case 3: // 16-bit stereo
				i=audio_z*4;
				s+=xrf_decode(&wave32[0],audio_z,s,&j,4); i-=j; // l
				s+=xrf_decode(&wave32[1],audio_z,s,&j,4); i-=j; // L
				s+=xrf_decode(&wave32[2],audio_z,s,&j,4); i-=j; // r
				s+=xrf_decode(&wave32[3],audio_z,s,&j,4); i-=j; // R
				break;
		}
		if (i) return 1; // damaged audio frame!
	}
	return ++xrf_count,0;
}
int xrf_close(void) // close and clean up; always 0 OK
{
	if (xrf_argb32) free(xrf_argb32),xrf_argb32=NULL;
	if (xrf_diff32) free(xrf_diff32),xrf_diff32=NULL;
	if (xrf_chunk) free(xrf_chunk),xrf_chunk=NULL;
	if (xrf_file) fclose(xrf_file),xrf_file=NULL;
	return 0;
}

int avi_videos,avi_audios; // video/audio counters

#ifdef _WIN32
#ifndef RAWWW // disable VFW!

// create and write an AVI file through Windows' AVIFILE API -------- //

char vfw_fourcc[5]=""; // uncompressed "DIB " is even worse than RGB24: it's ARGB32!
PAVIFILE vfw_file=NULL; IAVIStream *vfw_video,*vfw_codec,*vfw_audio;

int vfw_create(char *s) // !0 ERROR
{
	if (vfw_file) return 1;
	avi_videos=avi_audios=0; vfw_video=vfw_codec=vfw_audio=NULL;
	AVIFileInit(); remove(s); // VFW needs erasing the file beforehand!
	if (AVIFileOpen(&vfw_file,s,OF_CREATE|OF_WRITE,NULL))
		return AVIFileExit(),1;
	AVISTREAMINFO vhdr; memset(&vhdr,0,sizeof(vhdr));
	vhdr.fccType=streamtypeVIDEO;
	//vhdr.fccHandler=0;
	vhdr.dwRate=(vhdr.dwScale=1)*clock_z;
	vhdr.dwSuggestedBufferSize=video_x*video_y*4;
	//vhdr.rcFrame.left=vhdr.rcFrame.top=0;
	vhdr.rcFrame.right=video_x;
	vhdr.rcFrame.bottom=video_y;
	if (AVIFileCreateStream(vfw_file,&vfw_video,&vhdr))
		vfw_video=NULL;
	AVICOMPRESSOPTIONS vopt; memset(&vopt,0,sizeof(vopt));
	vopt.fccHandler=mmioFOURCC(vfw_fourcc[0],vfw_fourcc[1],vfw_fourcc[2],vfw_fourcc[3]);
	vopt.dwFlags=AVICOMPRESSF_KEYFRAMES; vopt.dwKeyFrameEvery=clock_z*10; // 10 s/keyframe
	BITMAPINFO bmi; memset(&bmi,0,sizeof(bmi)); bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biPlanes=1; bmi.bmiHeader.biBitCount=32; bmi.bmiHeader.biCompression=BI_RGB;
	bmi.bmiHeader.biWidth=video_x; bmi.bmiHeader.biHeight=video_y;
	// negative values mean top-to-bottom; Windows' default is bottom-to-top
	if (AVIMakeCompressedStream(&vfw_codec,vfw_video,&vopt,NULL))
		vfw_codec=NULL;
	else
		AVIStreamSetFormat(vfw_codec,0,&bmi,sizeof(bmi));
	if (audio_z)
	{
		AVISTREAMINFO ahdr; memset(&ahdr,0,sizeof(ahdr));
		ahdr.fccType=streamtypeAUDIO;
		ahdr.dwRate=(ahdr.dwSampleSize=ahdr.dwScale=flags_audio[flags_z&3])*clock_z*audio_z;
		ahdr.dwQuality=(DWORD)-1;
		WAVEFORMATEX wfex; memset(&wfex,0,sizeof(wfex)); wfex.wFormatTag=WAVE_FORMAT_PCM;
		wfex.nAvgBytesPerSec=(wfex.nBlockAlign=(wfex.wBitsPerSample=(flags_z&1)?16:8)/8*(wfex.nChannels=(flags_z&2)?2:1))
			*(wfex.nSamplesPerSec=audio_z*clock_z);
		if (AVIFileCreateStream(vfw_file,&vfw_audio,&ahdr))
			vfw_audio=NULL;
		else
			AVIStreamSetFormat(vfw_audio,0,&wfex,sizeof(wfex));
	}
	return 0;
}
int vfw_write(void) // !0 ERROR
{
	if (!vfw_file) return 1;
	if (vfw_video&&vfw_codec)
	{
		// unlike in past versions, the bitmap is already upside down :-)
		AVIStreamWrite(vfw_codec,avi_videos++,1,xrf_argb32,video_x*video_y*4,0,NULL,NULL);
		if (vfw_audio)
		{
			AVIStreamWrite(vfw_audio,avi_audios,audio_z,wave32,audio_z*flags_audio[flags_z&3],0,NULL,NULL);
			avi_audios+=audio_z;
		}
		return 0;
	}
	return 1;
}
int vfw_finish(void) // close and clean up; always 0 OK
{
	if (vfw_audio) AVIStreamRelease(vfw_audio),vfw_audio=NULL;
	if (vfw_codec) AVIStreamRelease(vfw_codec),vfw_codec=NULL;
	if (vfw_video) AVIStreamRelease(vfw_video),vfw_video=NULL;
	if (vfw_file) AVIFileRelease(vfw_file),vfw_file=NULL;
	AVIFileExit(); return 0;
}

#endif
#endif

// create and write an AVI file by hand, no compression ------------- //

FILE *avi_file=NULL; long long int avi_length=0;

BYTE avi_header[0x0200]= // padding is doubtful. VirtualDub uses 0x2000, VFW.DLL uses 0x0800...
{
	0x52,0x49,0x46,0x46,  64,  64,  64,  64,0x41,0x56,0x49,0x20,0x4C,0x49,0x53,0x54, // 0x000#
	0x24,0x01,0x00,0x00,0x68,0x64,0x72,0x6C,0x61,0x76,0x69,0x68,0x38,0x00,0x00,0x00, // 0x001#
	   0,   0,   0,   0,   0,   0,   0,   0,0x00,0x00,0x00,0x00,0x10,0x01,0x00,0x00, // 0x002#
	  64,  64,  64,   0,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x003#
	   0,   0,0x00,0x00,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x004#
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4C,0x49,0x53,0x54,0x74,0x00,0x00,0x00, // 0x005#
	0x73,0x74,0x72,0x6C,0x73,0x74,0x72,0x68,0x38,0x00,0x00,0x00,0x76,0x69,0x64,0x73, // 0x006#
	0x44,0x49,0x42,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x007#
	0x01,0x00,0x00,0x00,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,  64,  64,  64,   0, // 0x008#
	   0,   0,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x009#
	   0,   0,   0,   0,0x73,0x74,0x72,0x66,0x28,0x00,0x00,0x00,0x28,0x00,0x00,0x00, // 0x00A#
	   0,   0,0x00,0x00,   0,   0,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,0x00,0x00, // 0x00B#
	   0,   0,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x00C#
	0x00,0x00,0x00,0x00,0x4C,0x49,0x53,0x54,0x5C,0x00,0x00,0x00,0x73,0x74,0x72,0x6C, // 0x00D#
	0x73,0x74,0x72,0x68,0x38,0x00,0x00,0x00,0x61,0x75,0x64,0x73,0x00,0x00,0x00,0x00, // 0x00E#
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00, // 0x00F#
	   0,   0,   0,   0,0x00,0x00,0x00,0x00,  64,  64,  64,   8,   0,   0,   0,   0, // 0x010#
	0xFF,0xFF,0xFF,0xFF,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x011#
	0x73,0x74,0x72,0x66,0x10,0x00,0x00,0x00,0x01,0x00,   2,0x00,   0,   0,   0,   0, // 0x012#
	   0,   0,   0,   0,   0,0x00,   0,0x00,0x4A,0x55,0x4E,0x4B,   0,   0,   0,   0, // 0x013#
};
BYTE avi_h_mute[0x0200]= // see above
{
	0x52,0x49,0x46,0x46,  64,  64,  64,  64,0x41,0x56,0x49,0x20,0x4C,0x49,0x53,0x54, // 0x000#
	0xC0,0x00,0x00,0x00,0x68,0x64,0x72,0x6C,0x61,0x76,0x69,0x68,0x38,0x00,0x00,0x00, // 0x001#
	   0,   0,   0,   0,   0,   0,   0,   0,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00, // 0x002#
	  64,  64,  64,   0,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x003#
	   0,   0,0x00,0x00,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x004#
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4C,0x49,0x53,0x54,0x74,0x00,0x00,0x00, // 0x005#
	0x73,0x74,0x72,0x6C,0x73,0x74,0x72,0x68,0x38,0x00,0x00,0x00,0x76,0x69,0x64,0x73, // 0x006#
	0x44,0x49,0x42,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x007#
	0x01,0x00,0x00,0x00,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,  64,  64,  64,   0, // 0x008#
	   0,   0,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x009#
	   0,   0,   0,   0,0x73,0x74,0x72,0x66,0x28,0x00,0x00,0x00,0x28,0x00,0x00,0x00, // 0x00A#
	   0,   0,0x00,0x00,   0,   0,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,0x00,0x00, // 0x00B#
	   0,   0,   0,   0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0x00C#
	0x00,0x00,0x00,0x00,0x4A,0x55,0x4E,0x4B,   0,   0,   0,   0,0x00,0x00,0x00,0x00, // 0x00D#
};
BYTE avi_canvas[1<<12]; // scanline interleaving buffer -- no need to allocate a whole frame!

#define avi_fputc(x) fputc(x,avi_file)
int avi_fputcccc(int i) { avi_fputc(i); avi_fputc(i>>8); avi_fputc(i>>16); return avi_fputc(i>>24); }
void avi_mputcc(BYTE *x,int i) { *x=i; x[1]=i>>8; } // compatible, rather than ((*(WORD*))(x))=i)
void avi_mputcccc(BYTE *x,int i) { *x=i; x[1]=i>>8; x[2]=i>>16; x[3]=i>>24; } // ditto, for DWORD
//int avi_fwrite(BYTE *t,int l) { int i=0,j; while (i<l&&(j=fwrite(&t[i],1,l-i,avi_file))) i+=j; return i; }
#define avi_fwrite(t,l) fwrite(t,1,l,avi_file)

int avi_create(char *s)
{
	#ifdef _WIN32
	#ifndef RAWWW // disable VFW!
	if (*vfw_fourcc) return vfw_create(s);
	#endif
	#endif
	if (avi_file) return 1;
	if (!strcmp(s,"-"))
		avi_file=stdout; // STDOUT is always open!
	else if (!(avi_file=fopen(s,"wb")))
		return 1;
	int i=video_x*video_y*3;
	if (audio_z)
	{
		avi_mputcccc(&avi_header[0x0020],(1000000+clock_z/2)/clock_z);
		avi_mputcccc(&avi_header[0x0024],clock_z*(8+video_x*video_y*3+8+audio_z*flags_audio[flags_z&3])); // max byterate
		avi_mputcc(&avi_header[0x0040],video_x); avi_mputcc(&avi_header[0x0044],video_y);
		avi_mputcc(&avi_header[0x0084],clock_z);
		avi_mputcccc(&avi_header[0x90],i); avi_mputcccc(&avi_header[0xC0],i);
		avi_mputcc(&avi_header[0x00A0],video_x); avi_mputcc(&avi_header[0x00A2],video_y);
		avi_mputcccc(&avi_header[0x00B0],video_x); avi_mputcccc(&avi_header[0x00B4],video_y);
		i=audio_z*clock_z;
		avi_mputcccc(&avi_header[0x0100],i*flags_audio[flags_z&3]);
		avi_mputcccc(&avi_header[0x010C],audio_z*flags_audio[flags_z&3]);
		avi_header[0x012A]=(flags_z&2)?2:1;
		avi_mputcccc(&avi_header[0x012C],i);
		avi_mputcccc(&avi_header[0x0130],i*flags_audio[flags_z&3]);
		avi_header[0x00FC]=avi_header[0x0114]=avi_header[0x0134]=flags_audio[flags_z&3];
		avi_header[0x0136]=(flags_z&1)?16:8;
		avi_mputcccc(&avi_header[0x013C],sizeof(avi_header)-0x0140); // "JUNK" size
		// Notice that some fields need to know in advance the sizes;
		// fortunately we know the number of frames and the size of each.
		i=audio_z*flags_audio[flags_z&3]; if (i&1) ++i; // RIFF even-padding
		i=count_z*(8+video_x*video_y*3+8+i); // movie data
		avi_mputcccc(&avi_header[0x0004],sizeof(avi_header)+4+i+8+count_z*32); // "RIFF:AVI " size
		avi_mputcccc(&avi_header[0x0030],count_z); // avi_videos
		avi_mputcccc(&avi_header[0x008C],count_z); // avi_videos
		avi_mputcccc(&avi_header[0x0108],count_z*audio_z); // avi_audios
		avi_length=avi_fwrite(avi_header,sizeof(avi_header))+12;
	}
	else
	{
		avi_mputcccc(&avi_h_mute[0x0020],(1000000+clock_z/2)/clock_z);
		avi_mputcccc(&avi_h_mute[0x0024],clock_z*(8+video_x*video_y*3)); // max byterate
		avi_mputcc(&avi_h_mute[0x0040],video_x); avi_mputcc(&avi_h_mute[0x0044],video_y);
		avi_mputcc(&avi_h_mute[0x0084],clock_z);
		avi_mputcccc(&avi_h_mute[0x90],i); avi_mputcccc(&avi_h_mute[0xC0],i);
		avi_mputcc(&avi_h_mute[0x00A0],video_x); avi_mputcc(&avi_h_mute[0x00A2],video_y);
		avi_mputcccc(&avi_h_mute[0x00B0],video_x); avi_mputcccc(&avi_h_mute[0x00B4],video_y);
		avi_mputcccc(&avi_h_mute[0x00D8],sizeof(avi_h_mute)-0x00DC); // "JUNK" size
		// Notice that some fields need to know in advance the sizes;
		// fortunately we know the number of frames and the size of each.
		i=count_z*(8+video_x*video_y*3); // movie data
		avi_mputcccc(&avi_h_mute[0x0004],sizeof(avi_h_mute)+4+i+8+count_z*16); // "RIFF:AVI " size (movie+index)
		avi_mputcccc(&avi_h_mute[0x0030],count_z); // avi_videos
		avi_mputcccc(&avi_h_mute[0x008C],count_z); // avi_videos
		avi_length=avi_fwrite(avi_h_mute,sizeof(avi_h_mute))+12;
	}
	avi_fputcccc(0x5453494C); // "LIST"
	avi_fputcccc(4+i); // "LIST:movi" size
	avi_fputcccc(0x69766F6D); // "movi"
	return 0;
}
int avi_write(void)
{
	#ifdef _WIN32
	#ifndef RAWWW // disable VFW!
	if (*vfw_fourcc) return vfw_write();
	#endif
	#endif
	if (!avi_file) return 1;
	// the bitmap is already upside down, but we still have to compress it,
	// and the only available compression is the ARGB32->RGB24 clipping :-(
	int l=(video_x*3+3)&-4;
	avi_fputcccc(0x62643030); // "00db"
	avi_fputcccc(l*video_y);
	for (int y=0;y<video_y;++y)
	{
		BYTE *t=avi_canvas,*s=&xrf_argb32[y*video_x*4];
		for (int x=0;x<video_x;++x)
			*t++=*s++, // copy B
			*t++=*s++, // copy G
			*t++=*s++, // copy R
			++s; // skip A!
		if (l!=avi_fwrite(avi_canvas,l)) return 1;
	}
	avi_length+=l+8;
	if (audio_z)
	{
		avi_fputcccc(0x62773130); // "01wb"
		avi_fputcccc(l=audio_z*flags_audio[flags_z&3]);
		if (l&1) wave32[l++]=0; // RIFF even-padding
		if (l!=avi_fwrite(wave32,l)) return 1;
		avi_length+=l+8;
	}
	++avi_videos; avi_audios+=audio_z;
	return 0;
}
int avi_finish(void)
{
	#ifdef _WIN32
	#ifndef RAWWW // disable VFW!
	if (*vfw_fourcc) return vfw_finish();
	#endif
	#endif
	if (!avi_file) return 1;
	avi_length+=8;
	avi_fputcccc(0x31786469); // "idx1"
	avi_fputcccc(audio_z?count_z*32:count_z*16);
	int z=4,j=video_x*video_y*3;
	int k=audio_z*flags_audio[flags_z&3],l=k; if (l&1) ++l; // RIFF even-padding
	for (int i=0;i<count_z;++i)
	{
		avi_fputcccc(0x62643030); // "00db"
		avi_fputcccc(0x10);
		avi_fputcccc(z); // offset
		avi_fputcccc(j); // video size
		z+=j+8; avi_length+=16;
		if (audio_z)
		{
			avi_fputcccc(0x62773130); // "01wb"
			avi_fputcccc(0x10);
			avi_fputcccc(z); // offset
			avi_fputcccc(k); // audio size
			z+=l+8; avi_length+=16;
		}
	}
	if (avi_file!=stdout)
		fclose(avi_file); // close file if it isn't STDOUT!
	return avi_file=NULL,0;
}

// ------------------------------------------------------------------ //

int main(int argc,char *argv[])
{
	char *s=NULL,*t=NULL; int i=0;
	while (++i<argc)
	{
		if (!strcmp("-h",argv[i])||!strcmp("--help",argv[i]))
			i=argc; // help!
		else if (!s)
			s=argv[i];
		else if (!t)
			t=argv[i];
		#ifdef _WIN32
		#ifndef RAWWW // disable VFW!
		else if (!*vfw_fourcc&&strlen(argv[i])==4)
			strcpy(vfw_fourcc,argv[i]);
		#endif
		#endif
		else i=argc; // help!
	}
	if (!s||i>argc)
	{
		printf(MY_CAPTION " " MY_VERSION " " MY_LICENSE "\n"
			"\n"
			"usage: " my_caption " source.xrf [target.avi"
			#ifdef _WIN32
			#ifndef RAWWW // disable VFW!
			" [fourcc]"
			#endif
			#endif
			"]\n"
			"       " my_caption " source.xrf - | ffmpeg [filters] -i - [options] target\n"
			"\n"
			GPL_3_INFO
			"\n");
			#ifdef _WIN32
			#ifndef RAWWW // disable VFW!
			ICINFO lpicinfo; ICInfo(0,-1,&lpicinfo); int n=lpicinfo.fccHandler;
			for (int j=0;j<n;++j)
				ICInfo(0,j,&lpicinfo),printf("%s%c%c%c%c",j?j%10?" ":"\n\t\t":"\n- fourcc codes: ",
					(BYTE)lpicinfo.fccHandler,(BYTE)(lpicinfo.fccHandler>>8),(BYTE)(lpicinfo.fccHandler>>16),(BYTE)(lpicinfo.fccHandler>>24));
			printf("\n");
			#endif
			#endif
		return 1;
	}
	if (xrf_open(s))
		return xrf_close(),fprintf(stderr,"error: cannot open source!\n"),1;
	//if (!count_z) return xrf_close(),fprintf(stderr,"error: source is empty!\n"),1;
	fprintf(stderr,audio_z?"VIDEO %dx%dpx %dHz - AUDIO %dx%02db %dHz\n":"VIDEO %dx%dpx %dHz - NO AUDIO\n",
		video_x,video_y,clock_z,(flags_z&2)?2:1,(flags_z&1)?16:8,audio_z*clock_z);
	#define filesize_style "%d.%02d Mbytes"
	#define filesize_upper(x) ((int)(x>>20))
	#define filesize_lower(x) ((((int)(x>>15))&31)*3+3)
	if (t) // process
	{
		if (!(xrf_chunk=malloc(xrf_chunksize())))
			return xrf_close(),avi_finish(),fprintf(stderr,"error: cannot setup decoder!\n"),1;
		if (avi_create(t))
			return xrf_close(),avi_finish(),fprintf(stderr,"error: cannot create target!\n"),1;
		while (!xrf_read()&&!avi_write())
			if (!(xrf_count&15)) fprintf(stderr,"%d/%d\015",xrf_count,count_z);//(stderr,"%05.02f\015",xrf_cursor*100.0/xrf_length)
		if (avi_finish(),xrf_cursor!=xrf_length)
			return xrf_close(),fprintf(stderr,"error: something went wrong!\n"),1;
		else
		{
			#ifdef _WIN32
			#ifndef RAWWW // disable VFW!
			if (!avi_length) // using VFW32.DLL means we cannot know the output length in advance
				if (avi_file=fopen(t,"rb"))
					fseek(avi_file,0,SEEK_END),avi_length=ftell(avi_file),fclose(avi_file);
			#endif
			#endif
			fprintf(stderr,"%d frames (%d unused), " filesize_style ".\n",xrf_count,xrf_dummy,filesize_upper(avi_length),filesize_lower(avi_length));
		}
	}
	else // examine
	{
		int llv=video_x*video_y*3,lla=audio_z*flags_audio[flags_z&3]; avi_length=(long long int)(llv+lla)*count_z;
		printf("%d frames x (%d+%d video+audio bytes) ~ " filesize_style ".\n",count_z,llv,lla,filesize_upper(avi_length),filesize_lower(avi_length));
	}
	return xrf_close(); // always 0 OK
}
