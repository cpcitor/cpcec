 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// To compile the emulator for Windows 5.0+, the default platform, type
// "gcc cpcec.res -xc cpcec.c -luser32 -lgdi32 -lcomdlg32 -lshell32
// -lwinmm" or the equivalent options from your preferred C compiler.
// Optional DirectDraw support is enabled by appending "-DDDRAW -lddraw"

// Succesfully tested compilers: GCC 4.6.3 (-std=gnu99), 4.9.2, 5.1.0,
// 8.3.0 ; TCC 0.9.27 ; CLANG 3.7.1, 7.0.1 ; Pelles C 4.50.113 ; etc.

// START OF WINDOWS 5.0+ DEFINITIONS ================================ //

#include <windows.h> // KERNEL32.DLL, USER32.DLL, GDI32.DLL, WINMM.DLL, COMDLG32.DLL, SHELL32.DLL
#include <commdlg.h> // COMDLG32.DLL: getOpenFileName()...
#include <mmsystem.h> // WINMM.DLL: waveOutWrite()...
#include <shellapi.h> // SHELL32.DLL: DragQueryFile()...

#define STRMAX 288 // widespread in Windows
#define PATHCHAR '\\' // unlike '/' (POSIX)
#define strcasecmp _stricmp // from MSVCRT!
#define fsetsize(f,l) _chsize(_fileno(f),(l))
#include <io.h> // _chsize(),_fileno()...
// WIN32 is never UTF-8; it's either ANSI (...A) or UNICODE (...W)
#define I18N_MULTIPLY "\327"
#define I18N_DIVISION "\367"
#define I18N_L_AQUOTE "\042"//"\253"
#define I18N_R_AQUOTE "\042"//"\273"

#define MESSAGEBOX_WIDETAB "\t" // expect proportional font
#define GPL_3_LF " " // Windows provides its own line feeds

// general engine constants and variables --------------------------- //

//#define AUDIO_UNIT unsigned char // obsolete 8-bit audio
//#define AUDIO_BITDEPTH 8
//#define AUDIO_ZERO 128
#define AUDIO_UNIT signed short // standard 16-bit audio
#define AUDIO_BITDEPTH 16
#define AUDIO_ZERO 0
#define AUDIO_BYTESTEP (AUDIO_CHANNELS*AUDIO_BITDEPTH/8) // i.e. 1, 2 or 4 bytes
#define VIDEO_UNIT DWORD // the pixel style must be 0X00RRGGBB and nothing else!

BYTE audio_memory[AUDIO_BYTESTEP<<AUDIO_L2BUFFER]; // audio buffer
VIDEO_UNIT *video_frame,*video_blend; // video + blend frames, allocated on runtime
AUDIO_UNIT audio_frame[AUDIO_PLAYBACK/50*AUDIO_CHANNELS]; // audio frame; 50 is the lowest legal framerate
VIDEO_UNIT *video_target; // pointer to current video pixel
AUDIO_UNIT *audio_target; // pointer to current audio sample
unsigned char session_focused=0; // ignore joysticks when unfocused
unsigned char session_path[STRMAX],session_parmtr[STRMAX],session_tmpstr[STRMAX],session_substr[STRMAX],session_info[STRMAX]="";

RECT session_ideal; // ideal rectangle where the window fits perfectly; `right` and `bottom` are actually width and height
JOYINFOEX session_joy; // joystick+mouse buffers
HWND session_hwnd,session_hdlg=NULL; // window handle
HMENU session_menu=NULL; // menu handle
BYTE session_hidemenu=0; // normal or pop-up menu
HDC session_dc1=NULL,session_dc2=NULL; HGDIOBJ session_dib=NULL; // video structs
HWAVEOUT session_wo; WAVEHDR session_wh; MMTIME session_mmtime; // audio structs

VIDEO_UNIT *debug_frame; BYTE debug_dirty; // !0 (new redraw required) or 0 (redraw not required)
HGDIOBJ session_dbg=NULL; // the debug screen's own bitmap

#ifdef DDRAW
	#include <ddraw.h>
	LPDIRECTDRAW lpdd=NULL;
	LPDIRECTDRAWCLIPPER lpddclip=NULL;
	LPDIRECTDRAWSURFACE lpddfore=NULL,lpddback=NULL;
	DDSURFACEDESC ddsd;
	LPDIRECTDRAWSURFACE lpdd_dbg=NULL;
#endif

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

