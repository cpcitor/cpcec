 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// SDL2 is the second supported platform; to compile the emulator type
// "$(CC) -DSDL2 -xc cpcec.c -lSDL2" for GCC, TCC, CLANG et al.
// Support for Unix-like systems is provided by providing different
// snippets of code upon whether the symbol "_WIN32" exists or not.

// This runtime also includes its own graphic user interface (menu,
// input, file browser...) because SDL2 has no effective UI logic.

// START OF SDL 2.0+ DEFINITIONS ==================================== //

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED // required, BOOTSTRAP relies on our own main()
#endif

#include <SDL2/SDL.h>

#ifdef _WIN32
	#define STRMAX 640 // 288 // widespread in Windows
	#define PATHCHAR '\\' // WIN32
	#include <windows.h> // FindFirstFile...
	#include <io.h> // _chsize(),_fileno()...
	#define fsetsize(f,l) _chsize(_fileno(f),(l))
	#define strcasecmp _stricmp // see also SDL_strcasecmp
	// WIN32 is never UTF-8; it's either ANSI (...A) or UNICODE (...W)
	#define ARGVZERO (GetModuleFileName(NULL,session_substr,sizeof(session_substr))?session_substr:argv[0])
	#define I18N_MULTIPLY "x" //"\327"
	//#define I18N_DIVISION "\367"
#else
	#ifdef PATH_MAX
		#define STRMAX PATH_MAX
	#else
		#define STRMAX 640 // 512 // MacOS lacks PATH_MAX; TCC's LIMITS.H sets it to 512 and seems safe, with the exception of realpath()
	#endif
	#define PATHCHAR '/' // POSIX
	#include <sys/stat.h> // stat()...
	#include <dirent.h> // opendir()...
	#include <unistd.h> // ftruncate(),fileno()...
	#define fsetsize(f,l) (!ftruncate(fileno(f),(l)))
	#define INT8 Sint8 // can this be safely reduced to "signed char"?
	#define BYTE Uint8 // can this be safely reduced to "unsigned char"?
	#define WORD Uint16 // can this be safely reduced to "unsigned short"?
	#define DWORD Uint32 // this CANNOT be safely reduced to "unsigned int"!
	#define SDL2_UTF8 // is there any *NIX system that does NOT rely on UTF-8?
	#define ARGVZERO argv[0] // is there a better way to know the binary's path?
	#define I18N_MULTIPLY "x" //"\303\227"
	//#define I18N_DIVISION "\303\267"
#endif

#define MESSAGEBOX_WIDETAB "\t\t" // rely on monospace font
#define GPL_3_LF "\n" // our widgets need preset line feeds

// general engine constants and variables --------------------------- //

//typedef Uint8 AUDIO_UNIT; // obsolete 8-bit audio
//#define AUDIO_BITDEPTH 8
//#define AUDIO_ZERO 128
typedef Sint16 AUDIO_UNIT; // standard 16-bit audio
#define AUDIO_BITDEPTH 16
#define AUDIO_ZERO 0
#define AUDIO_BYTESTEP (AUDIO_CHANNELS*AUDIO_BITDEPTH/8) // i.e. 1, 2 or 4 bytes
typedef DWORD VIDEO_UNIT; // the pixel style must be 0X00RRGGBB and nothing else!

VIDEO_UNIT *menus_frame; // UI frame
VIDEO_UNIT *video_frame,*video_blend; // video + blend frames, allocated on runtime
AUDIO_UNIT audio_frame[AUDIO_PLAYBACK/50*AUDIO_CHANNELS]; // audio frame; 50 is the lowest legal framerate
VIDEO_UNIT *video_target; // pointer to current video pixel
AUDIO_UNIT *audio_target; // pointer to current audio sample
unsigned int session_joybits=0; // write joystick motions down here
char session_path[STRMAX],session_parmtr[STRMAX],session_tmpstr[STRMAX],session_substr[STRMAX],session_info[STRMAX]="";

// SDL2 follows the USB keyboard standard, so we're using the same values here!

// function keys
#define KBCODE_F1	 58
#define KBCODE_F2	 59
#define KBCODE_F3	 60
#define KBCODE_F4	 61
#define KBCODE_F5	 62
#define KBCODE_F6	 63
#define KBCODE_F7	 64
#define KBCODE_F8	 65
#define KBCODE_F9	 66
#define KBCODE_F10	 67
#define KBCODE_F11	 68
#define KBCODE_F12	 69
// leftmost keys
#define KBCODE_ESCAPE	 41
#define KBCODE_TAB	 43
#define KBCODE_CAPSLOCK	 57
#define KBCODE_L_SHIFT	225
#define KBCODE_L_CTRL	224
//#define KBCODE_L_ALT	226 // trapped by Win32
// alphanumeric row 1
#define KBCODE_1	 30
#define KBCODE_2	 31
#define KBCODE_3	 32
#define KBCODE_4	 33
#define KBCODE_5	 34
#define KBCODE_6	 35
#define KBCODE_7	 36
#define KBCODE_8	 37
#define KBCODE_9	 38
#define KBCODE_0	 39
#define KBCODE_CHR1_1	 45
#define KBCODE_CHR1_2	 46
#define KBCODE_CHR4_5	100 // this is the key before "1" in modern keyboards
// alphanumeric row 2
#define KBCODE_Q	 20
#define KBCODE_W	 26
#define KBCODE_E	  8
#define KBCODE_R	 21
#define KBCODE_T	 23
#define KBCODE_Y	 28
#define KBCODE_U	 24
#define KBCODE_I	 12
#define KBCODE_O	 18
#define KBCODE_P	 19
#define KBCODE_CHR2_1	 47
#define KBCODE_CHR2_2	 48
// alphanumeric row 3
#define KBCODE_A	  4
#define KBCODE_S	 22
#define KBCODE_D	  7
#define KBCODE_F	  9
#define KBCODE_G	 10
#define KBCODE_H	 11
#define KBCODE_J	 13
#define KBCODE_K	 14
#define KBCODE_L	 15
#define KBCODE_CHR3_1	 51
#define KBCODE_CHR3_2	 52
#define KBCODE_CHR3_3	 49
// alphanumeric row 4
#define KBCODE_Z	 29
#define KBCODE_X	 27
#define KBCODE_C	  6
#define KBCODE_V	 25
#define KBCODE_B	  5
#define KBCODE_N	 17
#define KBCODE_M	 16
#define KBCODE_CHR4_1	 54
#define KBCODE_CHR4_2	 55
#define KBCODE_CHR4_3	 56
#define KBCODE_CHR4_4	 53 // this is the key before "Z" in 105-key modern keyboards; missing in 104-key layouts!
// rightmost keys
#define KBCODE_SPACE	 44
#define KBCODE_BKSPACE	 42
#define KBCODE_ENTER	 40
#define KBCODE_R_SHIFT	229
#define KBCODE_R_CTRL	228
// #define KBCODE_R_ALT	230 // trapped by Win32
// extended keys
// #define KBCODE_PRINT	 70 // trapped by Win32
#define KBCODE_SCR_LOCK	 71
#define KBCODE_HOLD	 72
#define KBCODE_INSERT	 73
#define KBCODE_DELETE	 76
#define KBCODE_HOME	 74
#define KBCODE_END	 77
#define KBCODE_PRIOR	 75
#define KBCODE_NEXT	 78
#define KBCODE_UP	 82
#define KBCODE_DOWN	 81
#define KBCODE_LEFT	 80
#define KBCODE_RIGHT	 79
#define KBCODE_APPS	101
// numeric keypad
#define KBCODE_NUM_LOCK	 83
#define KBCODE_X_7	 95
#define KBCODE_X_8	 96
#define KBCODE_X_9	 97
#define KBCODE_X_4	 92
#define KBCODE_X_5	 93
#define KBCODE_X_6	 94
#define KBCODE_X_1	 89
#define KBCODE_X_2	 90
#define KBCODE_X_3	 91
#define KBCODE_X_0	 98
#define KBCODE_X_DOT	 99
#define KBCODE_X_ENTER	 88
#define KBCODE_X_ADD	 87
#define KBCODE_X_SUB	 86
#define KBCODE_X_MUL	 85
#define KBCODE_X_DIV	 84
#define usbkey2native memcpy // no translation is needed;
#define native2usbkey(str,n) // the SDL2 keyboard is USB!
BYTE kbd_k2j[]= // these keys can simulate a 4-button joystick
	{ KBCODE_UP, KBCODE_DOWN, KBCODE_LEFT, KBCODE_RIGHT, KBCODE_Z, KBCODE_X, KBCODE_C, KBCODE_V };

unsigned char kbd_map[256]; // key-to-key translation map

// general engine functions and procedures -------------------------- //

#ifdef _WIN32
char basepath[STRMAX]; // is this long enough for the GetFullPathName() target buffer?
char *makebasepath(const char *s) // fetch the absolute path of a relative path; returns NULL on error
	{ char *m; return GetFullPathName(s,sizeof(basepath),basepath,&m)?basepath:NULL; }
#else
char basepath[8<<9];  // the buffer of realpath() is unlike other STRMAX-length strings, it depends on the irregular PATH_MAX definition:
#define makebasepath(s) realpath((s),basepath) // the target must be at least 4096 bytes long in Linux 4.10.0-38-generic, Linux 4.15.0-96-generic...!
#endif

void session_please(void) // stop activity for a short while
	{ if (!session_wait) if (session_wait=1,session_audio) SDL_PauseAudioDevice(session_audio,1); }
void session_thanks(void) // resume activity after a "please"
	{ if (session_wait) if (session_wait=0,session_audio) SDL_PauseAudioDevice(session_audio,0); }
void session_kbdclear(void) // wipe keyboard and joystick bits, for example on focus loss
	{ joy_kbd=joy_bit=session_joybits=0; MEMZERO(kbd_bit); }
#define session_kbdreset() MEMBYTE(kbd_map,~~~0) // init and clean key map up
void session_kbdsetup(const unsigned char *s,int l) // maps a series of virtual keys to the real ones
{
	session_kbdclear();
	while (l--) { int k=*s++; kbd_map[k]=*s++; }
}
int session_k2joy(int k) // translate key code; -8..-1 = joystick bit 0..7, 0..127 = normal key, 128..255 = function key
{
	if (session_key2joy) for (int i=0;i<KBD_JOY_UNIQUE;++i) if (kbd_k2j[i]==k) return i-8;
	return kbd_map[k];
}

void *session_joy=NULL; int session_pad; // SDL_Joystick + SDL_GameController
SDL_Window *session_hwnd=NULL;

// unlike Windows, where the user interface is internally provided by the system and as well as the compositing work,
// we must provide our own UI here, and this means we must use one canvas for the emulation and another one for the UI.
// we show the first canvas during normal operation, but switch to the second one when the UI is active.
// the graphical debugger effectively behaves as a temporary substitute of the emulation canvas.
// notice that SDL_Texture, unlike SDL_Surface, doesn't rely on anything like SDL_GetWindowSurface()

VIDEO_UNIT *debug_frame;
BYTE session_hidemenu=0; // positive or negative UI
SDL_Renderer *session_blitter=NULL; // (==null?software:hardware)
SDL_Texture *session_dib=NULL,*session_gui=NULL,*session_dbg=NULL; // we can abuse these pointers in software mode by casting them into (SDL_Surface*)
SDL_Rect session_ideal; // used for calculations, see below

