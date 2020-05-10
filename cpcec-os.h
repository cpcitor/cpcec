 //  ####  ######    ####  #######   ####    ----------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####    ----------------------- //

// Because the goal of the emulation itself is to be OS-independent,
// the interactions between the emulator and the OS are kept behind an
// interface of variables and procedures that don't require particular
// knowledge of the emulation's intrinsic properties.

// To compile the emulator for Windows 5.0+, the default platform,
// "$(CC) -xc cpcec.c -luser32 -lgdi32 -lcomdlg32 -lshell32 -lwinmm"
// or the equivalent options as defined by your preferred compiler.
// Tested compilers: GCC 4.9.2, GCC 5.1.0, TCC 0.9.27, Pelles C 4.50.113

#define INLINE // 'inline': useless in TCC and GCC4, harmful in GCC5!
INLINE int ucase(int i) { return i>='a'&&i<='z'?i-32:i; }
INLINE int lcase(int i) { return i>='A'&&i<='Z'?i+32:i; }

unsigned char session_scratch[1<<18]; // at least 256k!

#include "cpcec-a7.h" //unsigned char *onscreen_chrs;

char caption_version[]=MY_CAPTION " " MY_VERSION;

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1 // low dependencies
#endif

#ifdef SDL_MAIN_HANDLED // SDL2?

#include "cpcec-ox.h"

#else // START OF WINDOWS 5.0+ DEFINITIONS ========================== //

#include <windows.h> // KERNEL32.DLL, USER32.DLL, GDI32.DLL, WINMM.DLL, COMDLG32.DLL, SHELL32.DLL
#include <commdlg.h> // COMDLG32.DLL: getOpenFileName()...
#include <mmsystem.h> // WINMM.DLL: waveOutWrite()...
#include <shellapi.h> // SHELL32.DLL: DragQueryFile()...

typedef union { unsigned short w; struct { unsigned char l,h; } b; } Z80W; // WIN32 is a lil-endian platform!

#define STRMAX 256 // widespread in Windows
#define PATH_SEPARATOR '\\' // '/' in POSIX
#define strcasecmp _stricmp
#define fsetsize(f,l) _chsize(_fileno(f),(l))
#include <io.h> // _chsize(),_fileno()...

#ifdef DEBUG
#define logprintf(...) (fprintf(stdout,__VA_ARGS__))
#else
#define logprintf(...)
#endif

#define MESSAGEBOX_WIDETAB "\t"

// general engine constants and variables --------------------------- //

#define VIDEO_DATATYPE DWORD
#define VIDEO1(x) (x)

#define VIDEO_FILTER_AVG(x,y) (((((x&0xFF00FF)*3+(y&0xFF00FF)+(x<y?0x30003:0))&0x3FC03FC)+(((x&0xFF00)*3+(y&0xFF00)+(x<y?0x300:0))&0x3FC00))>>2) // faster
#define VIDEO_FILTER_STEP(r,x,y) r=VIDEO_FILTER_AVG(x,y),x=r // hard interpolation
//#define VIDEO_FILTER_STEP(r,x,y) r=VIDEO_FILTER_AVG(x,y),x=y // soft interpolation
#define VIDEO_FILTER_X1(x) (((x>>1)&0x7F7F7F)+0x2B2B2B)
//#define VIDEO_FILTER_X1(x) (((x>>2)&0x3F3F3F)+0x404040) // heavier
//#define VIDEO_FILTER_X1(x) (((x>>2)&0x3F3F3F)*3+0x161616) // lighter

#if 0 // 8 bits
	#define AUDIO_DATATYPE unsigned char
	#define AUDIO_BITDEPTH 8
	#define AUDIO_ZERO 128
	#define AUDIO1(x) (x)
#else // 16 bits
	#define AUDIO_DATATYPE signed short
	#define AUDIO_BITDEPTH 16
	#define AUDIO_ZERO 0
	#define AUDIO1(x) (x)
#endif // bitsize
#define AUDIO_CHANNELS 1

VIDEO_DATATYPE *video_frame; // video frame, allocated on runtime
AUDIO_DATATYPE *audio_frame,audio_buffer[AUDIO_LENGTH_Z],audio_memory[AUDIO_N_FRAMES*AUDIO_LENGTH_Z]; // audio frame, cycles during playback
VIDEO_DATATYPE *video_target; // pointer to current video pixel
AUDIO_DATATYPE *audio_target; // pointer to current audio sample
int video_pos_x,video_pos_y,audio_pos_z; // counters to keep pointers within range
BYTE video_interlaced=0,video_interlaces=0,video_interlacez=0; // video scanline status
BYTE video_framelimit=0,video_framecount=0; // video frameskip counters
BYTE audio_disabled=0,audio_session=0; // audio status and counter
unsigned char session_path[STRMAX],session_parmtr[STRMAX],session_tmpstr[STRMAX],session_substr[STRMAX],session_info[STRMAX]="";

int session_timer,session_event=0; // timing synchronisation and user command
BYTE session_fast=0,session_wait=0,session_audio=1,session_blit=0; // timing and devices
BYTE session_stick=1,session_shift=0,session_key2joy=0; // keyboard and joystick
BYTE video_scanline=0,video_scanlinez=-1; // 0 = solid, 1 = scanlines, 2 = full interlace, 3 = half interlace
BYTE video_filter=0,audio_filter=0; // interpolation flags
BYTE session_intzoom=0;

FILE *session_wavefile=NULL; // audio recording is done on each session update

RECT session_ideal; // ideal rectangle where the window fits perfectly
JOYINFO session_ji; // joystick buffer
HWND session_hwnd; // window handle
HMENU session_menu=NULL; // menu handle
HDC session_dc,session_cdc; HGDIOBJ session_dib; // video structs
#ifndef DEBUG
	VIDEO_DATATYPE *debug_frame;
	BYTE debug_buffer[DEBUG_LENGTH_X*DEBUG_LENGTH_Y]; // [0] can be a valid character, 128 (new redraw required) or 0 (redraw not required)
	HGDIOBJ session_dbg_dib;
#endif
HWAVEOUT session_wo; WAVEHDR session_wh; MMTIME session_mmtime; // audio structs