// function keys
#define	KBCODE_F1	0X3B
#define	KBCODE_F2	0X3C
#define	KBCODE_F3	0X3D
#define	KBCODE_F4	0X3E
#define	KBCODE_F5	0X3F
#define	KBCODE_F6	0X40
#define	KBCODE_F7	0X41
#define	KBCODE_F8	0X42
#define	KBCODE_F9	0X43
#define	KBCODE_F10	0X44
#define	KBCODE_F11	0X57
#define	KBCODE_F12	0X58
// leftmost keys
#define	KBCODE_ESCAPE	0X01
#define	KBCODE_TAB	0X0F
#define	KBCODE_CAPSLOCK	0X3A
#define	KBCODE_L_SHIFT	0X2A
#define	KBCODE_L_CTRL	0X1D
//#define KBCODE_L_ALT	0X38 // trapped by Win32
// alphanumeric row 1
#define	KBCODE_1	0X02
#define	KBCODE_2	0X03
#define	KBCODE_3	0X04
#define	KBCODE_4	0X05
#define	KBCODE_5	0X06
#define	KBCODE_6	0X07
#define	KBCODE_7	0X08
#define	KBCODE_8	0X09
#define	KBCODE_9	0X0A
#define	KBCODE_0	0X0B
#define	KBCODE_CHR1_1	0X0C
#define	KBCODE_CHR1_2	0X0D
#define	KBCODE_CHR4_5	0X29 // this is the key before "1" in modern keyboards
// alphanumeric row 2
#define	KBCODE_Q	0X10
#define	KBCODE_W	0X11
#define	KBCODE_E	0X12
#define	KBCODE_R	0X13
#define	KBCODE_T	0X14
#define	KBCODE_Y	0X15
#define	KBCODE_U	0X16
#define	KBCODE_I	0X17
#define	KBCODE_O	0X18
#define	KBCODE_P	0X19
#define	KBCODE_CHR2_1	0X1A
#define	KBCODE_CHR2_2	0X1B
// alphanumeric row 3
#define	KBCODE_A	0X1E
#define	KBCODE_S	0X1F
#define	KBCODE_D	0X20
#define	KBCODE_F	0X21
#define	KBCODE_G	0X22
#define	KBCODE_H	0X23
#define	KBCODE_J	0X24
#define	KBCODE_K	0X25
#define	KBCODE_L	0X26
#define	KBCODE_CHR3_1	0X27
#define	KBCODE_CHR3_2	0X28
#define	KBCODE_CHR3_3	0X2B
// alphanumeric row 4
#define	KBCODE_Z	0X2C
#define	KBCODE_X	0X2D
#define	KBCODE_C	0X2E
#define	KBCODE_V	0X2F
#define	KBCODE_B	0X30
#define	KBCODE_N	0X31
#define	KBCODE_M	0X32
#define	KBCODE_CHR4_1	0X33
#define	KBCODE_CHR4_2	0X34
#define	KBCODE_CHR4_3	0X35
#define	KBCODE_CHR4_4	0X56 // this is the key before "Z" in 105-key modern keyboards; missing in 104-key layouts!
// rightmost keys
#define	KBCODE_SPACE	0X39
#define	KBCODE_BKSPACE	0X0E
#define	KBCODE_ENTER	0X1C
#define	KBCODE_R_SHIFT	0X36
#define	KBCODE_R_CTRL	0X9D
//#define KBCODE_R_ALT	0XB8 // trapped by Win32
// extended keys
//#define KBCODE_PRINT	0X54 // trapped by Win32
#define	KBCODE_SCR_LOCK	0X46
#define	KBCODE_HOLD	0X45
#define	KBCODE_INSERT	0XD2
#define	KBCODE_DELETE	0XD3
#define	KBCODE_HOME	0XC7
#define	KBCODE_END	0XCF
#define	KBCODE_PRIOR	0XC9
#define	KBCODE_NEXT	0XD1
#define	KBCODE_UP	0XC8
#define	KBCODE_DOWN	0XD0
#define	KBCODE_LEFT	0XCB
#define	KBCODE_RIGHT	0XCD
#define KBCODE_APPS	0XDD // 0X5D in docs!?
// numeric keypad
#define	KBCODE_NUM_LOCK	0XC5
#define	KBCODE_X_7	0X47
#define	KBCODE_X_8	0X48
#define	KBCODE_X_9	0X49
#define	KBCODE_X_4	0X4B
#define	KBCODE_X_5	0X4C
#define	KBCODE_X_6	0X4D
#define	KBCODE_X_1	0X4F
#define	KBCODE_X_2	0X50
#define	KBCODE_X_3	0X51
#define	KBCODE_X_0	0X52
#define	KBCODE_X_DOT	0X53
#define	KBCODE_X_ENTER	0X9C
#define	KBCODE_X_ADD	0X4E
#define	KBCODE_X_SUB	0X4A
#define	KBCODE_X_MUL	0X37
#define	KBCODE_X_DIV	0XB5
BYTE native_usbkey[]={ // WIN32-USB keyboard translation table; CONTROL, SHIFT, ALT and other modifiers are excluded
	0,		0,		0,		0,
	KBCODE_A,	KBCODE_B,	KBCODE_C,	KBCODE_D,
	KBCODE_E,	KBCODE_F,	KBCODE_G,	KBCODE_H,
	KBCODE_I,	KBCODE_J,	KBCODE_K,	KBCODE_L,
	KBCODE_M,	KBCODE_N,	KBCODE_O,	KBCODE_P,
	KBCODE_Q,	KBCODE_R,	KBCODE_S,	KBCODE_T,
	KBCODE_U,	KBCODE_V,	KBCODE_W,	KBCODE_X,
	KBCODE_Y,	KBCODE_Z,	KBCODE_1,	KBCODE_2,
	KBCODE_3,	KBCODE_4,	KBCODE_5,	KBCODE_6,
	KBCODE_7,	KBCODE_8,	KBCODE_9,	KBCODE_0,
	KBCODE_ENTER,	KBCODE_ESCAPE,	KBCODE_BKSPACE,	KBCODE_TAB,
	KBCODE_SPACE,	KBCODE_CHR1_1,	KBCODE_CHR1_2,	KBCODE_CHR2_1,
	KBCODE_CHR2_2,	KBCODE_CHR3_3,	0,		KBCODE_CHR3_1,
	KBCODE_CHR3_2,	KBCODE_CHR4_4,	KBCODE_CHR4_1,	KBCODE_CHR4_2,
	KBCODE_CHR4_3,	KBCODE_CAPSLOCK,KBCODE_F1,	KBCODE_F2,
	KBCODE_F3,	KBCODE_F4,	KBCODE_F5,	KBCODE_F6,
	KBCODE_F7,	KBCODE_F8,	KBCODE_F9,	KBCODE_F10,
	KBCODE_F11,	KBCODE_F12,	0,		KBCODE_SCR_LOCK,
	KBCODE_HOLD,	KBCODE_INSERT,	KBCODE_HOME,	KBCODE_PRIOR,
	KBCODE_DELETE,	KBCODE_END,	KBCODE_NEXT,	KBCODE_RIGHT,
	KBCODE_LEFT,	KBCODE_DOWN,	KBCODE_UP,	KBCODE_NUM_LOCK,
	KBCODE_X_DIV,	KBCODE_X_MUL,	KBCODE_X_SUB,	KBCODE_X_ADD,
	KBCODE_X_ENTER,	KBCODE_X_1,	KBCODE_X_2,	KBCODE_X_3,
	KBCODE_X_4,	KBCODE_X_5,	KBCODE_X_6,	KBCODE_X_7,
	KBCODE_X_8,	KBCODE_X_9,	KBCODE_X_0,	KBCODE_X_DOT,
	KBCODE_CHR4_5,	KBCODE_APPS};//=101
void usbkey2native(BYTE *t,const BYTE *s,int n) { while (n) { int k=*s++; *t++=k<sizeof(native_usbkey)?native_usbkey[k]:0; --n; } }
void native2usbkey(BYTE *t,const BYTE *s,int n) { while (n) { int k=0; while (k<sizeof(native_usbkey)&&*s!=native_usbkey[k]) ++k; *t++=k<sizeof(native_usbkey)?k:0; s++; --n; } }
BYTE kbd_k2j[]= // these keys can simulate a 4-button joystick
	{ KBCODE_UP, KBCODE_DOWN, KBCODE_LEFT, KBCODE_RIGHT, KBCODE_Z, KBCODE_X, KBCODE_C, KBCODE_V };

unsigned char kbd_map[256]; // key-to-key translation map

// general engine functions and procedures -------------------------- //

void session_please(void) // stop activity for a short while
{
	if (!session_wait) //video_framecount=1;
		if (session_wait=1,session_audio) waveOutPause(session_wo);
}

void session_kbdclear(void)
{
	MEMZERO(kbd_bit); MEMZERO(joy_bit);
}
#define session_kbdreset() MEMBYTE(kbd_map,~~~0) // init and clean key map up
void session_kbdsetup(const unsigned char *s,char l) // maps a series of virtual keys to the real ones
{
	session_kbdclear();
	while (l--)
	{
		int k=*s++;
		kbd_map[k]=*s++;
	}
}
int session_key_n_joy(int k) // handle some keys as joystick motions
{
	if (session_key2joy)
		for (int i=0;i<KBD_JOY_UNIQUE;++i)
			if (kbd_k2j[i]==k)
				return kbd_joy[i];
	return kbd_map[k];
}

