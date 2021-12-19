 //  ####  ######    ####  #######   ####    ----------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####    ----------------------- //

#define MY_CAPTION "XRF"
#define MY_VERSION "20211217"//"1555"
#define MY_LICENSE "Copyright (C) 2019-2021 Cesar Nicolas-Gonzalez"

#define GPL_3_INFO \
	"This program comes with ABSOLUTELY NO WARRANTY; for more details" "\n" \
	"please read the GNU General Public License. This is free software" "\n" \
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

// XRF is a small tool that reads the temporary XOR-RLE-FILM files
// generated by CPCEC and ZXSEC and decodes them into video and audio
// that can be processed by conventional codecs such as ZMBV and XVID.
// The Windows version relies on the Video For Windows (VFW) interface;
// other systems will get RGB24+PCM AVI files to feed FFMPEG with.

#include <stdio.h> // printf...
#include <string.h> // strcmp...
#include <stdlib.h> // malloc...

#ifdef _WIN32 // "BYTE", "WORD", "DWORD" and (unused) "QWORD" are always exactly 8, 16, 32 and 64 bits long

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <vfw.h>

#else // "char", "short int" and "long long int" are 8, 16 and 64 bits, but "long int" can be 32 or 64 bits

#include <stdint.h>
#define BYTE uint8_t // can this be safely reduced to "unsigned char"?
#define WORD uint16_t // can this be safely reduced to "unsigned short"?
#define DWORD uint32_t // this CANNOT be safely reduced to "unsigned int"!

#endif

// common data types -- mostly based on ARGB32 video and lLrR32 audio //

#define MEMZERO(x) memset(&x,0,sizeof(x))
#define MAXVIDEOBYTES (4096*512) // 2 MB > a frame of video at 800x600px 32bpp
#define MAXAUDIOBYTES (4096*48) // 192 kB > a second of audio at 48000x2ch 16b
BYTE *argb32,*diff32; // current and previous bitmap, respectively; the caller must assign them
BYTE flags_audio[4]={1,2,2,4}; // sample size according to 16bits (bit 0) and stereo (bit1)
BYTE wave32[MAXAUDIOBYTES]; // the current audio frame

int video_x,video_y,audio_z,clock_z,flags_z,count_z; // dimensions and features of the video and audio streams

// open and read a XRF file ----------------------------------------- //

FILE *xrf_file=NULL; int xrf_length,xrf_cursor,xrf_count,xrf_dummy;
BYTE *xrf_bitmap=NULL,*xrf_shadow=NULL,*xrf_chunk=NULL; // buffers
#define XRF_CHUNKSIZE ((MAXVIDEOBYTES+MAXAUDIOBYTES)*9/8+32)
#define xrf_decode0() (!(a>>=1)&&(b=*w++,a=128))
#define xrf_decode1() (xrf_decode0(),(b&a)?*w++:0)
int xrf_decode(BYTE *t,BYTE *s,int *l,int x) // equally hacky decoder based on an 8-bit RLE and an interleaved pseudo Huffman filter!
{
	BYTE *w=s,*z=t,a=0,b=0; int m=-1,n;
	for (;;)
	{
		xrf_decode0(); if (b&a) // special case of xrf_decode1(): we must catch "100000000"
		{
			if (!(n=*w++))
				break; // EOF!
		}
		else
			n=0;
		*z=n,z+=x; if (n==m)
		{
			if ((n=xrf_decode1())==255)
				n=510+(xrf_decode1()<<8);
			if (n>=254)
				n+=xrf_decode1();
			for (;n;--n)
				*z=m,z+=x;
			m=-1; // avoid accidental repetitions
		}
		else
			m=n;
	}
	return *l=(z-t)/x,w-s;
}

#define xrf_fgetc() fgetc(xrf_file)
int xrf_fgetcc(void) { int m=xrf_fgetc()<<8; return m+xrf_fgetc(); } // Motorola order, (*(WORD*))(x) is no use
int xrf_fgetcccc(void) { int m=xrf_fgetc()<<24; m+=xrf_fgetc()<<16; m+=xrf_fgetc()<<8; return m+xrf_fgetc(); }
int fread1(BYTE *t,int l,FILE *f) { int i=0,j; while (i<l&&(j=fread(&t[i],1,l-i,f))) i+=j; return i; }