#ifdef SDL_DISABLE_IMMINTRIN_H // __TINYC__ // TCC causes a segmentation fault (!?)
#define session_clrscr() ((void)0)
#else
#define session_clrscr() (session_hardblit?SDL_RenderClear(session_blitter):0) // defaults to black
#endif
#define session_clock (1000) // the SDL2 internal tick count unit is the millisecond
void session_backupvideo(VIDEO_UNIT *t); // make a clipped copy of the current screen. Must be defined later on!
int session_r_x,session_r_y,session_r_w,session_r_h; // actual location and size of the bitmap
int session_resize(void) // SDL2 handles almost everything, but we still have to keep a cache of the past state
{
	static BYTE z=~0; if (session_fullblit)
	{
		if (z)
		{
			SDL_SetWindowFullscreen(session_hwnd,SDL_WINDOW_FULLSCREEN_DESKTOP);
			z=0; session_clrscr(),session_dirty=1; // clean up and update options
		}
	}
	else if (z!=session_zoomblit+1)
	{
		if (!z)
			SDL_SetWindowFullscreen(session_hwnd,0);
		int x=(VIDEO_PIXELS_X>>1)*(session_zoomblit+2),y=(VIDEO_PIXELS_Y>>1)*(session_zoomblit+2);
		SDL_SetWindowSize(session_hwnd,x,y); // SDL2 adds the window borders on its own!
		SDL_SetWindowPosition(session_hwnd,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED); // SDL2 won't resize AND center windows in one operation
		z=session_zoomblit+1; session_clrscr(),session_dirty=1; // clean up and update options
	}
	return z;
}
void session_reveal(void) { if (session_hardblit) SDL_RenderPresent(session_blitter); else SDL_UpdateWindowSurface(session_hwnd); }
void session_redraw(int q) // redraw main canvas (!0) or user interface (0)
{
	SDL_Surface *o; if (session_hardblit)
		SDL_GetRendererOutputSize(session_blitter,&session_ideal.w,&session_ideal.h);
	else
		{ o=SDL_GetWindowSurface(session_hwnd); session_ideal.w=o->w,session_ideal.h=o->h; }
	if ((session_r_w=session_ideal.w)>0&&(session_r_h=session_ideal.h)>0) // don't redraw invalid windows!
	{
		if (session_r_w>session_r_h*VIDEO_PIXELS_X/VIDEO_PIXELS_Y) // window area is too wide?
			session_r_w=session_r_h*VIDEO_PIXELS_X/VIDEO_PIXELS_Y;
		if (session_r_h>session_r_w*VIDEO_PIXELS_Y/VIDEO_PIXELS_X) // window area is too tall?
			session_r_h=session_r_w*VIDEO_PIXELS_Y/VIDEO_PIXELS_X;
		//if (session_fullblit) // maximum integer zoom: 100%, 150%, 200%...
			session_r_w=((session_r_w*17)/VIDEO_PIXELS_X/8)*VIDEO_PIXELS_X/2, // "*17../16../1"
			session_r_h=((session_r_h*17)/VIDEO_PIXELS_Y/8)*VIDEO_PIXELS_Y/2; // forbids N+50%
		if (session_r_w<VIDEO_PIXELS_X||session_r_h<VIDEO_PIXELS_Y)
			session_r_w=VIDEO_PIXELS_X,session_r_h=VIDEO_PIXELS_Y; // window area is too small!
		session_ideal.x=session_r_x=(session_ideal.w-session_r_w)>>1; session_ideal.w=session_r_w;
		session_ideal.y=session_r_y=(session_ideal.h-session_r_h)>>1; session_ideal.h=session_r_h;
		SDL_Texture *s; int ox,oy;
		if (!q)
			s=session_gui,ox=0,oy=0;
		else if (session_signal&SESSION_SIGNAL_DEBUG)
			s=session_dbg,ox=0,oy=0;
		else
			s=session_dib,ox=VIDEO_OFFSET_X,oy=VIDEO_OFFSET_Y;
		SDL_Rect r; r.x=ox,r.y=oy,r.w=VIDEO_PIXELS_X,r.h=VIDEO_PIXELS_Y; // some C compilers dislike `SDL_Rect r={.x=ox,.y=oy,.w=VIDEO_PIXELS_X,.h=VIDEO_PIXELS_Y}`
		if (session_hardblit)
		{
			SDL_UnlockTexture(s); // prepare for sending
			if (SDL_RenderCopy(session_blitter,s,&r,&session_ideal)>=0) // send! (warning: this operation has a memory leak on several SDL2 versions)
				session_reveal(); // update window!
			VIDEO_UNIT *t; SDL_LockTexture(s,NULL,(void**)&t,&ox); // allow editing again; do we need to care about the new value at `ox`?
			if (!q)
				menus_frame=t;
			else if (session_signal&SESSION_SIGNAL_DEBUG)
				debug_frame=t;
			else
			{
				ox=video_target-video_frame; // the video target pointer...
				video_target=(video_frame=t)+ox; // ...must follow the frame!
			}
		}
		else
			if ((session_r_w>VIDEO_PIXELS_X?SDL_BlitScaled((SDL_Surface*)s,&r,o,&session_ideal):SDL_BlitSurface((SDL_Surface*)s,&r,o,&session_ideal))>=0)
				session_reveal(); // update window!
	}
}
#define session_drawme() session_redraw(1) // video shortcut!
void session_playme(void) // audio shortcut!
{
	static BYTE s=1; if (s!=!!audio_disabled) if (s=!s) MEMBYTE(audio_frame,AUDIO_ZERO); // mute mode cleanup!
	int i=audio_session/((4-session_softplay)<<(AUDIO_L2BUFFER+AUDIO_BYTESTEP/2-(2)-3));
	if (i<=8) // 0 void... 4 half... 8 full
	{
		//session_timer+=i-4; // stronger: -4 -3 -2 -1 =0 +1 +2 +3 +4
		if (i<3) session_timer+=i-3; else if (i>5) session_timer+=i-5; // average: -3 -2 -1 =0 =0 =0 +1 +2 +3
		//session_timer+=(i-4)/2; // softer: -2 -1 -1 =0 =0 =0 +1 +1 +2
		SDL_QueueAudio(session_audio,audio_frame,AUDIO_LENGTH_Z*AUDIO_BYTESTEP);
	}
	audio_session=SDL_GetQueuedAudioSize(session_audio); // ensure that the first `audio_session` is zero
}
char *session_blitinfo(void) { return session_hardblit?"SDL":"sdl"; }

#ifdef SDL2_UTF8
// SDL2 relies on the UTF-8 encoding for character and string manipulation; hence the following functions. //
// Similarly, non-Windows systems are going to encode filenames with UTF-8, instead of sticking to 8 bits. //
// Valid codes are those within the 0..2097151 range; technically 0..1114111 but we miss the binary logic. //
// The logic is very overengineered anyway: the built-in font is limited to the 8-bit ISO-8859-1 codepage. //
// Invalid codes (for example a Win32-made ZIP archive browsed within a non-Win32 system) are shown as -1. //
void utf8put(char **s,int i) // send a valid code `i` to a UTF-8 pointer `s`
{
	if (i&-2097152) {} else if (i<128) *((*s)++)=i; else
		{ if (i<2048) *((*s)++)=(i>>6)-64; else
			{ if (i<65536) *((*s)++)=(i>>12)-32; else
				*((*s)++)=(i>>18)-16,*((*s)++)=((i>>12)&63)-128;
			*((*s)++)=((i>>6)&63)-128; }
		*((*s)++)=(i&63)-128; }
}
int utf8get(const char **s) // get a code `i` from a UTF-8 pointer `s`; <0 INVALID CODE
{
	int i=(INT8)*((*s)++); if (i>=0) return i; if (i<-64) return -1;
	int j=0; INT8 k; while ((k=**s)<-64) j=j*64+k+128,(*s)++;
	if (i<-32) return ((i=((i+64)<<6)+j)>=128&&i<2048)?i:-1;
	if (i<-16) return ((i=((i+32)<<12)+j)>=2048&&i<65536)?i:-1;
	return ((i=((i+16)<<18)+j)>=65536&&i<2097152)?i:-1;
}
int utf8len(const char *s) // length (in codes, not bytes) of a UTF-8 string `s`
	{ int i=0,k; while (k=(INT8)*s++) if (++i,k>=-64&&k<0) while ((INT8)*s<-64) ++s; return i; }
int utf8chk(int i) // size in bytes of a valid UTF-8 code `i`; 0 INVALID CODE
	{ return i&-2097152?0:i<128?1:i<2048?2:i<65536?3:4; }
// translate offset `i` (positive or negative) from codes to bytes within a **VALID** UTF-8 pointer `s`
int utf8inc(char *s,int i) { int o=0; do do ++o; while ((INT8)*++s<-64); while (--i); return o; } // positive offset, go forwards
int utf8dec(char *s,int i) { int o=0; do do --o; while ((INT8)*--s<-64); while (++i); return o; } // negative, backwards
#define utf8add(s,i) ((i)>0?utf8inc((s),(i)):(i)<0?utf8dec((s),(i)):0) // remember, `s` is valid UTF-8: no error checking is performed!
#else // simple chars without UTF-8
#define utf8put(s,i) (*((*(s))++)=i)
#define utf8get(s) ((BYTE)*((*(s))++))
#define utf8len(s) strlen((s))
#define utf8chk(i) (1)
#define utf8add(s,i) (i)
#endif

// extremely tiny graphical user interface: SDL2 provides no widgets! //

#define SESSION_UI_HEIGHT 14 // must be equal or higher than ONSCREEN_SIZE!!

BYTE session_ui_chrs[ONSCREEN_CEIL*SESSION_UI_HEIGHT];//,session_ui_chrlen[256];
void session_ui_makechrs(void)
{
	MEMZERO(session_ui_chrs);
	for (int i=0;i<ONSCREEN_CEIL;++i)
		for (int j=0;j<ONSCREEN_SIZE;++j)
		{
			int z=onscreen_chrs[i*ONSCREEN_SIZE+j];
			session_ui_chrs[i*SESSION_UI_HEIGHT+(SESSION_UI_HEIGHT-ONSCREEN_SIZE)/2+j]=
				i<30?z:z|(z>>1); // normal, not thin or bold; special chars stay thin
		}
	/*{ // experimental monospace-to-proportional font rendering
		int bits=0; for (int j=0;j<SESSION_UI_HEIGHT;++j)
			bits|=session_ui_chrs[i*SESSION_UI_HEIGHT+j];
		if (bits)
		{
			int k=0,l=8+1; // extra space after char
			while (bits<128)
				++k,bits<<=1;
			while (!(bits&1))
				--l,bits>>=1;
			session_ui_chrlen[i]=l;
			for (int j=0;j<SESSION_UI_HEIGHT;++j)
				l=session_ui_chrs[i*SESSION_UI_HEIGHT+j]<<k;
		}
	}
	session_ui_chrlen[0]=session_ui_chrlen[1];*/ // space is as wide as "!"
}
/*int session_ui_strlen(const char *s) // get proportional string length in pixels
	{ int i=0; while (*s) i+=session_ui_chrlen[*s++-32]; return i; }*/