#define session_clrscr() InvalidateRect(session_hwnd,NULL,1)
int session_r_x,session_r_y,session_r_w,session_r_h; // actual location and size of the bitmap
void session_desktop(RECT *r) { SystemParametersInfo(SPI_GETWORKAREA,0,r,0); r->right-=r->left,r->bottom-=r->top; } // i.e. width and height
int session_resize(void) // dunno why, but one 100% render must happen before the resizing; performance falls otherwise :-/
{
	static char z=1; if (session_fullblit)
	{
		if (z)
		{
			SetWindowLong(session_hwnd,GWL_STYLE,(GetWindowLong(session_hwnd,GWL_STYLE)&~WS_CAPTION));
				//|WS_POPUP|WS_CLIPCHILDREN // hide caption and buttons
			/*if (!session_hidemenu)*/ SetMenu(session_hwnd,NULL); // hide menu
			ShowWindow(session_hwnd,SW_MAXIMIZE); // adjust to entire screen
			z=0; session_clrscr(),session_dirty=1; // clean up and update options
		}
	}
	else if (z!=session_zoomblit+1)
	{
		if (!z)
		{
			SetWindowLong(session_hwnd,GWL_STYLE,(GetWindowLong(session_hwnd,GWL_STYLE)|WS_CAPTION));
				//&~WS_POPUP&~WS_CLIPCHILDREN // show caption and buttons
			if (!session_hidemenu) SetMenu(session_hwnd,session_menu); // show menu
		}
		RECT r; session_desktop(&r); ShowWindow(session_hwnd,SW_RESTORE); // pseudo CW_CENTERED logic here :-(
		int x,y; while ((x=(VIDEO_PIXELS_X>>1)*(session_zoomblit+2),y=(VIDEO_PIXELS_Y>>1)*(session_zoomblit+2)),
			session_zoomblit>0&&(x*16>r.right*17||y*16>r.bottom*17)) // shrink if too big!
			--session_zoomblit;
		r.left+=(r.right-(x+=session_ideal.right-VIDEO_PIXELS_X))>>1;
		r.top+=(r.bottom-(y+=session_ideal.bottom-VIDEO_PIXELS_Y))>>1;
		MoveWindow(session_hwnd,r.left,r.top,x,y,1); // resize AND center!
		z=session_zoomblit+1; session_clrscr(),session_dirty=1; // clean up and update options
	}
	return z;
}
void session_redraw(HWND hwnd,HDC h) // redraw the window contents
{
	RECT r; GetClientRect(hwnd,&r); // calculate window area
	if ((session_r_w=(r.right-=r.left))>0&&(session_r_h=(r.bottom-=r.top))>0) // divisions by zero happen on WM_PAINT during window resizing!
	{
		int ox,oy; if (session_signal&SESSION_SIGNAL_DEBUG)
			ox=0,oy=0;
		else
			ox=VIDEO_OFFSET_X,oy=VIDEO_OFFSET_Y;
		if (session_r_w>session_r_h*VIDEO_PIXELS_X/VIDEO_PIXELS_Y) // window area is too wide?
			session_r_w=session_r_h*VIDEO_PIXELS_X/VIDEO_PIXELS_Y;
		if (session_r_h>session_r_w*VIDEO_PIXELS_Y/VIDEO_PIXELS_X) // window area is too tall?
			session_r_h=session_r_w*VIDEO_PIXELS_Y/VIDEO_PIXELS_X;
		//if (session_fullblit) // maximum integer zoom: 100%, 150%, 200%...
			session_r_w=((session_r_w*17)/VIDEO_PIXELS_X/8)*VIDEO_PIXELS_X/2, // "*17../16../1"
			session_r_h=((session_r_h*17)/VIDEO_PIXELS_Y/8)*VIDEO_PIXELS_Y/2; // forbids N+50%
		if (session_r_w<VIDEO_PIXELS_X||session_r_h<VIDEO_PIXELS_Y)
			session_r_w=VIDEO_PIXELS_X,session_r_h=VIDEO_PIXELS_Y; // window area is too small!
		session_r_x=(r.right-session_r_w)>>1,session_r_y=(r.bottom-session_r_h)>>1; // locate bitmap on window center
		#ifdef DDRAW
		if (lpddback)
		{
			LPDIRECTDRAWSURFACE l=(session_signal&SESSION_SIGNAL_DEBUG)?lpdd_dbg:lpddback;
			IDirectDrawSurface_Unlock(l,0);
			//int q=1; // don't redraw if something went wrong
			if (IDirectDrawSurface_IsLost(lpddfore))
				/*q=0,*/IDirectDrawSurface_Restore(lpddfore);
			else if (IDirectDrawSurface_IsLost(lpddback))
				/*q=0,*/IDirectDrawSurface_Restore(lpddback);
			else if (IDirectDrawSurface_IsLost(lpdd_dbg))
				/*q=0,*/IDirectDrawSurface_Restore(lpdd_dbg);
			else //if (q) // not sure if we can redraw even when !q ...
			{
				POINT p; p.x=p.y=0; // some C compilers dislike `POINT p={.x=0,.y=0}`... see also SDL_Rect in CPCEC-OX.H :-(
				if (ClientToScreen(hwnd,&p)) // can this ever fail!?
				{
					RECT rr;
					rr.right=(rr.left=ox)+VIDEO_PIXELS_X;
					rr.bottom=(rr.top=oy)+VIDEO_PIXELS_Y;
					r.right=(r.left=p.x+session_r_x)+session_r_w;
					r.bottom=(r.top=p.y+session_r_y)+session_r_h;
					IDirectDrawSurface_Blt(lpddfore,&r,l,&rr,DDBLT_WAIT,0);
				}
			}
			ddsd.dwSize=sizeof(ddsd); IDirectDrawSurface_Lock(l,0,&ddsd,DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT,0);
			if (session_signal&SESSION_SIGNAL_DEBUG)
				debug_frame=ddsd.lpSurface;
			else
			{
				ox=video_target-video_frame; // the video target pointer...
				video_target=(video_frame=ddsd.lpSurface)+ox; // ...must follow the frame!
			}
		}
		else
		#endif
		{
			HGDIOBJ session_oldselect; if (session_oldselect=SelectObject(session_dc2,session_signal&SESSION_SIGNAL_DEBUG?session_dbg:session_dib))
			{
				if (session_hardblit=(session_r_w<=VIDEO_PIXELS_X||session_r_h<=VIDEO_PIXELS_Y)) // window area is a perfect fit?
					BitBlt(h,session_r_x,session_r_y,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,session_dc2,ox,oy,SRCCOPY); // fast :-)
				else
					StretchBlt(h,session_r_x,session_r_y,session_r_w,session_r_h,session_dc2,ox,oy,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,SRCCOPY); // slow :-(
				SelectObject(session_dc2,session_oldselect);
			}
		}
	}
}
int session_contextmenu(void) // used only when the normal menu is disabled
{
	POINT p; return (session_hidemenu&&session_menu)?GetCursorPos(&p),TrackPopupMenu(session_menu,0,p.x,p.y,0,session_hwnd,NULL):0;
}
LRESULT CALLBACK mainproc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) // window callback function
{
	int k; switch (msg)
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
			session_clrscr(); // force full update! there's dirt otherwise!
			break;
		case WM_SETFOCUS:
			session_focused=1;
			break;
		case WM_PAINT:
			{
				PAINTSTRUCT ps; HDC h;
				if (h=BeginPaint(hwnd,&ps))
					session_redraw(hwnd,h),EndPaint(hwnd,&ps);
			}
			break;
		case WM_COMMAND:
			if (0X0080==(WORD)wparam) // Exit
				PostMessage(hwnd,WM_CLOSE,0,0);
			else
			{
				session_shift=!!(wparam&0X4000); // bit 6 means SHIFT KEY ON
				session_event=wparam&0XBFFF; // cfr infra: bit 7 means CONTROL KEY OFF
			}
			break;
		case WM_MBUTTONDBLCLK+1: // workaround: WINUSER.H sometimes lacks WM_MOUSEWHEEL
			if ((short)HIWORD(wparam)<0) session_event=debug_xlat(KBCODE_NEXT);
			else if ((short)HIWORD(wparam)) session_event=debug_xlat(KBCODE_PRIOR);
			break;
		case WM_LBUTTONUP:
			if (session_signal&SESSION_SIGNAL_DEBUG)
				session_event=debug_xlat(-1);
		case WM_LBUTTONDOWN:
		case WM_MOUSEMOVE: // no `break`!
			#ifdef MAUS_EMULATION
			session_maus_z=wparam&MK_LBUTTON;
			#endif
			session_maus_y=session_r_h>0?((HIWORD(lparam)-session_r_y)*VIDEO_PIXELS_Y+session_r_h/2)/session_r_h:-1;
			session_maus_x=session_r_w>0?((LOWORD(lparam)-session_r_x)*VIDEO_PIXELS_X+session_r_w/2)/session_r_w:-1;
			break;
		case WM_RBUTTONUP:
			session_contextmenu();
			break;
		case WM_KEYDOWN:
			session_shift=GetKeyState(VK_SHIFT)<0;
			if (session_signal&SESSION_SIGNAL_DEBUG) // only relevant inside debugger, see below
				session_event=debug_xlat(((HIWORD(lparam))&127)+(((HIWORD(lparam))>>1)&128));
			if ((k=session_key_n_joy(((HIWORD(lparam))&127)+(((HIWORD(lparam))>>1)&128)))<128) // normal key
			{
				if (!(session_signal&SESSION_SIGNAL_DEBUG)) // only relevant outside debugger
					kbd_bit_set(k);
			}
			else if (!session_event) // special key, but only if not already set by debugger
				session_event=(k-(GetKeyState(VK_CONTROL)<0?128:0))<<8;
			break;
		case WM_CHAR: // always follows WM_KEYDOWN
			if (session_signal&SESSION_SIGNAL_DEBUG) // only relevant inside debugger
				session_event=wparam>32&&wparam<=255?wparam:0; // exclude SPACE and non-visible codes!
			break;
		case WM_KEYUP:
			session_shift=GetKeyState(VK_SHIFT)<0;
			if ((k=session_key_n_joy(((HIWORD(lparam))&127)+(((HIWORD(lparam))>>1)&128)))<128) // normal key
				kbd_bit_res(k);
			break;
		case WM_DROPFILES:
			session_shift=GetKeyState(VK_SHIFT)<0,session_event=0X8000;
			DragQueryFile((HDROP)wparam,0,(LPTSTR)session_parmtr,STRMAX);
			DragFinish((HDROP)wparam);
			break;
		//case WM_ENTERIDLE: // pause before showing dialogboxes or menus
		case WM_ENTERSIZEMOVE: // pause before moving the window
		case WM_ENTERMENULOOP: // pause before showing the menus
			session_please();
		case WM_KILLFOCUS: // no `break`!
			session_kbdclear(); // loss of focus: no keys!
		default: // no `break`!
			if (msg==WM_SYSKEYDOWN)
				switch (wparam)
				{
					case VK_F10: // F10 shows the popup menu
						if (session_contextmenu()) return 0; break; // skip OS if the popup menu is allowed
					case VK_RETURN: // ALT+RETURN toggles fullscreen
						return session_fullblit=!session_fullblit,session_resize(),0; // skip OS
					case VK_UP: //case VK_ADD: case 0XBB: // ALT+UP raises the zoom
						return (!session_fullblit&&session_zoomblit<4&&(++session_zoomblit,session_resize())),0;
					case VK_DOWN: //case VK_SUBTRACT: case 0XBD: // ALT+DOWN lowers it
						return (!session_fullblit&&session_zoomblit>0&&(--session_zoomblit,session_resize())),0;
				}
			else if (msg==WM_KILLFOCUS)
				session_focused=0;
			return DefWindowProc(hwnd,msg,wparam,lparam);
	}
	return 0;
}