int xrf_open(char *s) // opens a XRF file and sets the common parameters up; !0 ERROR
{
	if (!xrf_bitmap&&!(xrf_bitmap=malloc(MAXVIDEOBYTES)))
		return 1; // cannot allocate video buffer!
	if (!xrf_shadow&&!(xrf_shadow=malloc(MAXVIDEOBYTES)))
		return 1; // cannot allocate audio buffer!

	if (xrf_file) return 1; // already open!
	if (!(xrf_file=fopen(s,"rb"))) return 1;

	fseek(xrf_file,0,SEEK_END);
	xrf_length=ftell(xrf_file);
	fseek(xrf_file,0,SEEK_SET);
	unsigned char id[9]; id[fread1(id,8,xrf_file)]=0;
	video_x=xrf_fgetcc();
	video_y=xrf_fgetcc();
	audio_z=xrf_fgetcc();
	clock_z=xrf_fgetc();
	flags_z=xrf_fgetc();
	count_z=xrf_fgetcccc();
	xrf_cursor=8+8+4; // the bytes we've read so far
	xrf_count=xrf_dummy=0;
	argb32=xrf_bitmap; diff32=xrf_shadow; // this speeds things up later: swapping pointers instead of their contents
	return (!(video_x>0&&video_y>0&&clock_z>0&&audio_z>=0)||strcmp(id,"XRF1!\015\012\032"))?fclose(xrf_file),1:0; // XRF-1 was limited to 8-bit RLE lengths, and XRF+1 didn't store the amount of frames!
}
int xrf_read(void) // reads a XRF chunk and decodes the current video and audio frames; !0 ERROR/EOF
{
	if (!xrf_file) return 1; // file not open!

	if (xrf_count>=count_z) return 1; // EOF!

	if (!xrf_chunk&&!(xrf_chunk=malloc(XRF_CHUNKSIZE))) // pathological case XX.XX -> XX.XX.00 plus end marker
		return 1; // cannot allocate memory!

	int i,j,k,l=xrf_fgetcccc();
	if (l<1||l>XRF_CHUNKSIZE)
		return 1; // improper chunk size!
	if (fread1(xrf_chunk,l,xrf_file)!=l)
		return 1; // file is truncated!
	xrf_cursor+=4+l;

	BYTE *s=xrf_chunk; i=0;
	s+=xrf_decode(&diff32[0],s,&j,4); i+=j; // B
	s+=xrf_decode(&diff32[1],s,&j,4); i+=j; // G
	s+=xrf_decode(&diff32[2],s,&j,4); i+=j; // R
	// s+=xrf_decode(&diff32[3],s,&j,4); i+=j; // A
	if (i)
	{
		if (i!=video_x*video_y*3) return 1; // damaged video frame!
		BYTE *xyz=argb32; argb32=diff32; diff32=xyz;
		for (int k=0;k<video_x*video_y;++k) ((DWORD*)argb32)[k]^=((DWORD*)diff32)[k]; // apply changes!
	}
	else
		++xrf_dummy;
	if (audio_z)
	{
		i=0;
		switch(flags_z&3)
		{
			case 0: // 8-bit mono
				s+=xrf_decode(&wave32[0],s,&k,1); i+=k; // M
				i-=audio_z;
				break;
			case 1: // 16-bit mono
			case 2: // 8-bit stereo: equal to 16-bit mono on the lil-endian AVI!
				s+=xrf_decode(&wave32[0],s,&k,2); i+=k; // m/L
				s+=xrf_decode(&wave32[1],s,&k,2); i+=k; // M/R
				i-=audio_z*2;
				break;
			case 3: // 16-bit stereo
				s+=xrf_decode(&wave32[0],s,&k,4); i+=k; // l
				s+=xrf_decode(&wave32[1],s,&k,4); i+=k; // L
				s+=xrf_decode(&wave32[2],s,&k,4); i+=k; // r
				s+=xrf_decode(&wave32[3],s,&k,4); i+=k; // R
				i-=audio_z*4;
				break;
		}
		if (i) return 1; // damaged audio frame!
	}

	return ++xrf_count,0;
}
int xrf_close(void) // close and clean up; always 0 OK
{
	if (xrf_bitmap) free(xrf_bitmap),xrf_bitmap=NULL;
	if (xrf_shadow) free(xrf_shadow),xrf_shadow=NULL;
	if (xrf_chunk) free(xrf_chunk),xrf_chunk=NULL;
	if (xrf_file) fclose(xrf_file),xrf_file=NULL;
	return 0;
}