BYTE session_ui_menudata[1<<12],session_ui_menusize; // encoded menu data
#ifdef _WIN32
int session_ui_drives=0; char session_ui_drive[]="::\\";
#endif
SDL_Surface *session_ui_icon=NULL; // the window icon

void session_fillrect(int rx,int ry,int rw,int rh,VIDEO_UNIT a) // n.b.: coords in pixels
{
	for (VIDEO_UNIT *p=&menus_frame[ry*VIDEO_PIXELS_X+rx];rh>0;--rh,p+=VIDEO_PIXELS_X-rw)
		for (int x=rw;x>0;--x) *p++=a;
}
void session_ui_fillrect(int x,int y,int w,int h,VIDEO_UNIT a) // coords in characters
	{ session_fillrect(x*8,y*SESSION_UI_HEIGHT,w*8,h*SESSION_UI_HEIGHT,a); }
void session_fillboth(int rx,int ry,int rw,int rh,VIDEO_UNIT a,VIDEO_UNIT b) // n.b.: coords in pixels
{
	for (VIDEO_UNIT *p=&menus_frame[ry*VIDEO_PIXELS_X+rx];rh>0;--rh,p+=VIDEO_PIXELS_X-rw)
		{ for (int x=rw;x>0;x-=2) *p++=a,*p++=b; a^=b,b^=a,a^=b; }
}
void session_ui_fillboth(int x,int y,int w,int h,VIDEO_UNIT a,VIDEO_UNIT b) // coords in characters
	{ session_fillboth(x*8,y*SESSION_UI_HEIGHT,w*8,h*SESSION_UI_HEIGHT,a,b); }
#if 0
void session_fillvert(int rx,int ry,int rw,int rh,VIDEO_UNIT a,VIDEO_UNIT b) // n.b.: coords in pixels
{
	for (VIDEO_UNIT *p=&menus_frame[ry*VIDEO_PIXELS_X+rx];rh>0;--rh,p+=VIDEO_PIXELS_X-rw)
		{ for (int x=rw;x>0;x-=2) *p++=a,*p++=b; }
}
void session_ui_fillvert(int x,int y,int w,int h,VIDEO_UNIT a,VIDEO_UNIT b) // coords in characters
	{ session_fillvert(x*8,y*SESSION_UI_HEIGHT,w*8,h*SESSION_UI_HEIGHT,a,b); }
void session_fillhorz(int rx,int ry,int rw,int rh,VIDEO_UNIT a,VIDEO_UNIT b) // n.b.: coords in pixels
{
	for (VIDEO_UNIT *p=&menus_frame[ry*VIDEO_PIXELS_X+rx];rh>0;--rh,p+=VIDEO_PIXELS_X-rw)
		{ for (int x=rw;x>0;--x) *p++=a; a^=b,b^=a,a^=b; }
}
void session_ui_fillhorz(int x,int y,int w,int h,VIDEO_UNIT a,VIDEO_UNIT b) // coords in characters
	{ session_fillhorz(x*8,y*SESSION_UI_HEIGHT,w*8,h*SESSION_UI_HEIGHT,a,b); }
#endif
int SESSION_UI_000=0X000000,SESSION_UI_033=0X808080,SESSION_UI_067=0XC0C0C0,SESSION_UI_100=0XFFFFFF;
int session_ui_style; // bigger than normal text; >0 draws a separator below the text (caption), <0 doesn't (menu)
#define SESSION_UI_GAP 2 // caption height, either 2 or 3
int session_ui_center=0; // REDEFINE texts (and perhaps others) must be centered on the dialog

void session_drawborder(int x,int y,int w,int h) // coords in pixels
{
	int z,l;
	z=x-2,l=w+4; // sharp corner
	//z=x-1,l=w+2; // round corner
	//z=x,l=w; // "hexagonal" corner
	session_fillrect(z,y-2,l,2,SESSION_UI_067); // top border
	session_fillrect(z,y+h,l,2,SESSION_UI_033); // bottom border
	z=y-1,l=h+2;
	session_fillrect(x-2,z,2,l,SESSION_UI_067); // left border
	session_fillrect(x+w,z,2,l,SESSION_UI_033); // right border
}
void session_ui_drawborder(int x,int y,int w,int h) // coords in characters
	{ session_drawborder(x*8,y*SESSION_UI_HEIGHT,w*8,h*SESSION_UI_HEIGHT); }

void session_topnbottom(int x,int y,int w) // coords in pixels
{
	session_fillrect(x,y-2,w,2,SESSION_UI_033); // high dash is dark
	session_fillrect(x,y  ,w,2,SESSION_UI_067); // low dash is light
}
void session_ui_middleline(int x,int y,int w) // coords in characters
	{ session_topnbottom(x*8,y*SESSION_UI_HEIGHT+SESSION_UI_HEIGHT/2,w*8); }
void session_ui_bottomline(int x,int y,int w) // coords in characters
	{ session_topnbottom(x*8,(y+SESSION_UI_GAP)*SESSION_UI_HEIGHT-2,(w+2)*8); }

VIDEO_UNIT session_glyph_q0,session_glyph_q1;
int session_ui_setstyle(int i) // 0 = normal, -1 = menu bar, +1 = dialog caption
{
	if (i>0)
		session_glyph_q0=SESSION_UI_000,session_glyph_q1=SESSION_UI_100;
	else
		session_glyph_q0=SESSION_UI_100,session_glyph_q1=SESSION_UI_000;
	return session_ui_style=i;
}

int session_ui_printglyph(VIDEO_UNIT *p,int z,int q) // print glyph `z` at target `*p` with `q` inverse video
{
	if (z<1||z>=ONSCREEN_CEIL) z=31; // unknown glyph :-/
	const int w=8; //=session_ui_chrlen[z]; // notice we drop unknown glyphs in advance
	{
		VIDEO_UNIT q0,q1; if (q) // inverse video? swap ink and paper!
			q1=session_glyph_q0,q0=session_glyph_q1; else q0=session_glyph_q0,q1=session_glyph_q1;
		BYTE const *r=&session_ui_chrs[z*SESSION_UI_HEIGHT];
		q=session_ui_style?(SESSION_UI_HEIGHT*(SESSION_UI_GAP-1))/2-2:0;
		for (int yy=q;yy>0;p+=VIDEO_PIXELS_X-w,--yy)
			for (int xx=w;xx>0;--xx) *p++=q0;
		for (int yy=SESSION_UI_HEIGHT;yy>0;p+=VIDEO_PIXELS_X-w,--yy)
			for (int rr=*r++,xx=0;xx<w;++xx)
				*p++=(rr&(128>>xx))?q1:q0;
		for (int yy=q;yy>0;p+=VIDEO_PIXELS_X-w,--yy)
			for (int xx=w;xx>0;--xx) *p++=q0;
	}
	return w; // pixel width
}
int session_ui_printasciz(char *s,int x,int y,int prae,int w,int post,int q1,int q2) // coords in characters
{
	if (w<0) w=utf8len(s); // negative `w` = whole string
	int n=prae+w+post; if ((q1|q2)<0) q1=q2=-1; // remember for later
	VIDEO_UNIT *t=&menus_frame[x*8+(y*SESSION_UI_HEIGHT)*VIDEO_PIXELS_X];
	while (prae-->0)
		t+=session_ui_printglyph(t,' ',q1<0);
	int i=w-utf8len(s),q=q1<0; char *r=s; // avoid another warning
	if (i>=0)
	{
		post+=i;
		do
		{
			if (s-r==q1) q=1; // enable inverse video?
			if (s-r==q2) q=0; // disable inverse video?
			if (i=(utf8get((const char**)&s))) t+=session_ui_printglyph(t,i,q);
		}
		while (i);
	}
	else
	{
		w-=2; // ellipsis, see below
		while (w-->0)
		{
			if (s-r==q1) q=1; // enable inverse video?
			if (s-r==q2) q=0; // disable inverse video?
			t+=session_ui_printglyph(t,utf8get((const char**)&s),q);
		}
		t+=session_ui_printglyph(t,127,q); // ellipsis,
		t+=session_ui_printglyph(t,'.',q); // see above
	}
	while (post-->0)
		t+=session_ui_printglyph(t,' ',q1<0);
	return n;
}

void session_ui_drawcaption(char *s,int x,int y,int w)
{
	session_ui_setstyle(1);
	session_ui_printasciz(s,x,y,1,w,1,0,+0);
	session_ui_setstyle(0);
	session_ui_bottomline(x,y,w); // the border under the caption (cfr. "HEIGHT/2-2" above)
	//SDL_SetWindowTitle(session_hwnd,(s)); // replicate the dialog caption on the window
}

int session_ui_base_x,session_ui_base_y,session_ui_size_x,session_ui_size_y; // used to calculate mouse clicks relative to widget
int session_ui_maus_x,session_ui_maus_y; // mouse X+Y, when the "key" is -1 (move), -2 (left click) or -3 (right click)
char session_ui_char0,session_ui_shift,session_ui_focus=0; // keyboard flags (avoid double clicks, check SHIFT) and window focus
int session_ui_char; // ASCII+Shift of the latest keystroke
#define SESSION_UI_MAXX (VIDEO_PIXELS_X/8-6)
#define SESSION_UI_MAXY (VIDEO_PIXELS_Y/SESSION_UI_HEIGHT-2)

int session_ui_exchange(void) // wait for a keystroke or a mouse motion
{
	session_ui_char=0; for (SDL_Event event;SDL_WaitEvent(&event);)
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				if (event.window.event==SDL_WINDOWEVENT_EXPOSED)
					session_reveal(); // fast redraw
				#if 0 // *!* some users expect the menus to vanish on focus loss, others don't... what do?
				else if (event.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&session_ui_focus) return KBCODE_ESCAPE;
				#endif
				break;
			case SDL_MOUSEWHEEL:
				if (event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED) event.wheel.y=-event.wheel.y;
				if (event.wheel.y) return event.wheel.y<0?KBCODE_NEXT:KBCODE_PRIOR;
				break;
			case SDL_MOUSEBUTTONUP: // better than SDL_MOUSEBUTTONDOWN
			case SDL_MOUSEMOTION:
				session_ui_maus_x=(event.button.x-session_ideal.x)*VIDEO_PIXELS_X/session_ideal.w/8-session_ui_base_x,session_ui_maus_y=(event.button.y-session_ideal.y)*VIDEO_PIXELS_Y/session_ideal.h/SESSION_UI_HEIGHT-session_ui_base_y;
				return event.type==SDL_MOUSEBUTTONUP?event.button.button==SDL_BUTTON_RIGHT?-3:-2:-1;
			case SDL_KEYDOWN:
				session_ui_char0=0; // see SDL_TEXTINPUT
				session_ui_shift=!!(event.key.keysym.mod&KMOD_SHIFT);
				return event.key.keysym.mod&KMOD_ALT?0:event.key.keysym.scancode;
			case SDL_TEXTINPUT: // always follows SDL_KEYDOWN
				if (session_ui_char0)
					session_ui_char0=0;
				else if ((session_ui_char=event.text.text[0])&128) // UTF-8? (not that it matters, we stick to ASCII)
					session_ui_char=128+(session_ui_char&1)*64+(event.text.text[1]&63);
				return 0;
			case SDL_QUIT:
				return KBCODE_ESCAPE;
		}
	return 0; // ignore event breakdown
}