BYTE session_paused=0,session_signal=0;
#define SESSION_SIGNAL_FRAME 1
#define SESSION_SIGNAL_DEBUG 2
#define SESSION_SIGNAL_PAUSE 4

#define kbd_bit_set(k) (kbd_bit[k/8]|=1<<(k%8))
#define kbd_bit_res(k) (kbd_bit[k/8]&=~(1<<(k%8)))
#define kbd_bit_tst(k) (kbd_bit[k/8]&(1<<(k%8)))
BYTE kbd_bit[16]; // up to 128 keys in 16 rows of 8 bits

// A modern keyboard as seen by Windows through WM_KEYDOWN and WK_KEYUP; extended keys are shown here with bit 7 on.
// +----+   +-------------------+ +-------------------+ +-------------------+ +--------------+ *1 = trapped by Win32
// | 01 |   | 3B | 3C | 3D | 3E | | 3F | 40 | 41 | 42 | | 43 | *1 | 57 | 58 | | *1 | 46 | 45 | *2 = sequence "1D B8"
// +----+   +-------------------+ +-------------------+ +-------------------+ +--------------+ *3 = "DB" ; *4 = "DC"
// +------------------------------------------------------------------------+ +--------------+ +-------------------+
// | 29 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 0A | 0B | 0C | 0D | 0E    | | D2 | C7 | C9 | | C5 | B5 | 37 | 4A |
// +------------------------------------------------------------------------+ +--------------+ +-------------------+
// | 0F  | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 1A | 1B |      | | D3 | CF | D1 | | 47 | 48 | 49 |    |
// +------------------------------------------------------------------+ 1C  | +--------------+ +--------------+ 4E |
// | 3A   | 1E | 1F | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 2B |     |                  | 4B | 4C | 4D |    |
// +------------------------------------------------------------------------+      +----+      +-------------------+
// | 2A  | 56 | 2C | 2D | 2E | 2F | 30 | 31 | 32 | 33 | 34 | 35 | 36        |      | C8 |      | 4F | 50 | 51 |    |
// +------------------------------------------------------------------------+ +--------------+ +--------------+ 9C |
// | 1D  | *3  | *1  | 39                           | *2  | *4  | DD  | 9D  | | CB | D0 | CD | | 52      | 53 |    |
// +------------------------------------------------------------------------+ +--------------+ +-------------------+
// watch out: we use the HARDWARE keyboard symbols rather than the SOFTWARE ones,
// to ensure that the emulator works regardless of the operating system language!
#define	KBCODE_NULL	0
// function keys
#define	KBCODE_F1	0x3B
#define	KBCODE_F2	0x3C
#define	KBCODE_F3	0x3D
#define	KBCODE_F4	0x3E
#define	KBCODE_F5	0x3F
#define	KBCODE_F6	0x40
#define	KBCODE_F7	0x41
#define	KBCODE_F8	0x42
#define	KBCODE_F9	0x43
#define	KBCODE_F10	0x44
#define	KBCODE_F11	0x57
#define	KBCODE_F12	0x58
// leftmost keys
#define	KBCODE_ESCAPE	0x01
#define	KBCODE_TAB	0x0F
#define	KBCODE_CAP_LOCK	0x3A
#define	KBCODE_L_SHIFT	0x2A
#define	KBCODE_L_CTRL	0x1D
#define	KBCODE_L_ALT	0x38 // trapped by Win32
// alphanumeric row 1
#define	KBCODE_1	0x02
#define	KBCODE_2	0x03
#define	KBCODE_3	0x04
#define	KBCODE_4	0x05
#define	KBCODE_5	0x06
#define	KBCODE_6	0x07
#define	KBCODE_7	0x08
#define	KBCODE_8	0x09
#define	KBCODE_9	0x0A
#define	KBCODE_0	0x0B
#define	KBCODE_CHR1_1	0x0C
#define	KBCODE_CHR1_2	0x0D
// alphanumeric row 2
#define	KBCODE_Q	0x10
#define	KBCODE_W	0x11
#define	KBCODE_E	0x12
#define	KBCODE_R	0x13
#define	KBCODE_T	0x14
#define	KBCODE_Y	0x15
#define	KBCODE_U	0x16
#define	KBCODE_I	0x17
#define	KBCODE_O	0x18
#define	KBCODE_P	0x19
#define	KBCODE_CHR2_1	0x1A
#define	KBCODE_CHR2_2	0x1B
// alphanumeric row 3
#define	KBCODE_A	0x1E
#define	KBCODE_S	0x1F
#define	KBCODE_D	0x20
#define	KBCODE_F	0x21
#define	KBCODE_G	0x22
#define	KBCODE_H	0x23
#define	KBCODE_J	0x24
#define	KBCODE_K	0x25
#define	KBCODE_L	0x26
#define	KBCODE_CHR3_1	0x27
#define	KBCODE_CHR3_2	0x28
#define	KBCODE_CHR3_3	0x2B
// alphanumeric row 4
#define	KBCODE_Z	0x2C
#define	KBCODE_X	0x2D
#define	KBCODE_C	0x2E
#define	KBCODE_V	0x2F
#define	KBCODE_B	0x30
#define	KBCODE_N	0x31
#define	KBCODE_M	0x32
#define	KBCODE_CHR4_1	0x33
#define	KBCODE_CHR4_2	0x34
#define	KBCODE_CHR4_3	0x35
#define	KBCODE_CHR4_4	0x56
#define	KBCODE_CHR4_5	0x29
// rightmost keys
#define	KBCODE_SPACE	0x39
#define	KBCODE_BKSPACE	0x0E
#define	KBCODE_ENTER	0x1C
#define	KBCODE_R_SHIFT	0x36
#define	KBCODE_R_CTRL	0x9D
#define	KBCODE_R_ALT	0xB8 // trapped by Win32
// extended keys
#define	KBCODE_PRINT	0x54 // trapped by Win32
#define	KBCODE_SCR_LOCK	0x46
#define	KBCODE_HOLD	0x45
#define	KBCODE_INSERT	0xD2
#define	KBCODE_DELETE	0xD3
#define	KBCODE_HOME	0xC7
#define	KBCODE_END	0xCF
#define	KBCODE_PRIOR	0xC9
#define	KBCODE_NEXT	0xD1
#define	KBCODE_UP	0xC8
#define	KBCODE_DOWN	0xD0
#define	KBCODE_LEFT	0xCB
#define	KBCODE_RIGHT	0xCD
#define	KBCODE_NUM_LOCK	0xC5
// numeric keypad
#define	KBCODE_X_7	0x47
#define	KBCODE_X_8	0x48
#define	KBCODE_X_9	0x49
#define	KBCODE_X_4	0x4B
#define	KBCODE_X_5	0x4C
#define	KBCODE_X_6	0x4D
#define	KBCODE_X_1	0x4F
#define	KBCODE_X_2	0x50
#define	KBCODE_X_3	0x51
#define	KBCODE_X_0	0x52
#define	KBCODE_X_DOT	0x53
#define	KBCODE_X_ENTER	0x9C
#define	KBCODE_X_ADD	0x4E
#define	KBCODE_X_SUB	0x4A
#define	KBCODE_X_MUL	0x37
#define	KBCODE_X_DIV	0xB5