// create and write an AVI file through Windows' AVIFILE API -------- //

int avi_videos,avi_audios;
BYTE *avi_canvas=NULL; // ARGB32 canvas

#ifdef _WIN32

char avi_fourcc[5]=""; // uncompressed "DIB " is even worse than RGB24: it's ARGB32!
PAVIFILE vfw_file=NULL; IAVIStream *vfw_video,*vfw_codec,*vfw_audio;

int vfw_create(char *s) // !0 ERROR
{
	if (vfw_file) return 1;

	avi_videos=avi_audios=0; vfw_video=vfw_codec=vfw_audio=NULL;
	AVIFileInit(); remove(s); // VFW needs erasing the file beforehand!
	if (AVIFileOpen(&vfw_file,s,OF_CREATE|OF_WRITE,NULL))
		return AVIFileExit(),1;

	AVISTREAMINFO vhdr; MEMZERO(vhdr);
	vhdr.fccType=streamtypeVIDEO;
	//vhdr.fccHandler=0;
	vhdr.dwRate=(vhdr.dwScale=1)*clock_z;
	vhdr.dwSuggestedBufferSize=video_x*video_y*4;
	//vhdr.rcFrame.left=vhdr.rcFrame.top=0;
	vhdr.rcFrame.right=video_x;
	vhdr.rcFrame.bottom=video_y;
	if (AVIFileCreateStream(vfw_file,&vfw_video,&vhdr))
		vfw_video=NULL;

	AVICOMPRESSOPTIONS vopt; MEMZERO(vopt);
	vopt.fccHandler=mmioFOURCC(avi_fourcc[0],avi_fourcc[1],avi_fourcc[2],avi_fourcc[3]);
	vopt.dwFlags=AVICOMPRESSF_KEYFRAMES; vopt.dwKeyFrameEvery=clock_z*10; // 10 s/keyframe
	BITMAPINFO bmi; MEMZERO(bmi); bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biPlanes=1; bmi.bmiHeader.biBitCount=32; bmi.bmiHeader.biCompression=BI_RGB;
	bmi.bmiHeader.biWidth=video_x; bmi.bmiHeader.biHeight=video_y;
	// negative values mean top-to-bottom; Windows' default is bottom-to-top
	if (AVIMakeCompressedStream(&vfw_codec,vfw_video,&vopt,NULL))
		vfw_codec=NULL;
	else
		AVIStreamSetFormat(vfw_codec,0,&bmi,sizeof(bmi));

	if (audio_z)
	{
		AVISTREAMINFO ahdr; MEMZERO(ahdr);
		ahdr.fccType=streamtypeAUDIO;
		ahdr.dwRate=(ahdr.dwSampleSize=ahdr.dwScale=flags_audio[flags_z&3])*clock_z*audio_z;
		ahdr.dwQuality=(DWORD)-1;
		WAVEFORMATEX wfex; MEMZERO(wfex); wfex.wFormatTag=WAVE_FORMAT_PCM;
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
		// it sucks, but we must turn the bitmap upside down, negative height won't do :-(
		for (int y=0,yy=video_y;--yy,y<video_y;++y)
			memcpy(&avi_canvas[y*video_x*4],&argb32[yy*video_x*4],video_x*4);
		AVIStreamWrite(vfw_codec,avi_videos++,1,avi_canvas,video_x*video_y*4,0,NULL,NULL);
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
	if (avi_canvas) free(avi_canvas),avi_canvas=NULL;
	if (vfw_audio) AVIStreamRelease(vfw_audio),vfw_audio=NULL;
	if (vfw_codec) AVIStreamRelease(vfw_codec),vfw_codec=NULL;
	if (vfw_video) AVIStreamRelease(vfw_video),vfw_video=NULL;
	if (vfw_file) AVIFileRelease(vfw_file),vfw_file=NULL;
	AVIFileExit(); return 0;
}

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

#define avi_fputc(x) fputc(x,avi_file)
int avi_fputcccc(int i) { avi_fputc(i); avi_fputc(i>>8); avi_fputc(i>>16); return avi_fputc(i>>24); }
void avi_mputcc(BYTE *x,int i) { *x=i; x[1]=i>>8; } // compatible, rather than ((*(WORD*))(x))=i)
void avi_mputcccc(BYTE *x,int i) { *x=i; x[1]=i>>8; x[2]=i>>16; x[3]=i>>24; } // ditto, for DWORD
int fwrite1(BYTE *t,int l,FILE *f) { int i=0,j; while (i<l&&(j=fwrite(&t[i],1,l-i,f))) i+=j; return i; }

int avi_create(char *s)
{
	#ifdef _WIN32
	if (*avi_fourcc) return vfw_create(s);
	#endif
	if (avi_file) return 1;

	if (!strcmp(s,"-"))
		avi_file=stdout; // STDOUT is always open!
	else if (!(avi_file=fopen(s,"wb")))
		return 1;

	if (audio_z)
	{
		avi_mputcccc(&avi_header[0x0020],(1000000+clock_z/2)/clock_z);
		avi_mputcccc(&avi_header[0x0024],clock_z*(8+video_x*video_y*3+8+audio_z*flags_audio[flags_z&3])); // max byterate
		avi_mputcc(&avi_header[0x0040],video_x); avi_mputcc(&avi_header[0x0044],video_y);
		avi_mputcc(&avi_header[0x0084],clock_z);
		int i; i=video_x*video_y*3;
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

		avi_length=fwrite1(avi_header,sizeof(avi_header),avi_file)+12;
		avi_fputcccc(0x5453494C); // "LIST"
		avi_fputcccc(4+i); // "LIST:movi" size
		avi_fputcccc(0x69766F6D); // "movi"
	}
	else
	{
		avi_mputcccc(&avi_h_mute[0x0020],(1000000+clock_z/2)/clock_z);
		avi_mputcccc(&avi_h_mute[0x0024],clock_z*(8+video_x*video_y*3)); // max byterate
		avi_mputcc(&avi_h_mute[0x0040],video_x); avi_mputcc(&avi_h_mute[0x0044],video_y);
		avi_mputcc(&avi_h_mute[0x0084],clock_z);
		int i; i=video_x*video_y*3;
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

		avi_length=fwrite1(avi_h_mute,sizeof(avi_h_mute),avi_file)+12;
		avi_fputcccc(0x5453494C); // "LIST"
		avi_fputcccc(4+i); // "LIST:movi" size
		avi_fputcccc(0x69766F6D); // "movi"
	}

	return 0;
}
int avi_write(void)
{
	if (!avi_canvas&&!(avi_canvas=malloc(MAXVIDEOBYTES)))
		return 1; // cannot allocate memory!

	#ifdef _WIN32
	if (*avi_fourcc) return vfw_write();
	#endif
	if (!avi_file) return 1;

	// it sucks, but the only raw compression is the ARGB32->RGB24 clipping
	// and we still have to store the resulting bitmap upside down, too :-(
	int l; BYTE *t=avi_canvas;
	for (int y=0;y<video_y;++y)
	{
		BYTE *s=&argb32[(video_y-y-1)*4*video_x];
		for (int x=0;x<video_x;++x)
			*t++=*s++, // copy B
			*t++=*s++, // copy G
			*t++=*s++, // copy R
			++s; // skip A!
	}
	avi_fputcccc(0x62643030); // "00db"
	avi_fputcccc(l=t-avi_canvas);
	if (l!=fwrite1(avi_canvas,l,avi_file)) return 1;
	avi_length+=8+l;
	if (audio_z)
	{
		avi_fputcccc(0x62773130); // "01wb"
		avi_fputcccc(l=audio_z*flags_audio[flags_z&3]);
		if (l&1) wave32[l++]=0; // RIFF even-padding
		if (l!=fwrite1(wave32,l,avi_file)) return 1;
		avi_length+=8+l;
	}
	++avi_videos; avi_audios+=audio_z;

	return 0;
}
int avi_finish(void)
{
	#ifdef _WIN32
	if (*avi_fourcc) return vfw_finish();
	#endif
	if (!avi_file) return 1;

	avi_length+=8;
	avi_fputcccc(0x31786469); // "idx1"
	if (audio_z)
		avi_fputcccc(count_z*32);
	else
		avi_fputcccc(count_z*16);

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
	char *s=NULL,*t=NULL,*z=NULL; int i=0;
	while (++i<argc)
	{
		if (!strcmp("-h",argv[i])||!strcmp("--help",argv[i]))
			i=argc; // help!
		else if (!s)
			s=argv[i];
		else if (!t)
			t=argv[i];
		#ifdef _WIN32
		else if (!z)
		{
			if (strlen(argv[i])==4)
				strcpy(avi_fourcc,z=argv[i]);
			else i=argc; // help!
		}
		#endif
		else i=argc; // help!
	}
	if (!s||i>argc)
	{
		printf(MY_CAPTION " " MY_VERSION " " MY_LICENSE "\n"
			"\n"
			#ifdef _WIN32
			"usage: xrf source.xrf [target.avi [fourcc]]\n"
			#else
			"usage: xrf source.xrf [target.avi]\n"
			#endif
			"       xrf source.xrf - | ffmpeg [filters] -i - [options] target\n"
			"\n"
			GPL_3_INFO
			"\n");
			#ifdef _WIN32
			ICINFO lpicinfo; ICInfo(0,-1,&lpicinfo); int n=lpicinfo.fccHandler;
			for (int i=0;i<n;++i)
				ICInfo(0,i,&lpicinfo),printf("%s%c%c%c%c",i?i%10?" ":"\n\t\t":"\n- fourcc codes: ",(char)lpicinfo.fccHandler,(char)(lpicinfo.fccHandler>>8),(char)(lpicinfo.fccHandler>>16),(char)(lpicinfo.fccHandler>>24));
			printf("\n");
			#endif
		return 1;
	}
	if (xrf_open(s))
		return xrf_close(),fprintf(stderr,"error: cannot open source!\n"),1;
	if (!count_z)
		return xrf_close(),fprintf(stderr,"error: source is empty!\n"),1;
	fprintf(stderr,audio_z?"VIDEO %dx%dpx %dHz - AUDIO %dch%02db %dHz\n":"VIDEO %dx%dpx %dHz - NO AUDIO\n",video_x,video_y,clock_z,(flags_z&2)?2:1,(flags_z&1)?16:8,audio_z*clock_z);

	if (t) // process
	{
		if (avi_create(t))
			return xrf_close(),avi_finish(),fprintf(stderr,"error: cannot create target!\n"),1;
		while (!xrf_read()&&!avi_write())
			fprintf(stderr,"%05.02f\015",xrf_cursor*100.0/xrf_length);
		avi_finish();
		if (xrf_cursor!=xrf_length)
			fprintf(stderr,"error: cannot decode/encode data!\n");
		else
		{
			#ifdef _WIN32
			if (!avi_length) // using VFW32.DLL means we cannot know the output length in advance
				if (avi_file=fopen(t,"rb"))
					fseek(avi_file,0,SEEK_END),avi_length=ftell(avi_file),fclose(avi_file);
			#endif
			fprintf(stderr,"%d frames (%d unused), %lld bytes.\n",xrf_count,xrf_dummy,avi_length);
		}
	}
	else // examine
	{
		long long int lv=count_z*video_x*video_y*3,la=count_z*audio_z*flags_audio[flags_z&3],lz=lv+la;
		printf("%d frames, %lld video + %lld audio = %lld bytes.\n",count_z,lv,la,lz); // not "%lli"!
	}
	return xrf_close(),t&&xrf_cursor!=xrf_length;
}