void session_ui_loop(void) // get background painted again to erase old widgets
{
	if (session_signal&SESSION_SIGNAL_DEBUG)
		memcpy(menus_frame,debug_frame,sizeof(VIDEO_UNIT[VIDEO_PIXELS_X*VIDEO_PIXELS_Y])); // use the debugger as the background, rather than the emulation
	else
		session_backupvideo(menus_frame);
}
void session_ui_init(void) { session_kbdclear(); session_please(); session_ui_loop(); }
void session_ui_exit(void) // wipe all widgets and restore the window contents
{
	//if (session_signal&(SESSION_SIGNAL_DEBUG|SESSION_SIGNAL_PAUSE)) // redraw if waiting // this fails when we enable PAUSE from the menu
		session_redraw(1);
}

void session_ui_menu(void) // show the menu and set session_event accordingly
{
	if (!session_ui_menusize)
		return;
	session_ui_init();
	int menuxs[1<<4],events[1<<6]; // hard limits, please don't create too many items :_(
	int menu=0,menus=0,menuz=-1,item=0,items=0,itemz=-1,itemx=0,itemw=0,itemy=0,itemh=0;
	char *zz=NULL,*z=NULL; // `zz` points to the current submenu's first item, `z` points to the current item
	int done,ox=session_ui_base_x=1,oy=session_ui_base_y=1,q=1;
	session_event=0X8000; do
	{
		done=0; do // redraw menus and items as required, then obey the user
		{
			if (menuz!=menu)
			{
				menuz=menu; q=itemz=session_ui_setstyle(-1); // the super menu is the caption
				int menux=menus=item=items=itemy=0; BYTE *m=session_ui_menudata;
				while (*m) // scan menu data for menus
				{
					if (menus==menu)
						itemx=menux;
					menux+=session_ui_printasciz(m,ox+menux,oy+0,1,-1,1,0,menus==menu?-1:0);
					int i=0,j=0,k=0;
					while (*m++) // skip menu name
						++j;
					int l; while (l=*m++,l|=*m++,l) // skip menu items
					{
						++k; i=0;
						while (*m++) // skip item name
							++i;
						if (j<i)
							j=i;
					}
					if (menus==menu)
					{
						itemw=j,items=k;
						if (itemx+itemw>SESSION_UI_MAXX+2) // too wide?
							itemw=SESSION_UI_MAXX+2-itemx; // clip!
						if ((itemh=SESSION_UI_MAXY-SESSION_UI_GAP)>items)
							itemh=items; // avoid vertical overrun!
					}
					menuxs[menus++]=menux;
				}
				//session_ui_drawborder(ox,oy,menux,SESSION_UI_GAP);
				session_drawborder(ox*8,oy*SESSION_UI_HEIGHT,menux*8,SESSION_UI_GAP*SESSION_UI_HEIGHT-4);
				session_ui_drawborder(itemx+ox,SESSION_UI_GAP+oy,itemw+2,itemh);
				session_ui_setstyle(0);
			}
			if (itemz!=item)
			{
				if (item<0)
					item=0;
				else if (item>items-1)
					item=items-1;
				itemz=item;
				if (itemy>item)
					itemy=item; // autoscroll
				else if (itemy<item-itemh+1)
					itemy=item-itemh+1; // autoscroll
				if (itemy>items-itemh)
					itemy=items-itemh; // keep within bounds
				else if (itemy<0)
					itemy=0; // keep within bounds
				q=1; int i=0; BYTE *m=session_ui_menudata;
				while (*m) // scan menu data for items
				{
					while (*m++) {} // skip menu name
					int j,k=0;
					if (i==menu)
						zz=(char*)m; // avoid a fastidious warning
					while (j=*m++<<8,j+=*m++,j)
					{
						if (i==menu)
						{
							if (item==k)
								z=(char*)&m[-2],session_event=j; // avoid another fastidious warning
							if (k>=itemy&&k<itemy+itemh)
							{
								session_ui_printasciz(m,itemx+ox+0,SESSION_UI_GAP+oy+k-itemy,1,itemw,1,0,item==k?-1:+0);
								if (j==0X8000) // empty item? separator!
									session_ui_middleline(itemx+ox,SESSION_UI_GAP+oy+k-itemy,itemw+2);
							}
							events[k++]=j;
						}
						while (*m++) {} // skip item name
					}
					++i;
				}
			}
			if (q)
				session_redraw(q=0);
			session_ui_focus=1; // not too useful, disable it if you wish
			switch (session_ui_exchange())
			{
				case KBCODE_NEXT:
					++itemy,itemz=-1;
					break;
				case KBCODE_PRIOR:
					--itemy,itemz=-1;
					break;
				case KBCODE_UP:
					if (--item<0)
				case KBCODE_END:
						item=items-1;
					break;
				case KBCODE_DOWN:
					if (++item>=items)
				case KBCODE_HOME:
						item=0;
					break;
				case KBCODE_LEFT:
					if (--menu<0)
						menu=menus-1;
					break;
				case KBCODE_RIGHT:
					if (++menu>=menus)
						menu=0;
					break;
				case KBCODE_ESCAPE:
				case KBCODE_F10:
					done=-1;
					break;
				case KBCODE_X_ENTER:
				case KBCODE_ENTER:
					//if (session_event!=0X8000) // gap?
						done=1;
					break;
				case -1: // mouse move
					if (session_ui_maus_y<SESSION_UI_GAP) // select menu?
					{
						if (session_ui_maus_x>=0&&session_ui_maus_x<session_ui_menusize)
							for (menu=0;session_ui_maus_x>=menuxs[menu];++menu) {}
					}
					else // select item?
						if (session_ui_maus_y<itemh+SESSION_UI_GAP&&session_ui_maus_x>=itemx&&session_ui_maus_x<itemx+itemw+2)
							item=session_ui_maus_y-SESSION_UI_GAP+itemy; // hover, not click!
					break;
				case -3: // mouse right click
				case -2: // mouse left click
					if (session_ui_maus_y<SESSION_UI_GAP&&session_ui_maus_x>=0&&session_ui_maus_x<session_ui_menusize) // select menu?
						; // already done above
					else if (session_ui_maus_y>=SESSION_UI_GAP&&session_ui_maus_y<itemh+SESSION_UI_GAP&&session_ui_maus_x>=itemx&&session_ui_maus_x<itemx+itemw+2) // select item?
						session_event=events[session_ui_maus_y-SESSION_UI_GAP+itemy],done=1;
					else // quit?
						session_event=0,done=1;
					break;
				default:
					if (session_ui_char>=32)
						for (int o=ucase(session_ui_char),n=items;n;--n)
						{
							++z,++z; // skip ID
							while (*z++) {} // skip text
							if (++item>=items)
								item=0,z=zz;
							if (ucase(z[4])==o)
								break;
						}
			}
			session_ui_focus=0;
			if (menuz!=menu)
				session_ui_loop(); // redrawing the items doesn't need any wiping, unlike the menus
		}
		while (!done);
	}
	while (session_event==0X8000&&done>0); // empty menu items must be ignored, unless we're quitting
	session_ui_exit();
	if (done<0)
		session_shift=session_event=0; // quit!
	else
		session_shift=!!(session_event&0X4000),session_event&=0XBFFF;
	return;
}

void session_ui_textinit(char *s,char *t,char q) // used by session_ui_text and session_ui_scan
{
	session_ui_init();
	int i,j,textw=0,texth=SESSION_UI_GAP+2; // caption + blank + text + blank
	char *m=t;
	while (*m++)
		++textw;
	i=0;
	m=s;
	while (*m) // get full text width
	{
		if (*m=='\n')
		{
			++texth;
			if (textw<i)
				textw=i;
			i=0;
		}
		else if (*m=='\t')
			i=(i|7)+1;
		else
			++i;
		++m;
	}
	if (textw<i)
		textw=i;
	if (i)
		++texth;
	if (q)
		textw+=q=6; // 6: four characters for the icon, plus one extra char per side
	if (textw>SESSION_UI_MAXX)
		textw=SESSION_UI_MAXX; // avoid horizontal overruns!
	if (texth>SESSION_UI_MAXY)
		texth=SESSION_UI_MAXY; // avoid vertical overruns!
	textw+=2; // include left+right margins
	int textx=((VIDEO_PIXELS_X/8)-textw)/2,texty=((VIDEO_PIXELS_Y/SESSION_UI_HEIGHT)-texth)/2;
	session_ui_drawborder(session_ui_base_x=textx,session_ui_base_y=texty,session_ui_size_x=textw,session_ui_size_y=texth);
	textw-=2;
	session_ui_drawcaption(t,textx,texty,textw);
	j=0; i=SESSION_UI_GAP;
	m=session_parmtr;
	session_ui_printasciz("",textx+q,texty+i++,1,textw-q,1,0,+0); // top blank line
	--texth; while (*s&&i<texth) // render text proper
	{
		int k=(*m++=*s++);
		++j; // tabulation
		if (k=='\n')
		{
			m[-1]=j=0;
			session_ui_printasciz(m=session_parmtr,textx+q,texty+i++,1,textw-q,1,0,+0);
		}
		else if (k=='\t')
		{
			m[-1]=' ';
			while (j&7) // tab = 8 spc
				++j,*m++=' ';
		}
	}
	++texth; if (m!=session_parmtr) // last line lacks a line feed?
	{
		*m=0; int l=session_ui_center?(textw-q-utf8len(session_parmtr))/2:0;
		session_ui_printasciz(m=session_parmtr,textx+q,texty+i++,l+1,textw-q-l,1,0,+0);
	}
	session_ui_printasciz("",textx+q,texty+i++,1,textw-q,1,0,+0); // bottom blank line
	if (q) // draw icon?
	{
		session_ui_fillboth(textx,texty+SESSION_UI_GAP,q,texth-SESSION_UI_GAP,SESSION_UI_033,SESSION_UI_067);
		//for (int z=0;z<q;++z) session_ui_fillrect(textx+z,texty+1,1,texth-1,0X010101*((0XFF*z+0XC0*(q-z)+q/2)/q)); // gradient!
		// the SDL2 blit functions won't work here -- the target isn't a surface but a texture
		//SDL_Rect r; r.h=r.w=32; r.y=(texty+2)*SESSION_UI_HEIGHT*VIDEO_PIXELS_X; r.x=(textx+1)*8; SDL_BlitSurface(session_ui_icon,NULL,session_gui,&r);
		VIDEO_UNIT *tgt=&menus_frame[(texty+SESSION_UI_GAP+1)*SESSION_UI_HEIGHT*VIDEO_PIXELS_X+(textx+1)*8];
		for (int z=0,y=0;y<32;++y,tgt+=VIDEO_PIXELS_X-32)
			for (int x=0;x<32;++tgt,++z,++x)
			#if 0 // full alpha
			{
				int o=*tgt,a=((i=session_icon32xx16[z])>>12)*17; if (a>=128) ++a;
				*tgt=(((((i&0XF00)*0X1100+(i&0XF)*0X11)*a+(o&0XFF00FF)*(256-a)+0X800080)>>8)&0XFF00FF)
					+((((i&0XF0)*0X110*a+(o&0XFF00)*(256-a)+0X8000)>>8)&0XFF00);
			}
			#else // easy alpha
				if ((i=session_icon32xx16[z])&0X8000)
					*tgt=(i&0XF00)*0X1100+(i&0XF0)*0X110+(i&0XF)*0X11;
			#endif
	}
	session_redraw(0);
}