const BYTE kbd_k2j[]= // these keys can simulate a joystick
	{ KBCODE_UP, KBCODE_DOWN, KBCODE_LEFT, KBCODE_RIGHT, KBCODE_Z, KBCODE_X, KBCODE_C, KBCODE_V };

unsigned char kbd_map[256]; // key-to-key translation map

// general engine functions and procedures -------------------------- //

int session_user(int k); // handle the user's commands; 0 OK, !0 ERROR. Must be defined later on!
#ifndef DEBUG
	void session_debug_show(void);
	int session_debug_user(int k); // debug logic is a bit different: 0 UNKNOWN COMMAND, !0 OK
#endif
INLINE void audio_playframe(int q,AUDIO_DATATYPE *ao); // handle the sound filtering; is defined in CPCEC-RT.H!

void session_please(void) // stop activity for a short while
{
	if (!session_wait)
	{
		if (session_audio)
			waveOutPause(session_wo);
		//video_framecount=-1;
		session_wait=1;
	}
}

#define session_kbdreset() memset(kbd_map,~~~0,sizeof(kbd_map)) // init and clean key map up
void session_kbdsetup(const unsigned char *s,char l) // maps a series of virtual keys to the real ones
{
	while (l--)
	{
		int k=*s++;
		kbd_map[k]=*s++;
	}
}
int session_key_n_joy(int k) // handle some keys as joystick motions
{
	if (session_key2joy)
		for (int i=0;i<sizeof(kbd_joy);++i)
			if (kbd_k2j[i]==k)
				return kbd_joy[i];
	return kbd_map[k];
}
BYTE session_dontblit=0;
void session_redraw(HWND hwnd,HDC h) // redraw the window contents
{
	RECT r; GetClientRect(hwnd,&r);
	int xx,yy; // calculate window area
	if ((xx=(r.right-=r.left))>0&&(yy=(r.bottom-=r.top))>0) // divisions by zero happen on WM_PAINT during window resizing!
	{
		if (xx>yy*VIDEO_PIXELS_X/VIDEO_PIXELS_Y) // window area is too wide?
			xx=yy*VIDEO_PIXELS_X/VIDEO_PIXELS_Y;
		if (yy>xx*VIDEO_PIXELS_Y/VIDEO_PIXELS_X) // window area is too tall?
			yy=xx*VIDEO_PIXELS_Y/VIDEO_PIXELS_X;
		if (session_intzoom) // integer zoom? (100%, 200%, 300%...)
		{
			xx=(xx/(VIDEO_PIXELS_X*62/64))*VIDEO_PIXELS_X; // the MM/NN factor is a tolerance margin:
			yy=(yy/(VIDEO_PIXELS_Y*62/64))*VIDEO_PIXELS_Y; // 61/64 allows 200% on windowed 1920x1080
		}
		if (!(xx*yy))
			xx=VIDEO_PIXELS_X,yy=VIDEO_PIXELS_Y; // window area is too small!
		int x=(r.right-xx)/2,y=(r.bottom-yy)/2; // locate bitmap on window center
		static HGDIOBJ session_oldselect=NULL;
		if (!session_dontblit)
		{
			if (session_oldselect!=session_dib)
				SelectObject(session_cdc,session_oldselect=session_dib);
			if (session_blit=(xx<=VIDEO_PIXELS_X||yy<=VIDEO_PIXELS_Y)) // window area is a perfect fit?
				BitBlt(h,x,y,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,session_cdc,VIDEO_OFFSET_X,VIDEO_OFFSET_Y,SRCCOPY); // fast :-)
			else
				StretchBlt(h,x,y,xx,yy,session_cdc,VIDEO_OFFSET_X,VIDEO_OFFSET_Y,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,SRCCOPY); // slow :-(
		}
		else
			session_dontblit=0;
		#ifndef DEBUG
		if (session_signal&SESSION_SIGNAL_DEBUG)
		{
			if (session_oldselect!=session_dbg_dib)
				SelectObject(session_cdc,session_oldselect=session_dbg_dib);
			BitBlt(h,(r.right-DEBUG_LENGTH_X*8)/2,(r.bottom-DEBUG_LENGTH_Y*DEBUG_LENGTH_Z)/2,DEBUG_LENGTH_X*8,DEBUG_LENGTH_Y*DEBUG_LENGTH_Y,session_cdc,0,0,SRCCOPY);
		}
		#endif
	}
}

void session_togglefullscreen(void)
{
	if (IsZoomed(session_hwnd))
	{
		SetWindowLong(session_hwnd,GWL_STYLE,(GetWindowLong(session_hwnd,GWL_STYLE)|WS_CAPTION)); //&~WS_POPUP&~WS_CLIPCHILDREN // show caption and buttons
		SetMenu(session_hwnd,session_menu); // show menu
		RECT r; GetWindowRect(session_hwnd,&r); // adjust to screen center
		ShowWindow(session_hwnd,SW_RESTORE);
		r.left+=((r.right-r.left)-session_ideal.right)/2;
		r.top+=((r.bottom-r.top)-session_ideal.bottom)/2;
		MoveWindow(session_hwnd,r.left,r.top,session_ideal.right,session_ideal.bottom,1);
	}
	else
	{
		SetWindowLong(session_hwnd,GWL_STYLE,(GetWindowLong(session_hwnd,GWL_STYLE)&~WS_CAPTION)); //|WS_POPUP|WS_CLIPCHILDREN // hide caption and buttons
		SetMenu(session_hwnd,NULL); // hide menu
		ShowWindow(session_hwnd,SW_MAXIMIZE); // adjust to entire screen
	}
}