INLINE char *session_create(char *s) // create video+audio devices and set menu; 0 OK, !0 ERROR
{
	OSVERSIONINFO win32_version; HMENU session_submenu=NULL;
	win32_version.dwOSVersionInfoSize=sizeof(win32_version); GetVersionEx(&win32_version);
	sprintf(session_version,"%lu.%lu",win32_version.dwMajorVersion,win32_version.dwMinorVersion);
	char c,*t; int i,j;
	/*if (!session_softblit)*/ while (c=*s++)
	{
		if (c=='=') // separator?
		{
			while (*s++!='\n') {} // ignore remainder
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
				session_menu=session_hidemenu?CreatePopupMenu():CreateMenu();
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
	WNDCLASS wc; memset(&wc,0,sizeof(wc));
	wc.lpfnWndProc=mainproc;
	wc.hInstance=GetModuleHandle(0);
	wc.hIcon=LoadIcon(wc.hInstance,MAKEINTRESOURCE(34002)); // value from RC file
	wc.hCursor=LoadCursor(NULL,IDC_ARROW); // cannot be zero!!
	wc.hbrBackground=(HBRUSH)(1+COLOR_WINDOWTEXT);//(COLOR_WINDOW+2);//0;//
	//wc.lpszMenuName=NULL;
	wc.lpszClassName=MY_CAPTION;
	RegisterClass(&wc);
	session_ideal.left=session_ideal.top=0;
	session_ideal.right=VIDEO_PIXELS_X;
	session_ideal.bottom=VIDEO_PIXELS_Y;
	if (!session_submenu)
		session_hidemenu=1,session_menu=NULL;
	AdjustWindowRect(&session_ideal,i=WS_OVERLAPPEDWINDOW,!session_hidemenu); // calculate ideal size
	session_ideal.right-=session_ideal.left;
	session_ideal.bottom-=session_ideal.top;
	//session_ideal.left=session_ideal.top=0; // ensure that the ideal area is defined as (0,0,WIDTH,HEIGHT)
	RECT r; session_desktop(&r); // unlike SDL2, we lack a true CW_CENTERED option and we must calculate everything :-(
	if (!(session_hwnd=CreateWindow(wc.lpszClassName,NULL,i,(r.right-session_ideal.right)>>1,(r.bottom-session_ideal.bottom)>>1,
		session_ideal.right,session_ideal.bottom,NULL,session_hidemenu?NULL:session_menu,wc.hInstance,NULL))||
		#ifdef VIDEO_HI_Y_RES
		!(video_blend=malloc(sizeof(VIDEO_UNIT[VIDEO_PIXELS_Y*VIDEO_PIXELS_X])))
		#else
		!(video_blend=malloc(sizeof(VIDEO_UNIT[VIDEO_PIXELS_Y/2*VIDEO_PIXELS_X])))
		#endif
		) return "cannot create window"; // the OS will do DestroyWindow(session_hwnd) ...will it? :-/
	DragAcceptFiles(session_hwnd,1);

	#ifdef DDRAW
	if (!session_softblit) // hardware-able DirectDraw
	{
		session_softblit=1; // fallback!
		if (DirectDrawCreate(NULL,&lpdd,NULL)>=0)
		{
			IDirectDraw_SetCooperativeLevel(lpdd,session_hwnd,DDSCL_NORMAL);
			//memset(&ddsd,0,sizeof(ddsd));//ZeroMemory(&ddsd,sizeof(ddsd));
			ddsd.dwSize=sizeof(ddsd); ddsd.dwFlags=DDSD_CAPS;
			ddsd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
			IDirectDraw_CreateSurface(lpdd,&ddsd,&lpddfore,NULL);
			DDPIXELFORMAT ddpf; ddpf.dwSize=sizeof(ddpf);
			IDirectDrawSurface_GetPixelFormat(lpddfore,&ddpf);
			if (ddpf.dwRGBBitCount!=32) // lazy check, translating ARGB8888 to other bitdepths is a task better left to GDI
				;//IDirectDrawSurface_Release(lpddback),lpddback=NULL; // failure
			else
			{
				IDirectDraw_CreateClipper(lpdd,0,&lpddclip,NULL);
				IDirectDrawClipper_SetHWnd(lpddclip,0,session_hwnd);
				IDirectDrawSurface_SetClipper(lpddfore,lpddclip);
				ddsd.dwSize=sizeof(ddsd); ddsd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
				ddsd.dwWidth=VIDEO_LENGTH_X; ddsd.dwHeight=VIDEO_LENGTH_Y;
				ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;//DDSCAPS_VIDEOMEMORY;//
				IDirectDraw_CreateSurface(lpdd,&ddsd,&lpddback,NULL);
				IDirectDrawSurface_Lock(lpddback,0,&ddsd,DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT,0),video_frame=ddsd.lpSurface;
				session_softblit=ddsd.lPitch!=4*VIDEO_LENGTH_X; // success (1/2)
				ddsd.dwSize=sizeof(ddsd); ddsd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
				ddsd.dwWidth=VIDEO_PIXELS_X; ddsd.dwHeight=VIDEO_PIXELS_Y;
				ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;//DDSCAPS_VIDEOMEMORY;//
				IDirectDraw_CreateSurface(lpdd,&ddsd,&lpdd_dbg,NULL);
				IDirectDrawSurface_Lock(lpdd_dbg,0,&ddsd,DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT,0),debug_frame=ddsd.lpSurface;
				session_softblit|=ddsd.lPitch!=4*VIDEO_PIXELS_X; // success (2/2)
			}
		}
	}
	if (session_softblit) // software-only GDI bitmap
	#endif
	{
		BITMAPINFO bmi; memset(&bmi,0,sizeof(bmi));
		bmi.bmiHeader.biSize=sizeof(bmi.bmiHeader); // not `sizeof(bmi)`!
		bmi.bmiHeader.biWidth=VIDEO_LENGTH_X;
		bmi.bmiHeader.biHeight=-VIDEO_LENGTH_Y; // negative values make a top-to-bottom bitmap; Windows' default bitmap is bottom-to-top
		bmi.bmiHeader.biPlanes=1; bmi.bmiHeader.biBitCount=32; // cfr. VIDEO_UNIT
		bmi.bmiHeader.biCompression=BI_RGB;
		session_dc1=GetDC(session_hwnd); // beware, we're assuming that if CreateWindow() succeeds all other USER and GDI calls will succeed too
		session_dc2=CreateCompatibleDC(session_dc1);
		session_dib=CreateDIBSection(session_dc1,&bmi,DIB_RGB_COLORS,(void**)&video_frame,NULL,0);
		bmi.bmiHeader.biWidth=VIDEO_PIXELS_X;
		bmi.bmiHeader.biHeight=-VIDEO_PIXELS_Y;
		session_dbg=CreateDIBSection(session_dc1,&bmi,DIB_RGB_COLORS,(void**)&debug_frame,NULL,0);
	}

	// cleanup, joystick and sound
	ShowWindow(session_hwnd,SW_SHOWDEFAULT); //UpdateWindow(session_hwnd);
	session_joy.dwSize=sizeof(session_joy);
	session_joy.dwFlags=JOY_RETURNALL;
	if (session_stick)
	{
		JOYCAPS jc; i=joyGetNumDevs(),j=0;
		cprintf("Detected %d joystick[s]: ",i);
		while (j<i&&(joyGetDevCaps(j,&jc,sizeof(jc))||(cprintf("Joystick/controller #%d '%s' ",j,jc.szPname),joyGetPosEx(j,&session_joy))))
			++j; // scan joysticks until we run out or one is OK
		session_stick=(j<i)?j+1:0; cprintf(session_stick?"Joystick enabled!\n":"No joystick!\n"); // ID+1 if available, 0 if missing
	}
	//session_timer=GetTickCount(); // millisecond timer by default
	session_wo=0; // no audio unless device is detected and enabled
	if (session_audio)
	{
		memset(&session_mmtime,0,sizeof(session_mmtime));
		session_mmtime.wType=TIME_SAMPLES; // Windows doesn't always provide TIME_MS!
		WAVEFORMATEX wfex;
		memset(&wfex,0,sizeof(wfex)); wfex.wFormatTag=WAVE_FORMAT_PCM;
		wfex.nBlockAlign=(wfex.wBitsPerSample=AUDIO_BITDEPTH)/8*(wfex.nChannels=AUDIO_CHANNELS);
		wfex.nAvgBytesPerSec=wfex.nBlockAlign*(wfex.nSamplesPerSec=AUDIO_PLAYBACK);
		if (session_audio=!waveOutOpen(&session_wo,WAVE_MAPPER,&wfex,0,0,0))
		{
			memset(&session_wh,0,sizeof(session_wh));
			MEMBYTE(audio_memory,AUDIO_ZERO);
			session_wh.lpData=(BYTE*)audio_memory;
			session_wh.dwBufferLength=wfex.nBlockAlign<<AUDIO_L2BUFFER;
			session_wh.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP; // circular buffer
			session_wh.dwLoops=-1; // loop forever!
			waveOutPrepareHeader(session_wo,&session_wh,sizeof(session_wh));
			waveOutWrite(session_wo,&session_wh,sizeof(session_wh)); // should be zero!
		}
	}
	session_preset(); session_clean(); session_please();
	session_redraw(session_hwnd,session_dc1); // dummy first redraw, session_resize() hinges on it
	timeBeginPeriod(8); // WIN10 sets this value too high by default; 8 is recommended in multiple sources
	return NULL;
}
INLINE int session_listen(void) // handle all pending messages; 0 OK, !0 EXIT
{
	static int s=-1; if (s!=session_signal) // catch DEBUG and PAUSE
		s=session_signal,session_dirty=debug_dirty=2;
	if (session_signal&(SESSION_SIGNAL_DEBUG|SESSION_SIGNAL_PAUSE))
	{
		session_signal_frames&=~(SESSION_SIGNAL_DEBUG|SESSION_SIGNAL_PAUSE);
		session_signal_scanlines&=~(SESSION_SIGNAL_DEBUG|SESSION_SIGNAL_PAUSE); // reset traps!
		if (session_signal&SESSION_SIGNAL_DEBUG)
			if (debug_dirty)
			{
				session_please(),session_debug_show(debug_dirty!=1),
				session_redraw(session_hwnd,session_dc1),debug_dirty=0;
			}
		if (!session_paused) // set the caption just once
		{
			sprintf(session_tmpstr,"%s | %s | PAUSED",session_caption,session_info);
			SetWindowText(session_hwnd,session_tmpstr);
			session_please(),session_paused=1;
		}
		WaitMessage();
	}
	for (MSG msg;PeekMessage(&msg,NULL,0,0,PM_REMOVE);) //if (!session_hdlg||!IsDialogMessage(session_hdlg,&msg))
	{
		TranslateMessage(&msg);
		if (msg.message==WM_QUIT)
			return 1;
		DispatchMessage(&msg);
		if (session_event)
		{
			if (!((session_signal&SESSION_SIGNAL_DEBUG)&&session_debug_user(session_event)))
				session_dirty=1,session_user(session_event);
			session_event=0; //if (session_paused) ...?
		}
	}
	if (session_dirty) session_dirty=0,session_clean();
	return 0;
}

INLINE void session_render(void) // update video, audio and timers
{
	int i,j; static int performance_t=0,performance_f=0,performance_b=0; ++performance_f;
	static BYTE r=0,q=0; if (++r>session_rhythm||session_wait) r=0; // force update after wait
	if (!video_framecount) // do we need to hurry up?
	{
		if (++performance_b,((video_interlaces=!video_interlaces)||!video_interlaced))
			if (q) session_redraw(session_hwnd,session_dc1),q=0; // redraw once between two pauses
		if (session_stick&&!session_key2joy) // do we need to check the joystick?
		{
			session_joy.dwSize=sizeof(session_joy);
			session_joy.dwFlags=JOY_RETURNBUTTONS|JOY_RETURNPOVCTS|JOY_RETURNX|JOY_RETURNY|JOY_RETURNZ|JOY_RETURNR|JOY_RETURNCENTERED;
			if (session_focused&&!joyGetPosEx(session_stick-1,&session_joy)) // without focus, ignore the joystick
			{
				j=((/*session_joy.dwPOV<0||*/session_joy.dwPOV>=36000)?(session_joy.dwYpos<0X4000?1:0)+(session_joy.dwYpos>=0XC000?2:0)+(session_joy.dwXpos<0X4000?4:0)+(session_joy.dwXpos>=0XC000?8:0) // axial
				:(session_joy.dwPOV< 2250?1:session_joy.dwPOV< 6750?9:session_joy.dwPOV<11250?8:session_joy.dwPOV<15750?10: // angular: U (0), U-R (4500), R (9000), R-D (13500)
				session_joy.dwPOV<20250?2:session_joy.dwPOV<24750?6:session_joy.dwPOV<29250?4:session_joy.dwPOV<33750?5:1)) // D (18000), D-L (22500), L (27000), L-U (31500)
				+((session_joy.dwButtons&(JOY_BUTTON1/*|JOY_BUTTON5*/))?16:0)+((session_joy.dwButtons&(JOY_BUTTON2/*|JOY_BUTTON6*/))?32:0) // FIRE1, FIRE2 ...
				+((session_joy.dwButtons&(JOY_BUTTON3/*|JOY_BUTTON7*/))?64:0)+((session_joy.dwButtons&(JOY_BUTTON4/*|JOY_BUTTON8*/))?128:0) // FIRE3, FIRE4 ...
				/*+((session_joy.dwZpos<0X4000?4:0)+(session_joy.dwZpos>0XC000?8:0))
				+((session_joy.dwRpos<0X4000?1:0)+(session_joy.dwRpos>0XC000?2:0))*/ // this is safe on my controller (Axis Z + Rotation Z) but perhaps not on other controllers
				;
			}
			else
				j=0; // joystick failure, release its keys
			MEMZERO(joy_bit); /*if (j)*/ for (i=0;i<length(kbd_joy);++i)
				if (j&(1<<i)) // joystick bit is set?
					{ int k=kbd_joy[i]; joy_bit[k>>3]|=1<<(k&7); }
		}
	}
	if (audio_required&&audio_filter) audio_playframe(); // audio filter: sample averaging
	session_writewave(); session_writefilm(); // record wave+film frame

	if (!r) // check timers and pauses
	{
		if (session_audio) // rely on audio clock; unlike SDL2, the Win32 audio clock is reliable :-)
		{
			static BYTE t=1; if (t!=(session_fast|audio_disabled)) // sound needs higher priority, but only on realtime
			{
				//BELOW_NORMAL_PRIORITY_CLASS was overkill and caused trouble on busy systems :-(
				SetPriorityClass(GetCurrentProcess(),(t=session_fast|audio_disabled)?NORMAL_PRIORITY_CLASS:ABOVE_NORMAL_PRIORITY_CLASS);
				//SetThreadPriority(GetCurrentThread(),(t=session_fast|audio_disabled)?THREAD_PRIORITY_NORMAL:THREAD_PRIORITY_ABOVE_NORMAL);
			}
			waveOutGetPosition(session_wo,&session_mmtime,sizeof(session_mmtime));
			//if (!=MMSYSERR_NOERROR) session_audio=0,audio_disabled=-1; // audio device is lost! // can this really happen!?
			i=session_mmtime.u.sample,j=AUDIO_PLAYBACK; // questionable -- this will break the timing every 13 hours of emulation at 44100 Hz :-(
		}
		else // use internal tick count as clock
			i=GetTickCount(),j=1000; // questionable for similar reasons, albeit every 23 days :-(
		if (i-performance_t>=0) // update performance percentage?
		{
			sprintf(session_tmpstr,"%s | %s | %s %s %d:%d%%",
				session_caption,session_info,
			#ifdef DDRAW
				lpddback?"DDRAW":
			#endif
				session_hardblit?"GDI":"gdi",session_version,
				(performance_b*100+VIDEO_PLAYBACK/2)/VIDEO_PLAYBACK,(performance_f*100+VIDEO_PLAYBACK/2)/VIDEO_PLAYBACK);
			SetWindowText(session_hwnd,session_tmpstr);
			performance_t=i+j,performance_f=performance_b=session_paused=0;
		}
		q=1; static BYTE p=0; if (session_wait|session_fast)
		{
			if (audio_fastmute) audio_disabled|=+8;
			p=1,session_timer=i; // ensure that the next frame can be valid!
		}
		else
		{
			audio_disabled&=~8;
			if (p) // recalc audio buffer?
				p=0,audio_session=(((4-session_softplay)<<(AUDIO_L2BUFFER-(2)-1))+session_timer)*AUDIO_BYTESTEP;
			int s; if (VIDEO_PLAYBACK==50) // always true on pure PAL systems
				s=j/50; // PAL never needs any adjusting (`j` is 1000, 22050, 24000, 44100 or 48000)
			else // (we avoid a warning here) always true on pure NTSC systems
				{ static int s0=0; s=(s0+=j)/VIDEO_PLAYBACK; s0%=VIDEO_PLAYBACK; } // 60 Hz: [16,17,17]
			if ((i=(session_timer+=s)-i)>=0)
				{ if (i=(i>s?s:i)*1000/j) Sleep(i); } // avoid zero and overflows!
			else if (i<-s&&!session_filmfile)//&&!video_framecount) // *!* threshold?
				video_framecount=video_framelimit+2; // skip frame on timeout!
		}
	}
	audio_session=(audio_session+AUDIO_LENGTH_Z*AUDIO_BYTESTEP)&((AUDIO_BYTESTEP<<AUDIO_L2BUFFER)-AUDIO_BYTESTEP); // add block size and wrap around buffer
	if (session_audio) // manage audio buffer
	{
		static BYTE s=1; if (s!=audio_disabled)
			if (s=audio_disabled) // silent mode needs cleanup
				MEMBYTE(audio_memory,AUDIO_ZERO);
		if (!audio_disabled)
		{
			int t=AUDIO_LENGTH_Z*AUDIO_BYTESTEP,u;
			if ((u=(AUDIO_BYTESTEP<<AUDIO_L2BUFFER)-audio_session)<t) // wrap around?
				memcpy(&audio_memory[audio_session],audio_frame,u),
				memcpy( audio_memory,&((BYTE*)audio_frame)[u],t-u);
			else
				memcpy(&audio_memory[audio_session],audio_frame,t);
		}
	}
	if (session_wait) // resume activity after a pause
		if (session_wait=0,session_audio) waveOutRestart(session_wo);
}

INLINE void session_byebye(void) // delete video+audio devices
{
	session_wrapup();
	if (session_wo)
		waveOutReset(session_wo),waveOutUnprepareHeader(session_wo,&session_wh,sizeof(session_wh)),waveOutClose(session_wo);
	if (session_menu)
		DestroyMenu(session_menu);

	#ifdef DDRAW
	if (lpddfore) IDirectDrawSurface_SetClipper(lpddfore,NULL),IDirectDrawSurface_Release(lpddfore);
	if (lpddback) IDirectDrawSurface_Unlock(lpddback,0),IDirectDrawSurface_Release(lpddback);
	if (lpdd_dbg) IDirectDrawSurface_Unlock(lpdd_dbg,0),IDirectDrawSurface_Release(lpdd_dbg);
	if (lpddclip) IDirectDrawClipper_Release(lpddclip);
	if (lpdd) IDirectDraw_Release(lpdd);
	#endif

	if (session_dbg) DeleteObject(session_dbg);
	if (session_dc2) DeleteDC(session_dc2);
	if (session_dib) DeleteObject(session_dib);
	ReleaseDC(session_hwnd,session_dc1);
	free(video_blend);
	timeEndPeriod(8); // possibly unnecessary even on WIN10, as the programme is ending
}

// menu item functions ---------------------------------------------- //

void session_menucheck(int id,int q) // set the state of option `id` as `q`
	{ if (session_menu) CheckMenuItem(session_menu,id,MF_BYCOMMAND+(q?MF_CHECKED:MF_UNCHECKED)); }
void session_menuradio(int id,int a,int z) // set the option `id` in the range `a-z`
	{ if (session_menu) CheckMenuRadioItem(session_menu,a,z,id,MF_BYCOMMAND); }

// message box ------------------------------------------------------ //

void session_message(const char *s,const char *t) // show multi-lined text `s` under caption `t`
	{ session_please(); MessageBox(session_hwnd,s,t,strchr(t,'?')?MB_ICONQUESTION:(strchr(t,'!')?MB_ICONEXCLAMATION:MB_OK)); }
void session_aboutme(const char *s,const char *t) // special case: "About.."
{
	session_please();
	MSGBOXPARAMS mbp;
	mbp.cbSize=sizeof(mbp);
	mbp.hwndOwner=session_hwnd;
	mbp.hInstance=GetModuleHandle(0);
	mbp.lpszText=s;
	mbp.lpszCaption=t;
	mbp.dwStyle=MB_OK|MB_USERICON;
	mbp.lpszIcon=MAKEINTRESOURCE(34002);
	mbp.dwContextHelpId=mbp.dwLanguageId=0;
	mbp.lpfnMsgBoxCallback=NULL;
	MessageBoxIndirect(&mbp);
}

// shared dialog data ----------------------------------------------- //

int session_dialog_return; // dialog outcome: integer
char *session_dialog_text; // dialog outcome: string

// input dialog ----------------------------------------------------- //

LRESULT CALLBACK inputproc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) // dialog callback function
{
	switch (msg)
	{
		case WM_INITDIALOG:
			SetWindowText(hwnd,session_dialog_text);
			SendDlgItemMessage(hwnd,12345,EM_SETLIMITTEXT,STRMAX-1,0);
			return SendDlgItemMessage(hwnd,12345,WM_SETTEXT,0,(LPARAM)session_parmtr),session_dialog_return=-1;
		/*case WM_SIZE:
			RECT r; GetClientRect(hwnd,&r);
			SetWindowPos(session_dialog_item,NULL,0,0,r.right,r.bottom*1/2,SWP_NOZORDER);
			SetWindowPos(GetDlgItem(hwnd,IDOK),NULL,0,r.bottom*1/2,r.right/2,r.bottom/2,SWP_NOZORDER);
			return SetWindowPos(GetDlgItem(hwnd,IDCANCEL),NULL,r.right/2,r.bottom*1/2,r.right/2,r.bottom/2,SWP_NOZORDER),1;*/
		case WM_COMMAND:
			if (LOWORD(wparam)==IDCANCEL)
				EndDialog(hwnd,0);
			else if (/*!HIWORD(wparam)&&*/LOWORD(wparam)==IDOK)
			{
				session_dialog_return=SendDlgItemMessage(hwnd,12345,WM_GETTEXT,STRMAX,(LPARAM)session_parmtr);
				EndDialog(hwnd,0);
			}
			return 1;
	}
	return 0; // ignore other messages
}
int session_input(char *t) // `t` is the caption; returns <0 on error or LENGTH on success
{
	session_please();
	session_dialog_text=t;
	DialogBoxParam(GetModuleHandle(0),(LPCSTR)34004,session_hwnd,(DLGPROC)inputproc,0);
	return session_dialog_return; // both the source and target strings are in `session_parmtr`
}

// list dialog ------------------------------------------------------ //

LRESULT CALLBACK listproc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) // dialog callback function
{
	switch (msg)
	{
		case WM_INITDIALOG:
			SetWindowText(hwnd,session_dialog_text);
			char *l=(char*)lparam; while (*l)
			{
				SendDlgItemMessage(hwnd,12345,LB_ADDSTRING,0,(LPARAM)l);
				while (*l++) {}
			}
			return SendDlgItemMessage(hwnd,12345,LB_SETCURSEL,session_dialog_return,0),session_dialog_return=-1; // select item
		/*case WM_SIZE:
			RECT r; GetClientRect(hwnd,&r);
			SetWindowPos(session_dialog_item,NULL,0,0,r.right,r.bottom*15/16,SWP_NOZORDER);
			SetWindowPos(GetDlgItem(hwnd,IDOK),NULL,0,r.bottom*15/16,r.right/2,r.bottom/16,SWP_NOZORDER);
			return SetWindowPos(GetDlgItem(hwnd,IDCANCEL),NULL,r.right/2,r.bottom*15/16,r.right/2,r.bottom/16,SWP_NOZORDER),1;*/
		case WM_COMMAND:
			if (LOWORD(wparam)==IDCANCEL)
				EndDialog(hwnd,0);
			else if ((HIWORD(wparam)==LBN_DBLCLK)||(/*!HIWORD(wparam)&&*/LOWORD(wparam)==IDOK))
				if ((session_dialog_return=SendDlgItemMessage(hwnd,12345,LB_GETCURSEL,0,0))>=0) // ignore OK if no item is selected
				{
					SendDlgItemMessage(hwnd,12345,LB_GETTEXT,session_dialog_return,(LPARAM)session_parmtr);
					EndDialog(hwnd,0);
				}
			return 1;
	}
	return 0; // ignore other messages
}
int session_list(int i,const char *s,char *t) // `s` is a list of ASCIZ entries, `i` is the default chosen item, `t` is the caption; returns <0 on error or 0..n-1 on success
{
	if (!*s) return -1; // empty!
	session_please();
	session_dialog_text=t;
	session_dialog_return=i;
	DialogBoxParam(GetModuleHandle(0),(LPCSTR)34003,session_hwnd,(DLGPROC)listproc,(LPARAM)s);
	return session_dialog_return; // the string is in `session_parmtr`
}