int session_ui_text(char *s,char *t,char q) // see session_message
{
	if (!s||!t) return -1;
	session_ui_textinit(s,t,q); // includes session_ui_init
	for (;;)
		switch (session_ui_exchange())
		{
			case -3: // mouse right click
			case -2: // mouse left click
				//if (session_ui_maus_x>=0&&session_ui_maus_x<session_ui_size_x&&session_ui_maus_y>=0&&session_ui_maus_y<session_ui_size_y) break; // click anywhere, it doesn't matter
			case KBCODE_ESCAPE:
			case KBCODE_SPACE:
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				session_ui_exit();
				return 0;
		}
}

int session_ui_line(char *t) // see session_line
{
	int i=0,j=strlen(session_parmtr),q,dirty=1,textw=SESSION_UI_MAXX;
	if ((q=utf8len(session_parmtr))>=textw) return -1; // error!

	session_ui_init();
	int textx=((VIDEO_PIXELS_X/8)-(session_ui_size_x=textw+2))/2,texty=((VIDEO_PIXELS_Y/SESSION_UI_HEIGHT)-(session_ui_size_y=SESSION_UI_GAP+1))/2; // add left+right margins
	session_ui_drawborder(session_ui_base_x=textx,session_ui_base_y=texty,session_ui_size_x,session_ui_size_y);
	session_ui_drawcaption(t,textx,texty,textw);
	int done=0; do
	{
		if (dirty)
		{
			if (q)
				session_ui_printasciz(session_parmtr,textx+0,texty+SESSION_UI_GAP,1,textw,1,0,j); // the whole string is selected
			else if (i<j)
				session_ui_printasciz(session_parmtr,textx+0,texty+SESSION_UI_GAP,1,textw,1,i,i+utf8add(&session_parmtr[i],+1)); // the cursor is an inverse char
			else
			{
				session_parmtr[i]=' '; session_parmtr[i+1]=0; // the cursor is actually an inverse space at the end
				session_ui_printasciz(session_parmtr,textx+0,texty+SESSION_UI_GAP,1,textw,1,i,i+1);
				session_parmtr[i]=0; // end of string
			}
			session_redraw(0);
			dirty=0;
		}
		switch (dirty=session_ui_exchange())
		{
			case KBCODE_LEFT:
				if (q||i<=0||(i+=utf8add(&session_parmtr[i],-1))<0)
			case KBCODE_HOME:
					i=0;
				q=0;
				break;
			case KBCODE_RIGHT:
				if (q||i>=j||(i+=utf8add(&session_parmtr[i],+1))>j)
			case KBCODE_END:
					i=j;
				q=0;
				break;
			case -3: // mouse right click
			case -2: // mouse left click
				if (session_ui_maus_x<0||session_ui_maus_x>=session_ui_size_x||session_ui_maus_y<0||session_ui_maus_y>=session_ui_size_y)
					j=-1,done=1; // quit!
				else if (session_ui_maus_y==SESSION_UI_GAP)
				{
					q=0; if (session_ui_maus_x>j)
						i=j;
					else if (session_ui_maus_x<=1||(i=utf8add(session_parmtr,session_ui_maus_x-1))<0)
						i=0;
					else if (i>j)
						i=j;
				}
				break;
			case KBCODE_ESCAPE:
				j=-1;
				//break;
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				done=1;
				break;
			case KBCODE_BKSPACE:
			case KBCODE_DELETE:
				if (q) // erase all?
					*session_parmtr=i=j=q=0;
				else if (dirty==KBCODE_BKSPACE?i>0:i<j)
				{
					int o=i; if (dirty==KBCODE_BKSPACE) i+=utf8add(&session_parmtr[i],-1); else o+=utf8add(&session_parmtr[i],+1);
					memmove(&session_parmtr[i],&session_parmtr[o],j-i+1); j+=i-o;
				}
				break;
			default:
				if (dirty=(session_ui_char>=32&&session_ui_char<ONSCREEN_CEIL))
				{
					int o=utf8chk(session_ui_char); if (q)
					{
						char *z=session_parmtr; utf8put(&z,session_ui_char);
						*z=q=0; i=j=o;
					}
					#ifdef SDL2_UTF8
					else if (dirty=(utf8len(session_parmtr)<textw-1))
					#else
					else if (dirty=(j<textw-1))
					#endif
					{
						memmove(&session_parmtr[i+o],&session_parmtr[i],j-i+1);
						char *z=&session_parmtr[i]; utf8put(&z,session_ui_char);
						i+=o,j+=o;
					}
				}
		}
	}
	while (!done);
	session_ui_exit();
	return j;
}

int session_ui_list(int item,char *s,char *t,void x(void),int q) // see session_list
{
	if (!*s) return -1; // empty!
	int i,dirty=1,dblclk=0,items=0,itemz=-2,listw=0,listh,listz=0,scrollx=0,scrolly=0;
	char *m=t,*z=NULL;
	while (utf8get((const char**)&m))
		++listw; // title width
	m=s;
	while (*m) // walk list for item number and width
	{
		++items; i=0;
		while (utf8get((const char**)&m)) ++i;
		if (listw<i) listw=i;
	}
	//if (!items) return -1;
	session_ui_init();
	if ((listh=items+SESSION_UI_GAP)>SESSION_UI_MAXY) // h+caption
		listw+=scrollx=2, // w+scrollbar
		listh=SESSION_UI_MAXY;
	if (listw>SESSION_UI_MAXX)
		listw=SESSION_UI_MAXX;
	int listx=((VIDEO_PIXELS_X/8)-listw-2)/2,listy=((VIDEO_PIXELS_Y/SESSION_UI_HEIGHT)-listh)/2;
	if (scrollx) // list is long enough?
		if (item>=listh/2) // go to center?
			if ((listz=item-listh/2+SESSION_UI_GAP)>items-listh) // too deep?
				listz=items-listh+SESSION_UI_GAP;
	int done=0; do
	{
		if (dirty) // showing x() must request a redraw!
		{
			session_ui_drawborder(session_ui_base_x=listx,session_ui_base_y=listy,session_ui_size_x=listw+2,session_ui_size_y=listh);
			session_ui_drawcaption(t,listx,listy,listw);
			dirty=0; itemz=~item;
			if (scrollx) // scrollbar?
			{
				int zzz=session_glyph_q0; session_glyph_q0=SESSION_UI_067;
				session_ui_printasciz("\030\031",listx+listw,listy+SESSION_UI_GAP+                     0,0,2,0,0,0); // arrow up
				session_ui_printasciz("\032\033",listx+listw,listy+SESSION_UI_GAP+listh-SESSION_UI_GAP-1,0,2,0,0,0); // arrow down
				session_glyph_q0=zzz;
			}
		}
		if (itemz!=item)
		{
			itemz=item;
			if (listz>item)
				if ((listz=item)<0)
					listz=0; // allow index -1
			if (listz<item-listh+SESSION_UI_GAP+1)
				listz=item-listh+SESSION_UI_GAP+1;
			m=s; i=0;
			while (*m&&i<listz+listh-SESSION_UI_GAP)
			{
				if (i>=listz)
				{
					if (i==item)
						z=m;
					session_ui_printasciz(m,listx+0,listy+SESSION_UI_GAP+i-listz,1,listw-scrollx,1,0,i==item?-1:0);
				}
				if (*m) while (*m++) {} // next item
				++i;
			}
			if (scrollx) // scrollbar?
			{
				session_ui_fillboth(listx+listw,listy+SESSION_UI_GAP+1,2,listh-SESSION_UI_GAP-2,SESSION_UI_067,SESSION_UI_100); // bkground
				session_ui_fillboth(listx+listw,listy+
					(scrolly=SESSION_UI_GAP+1+item*(listh-SESSION_UI_GAP-2)/items),2,1,SESSION_UI_033,SESSION_UI_000); // active cursor
			}
			session_redraw(0);
		}
		switch (session_ui_exchange())
		{
			case KBCODE_PRIOR:
			case KBCODE_LEFT:
				item-=listh-SESSION_UI_GAP-2; // no `break`!
			case KBCODE_UP:
				if (--item<0)
			case KBCODE_HOME:
					item=0;
				dblclk=0;
				break;
			case KBCODE_NEXT:
			case KBCODE_RIGHT:
				if (item<0) item=0; // catch particular case
				item+=listh-SESSION_UI_GAP-2; // no `break`!
			case KBCODE_DOWN:
				if (++item>=items)
			case KBCODE_END:
					item=items-1;
				dblclk=0;
				break;
			case KBCODE_TAB:
				if (q) // browse items if `q` is true
				{
					if (session_ui_shift)
					{
						if (--item<0) // prev?
							item=items-1; // wrap
					}
					else
					{
						if (++item>=items) // next?
							item=0; // wrap
					}
				}
				else if (x) // special context widget
					x(),dblclk=0,dirty=1;
				break;
			case -1: // mouse move
				if (q&&session_ui_maus_x>=0&&session_ui_maus_x<session_ui_size_x&&session_ui_maus_y>=SESSION_UI_GAP&&session_ui_maus_y<items+SESSION_UI_GAP) // single click mode?
					item=session_ui_maus_y-SESSION_UI_GAP;
				break;
			case -3: // mouse right click
				if (x&&session_ui_maus_x>=0&&session_ui_maus_x<session_ui_size_x&&session_ui_maus_y>=0&&session_ui_maus_y<session_ui_size_y)
					x(),dblclk=0,dirty=1;
				else // no `break`!
			case -2: // mouse left click
				if (session_ui_maus_x<0||session_ui_maus_x>=session_ui_size_x||(!scrollx&&(session_ui_maus_y<0||session_ui_maus_y>=session_ui_size_y)))
					item=-1,done=1; // quit!
				else if (session_ui_maus_y<SESSION_UI_GAP) // scroll up?
				{
					dblclk=0; if (session_ui_maus_x>=session_ui_size_x-scrollx||(item-=listh-SESSION_UI_GAP-1)<0) item=0;
				}
				else if (session_ui_maus_y>=session_ui_size_y) // scroll down?
				{
					dblclk=0; if (session_ui_maus_x>=session_ui_size_x-scrollx||(item+=listh-SESSION_UI_GAP-1)>=items) item=items-1;
				}
				else if (session_ui_maus_x>=session_ui_size_x-scrollx) // scrollbar?
				{
					dblclk=0; if (session_ui_maus_y<scrolly) { if ((item-=listh-SESSION_UI_GAP-1)<0) item=0; }
					else if (session_ui_maus_y>scrolly) { if ((item+=listh-SESSION_UI_GAP-1)>=items) item=items-1; }
				}
				else // select an item?
				{
					int button=listz+session_ui_maus_y-SESSION_UI_GAP;
					if (q||(item==button&&dblclk)) // `q` (single click mode, f.e. "YES"/"NO") or same item? (=double click)
						item=button,done=1;
					else // different item!
					{
						item=button; dblclk=1;
						if (item<0)
							item=0;
						else if (item>=items)
							item=items-1;
					}
				}
				break;
			case KBCODE_ESCAPE:
			case KBCODE_F10:
				item=-1,done=1;
				break;
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				if (item>=0) done=1; // ignore OK if no item is selected
				break;
			default:
				if (session_ui_char>=32)
				{
					int itemo=item,n=items;
					for (int o=ucase(session_ui_char);n;--n)
					{
						if (item<0)
							item=0,z=s;
						else
						{
							++item;
							while (*z++) {} // next item
						}
						if (!(*z))
							item=0,z=s;
						if (ucase(*z)==o)
							break;
					}
					if (!n)
						item=itemo; // allow rewinding to -1
					dblclk=0;
				}
		}
	}
	while (!done);
	if (item>=0)
		strcpy(session_parmtr,z);
	session_ui_exit();
	return item;
}