LRESULT CALLBACK mainproc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) // window callback function
{
	switch (msg)
	{
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_SIZING:
			if (((LPRECT)lparam)->right-((LPRECT)lparam)->left<session_ideal.right) // too narrow?
				switch (wparam)
				{
					case WMSZ_TOPLEFT: case WMSZ_LEFT: case WMSZ_BOTTOMLEFT:
						((LPRECT)lparam)->left=((LPRECT)lparam)->right-session_ideal.right;
						break;
					case WMSZ_RIGHT: case WMSZ_TOPRIGHT: case WMSZ_BOTTOMRIGHT:
						((LPRECT)lparam)->right=((LPRECT)lparam)->left+session_ideal.right;
						break;
				}
			if (((LPRECT)lparam)->bottom-((LPRECT)lparam)->top<session_ideal.bottom) // too short?
				switch (wparam)
				{
					case WMSZ_TOPLEFT: case WMSZ_TOP: case WMSZ_TOPRIGHT:
						((LPRECT)lparam)->top=((LPRECT)lparam)->bottom-session_ideal.bottom;
						break;
					case WMSZ_BOTTOMLEFT: case WMSZ_BOTTOM: case WMSZ_BOTTOMRIGHT:
						((LPRECT)lparam)->bottom=((LPRECT)lparam)->top+session_ideal.bottom;
						break;
				}
			break;
		case WM_SIZE:
			InvalidateRect(hwnd,NULL,1); // force full update! there's dirt otherwise!
			break;
		case WM_SETFOCUS: // force full redraw
		case WM_PAINT:
			{
				PAINTSTRUCT ps; HDC h;
				if (h=BeginPaint(hwnd,&ps))
				{
					session_redraw(hwnd,h);
					EndPaint(hwnd,&ps);
				}
			}
			break;
		case WM_COMMAND:
			if (0x3F00==(WORD)wparam)
				PostMessage(hwnd,WM_CLOSE,0,0);
			else
			{
				session_shift=!!(wparam&0x4000); // bit 6 means SHIFT KEY ON
				session_event=wparam&0xBFFF; // cfr infra: bit 7 means CONTROL KEY OFF
			}
			break;
		#ifndef DEBUG
		case WM_CHAR:
			if (session_signal&SESSION_SIGNAL_DEBUG) // only relevant for the debugger, see below
				session_event=wparam>=32&&wparam<=255?wparam:0;
			break;
		#endif
		case WM_KEYDOWN:
			{
				int vkc=GetKeyState(VK_CONTROL)<0;
				session_shift=GetKeyState(VK_SHIFT)<0;
				#ifndef DEBUG
				if (session_signal&SESSION_SIGNAL_DEBUG)
					switch (wparam)
					{
						case VK_LEFT: session_event=8; break;
						case VK_RIGHT: session_event=9; break;
						case VK_DOWN: session_event=10; break;
						case VK_UP: session_event=11; break;
						case VK_RETURN: session_event=13; break;
						case VK_ESCAPE: session_event=27; break;
						case VK_HOME: session_event=28; break;
						case VK_END: session_event=29; break;
						case VK_NEXT: session_event=30; break;
						case VK_PRIOR: session_event=31; break;
						case VK_SPACE: if (session_shift) session_event=160; break; // special case unlike WM_CHAR
						case VK_BACK: session_event=session_shift?9:8; break;
						case VK_TAB: session_event=session_shift?7:12; break;
						default: session_event=0; break;
					}
				#endif
				int k=session_key_n_joy(((lparam>>16)&127)+((lparam>>17)&128));
				if (k<128) // normal key
				{
					#ifndef DEBUG
					if (!(session_signal&SESSION_SIGNAL_DEBUG))
					#endif
						kbd_bit_set(k);
				}
				else if (!session_event) // special key
					session_event=(k-(vkc?128:0))<<8;
			}
			break;
		case WM_KEYUP:
			{
				int k;
				if ((k=session_key_n_joy(((lparam>>16)&127)+((lparam>>17)&128)))<128) // normal key
					kbd_bit_res(k);
			}
			break;
		case WM_DROPFILES:
			DragQueryFile((HDROP)wparam,0,(LPTSTR)session_parmtr,STRMAX);
			DragFinish((HDROP)wparam);
			session_shift=GetKeyState(VK_SHIFT)<0,session_event=0x8000;
			break;
		case WM_ENTERSIZEMOVE: // pause before moving the window
		case WM_ENTERMENULOOP: // pause before showing the menus
			session_please();
		case WM_KILLFOCUS: // no 'break'!
			memset(kbd_bit,0,sizeof(kbd_bit)); // loss of focus: no keys!
		default: // no 'break'!
			if (msg==WM_SYSKEYDOWN&&wparam==VK_RETURN) // ALT+RETURN toggles MAXIMIZE/RESTORE!
				session_togglefullscreen();
			return DefWindowProc(hwnd,msg,wparam,lparam);
	}
	return 0;
}