// scan dialog ------------------------------------------------------ //

LRESULT CALLBACK scanproc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) // dialog callback function
{
	switch (msg)
	{
		case WM_INITDIALOG:
			EnableWindow(session_hwnd,0);
			SetWindowText(hwnd,session_dialog_text);
			return SendDlgItemMessage(hwnd,12345,WM_SETTEXT,0,lparam),session_dialog_return=-1;
		/*case WM_SIZE:
			RECT r; GetClientRect(hwnd,&r);
			return SetWindowPos(session_dialog_item,NULL,0,0,r.right,r.bottom,SWP_NOZORDER),1;*/
		case WM_CLOSE:
			EnableWindow(session_hwnd,1);
			SetFocus(session_hwnd); // avoid glitches if the user switches to another window and then back to our dialog
			session_hdlg=NULL;
			return DestroyWindow(hwnd),1;
	}
	return 0; // ignore other messages
}
int session_scan(const char *s) // `s` is the name of the event; returns <0 on error or >=0 (keyboard code) on success
{
	session_please();
	session_dialog_text=txt_session_scan;
	session_hdlg=CreateDialogParam(GetModuleHandle(0),(LPCSTR)34005,session_hwnd,(DLGPROC)scanproc,(LPARAM)s);
	ShowWindow(session_hdlg,SW_SHOWDEFAULT); //UpdateWindow(session_hdlg);
	for (MSG msg;session_hdlg&&GetMessage(&msg,NULL,0,0)>0;TranslateMessage(&msg),DispatchMessage(&msg))
		if (msg.message==WM_KEYDOWN)
		{
			int i=((HIWORD(msg.lParam))&127)+(((HIWORD(msg.lParam))>>1)&128);
			if (i!=KBCODE_L_SHIFT&&i!=KBCODE_R_SHIFT&&i!=KBCODE_L_CTRL&&i!=KBCODE_R_CTRL
				&&!((i>=KBCODE_F1&&i<=KBCODE_F10)||(i>=KBCODE_F11&&i<=KBCODE_F12)))
				PostMessage(session_hdlg,WM_CLOSE,0,0),session_dialog_return=i==KBCODE_ESCAPE?-1:i;
		}
	return session_dialog_return; // the string is in `session_parmtr`
}