int session_ui_redefine(void) // wait for a key, even if it isn't a character
{
	for (SDL_Event event;SDL_WaitEvent(&event);) // custom message handler
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				if (event.window.event==SDL_WINDOWEVENT_EXPOSED)
					session_reveal(); // fast redraw
				break;
			case SDL_MOUSEBUTTONUP: // better than SDL_MOUSEBUTTONDOWN
				session_ui_maus_x=(event.button.x-session_ideal.x)*VIDEO_PIXELS_X/session_ideal.w/8-session_ui_base_x,session_ui_maus_y=(event.button.y-session_ideal.y)*VIDEO_PIXELS_Y/session_ideal.h/SESSION_UI_HEIGHT-session_ui_base_y;
				return session_ui_maus_x>=0&&session_ui_maus_x<session_ui_size_x&&session_ui_maus_y>=0&&session_ui_maus_y<session_ui_size_y?0:-1;
			case SDL_KEYDOWN:
				session_ui_char0=1; // we do NOT handle SDL_TEXTINPUT here!!!
				if ((event.key.keysym.scancode>=KBCODE_F1&&event.key.keysym.scancode<=KBCODE_F12)||
					(event.key.keysym.mod&(KMOD_CTRL|KMOD_SHIFT|KMOD_ALT|KMOD_GUI))) break; // reject!
				return event.key.keysym.scancode==KBCODE_ESCAPE?-1:event.key.keysym.scancode; // escape/return
			case SDL_QUIT:
				return -1;
		}
	return 0; // ignore event breakdown
}
int session_ui_scan(char *s) // see session_scan
{
	if (!s) return -1;
	session_ui_center=1;
	session_ui_textinit(s,txt_session_scan,0); // includes session_ui_init
	session_ui_center=0;
	int i; while (!(i=session_ui_redefine())) {}
	session_ui_exit(); return i;
}