INLINE int session_create(char *s) // create video+audio devices and set menu; 0 OK, !0 ERROR
{
	HMENU session_submenu=NULL;
	char c,*t; int i;
	if (!session_blit) while (c=*s++)
	{
		if (c=='=') // separator?
		{
			while (*s++!='\n') // ignore remainder
				{}
			AppendMenu(session_submenu,MF_SEPARATOR,0,0);
		}
		else if (c=='0') // menu item?
		{
			t=--s;
			i=strtol(t,&s,0); // allow either hex or decimal
			t=session_tmpstr;
			++s;
			while ((c=*s++)!='\n') // string with shortcuts
				*t++=c=='_'?'&':c;
			*t=0;
			AppendMenu(session_submenu,MF_STRING,i,session_tmpstr);
		}
		else // menu block
		{
			if (!session_menu)
				session_menu=CreateMenu();
			if (session_submenu)
				AppendMenu(session_menu,MF_POPUP,(UINT_PTR)session_submenu,session_parmtr);
			session_submenu=CreateMenu();
			t=session_parmtr;
			*t++='&'; // shortcut
			--s;
			while ((c=*s++)!='\n') // string, no shortcuts
				*t++=c;
			*t=0;
		}
	}
	if (session_menu&&session_submenu)
		AppendMenu(session_menu,MF_POPUP,(UINT_PTR)session_submenu,session_parmtr);
	WNDCLASS wc;
	wc.style=wc.cbClsExtra=wc.cbWndExtra=0;
	wc.lpfnWndProc=mainproc;
	wc.hInstance=GetModuleHandle(0);
	wc.hIcon=LoadIcon(wc.hInstance,MAKEINTRESOURCE(34002));//IDI_APPLICATION
	wc.hCursor=LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)(1+COLOR_WINDOWTEXT);//(COLOR_WINDOW+2);//0;//
	wc.lpszMenuName=NULL;
	wc.lpszClassName=MY_CAPTION;
	RegisterClass(&wc);
	session_ideal.left=session_ideal.top=0; // calculate ideal size
	session_ideal.right=VIDEO_PIXELS_X;
	session_ideal.bottom=VIDEO_PIXELS_Y;
	AdjustWindowRect(&session_ideal,i=session_blit?WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU:WS_OVERLAPPEDWINDOW,!!session_submenu);
	session_ideal.right-=session_ideal.left;
	session_ideal.bottom-=session_ideal.top;
	session_ideal.left=session_ideal.top=0; // ensure that the ideal area is defined as (0,0,WIDTH,HEIGHT)
	if (!(session_hwnd=CreateWindow(wc.lpszClassName,caption_version,i,CW_USEDEFAULT,CW_USEDEFAULT,session_ideal.right,session_ideal.bottom,NULL,session_submenu?session_menu:NULL,wc.hInstance,NULL)))
		return 1;
	DragAcceptFiles(session_hwnd,1);
	BITMAPINFO bmi;
	memset(&bmi,0,sizeof(bmi));
	bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth=VIDEO_LENGTH_X;
	bmi.bmiHeader.biHeight=-VIDEO_LENGTH_Y; // negative values make a top-to-bottom bitmap; Windows' default bitmap is bottom-to-top
	bmi.bmiHeader.biPlanes=1;
	bmi.bmiHeader.biBitCount=32; // cfr. VIDEO_DATATYPE
	bmi.bmiHeader.biCompression=BI_RGB;
	session_dc=GetDC(session_hwnd); // caution: we assume that if CreateWindow() succeeds all other USER and GDI calls will succeed too
	session_cdc=CreateCompatibleDC(session_dc); // ditto
	session_dib=CreateDIBSection(session_dc,&bmi,DIB_RGB_COLORS,(void **)&video_frame,NULL,0); // ditto
	#ifndef DEBUG
		bmi.bmiHeader.biWidth=DEBUG_LENGTH_X*8;
		bmi.bmiHeader.biHeight=-DEBUG_LENGTH_Y*DEBUG_LENGTH_Z;
		session_dbg_dib=CreateDIBSection(session_dc,&bmi,DIB_RGB_COLORS,(void **)&debug_frame,NULL,0);
	#endif
	ShowWindow(session_hwnd,SW_SHOWDEFAULT);
	UpdateWindow(session_hwnd);
	session_timer=GetTickCount();
	if (session_stick)
	{
		int i=joyGetNumDevs(),j=0;
		while (j<i&&joyGetPos(j,&session_ji)) // scan joysticks until we run out or one is OK
			++j;
		session_stick=(j<i)?j+1:0; // ID+1 if available, 0 if missing
	}
	session_wo=0; // no audio unless device is detected
	if (session_audio)
	{
		memset(&session_mmtime,0,sizeof(session_mmtime));
		session_mmtime.wType=TIME_SAMPLES; // Windows doesn't always provide TIME_MS!
		WAVEFORMATEX wfex;
		memset(&wfex,0,sizeof(wfex));
		wfex.wFormatTag=WAVE_FORMAT_PCM;
		wfex.wBitsPerSample=AUDIO_BITDEPTH, wfex.nBlockAlign=AUDIO_BITDEPTH/8*(wfex.nChannels=AUDIO_CHANNELS);
		wfex.nAvgBytesPerSec=wfex.nBlockAlign*(wfex.nSamplesPerSec=AUDIO_PLAYBACK);
		if (session_audio=!waveOutOpen(&session_wo,WAVE_MAPPER,&wfex,0,0,0))
		{
			memset(&session_wh,0,sizeof(WAVEHDR));
			memset(audio_frame=audio_memory,AUDIO_ZERO,sizeof(audio_memory));
			session_wh.lpData=(BYTE *)audio_memory;
			session_wh.dwBufferLength=AUDIO_N_FRAMES*AUDIO_LENGTH_Z*wfex.nBlockAlign;
			session_wh.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP;
			session_wh.dwLoops=-1; // loop forever!
			waveOutPrepareHeader(session_wo,&session_wh,sizeof(WAVEHDR));
			waveOutWrite(session_wo,&session_wh,sizeof(WAVEHDR));
			session_timer=0;
		}
	}
	session_please();
	return 0;
}

void session_menuinfo(void); // set the current menu flags. Must be defined later on!
BYTE session_dirtymenu=1; // to force new status text
INLINE int session_listen(void) // handle all pending messages; 0 OK, !0 EXIT
{
	static int config_signal=0; // catch DEBUG and PAUSE signals
	if (config_signal!=session_signal)
		config_signal=session_signal,session_dirtymenu=1;
	if (session_dirtymenu)
		session_dirtymenu=0,session_menuinfo();
	#ifndef DEBUG
	if (session_signal)
	#else
	if (session_signal&~SESSION_SIGNAL_DEBUG)
	#endif
	{
		#ifndef DEBUG
		if (session_signal&SESSION_SIGNAL_DEBUG)
		{
			if (*debug_buffer==128)
				session_please(),session_debug_show();
			if (*debug_buffer)//!=0
				session_redraw(session_hwnd,session_dc),*debug_buffer=0;
		}
		else
		#endif
		if (!session_paused) // set the caption just once
		{
			session_please();
			sprintf(session_tmpstr,"%s | %s | PAUSED",caption_version,session_info);
			SetWindowText(session_hwnd,session_tmpstr);
			session_paused=1;
		}
		WaitMessage(); session_dontblit=1; // reduce flickering
	}
	MSG msg;
	int q=0;
	while (PeekMessage(&msg,0,0,0,PM_REMOVE))
	{
		TranslateMessage(&msg);
		q|=msg.message==WM_QUIT;
		DispatchMessage(&msg);
		if (session_event)
		{
			#ifndef DEBUG
			if (!((session_signal&SESSION_SIGNAL_DEBUG)&&session_debug_user(session_event)))
			#endif
				q|=session_user(session_event);
			session_event=0;
		}
	}
	return q;
}