// file dialog ------------------------------------------------------ //

OPENFILENAME session_ofn;
int getftype(const char *s) // <0 = invalid path / nothing, 0 = file, >0 = directory
{
	if (!s||!*s) return -1; // invalid path
	int i=GetFileAttributes(s); return i>0?i&FILE_ATTRIBUTE_DIRECTORY:i;
}
int session_filedialog(char *r,const char *s,const char *t,int q,int f) // auxiliar function, see below
{
	if (!*s) return 0; // empty! GetOpenFileName and co. return ZERO when an error happens or the user hits "CANCEL"
	session_please();
	memset(&session_ofn,0,sizeof(session_ofn));
	session_ofn.lStructSize=sizeof(session_ofn);
	session_ofn.hwndOwner=session_hwnd;
	if (!r) r=session_path; // NULL path = default!
	if (r!=(char*)session_tmpstr)
		strcpy(session_tmpstr,r); // copy path, if required
	int i=strlen(session_tmpstr); // sanitize path
	if (i&&session_tmpstr[--i]=='\\') // pure path?
		if (i&&session_tmpstr[i-1]!=':') // avoid special case "C:\"
			session_tmpstr[i]=0; // turn "C:\ABC\" into "C:\ABC"
	r=strrchr(session_tmpstr,'\\');
	*session_parmtr=0; // no file by default
	if (!(i=getftype(session_tmpstr))) // valid file?
	{
		if (r)
			strcpy(session_parmtr,++r),*r=0; // file with path
		else
			strcpy(session_parmtr,session_tmpstr),strcpy(session_tmpstr,session_path); // file without path
	}
	else if (i<0&&r) // invalid file; valid path?
	{
		r[1]=0; // remove invalid file, keep path
		if (getftype(session_tmpstr)<=0)
			strcpy(session_tmpstr,session_path); // invalid path = default!
	}
	strcpy(session_substr,s);
	strcpy(&session_substr[strlen(s)+1],s); // one NULL char between two copies of the same string
	session_substr[strlen(s)*2+2]=session_substr[strlen(s)*2+3]=0;
	// tmpstr: "C:\ABC"; parmtr: "XYZ.EXT"; substr: "*.EXT;*.EXU","*.EXT;*.EXU",""
	session_ofn.lpstrTitle=t;
	session_ofn.lpstrInitialDir=session_tmpstr;
	session_ofn.lpstrFile=session_parmtr;
	session_ofn.nMaxFile=sizeof(session_parmtr);
	session_ofn.lpstrFilter=session_substr;
	session_ofn.nFilterIndex=1;
	session_ofn.lpstrDefExt=((t=strrchr(s,'.'))&&(*++t!='*'))?t:NULL;
	session_ofn.Flags=OFN_PATHMUSTEXIST|OFN_NONETWORKBUTTON|OFN_NOCHANGEDIR|(q?OFN_OVERWRITEPROMPT:(OFN_FILEMUSTEXIST|f));
	return q?GetSaveFileName(&session_ofn):GetOpenFileName(&session_ofn);
}
#define session_filedialog_get_readonly() (session_ofn.Flags&OFN_READONLY)
#define session_filedialog_set_readonly(q) ((q)?(session_ofn.Flags|=OFN_READONLY):(session_ofn.Flags&=~OFN_READONLY))
char *session_newfile(char *r,const char *s,const char *t) // "Create File" | ...and returns NULL on failure, or `session_parmtr` (with a file path) on success.
	{ return session_filedialog(r,s,t,1,0)?session_parmtr:NULL; }