int multiglobbing(const char *w,char *t,int q); // multi-pattern globbing; must be defined later on!
int sortedinsert(char *t,int z,const char *s); // ordered list inserting; must be defined later on!
int sortedsearch(char *t,int z,const char *s); // ordered list searching; must be defined later on!
int session_ui_fileflags; // see below for the `readonly` flag and others
char *session_ui_filedialog_sanitize(char *s) // restore PATHCHAR at the end of a path name
{
	if (s&&*s)
	{
		char *t=s;
		while (*t)
			++t;
		if (t[-1]!=PATHCHAR)
			*t=PATHCHAR,t[1]=0; // turn "C:\ABC" into "C:\ABC\"
	}
	return s;
}
void session_ui_filedialog_tabkey(void)
{
	char *l={"Read/Write\000Read-Only\000"};
	int i=session_ui_list(session_ui_fileflags&1,l,"File access",NULL,1);
	if (i>=0) session_ui_fileflags=i;
}
int getftype(const char *s) // <0 = invalid path / nothing, 0 = file, >0 = directory
{
	#ifdef _WIN32
	int i; return (!s||!*s)?-1:(i=GetFileAttributes(s))>0?i&FILE_ATTRIBUTE_DIRECTORY:i;
	#else
	struct stat a; int i; return (!s||!*s)?-1:(i=stat(s,&a))>=0?S_ISDIR(a.st_mode):i;
	#endif
}
int session_ui_filedialog(char *r,char *s,char *t,int q,int f) // see session_filedialog; q here means TAB is available or not, and f means the starting TAB value (q) or whether we're reading (!q)
{
	if (!*s) return 0; // empty!
	int i; char *m,*n,pastname[STRMAX],pastfile[STRMAX],basefind[STRMAX];
	session_ui_fileflags=f;
	if (!r) r=session_path; // NULL path = default!
	if (m=strrchr(r,PATHCHAR))
		i=m[1],m[1]=0,strcpy(basepath,r),m[1]=i,strcpy(pastname,&m[1]); // it's important to restore `r`: 1.- it can be `session_path`; 2.- the caller may need it
	else
		*basepath=0,strcpy(pastname,r);
	strcpy(pastfile,pastname); // this will stick till the user accepts or cancels the dialog; see below
	for (;;)
	{
		int j=-1,k=i=0; char *half=m=session_scratch; // the list doesn't mix folders and files, so we must search the past name in "steps"

		#ifdef _WIN32
		const char prevpath[]="..\\"; // ".." + PATHCHAR
		if (getftype(basepath)<=0) // ensure that basepath exists and is a valid directory
		{
			GetCurrentDirectory(STRMAX,basepath); // fall back to current directory
			session_ui_filedialog_sanitize(basepath);
		}
		for (session_ui_drive[0]='A';session_ui_drive[0]<='Z';++session_ui_drive[0])
			if (session_ui_drives&(1<<(session_ui_drive[0]-'A')))
				++i,m+=sortedinsert(m,0,session_ui_drive); // append, already sorted
		j=sortedsearch(half,m-half,pastname); // j and k are still -1 and 0 here
		half=m,k=i;
		if (basepath[3]) // not root directory?
		{
			if (j<0&&!strcmp(pastname,prevpath)) j=i; // search parent link
			++i,m+=sortedinsert(m,0,prevpath);
		}
		WIN32_FIND_DATA wfd; HANDLE h; strcpy(basefind,basepath); strcat(basefind,"*");
		if ((h=FindFirstFile(basefind,&wfd))!=INVALID_HANDLE_VALUE)
		{
			do
				if (!(wfd.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN)&&(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) // reject invisible directories
					if ((n=wfd.cFileName)[0]!='.'||(n[1]&&(n[1]!='.'||n[2]))) // reject "." and ".."
						++i,m=&half[sortedinsert(half,m-half,session_ui_filedialog_sanitize(n))]; // add directory name plus separator
			while (FindNextFile(h,&wfd)&&m-(char*)session_scratch<sizeof(session_scratch)-STRMAX);
			FindClose(h);
		}
		if (j<0) if ((j=k+sortedsearch(half,m-half,pastname))<k) j=-1; // search directories
		half=m,k=i;
		if ((h=FindFirstFile(basefind,&wfd))!=INVALID_HANDLE_VALUE)
		{
			do
				if (!(wfd.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN)&&!(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) // reject invisible files
					if (multiglobbing(s,n=wfd.cFileName,1)) // add file name
						++i,m=&half[sortedinsert(half,m-half,n)];
			while (FindNextFile(h,&wfd)&&m-(char*)session_scratch<sizeof(session_scratch)-STRMAX);
			FindClose(h);
		}
		#else
		const char prevpath[]="../"; // ".." + PATHCHAR
		DIR *d; struct dirent *e;
		if (getftype(basepath)<=0) // ensure that basepath exists and is a valid directory
		{
			if (getcwd(basepath,STRMAX)) // fall back to current directory
				session_ui_filedialog_sanitize(basepath);
		}
		if (basepath[1]) // not root directory?
		{
			if (!strcmp(pastname,prevpath)) j=i; // j and k are still -1 and 0 here
			++i,m+=sortedinsert(m,0,prevpath);
		}
		half=m,k=i;
		if (d=opendir(basepath))
		{
			while ((e=readdir(d))&&m-(char*)session_scratch<sizeof(session_scratch)-STRMAX)
				if (n=e->d_name,*n!='.') // reject ".", ".." and invisible entries (".*")
					if (getftype(strcat(strcpy(basefind,basepath),n))>0) // add directory name plus separator
						++i,m=&half[sortedinsert(half,m-half,session_ui_filedialog_sanitize(n))];
			closedir(d);
		}
		if (j<0) if ((j=k+sortedsearch(half,m-half,pastname))<k) j=-1; // search directories
		half=m,k=i;
		if (d=opendir(basepath))
		{
			while ((e=readdir(d))&&m-(char*)session_scratch<sizeof(session_scratch)-STRMAX)
				if (n=e->d_name,*n!='.'&&multiglobbing(s,n,1)) // reject ".*" entries again
					if (!getftype(strcat(strcpy(basefind,basepath),n))) // add file name
						++i,m=&half[sortedinsert(half,m-half,n)];
			closedir(d);
		}
		#endif

		if (j<0) if ((j=k+sortedsearch(half,m-half,pastname))<k) j=-1; // search files
		if (!q&&!f)
		{
			m=&m[sortedinsert(m,0,"** NEW **")]; // point at '*' by default on SAVE, rather than on any past names...
			if (!*pastname) j=i; // ...but only if we weren't browsing the file system
		}
		if (q||f||*pastname) i=j; // point to the entry that fits the previous name
		*m=0; // end of list

		sprintf(session_tmpstr,"%s -- %s",t,basepath); //,s
		if (session_ui_list(i,session_scratch,session_tmpstr,q?session_ui_filedialog_tabkey:NULL,0)<0)
			return 0; // user closed the dialog!

		m=session_parmtr;
		while (*m)
			++m;
		if (m[-1]==PATHCHAR) // the user chose a directory
		{
			strcat(strcpy(session_scratch,basepath),session_parmtr);
			#ifdef _WIN32
			if (session_parmtr[1]==':') // the user chose a drive root directory
				strcpy(pastname,strcpy(basepath,session_parmtr)); // return to itself (what else?)
			else
			#endif
			{
				if (!strcmp(session_parmtr,strcpy(pastname,prevpath)))
				{
					if (m=strrchr(basepath,PATHCHAR))
					{
						while (m!=basepath&&*--m!=PATHCHAR) {}
						strcpy(pastname,&m[1]); // allow returning to previous directory
					}
					else *pastname=0; // nowhere to return
				}
				session_ui_filedialog_sanitize(makebasepath(session_scratch)); // can this ever fail?
			}
		}
		else // the user chose a file
		{
			if (*session_parmtr=='*') // the user wants to create a file
			{
				strcpy(session_parmtr,pastfile); // reuse previous name if possible
				if (session_ui_line(t)>0)
				{
					if (!multiglobbing(s,session_parmtr,1)) // unknown extension?
					{
						m=session_parmtr; n=s;
						while (*m) // go to end of target string
							++m;
						while (*n!='*') // look for asterisks in source
							++n;
						while (*n=='*') // skip all asterisks
							++n;
						while (*n&&*n!=';') // copy till end of extension
							*m++=*n++;
						*m=0; // and so we append the default extension
					}
				}
				else
					return 0; // failure, quit!
			}
			strcat(strcpy(session_scratch,basepath),session_parmtr); // build full name
			strcpy(session_parmtr,session_scratch); // copy to target, but keep the source...
			if (q||f||getftype(session_parmtr)<0)
				return 1; // the user successfully chose a file, either extant for reading or new for writing!
			if (session_ui_list(0,"YES\000NO\000","Overwrite?",NULL,1)==0)
				return strcpy(session_parmtr,session_scratch),1; // ...otherwise session_parmtr would hold "YES"!
		}
	}
}

// create, handle and destroy session ------------------------------- //

INLINE char *session_create(char *s) // create video+audio devices and set menu; NULL OK, *char ERROR!
{
	int i; SDL_version sdl_version; SDL_SetMainReady();
	SDL_GetVersion(&sdl_version); sprintf(session_version,"%d.%d.%d",sdl_version.major,sdl_version.minor,sdl_version.patch);
	if (SDL_Init(SDL_INIT_EVENTS|SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER)<0)
		return (char*)SDL_GetError();
	if (!(session_hwnd=SDL_CreateWindow(NULL,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,0))|| // SDL_WINDOWPOS_CENTERED is wonderful :-)
		!(video_blend=malloc(sizeof(VIDEO_UNIT[16+(VIDEO_PIXELS_Y>>!!VIDEO_HALFBLEND)*VIDEO_PIXELS_X])))
		) return SDL_Quit(),(char*)SDL_GetError();

	if (!session_softblit&&(session_blitter=SDL_CreateRenderer(session_hwnd,-1,0))) // ...SDL_CreateRenderer(session_hwnd,-1,SDL_RENDERER_SOFTWARE)
	{
		session_hardblit=1;
		SDL_SetRenderTarget(session_blitter,NULL); // necessary?
		// ARGB8888 equates to masks A = 0XFF000000, R = 0X00FF0000, G = 0X0000FF00, B = 0X000000FF ; it provides the best performance AFAIK.
		session_dib=SDL_CreateTexture(session_blitter,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,VIDEO_LENGTH_X,VIDEO_LENGTH_Y);
		session_gui=SDL_CreateTexture(session_blitter,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,VIDEO_PIXELS_X,VIDEO_PIXELS_Y);
		session_dbg=SDL_CreateTexture(session_blitter,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,VIDEO_PIXELS_X,VIDEO_PIXELS_Y);
		SDL_SetTextureBlendMode(session_dib,SDL_BLENDMODE_NONE);
		SDL_SetTextureBlendMode(session_gui,SDL_BLENDMODE_NONE);
		SDL_SetTextureBlendMode(session_dbg,SDL_BLENDMODE_NONE);
		SDL_LockTexture(session_dib,NULL,(void*)&video_frame,&i); // pitch must always equal VIDEO_LENGTH_X*4 !!!
		//if (i!=4*VIDEO_LENGTH_X) return SDL_Quit(),"pitch mismatch"; // can this ever fail!?
		SDL_LockTexture(session_gui,NULL,(void*)&menus_frame,&i); // ditto, pitch must always equal VIDEO_PIXELS_X*4 !!!
		SDL_LockTexture(session_dbg,NULL,(void*)&debug_frame,&i);
	}
	else //if (1)
	{
		session_hardblit=0;
		session_dib=(SDL_Texture*)SDL_CreateRGBSurface(0,VIDEO_LENGTH_X,VIDEO_LENGTH_Y,32,0xFF0000,0x00FF00,0x0000FF,0);
		session_gui=(SDL_Texture*)SDL_CreateRGBSurface(0,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,32,0xFF0000,0x00FF00,0x0000FF,0);
		session_dbg=(SDL_Texture*)SDL_CreateRGBSurface(0,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,32,0xFF0000,0x00FF00,0x0000FF,0);
		SDL_SetSurfaceBlendMode((SDL_Surface*)session_dib,SDL_BLENDMODE_NONE);
		SDL_SetSurfaceBlendMode((SDL_Surface*)session_gui,SDL_BLENDMODE_NONE);
		SDL_SetSurfaceBlendMode((SDL_Surface*)session_dbg,SDL_BLENDMODE_NONE);
		video_frame=((SDL_Surface*)session_dib)->pixels;
		menus_frame=((SDL_Surface*)session_gui)->pixels;
		debug_frame=((SDL_Surface*)session_dbg)->pixels;
	}
	//else return SDL_Quit(),(char*)SDL_GetError(); // give up if neither hard or soft blit cannot be allocated!

	#ifdef _WIN32
	DWORD z;
	for (session_ui_drive[0]='A';session_ui_drive[0]<='Z';++session_ui_drive[0]) // scan session_ui_drive just once
		if (GetDriveType(session_ui_drive)>2&&GetDiskFreeSpace(session_ui_drive,&z,&z,&z,&z))
			session_ui_drives|=1<<(session_ui_drive[0]-'A');
	#endif
	SDL_SetWindowIcon(session_hwnd,(session_ui_icon=SDL_CreateRGBSurfaceFrom(session_icon32xx16,32,32,16,32*2,0XF00,0XF0,0XF,0XF000))); // ARGB4444
	SDL_StartTextInput();

	// translate menu data into internal format
	BYTE *t=session_ui_menudata; session_ui_menusize=0;
	while (*s)
	{
		// scan and generate menu header
		while ((*t++=*s++)!='\n')
			++session_ui_menusize;
		session_ui_menusize+=2;
		t[-1]=0;
		// first scan: maximum item width
		char *r=s,mxx=0;
		for (;;)
		{
			if (*r=='=') // separator?
				while (*r++!='\n') {}
			else if (*r=='0') // menu item?
			{
				while (*r++!=' ') {}
				#if 1 // shortcuts aligned to the right; more packed
				char n=0; while (*r++!='\n') ++n;
				#else // shortcuts aligned to the left; wider spaces
				char n=0; while (*r++>=' ') ++n;
				#endif
				if (mxx<n) mxx=n;
				--r; while (*r++!='\n') {}
			}
			else break;
		}
		// second scan: generate items
		for (;;)
			if (*s=='=') // separator?
			{
				*t++=0X80; *t++=0;*t++=0; // 0X8000 is an empty menu item (menus cannot generate the DROPFILE event)
				while (*s++!='\n') {}
			}
			else if (*s=='0') // menu item?
			{
				i=strtol(s,&s,0); // allow either hexa or decimal
				*t++=i>>8; *t++=i; *t++=' ';*t++=' ';
				++s; int h=-2; // spaces between body and shortcut
				while ((i=(*t++=*s++))!='\n')
					if (i=='\t') // keyboard shortcut?
					{
						--t; // replace tab with spaces...
						#if 1 // shortcuts aligned to the right; more packed
						int l=0; while (*s!='\n') ++l,++s; s-=l; for (int n=h;n<mxx-l;++n) *t++=' '; // pad with spaces
						#else // shortcuts aligned to the left; wider spaces
						for (int n=h;n<mxx;++n) *t++=' '; // pad with spaces
						#endif
					}
					else if (i=='_') // string shortcut?
						--t;
					else
						++h;
				t[-1]=0;
			}
			else break;
		*t++=0;*t++=0;
	}
	*t=0;

	// cleanup, joystick and sound
	session_ui_makechrs();
	if (session_hidemenu) // user interface style
	{
		i=SESSION_UI_000; SESSION_UI_000=SESSION_UI_100; SESSION_UI_100=i; // inverse palette
		i=SESSION_UI_033; SESSION_UI_033=SESSION_UI_067; SESSION_UI_067=i; // blacks / whites
	}
	if (session_stick)
	{
		i=SDL_NumJoysticks(); cprintf("Detected %d joystick[s]: ",i); // unlike Win32, SDL2 lists the joysticks from last to first
		while (--i>=0&&!((session_pad=SDL_IsGameController(i)),(cprintf("%s #%d '%s' ",
			session_pad?"Controller":"Joystick",i,session_pad?SDL_GameControllerNameForIndex(i):SDL_JoystickNameForIndex(i))),
			session_joy=(session_pad?(void*)SDL_GameControllerOpen(i):(void*)SDL_JoystickOpen(i))))
			; // scan joysticks and game controllers until we run out or one is OK
		session_stick=i>=0; cprintf(session_stick?"Joystick enabled!\n":"No joystick!\n");
	}
	if (session_audio)
	{
		SDL_AudioSpec spec; SDL_zero(spec);
		spec.freq=AUDIO_PLAYBACK; spec.channels=AUDIO_CHANNELS;
		spec.format=AUDIO_BITDEPTH>8?AUDIO_S16SYS:AUDIO_U8;
		//spec.samples=AUDIO_LENGTH_Z*1; // 0 = SDL2 default; users may try 1, 2, 3...
		session_audio=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);
	}
	session_clean(); session_please();
	//session_timer=SDL_GetTicks();
	return NULL;
}
int session_pad2bit(int i) // translate motions and buttons into codes
{
	switch (i)
	{
		case SDL_CONTROLLER_BUTTON_A: // button 0; the order isn't the same used in Win32
		//case 4: // SDL_CONTROLLER_BUTTON_BACK?
			return 16;
		case SDL_CONTROLLER_BUTTON_B: // button 1
		//case 5: // SDL_CONTROLLER_BUTTON_GUIDE?
			return 32;
		case SDL_CONTROLLER_BUTTON_X: // button 2
		//case 6: // SDL_CONTROLLER_BUTTON_START?
			return 64;
		case SDL_CONTROLLER_BUTTON_Y: // button 3
		//case 7: // SDL_CONTROLLER_BUTTON_???
			return 128;
		case SDL_CONTROLLER_BUTTON_DPAD_UP:
			return 1;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
			return 2;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
			return 4;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
			return 8;
		default:
			return 0;
	}
}

INLINE void session_byebye(void) // delete video+audio devices
{
	#if 0 // SDL_Quit cleans everything up
	if (session_joy)
		session_pad?SDL_GameControllerClose(session_joy):SDL_JoystickClose(session_joy);
	if (session_audio)
		SDL_ClearQueuedAudio(session_audio),SDL_CloseAudioDevice(session_audio);
	SDL_StopTextInput();
	if (session_hardblit)
	{
		SDL_UnlockTexture(session_dib);
		SDL_DestroyTexture(session_dib);
		SDL_UnlockTexture(session_gui);
		SDL_DestroyTexture(session_gui);
		SDL_UnlockTexture(session_dbg);
		SDL_DestroyTexture(session_dbg);
		SDL_DestroyRenderer(session_blitter);
	}
	else
	{
		SDL_FreeSurface(session_dib);
		SDL_FreeSurface(session_gui);
		SDL_FreeSurface(session_dbg);
	}
	SDL_DestroyWindow(session_hwnd);
	SDL_FreeSurface(session_ui_icon);
	#endif
	SDL_Quit();
	free(video_blend);
}

// operations summonned by session_listen() and session_update()

#define session_title(s) SDL_SetWindowTitle(session_hwnd,(s)) // set the window caption
#define session_sleep() SDL_WaitEvent(NULL) // sleep till a system event happens
#define session_delay(i) SDL_Delay(i) // wait for approx `i` milliseconds
#define audio_mustsync() ((void)0) // nothing to do here;
#define audio_resyncme() // SDL2 handles the sync for us!
int session_queue(void) // walk message queue: NONZERO = QUIT
{
	int k; for (SDL_Event event;SDL_PollEvent(&event);)
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				if (event.window.event==SDL_WINDOWEVENT_EXPOSED)
					session_clrscr(),session_redraw(1); // clear and redraw
				else if (event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) session_kbdclear(); // loss of focus: no keys!
				break;
			case SDL_MOUSEWHEEL:
				if (event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED) event.wheel.y=-event.wheel.y;
				if (event.wheel.y<0) session_event=debug_xlat(KBCODE_NEXT);
				else if (event.wheel.y) session_event=debug_xlat(KBCODE_PRIOR);
				break;
			case SDL_MOUSEBUTTONUP:
				if (event.button.button==SDL_BUTTON_RIGHT) // better here than in SDL_MOUSEBUTTONDOWN
				{
					session_event=0X8080; // show menu
					break;
				}
				if (event.button.button==SDL_BUTTON_LEFT&&session_signal&SESSION_SIGNAL_DEBUG)
				{
					session_maus_x=((event.button.x-session_r_x)*VIDEO_PIXELS_X+session_r_w/2)/session_r_w; // notice the `button`
					session_maus_y=((event.button.y-session_r_y)*VIDEO_PIXELS_Y+session_r_h/2)/session_r_h; // instead of `motion`!
					session_event=debug_xlat(-1);
					break;
				}
			#ifdef MAUS_EMULATION
			case SDL_MOUSEBUTTONDOWN: // no `break`!
				if (event.button.button==SDL_BUTTON_LEFT) session_maus_z=event.type==SDL_MOUSEBUTTONDOWN;
				break;
			case SDL_MOUSEMOTION:
				session_maus_x=session_r_w>0?((event.motion.x-session_r_x)*VIDEO_PIXELS_X+session_r_w/2)/session_r_w:-1;
				session_maus_y=session_r_h>0?((event.motion.y-session_r_y)*VIDEO_PIXELS_Y+session_r_h/2)/session_r_h:-1;
			#endif
				break;
			case SDL_KEYDOWN:
				session_ui_char0=0; // see SDL_TEXTINPUT
				session_shift=!!(event.key.keysym.mod&KMOD_SHIFT);
				if (event.key.keysym.mod&KMOD_ALT)
				{
					switch (event.key.keysym.sym)
					{
						case SDLK_RETURN: // ALT+RETURN toggles fullscreen
							session_fullblit^=1,session_resize(); break;
						case SDLK_UP: //case SDLK_PLUS: case SDLK_KP_PLUS: // ALT+UP raises the zoom
							if (!session_fullblit&&session_zoomblit<4) ++session_zoomblit,session_resize();
							break;
						case SDLK_DOWN: //case SDLK_MINUS: case SDLK_KP_MINUS: // ALT+DOWN lowers it
							if (!session_fullblit&&session_zoomblit>0) --session_zoomblit,session_resize();
							break;
						case SDLK_LEFT: // ALT+LEFT slows the emulation down
							if (session_rhythm>0) session_wait=session_dirty=1,--session_rhythm; // shall we do session_fast&=~1 too?
							break;
						case SDLK_RIGHT: // ALT+RIGHT speeds it up (fast-forward)
							if (session_rhythm<3) session_wait=session_dirty=1,++session_rhythm; // ditto?
							break;
					}
					break; // reject other ALT-combinations; notice that SDL2 overrides parts of the Win32 ALT+key logic
				}
				if (event.key.keysym.sym==SDLK_F10)
				{
					session_event=0X8080; // pressing F10 (not releasing) shows the popup menu
					break;
				}
				if (session_signal&SESSION_SIGNAL_DEBUG) // only relevant inside debugger, see below
					session_event=debug_xlat(event.key.keysym.scancode);
				if ((k=session_k2joy(event.key.keysym.scancode))<128) // normal key
				{
					if (!(session_signal&SESSION_SIGNAL_DEBUG)) // only relevant outside debugger
						{ if (k<0) joy_kbd|=+1<<(k+8); else kbd_bit_set(k); }
				}
				else if (!session_event) // special key, but only if not already set by debugger
					session_event=(k-((event.key.keysym.mod&KMOD_CTRL)?128:0))<<8;
				break;
			case SDL_TEXTINPUT: // always follows SDL_KEYDOWN
				if (session_ui_char0)
					session_ui_char0=0;
				else if (session_signal&SESSION_SIGNAL_DEBUG) // only relevant inside debugger
					if (event.text.text[0]>32) // exclude SPACE and non-visible codes!
						if ((session_event=event.text.text[0])&128) // UTF-8?
							session_event=128+(session_event&31)*64+(event.text.text[1]&63);
				break;
			case SDL_KEYUP:
				session_shift=!!(event.key.keysym.mod&KMOD_SHIFT);
				if ((k=session_k2joy(event.key.keysym.scancode))<128)
					//if (!(session_signal&SESSION_SIGNAL_DEBUG)) // redundant, unlike in SDL_KEYDOWN
						{ if (k<0) joy_kbd&=~(1<<(k+8)); else kbd_bit_res(k); }
				break;
			case SDL_JOYAXISMOTION:
				//if (!session_pad) // redundant
					switch (event.jaxis.axis) // warning, there can be more than two axes
					{
						case SDL_CONTROLLER_AXIS_LEFTX: // safe
						//case SDL_CONTROLLER_AXIS_RIGHTX: // unsafe?
							session_joybits=(session_joybits&~(4+8))+(event.jaxis.value<-0X4000?4:event.jaxis.value>=0X4000?8:0); break;
						case SDL_CONTROLLER_AXIS_LEFTY: // safe
						//case SDL_CONTROLLER_AXIS_RIGHTY: // unsafe?
							session_joybits=(session_joybits&~(1+2))+(event.jaxis.value<-0X4000?1:event.jaxis.value>=0X4000?2:0); break;
					}
				break;
			case SDL_CONTROLLERAXISMOTION:
				//if (session_pad) // redundant
					switch (event.caxis.axis) // only the first two axes (X and Y) are safe
					{
						case 0: session_joybits=(session_joybits&~(4+8))+(event.caxis.value<-0X4000?4:event.caxis.value>=0X4000?8:0); break;
						case 1: session_joybits=(session_joybits&~(1+2))+(event.caxis.value<-0X4000?1:event.caxis.value>=0X4000?2:0); break;
					}
				break;
			case SDL_JOYBUTTONDOWN:
				if (!session_pad) // required!
					session_joybits|=16<<event.jbutton.button;
				break;
			case SDL_JOYBUTTONUP:
				if (!session_pad) // required!
					session_joybits&=~(16<<event.jbutton.button);
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				//if (session_pad) // redundant
					session_joybits|=session_pad2bit(event.cbutton.button);
				break;
			case SDL_CONTROLLERBUTTONUP:
				//if (session_pad) // redundant
					session_joybits&=~session_pad2bit(event.cbutton.button);
				break;
			case SDL_DROPFILE:
				strcpy(session_parmtr,event.drop.file);
				SDL_free(event.drop.file);
				session_event=0X8000;
				break;
			case SDL_QUIT:
				return 1;
		}
		if (0X8080==session_event) // F10?
			session_ui_menu(); // can assign new values to session_event
		if (session_event)
		{
			if (0X0080==session_event) // Menu entry "Exit"
				return 1;
			if (!((session_signal&SESSION_SIGNAL_DEBUG)&&session_debug_user(session_event)))
				session_dirty=1,session_user(session_event);
			session_event=0; //if (session_paused) ...?
		}
	}
	return 0; // OK
}
int session_joy2k(void) // turn the joystick status into bits
	{ return session_joybits; }