INLINE void session_writewave(AUDIO_DATATYPE *t); // save the current sample frame. Must be defined later on!
INLINE void session_render(void) // update video, audio and timers
{
	int i,j,k;
	static int performance_t=0,performance_f=0,performance_b=0; ++performance_f;
	if (!video_framecount) // do we need to hurry up?
	{
		if ((video_interlaces=!video_interlaces)||!video_interlaced)
			++performance_b,session_redraw(session_hwnd,session_dc);
		if (session_stick&&!session_key2joy) // do we need to check the joystick?
		{
			if (!joyGetPos(session_stick-1,&session_ji))
				j=((session_ji.wYpos<0x4000)?1:0)+((session_ji.wYpos>=0xC000)?2:0) // UP, DOWN
				+((session_ji.wXpos<0x4000)?4:0)+((session_ji.wXpos>=0xC000)?8:0) // LEFT, RIGHT
				+((session_ji.wButtons&JOY_BUTTON1)?16:0)+((session_ji.wButtons&JOY_BUTTON2)?32:0) // FIRE1, FIRE2
				+((session_ji.wButtons&JOY_BUTTON3)?64:0)+((session_ji.wButtons&JOY_BUTTON4)?128:0); // FIRE3, FIRE4
			else
				j=0; // joystick failure, release its keys
			for (i=0;i<sizeof(kbd_joy);++i)
			{
				k=kbd_joy[i];
				if (j&(1<<i))
					kbd_bit_set(k); // key is down
				else
					kbd_bit_res(k); // key is up
			}
		}
	}
	audio_target=&audio_memory[AUDIO_LENGTH_Z*audio_session];
	if (!audio_disabled) // avoid conflicts when realtime is off: output and playback buffers clash!
	{
		if (audio_filter) // audio interpolation: sample averaging
			audio_playframe(audio_filter,audio_frame==audio_buffer?audio_target:audio_frame);
		else if (audio_frame==audio_buffer)
			memcpy(audio_target,audio_buffer,sizeof(audio_buffer));
	}
	if (session_wavefile) // record audio output, if required
	{
		session_writewave(audio_frame);
		audio_frame=audio_buffer; // secondary buffer
	}
	else
		audio_frame=audio_target; // primary buffer
	if (session_audio) // use audio as clock
	{
		static BYTE s=1;
		if (s!=audio_disabled)
			if (s=audio_disabled) // silent mode needs cleanup
				memset(audio_memory,AUDIO_ZERO,sizeof(audio_memory));
		static BYTE o=1;
		if (o!=(session_fast|audio_disabled)) // sound needs higher priority, but only on realtime
		{
			SetPriorityClass(GetCurrentProcess(),(o=session_fast|audio_disabled)?BELOW_NORMAL_PRIORITY_CLASS:ABOVE_NORMAL_PRIORITY_CLASS);
			//SetThreadPriority(GetCurrentThread(),(o=session_fast|audio_disabled)?THREAD_PRIORITY_NORMAL:THREAD_PRIORITY_ABOVE_NORMAL);
		}
		if (waveOutGetPosition(session_wo,&session_mmtime,sizeof(MMTIME))) // MMSYSERR_NOERROR?
			session_audio=0,audio_disabled=-1; // audio device is lost!
		i=session_mmtime.u.sample,j=AUDIO_PLAYBACK;
	}
	else // use internal tick count as clock
		i=GetTickCount(),j=1000;
	if (session_wait||session_fast)
	{
		audio_session=((i/AUDIO_LENGTH_Z)+AUDIO_N_FRAMES-1)%AUDIO_N_FRAMES;
		session_timer=i; // ensure that the next frame can be valid!
	}
	else
	{
		if ((i=((session_timer+=(j/VIDEO_PLAYBACK))-i))>0)
		{
			if (i=(1000*i/j)) // avoid zero, it has a special value in Windows!
				Sleep(i>1000/VIDEO_PLAYBACK?1+1000/VIDEO_PLAYBACK:i);
		}
		else if (!video_framecount) video_framecount=-2; // automatic frameskip!
		audio_session=(audio_session+1)%AUDIO_N_FRAMES;
	}
	if (session_wait) // resume activity after a pause
	{
		if (session_audio)
			waveOutRestart(session_wo);
		//else
			//session_timer=GetTickCount();
		session_wait=0;
	}
	if ((i=GetTickCount())>(performance_t+1000)) // performance percentage
	{
		if (performance_t)
		{
			sprintf(session_tmpstr,"%s | %s | %g%% CPU %g%% %s",caption_version,session_info,performance_f*100.0/VIDEO_PLAYBACK,performance_b*100.0/VIDEO_PLAYBACK,session_blit?"GFX":"gfx");
			SetWindowText(session_hwnd,session_tmpstr);
		}
		performance_t=i,performance_f=performance_b=session_paused=0;
	}
}

INLINE void session_byebye(void) // delete video+audio devices
{
	if (session_wo)
	{
		waveOutReset(session_wo);
		waveOutUnprepareHeader(session_wo,&session_wh,sizeof(WAVEHDR));
		waveOutClose(session_wo);
	}
	#ifndef DEBUG
		DeleteObject(session_dbg_dib);
	#endif
	DeleteDC(session_cdc);
	DeleteObject(session_dib);
	ReleaseDC(session_hwnd,session_dc);
}

// auxiliary functions ---------------------------------------------- //