char *session_getfile(char *r,const char *s,const char *t) // "Open a File" | lists files in path `r` matching pattern `s` under caption `t`, etc.
	{ return session_filedialog(r,s,t,0,OFN_HIDEREADONLY)?session_parmtr:NULL; }
char *session_getfilereadonly(char *r,const char *s,const char *t,int q) // "Open a File" with Read Only option | lists files in path `r` matching pattern `s` under caption `t`; `q` is the default Read Only value, etc.
	{ return session_filedialog(r,s,t,0,q?OFN_READONLY:0)?session_parmtr:NULL; }

// final definitions ------------------------------------------------ //

// dummy SDL2 definitions
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER SDL_LIL_ENDIAN
// unlike SDL2, Win32 doesn't include "math.h" either directly or indirectly :-/
#include <math.h>
#ifndef M_PI // old MSVCRT versions lack M_PI!
#define M_PI 3.14159265358979323846264338327950288 // used by SDL2; for comparison, FFmpeg's M_PI is 3.14159265358979323846 and Direct3D's D3DX_PI is 3.141592654
#endif
// rely on SDL wrappers to ensure compatibility
#define SDL_pow pow
#define SDL_sin sin
//#define SDL_cos cos // cos(x)=sin(x+M_PI/2)
//#define SDL_sqrt sqrt // sqrt(x)=pow(x,0.5)
// main-WinMain bootstrap: the normal binary is window-driven
#ifdef DEBUG
#define BOOTSTRAP // the debug version is terminal-driven
#else
#ifndef __argc
	extern int __argc; extern char **__argv; // GCC5's -std=gnu99 does NOT define them by default, despite being part of STDLIB.H and MSVCRT!
#endif
#define BOOTSTRAP int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) { return main(__argc,__argv); }
#endif

// ================================== END OF WINDOWS 5.0+ DEFINITIONS //