int session_ticks(void) // get the `session_clock` tick count
	{ return SDL_GetTicks(); } // stick to system clock; unlike Win32, the audio clock is unreliable in SDL2 :-(

// menu item functions ---------------------------------------------- //

void session_menuflags(int id,int a,int z,BYTE q,BYTE r) // auxiliar, see below
{
	BYTE *m=session_ui_menudata; int j;
	while (*m) // scan menu data for items
	{
		while (*m++) {} // skip menu name
		while (j=*m++<<8,j+=*m++)
		{
			if (j>=a&&j<=z)
			{
				*m=j==id?r:q;
				if (a==z) return;
			}
			while (*m++) {} // skip item name
		}
	}
}
void session_menucheck(int id,int q) // assign bool `q` to status of option `id`
	{ session_menuflags(q?id:-1,id,id,16,17); } // ...16 can become 32 (SPACE)
void session_menuradio(int id,int a,int z) // reset option range `a-z` and set option `id`
	{ session_menuflags(id,a,z,18,19); } // ...18 can become 32 (SPACE) too

// message box ------------------------------------------------------ //

int session_message(char *s,char *t) { return session_ui_text(s,t,0); } // show multi-lined text `s` under caption `t`
int session_aboutme(char *s,char *t) { return session_ui_text(s,t,1); } // special case: "About.."

// input dialog ----------------------------------------------------- //

#define session_line(t) session_ui_line((t)) // `t` is the caption; returns -1 on error or LENGTH on success; both the source and target strings are in `session_parmtr`

// list dialog ------------------------------------------------------ //

int session_list(int i,char *s,char *t) { return session_ui_list(i,s,t,NULL,0); } // `s` is a list of ASCIZ entries, `i` is the default chosen item, `t` is the caption; returns -1 on error or 0..n-1 on success

// scan dialog ------------------------------------------------------ //

#define session_scan(s) session_ui_scan((s)) // `s` is the name of the event; returns <0 on error or 0..n-1 (keyboard code) on success

// file dialog ------------------------------------------------------ //

#define session_filedialog_get_readonly() (session_ui_fileflags&1)
#define session_filedialog_set_readonly(q) (q?(session_ui_fileflags|=1):(session_ui_fileflags&=~1))
char *session_newfile(char *r,char *s,char *t) // "Create File" | ...and returns NULL on failure, or `session_parmtr` (with a file path) on success.
	{ return session_ui_filedialog(r,s,t,0,0)?session_parmtr:NULL; }
char *session_getfile(char *r,char *s,char *t) // "Open a File" | lists files in path `r` matching pattern `s` under caption `t`, etc.
	{ return session_ui_filedialog(r,s,t,0,1)?session_parmtr:NULL; }
char *session_getfilereadonly(char *r,char *s,char *t,int q) // "Open a File" with Read Only option | lists files in path `r` matching pattern `s` under caption `t`; `q` is the default Read Only value, etc.
	{ return session_ui_filedialog(r,s,t,1,q)?session_parmtr:NULL; }

// final definitions ------------------------------------------------ //

// unlike Win32, SDL2 indirectly includes "math.h" and declares SDL_pow, SDL_sin...
#define BOOTSTRAP // SDL2 doesn't require a main-WinMain bootstrap

// ====================================== END OF SDL 2.0+ DEFINITIONS //