void session_detectpath(char *s) // detects session path
{
	char *t; if ((t=strrchr(strcpy(session_path,s),'\\'))||(t=strrchr(strcpy(session_path,s),':')))
		t[1]=0; // keep separator
	else
		*session_path=0; // no path
}
char *session_configfile(void) // returns path to configuration file
{
	return strcat(strcpy(session_parmtr,session_path),MY_CAPTION ".INI");
}

void session_writebitmap(FILE *f) // write current bitmap into a BMP file
{
	static BYTE r[VIDEO_PIXELS_X*3];
	for (int i=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-1;i>=VIDEO_OFFSET_Y;--i)
	{
		BYTE *s=(BYTE *)&video_frame[(VIDEO_OFFSET_X+i*VIDEO_LENGTH_X)],*t=r;
		for (int j=0;j<VIDEO_PIXELS_X;++j) // turn RGBA (32 bits) into RGB (24 bits)
			*t++=*s++, // copy R
			*t++=*s++, // copy G
			*t++=*s++, // copy B
			s++; // skip A
		fwrite(r,1,VIDEO_PIXELS_X*3,f);
	}
}

// menu item functions ---------------------------------------------- //

void session_menucheck(int id,int q) // set option `id`state as `q`
{
	if (session_menu)
		CheckMenuItem(session_menu,id,MF_BYCOMMAND+(q?MF_CHECKED:MF_UNCHECKED));
}
void session_menuradio(int id,int a,int z) // select option `id` in the range `a-z`
{
	if (session_menu)
		CheckMenuRadioItem(session_menu,a,z,id,MF_BYCOMMAND);
}

// message box ------------------------------------------------------ //

void session_message(char *s,char *t) // show multi-lined text `s` under caption `t`
	{ session_please(); MessageBox(session_hwnd,s,t,strchr(t,'?')?MB_ICONQUESTION:(strchr(t,'!')?MB_ICONEXCLAMATION:MB_OK)); }
void session_aboutme(char *s,char *t) // special case: "About.."
{
	session_please();
	MSGBOXPARAMS mbp;
	mbp.cbSize=sizeof(MSGBOXPARAMS);
	mbp.hwndOwner=session_hwnd;
	mbp.hInstance=GetModuleHandle(0);
	mbp.lpszText=s;
	mbp.lpszCaption=t;
	mbp.dwStyle=MB_OK|MB_USERICON;
	mbp.lpszIcon=MAKEINTRESOURCE(34002);
	mbp.dwContextHelpId=0;
	mbp.lpfnMsgBoxCallback=NULL;
	mbp.dwLanguageId=0;
	MessageBoxIndirect(&mbp);
}

HWND session_dialog_item;
char *session_dialog_text;
int session_dialog_return;

// input dialog ----------------------------------------------------- //

LRESULT CALLBACK inputproc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) // dialog callback function
{
	switch (msg)
	{
		case WM_INITDIALOG:
			{
				SetWindowText(hwnd,session_dialog_text);
				session_dialog_item=GetDlgItem(hwnd,12345);
				SendMessage(session_dialog_item,WM_SETTEXT,0,lparam);
				SendMessage(session_dialog_item,EM_SETLIMITTEXT,STRMAX-1,0);
			}
			break;
		/*case WM_SIZE:
			{
				RECT r; GetClientRect(hwnd,&r);
				SetWindowPos(session_dialog_item,NULL,0,0,r.right,r.bottom*1/2,SWP_NOZORDER);
				SetWindowPos(GetDlgItem(hwnd,IDOK),NULL,0,r.bottom*1/2,r.right/2,r.bottom/2,SWP_NOZORDER);
				SetWindowPos(GetDlgItem(hwnd,IDCANCEL),NULL,r.right/2,r.bottom*1/2,r.right/2,r.bottom/2,SWP_NOZORDER);
			}
			break;*/
		case WM_COMMAND:
			if (LOWORD(wparam)==IDCANCEL)
				EndDialog(hwnd,0);
			else if (!HIWORD(wparam)&&LOWORD(wparam)==IDOK)
			{
				session_dialog_return=SendMessage(session_dialog_item,WM_GETTEXT,STRMAX,(LPARAM)session_parmtr);
				EndDialog(hwnd,0);
			}
			break;
		default:
			return 0;
	}
	return 1;
}
int session_input(char *s,char *t) // `s` is the target string (empty or not), `t` is the caption; returns -1 on error or LENGTH on success
{
	session_dialog_return=-1;
	session_dialog_text=t;
	session_please();
	DialogBoxParam(GetModuleHandle(0),(LPCSTR)34004,session_hwnd,(DLGPROC)inputproc,(LPARAM)s);
	return session_dialog_return; // the string is in `session_parmtr`
}

// list dialog ------------------------------------------------------ //

LRESULT CALLBACK listproc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) // dialog callback function
{
	switch (msg)
	{
		case WM_INITDIALOG:
			{
				SetWindowText(hwnd,session_dialog_text);
				session_dialog_item=GetDlgItem(hwnd,12345);
				char *l=(char *)lparam;
				while (*l)
				{
					SendMessage(session_dialog_item,LB_ADDSTRING,0,(LPARAM)l);
					while (*l++)
						{}
				}
				SendMessage(session_dialog_item,LB_SETCURSEL,session_dialog_return,0); // select item
				session_dialog_return=-1;
			}
			break;
		/*case WM_SIZE:
			{
				RECT r; GetClientRect(hwnd,&r);
				SetWindowPos(session_dialog_item,NULL,0,0,r.right,r.bottom*15/16,SWP_NOZORDER);
				SetWindowPos(GetDlgItem(hwnd,IDOK),NULL,0,r.bottom*15/16,r.right/2,r.bottom/16,SWP_NOZORDER);
				SetWindowPos(GetDlgItem(hwnd,IDCANCEL),NULL,r.right/2,r.bottom*15/16,r.right/2,r.bottom/16,SWP_NOZORDER);
			}
			break;*/
		case WM_COMMAND:
			if (LOWORD(wparam)==IDCANCEL)
				EndDialog(hwnd,0);
			else if ((HIWORD(wparam)==LBN_DBLCLK)||(!HIWORD(wparam)&&LOWORD(wparam)==IDOK))
				if ((session_dialog_return=SendMessage(session_dialog_item,LB_GETCURSEL,0,0))>=0)
				{
					SendMessage(session_dialog_item,LB_GETTEXT,session_dialog_return,(LPARAM)session_parmtr);
					EndDialog(hwnd,0);
				}
			break;
		default:
			return 0;
	}
	return 1;
}
int session_list(int i,char *s,char *t) // `s` is a list of ASCIZ entries, `i` is the default chosen item, `t` is the caption; returns -1 on error or 0..n-1 on success
{
	if (!*s) // empty?
		return -1;
	session_dialog_return=i;
	session_dialog_text=t;
	session_please();
	DialogBoxParam(GetModuleHandle(0),(LPCSTR)34003,session_hwnd,(DLGPROC)listproc,(LPARAM)s);
	return session_dialog_return; // the string is in `session_parmtr`
}

// file dialog ------------------------------------------------------ //

OPENFILENAME session_ofn;
int session_filedialog(char *r,char *s,char *t,int q,int f) // auxiliar function, see below
{
	memset(&session_ofn,0,sizeof(session_ofn));
	session_ofn.lStructSize=sizeof(OPENFILENAME);
	session_ofn.hwndOwner=session_hwnd;
	if (r!=(char *)session_tmpstr)
		strcpy(session_tmpstr,r);
	if (r=strrchr(session_tmpstr,'\\'))
		strcpy(session_parmtr,++r),*r=0;
	else
		strcpy(session_parmtr,session_tmpstr),strcpy(session_tmpstr,".\\");
	if (!strcmp(session_parmtr,".")) // BUG: Win10 cancels showing a file selector if the source FILENAME is "." (valid path but invalid file)
		*session_parmtr=0;
	strcpy(session_substr,s);
	strcpy(&session_substr[strlen(s)+1],s);
	session_substr[strlen(s)*2+2]=session_substr[strlen(s)*2+3]=0;
	//printf("T:%s // P:%s // S:%s\n",session_tmpstr,session_parmtr,session_substr); // tmpstr: "C:\ABC"; parmtr: "XYZ.EXT"; substr: "*.EX1;*.EX2"(x2)
	session_ofn.lpstrFilter=session_substr;
	session_ofn.nFilterIndex=1;
	session_ofn.lpstrFile=session_parmtr;
	session_ofn.nMaxFile=sizeof(session_parmtr);
	session_ofn.lpstrInitialDir=session_tmpstr;
	session_ofn.lpstrTitle=t;
	session_ofn.lpstrDefExt=((t=strrchr(s,'.'))&&(*++t!='*'))?t:NULL;
	session_ofn.Flags=OFN_PATHMUSTEXIST|OFN_NONETWORKBUTTON|OFN_NOCHANGEDIR|(q?OFN_OVERWRITEPROMPT:(OFN_FILEMUSTEXIST|f));
	session_please();
	return q?GetSaveFileName(&session_ofn):GetOpenFileName(&session_ofn);
}
#define session_filedialog_readonly (session_ofn.Flags&OFN_READONLY)
#define session_filedialog_readonly0() (session_ofn.Flags&=~OFN_READONLY)
#define session_filedialog_readonly1() (session_ofn.Flags|=OFN_READONLY)
char *session_newfile(char *r,char *s,char *t) // "Create File" | ...and returns NULL on failure, or a string on success.
	{ return session_filedialog(r,s,t,1,0)?session_parmtr:NULL; }
char *session_getfile(char *r,char *s,char *t) // "Open a File" | lists files in path `r` matching pattern `s` under caption `t`, etc.
	{ return session_filedialog(r,s,t,0,OFN_HIDEREADONLY)?session_parmtr:NULL; }
char *session_getfilereadonly(char *r,char *s,char *t,int q) // "Open a File" with Read Only option | lists files in path `r` matching pattern `s` under caption `t`; `q` is the default Read Only value, etc.
	{ return session_filedialog(r,s,t,0,q?OFN_READONLY:0)?session_parmtr:NULL; }

// OS-dependant composite funcions ---------------------------------- //

// 'i' = lil-endian (Intel), 'm' = big-endian (Motorola)

//#define mgetc(x) (*(x))
//#define mputc(x,y) (*(x)=(y))
#define mgetii(x) (*(WORD*)(x))
#define mgetiiii(x) (*(DWORD*)(x))
#define mputii(x,y) ((*(WORD*)(x))=(y))
#define mputiiii(x,y) ((*(DWORD*)(x))=(y))
int  mgetmm(unsigned char *x) { return (*x<<8)+x[1]; }
int  mgetmmmm(unsigned char *x) { return (*x<<24)+(x[1]<<16)+(x[2]<<8)+x[3]; }
void mputmm(unsigned char *x,int y) { *x=y>>8; x[1]=y; }
void mputmmmm(unsigned char *x,int y) { *x=y>>24; x[1]=y>>16; x[2]=y>>8; x[3]=y; }

int fgetii(FILE *f) { int i=0; return (fread(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { return (fwrite(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fputc()
int fgetiiii(FILE *f) { int i=0; return (fread(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { return (fwrite(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fputc()
int fgetmm(FILE *f) { int i=fgetc(f)<<8; return i+fgetc(f); } // common big-endian 16-bit fgetc()
int fputmm(int i,FILE *f) { fputc(i>>8,f); return fputc(i,f); } // common big-endian 16-bit fputc()
int fgetmmmm(FILE *f) { int i=fgetc(f)<<24; i+=fgetc(f)<<16; i+=fgetc(f)<<8; return i+fgetc(f); } // common big-endian 32-bit fgetc()
int fputmmmm(int i,FILE *f) { fputc(i>>24,f); fputc(i>>16,f); fputc(i>>8,f); return fputc(i,f); } // common big-endian 32-bit fputc()

#ifdef DEBUG
#define BOOTSTRAP
#else
#ifndef __argc
	extern int __argc; extern char **__argv; // GCC5's -std=gnu99 does NOT define them by default, despite being part of STDLIB.H and MSVCRT!
#endif
#define BOOTSTRAP int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) { return main(__argc,__argv); }
#endif

#endif // =========================== END OF WINDOWS 5.0+ DEFINITIONS //
