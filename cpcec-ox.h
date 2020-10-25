 //  ####  ######    ####  #######   ####    ----------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####    ----------------------- //

// SDL2 is the second supported platform; compiling the emulator needs
// "$(CC) -DSDL_MAIN_HANDLED -xc cpcec.c -lSDL2" for GCC, TCC et al.
// Support for Unix-like systems is provided by providing different
// snippets of code upon whether the symbol "_WIN32" exists or not.

// START OF SDL 2.0+ DEFINITIONS ==================================== //

#include <SDL2/SDL.h> // requires defining the symbol SDL_MAIN_HANDLED!

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	typedef union { unsigned short w; struct { unsigned char h,l; } b; } Z80W; // big-endian: PPC, ARM...
#else
	typedef union { unsigned short w; struct { unsigned char l,h; } b; } Z80W; // lil-endian: i86, x64...
#endif

#define strcasecmp SDL_strcasecmp
#ifdef _WIN32
	#define STRMAX 256 // widespread in Windows
	#define PATHCHAR '\\' // WIN32
	#include <windows.h> // FindFirstFile...
	#include <io.h> // _chsize(),_fileno()...
	#define fsetsize(f,l) _chsize(_fileno(f),(l))
#else
	#ifdef PATH_MAX
		#define STRMAX PATH_MAX
	#else
		#define STRMAX 512 // MacOS lacks PATH_MAX; TCC's LIMITS.H sets it to 512 and seems safe, with the exception of realpath()
	#endif
	#define PATHCHAR '/' // POSIX
	#include <dirent.h> // opendir()...
	#include <sys/stat.h> // stat()...
	#include <unistd.h> // ftruncate(),fileno()...
	#define fsetsize(f,l) (!ftruncate(fileno(f),(l)))
	#define BYTE Uint8
	#define WORD Uint16
	#define DWORD Uint32
#endif

#ifdef CONSOLE_DEBUGGER
#define logprintf(...) (fprintf(stdout,__VA_ARGS__))
#else
#define logprintf(...) 0
#endif

#define MESSAGEBOX_WIDETAB "\t\t"

// general engine constants and variables --------------------------- //

#define VIDEO_UNIT DWORD // 0x00RRGGBB style
#define VIDEO1(x) (x) // no conversion required!

#define VIDEO_FILTER_AVG(x,y) (((((x&0xFF00FF)*3+(y&0xFF00FF)+(x<y?0x30003:0))&0x3FC03FC)+(((x&0xFF00)*3+(y&0xFF00)+(x<y?0x300:0))&0x3FC00))>>2) // faster
#define VIDEO_FILTER_STEP(r,x,y) r=VIDEO_FILTER_AVG(x,y),x=r // hard interpolation
//#define VIDEO_FILTER_STEP(r,x,y) r=VIDEO_FILTER_AVG(x,y),x=y // soft interpolation
#define VIDEO_FILTER_X1(x) (((x>>1)&0x7F7F7F)+0x2B2B2B)
//#define VIDEO_FILTER_X1(x) (((x>>2)&0x3F3F3F)+0x404040) // heavier
//#define VIDEO_FILTER_X1(x) (((x>>2)&0x3F3F3F)*3+0x161616) // lighter

#if 0 // 8 bits
	#define AUDIO_UNIT unsigned char
	#define AUDIO_BITDEPTH 8
	#define AUDIO_ZERO 128
	#define AUDIO1(x) (x)
#else // 16 bits
	#define AUDIO_UNIT signed short
	#define AUDIO_BITDEPTH 16
	#define AUDIO_ZERO 0
	#define AUDIO1(x) (x)
#endif // bitsize
#define AUDIO_STEREO 1

VIDEO_UNIT *video_frame; // video frame, allocated on runtime
AUDIO_UNIT *audio_frame,audio_buffer[AUDIO_LENGTH_Z*(AUDIO_STEREO+1)]; // audio frame
VIDEO_UNIT *video_target; // pointer to current video pixel
AUDIO_UNIT *audio_target; // pointer to current audio sample
int video_pos_x,video_pos_y,audio_pos_z; // counters to keep pointers within range
BYTE video_interlaced=0,video_interlaces=0,video_interlacez=0; // video scanline status
char video_framelimit=0,video_framecount=0; // video frameskip counters; must be signed!
BYTE audio_disabled=0,audio_channels=1,audio_session=0; // audio status and counter
unsigned char session_path[STRMAX],session_parmtr[STRMAX],session_tmpstr[STRMAX],session_substr[STRMAX],session_info[STRMAX]="";

int session_timer,session_event=0; // timing synchronisation and user command
BYTE session_fast=0,session_wait=0,session_audio=1,session_blit=0; // timing and devices
BYTE session_stick=1,session_shift=0,session_key2joy=0; // keyboard and joystick
BYTE video_scanline=0,video_scanlinez=-1; // 0 = solid, 1 = scanlines, 2 = full interlace, 3 = half interlace
BYTE video_filter=0,audio_filter=0; // interpolation flags
BYTE session_intzoom=0,session_recording=0; int session_joybits=0;

FILE *session_wavefile=NULL; // audio recording is done on each session update

#ifndef CONSOLE_DEBUGGER
	VIDEO_UNIT *debug_frame;
	BYTE debug_buffer[DEBUG_LENGTH_X*DEBUG_LENGTH_Y]; // [0] can be a valid character, 128 (new redraw required) or 0 (redraw not required)
	SDL_Surface *session_dbg_dib;
#endif

BYTE session_paused=0,session_signal=0;
#define SESSION_SIGNAL_FRAME 1
#define SESSION_SIGNAL_DEBUG 2
#define SESSION_SIGNAL_PAUSE 4

BYTE session_dirtymenu=1; // to force new status text

#define kbd_bit_set(k) (kbd_bit[k/8]|=1<<(k%8))
#define kbd_bit_res(k) (kbd_bit[k/8]&=~(1<<(k%8)))
#define kbd_bit_tst(k) (kbd_bit[k/8]&(1<<(k%8)))
BYTE kbd_bit[16]; // up to 128 keys in 16 rows of 8 bits

// SDL2 follows the USB keyboard standard, so we're using the same values here:

#define	KBCODE_NULL	  0
// function keys
#define	KBCODE_F1	 58
#define	KBCODE_F2	 59
#define	KBCODE_F3	 60
#define	KBCODE_F4	 61
#define	KBCODE_F5	 62
#define	KBCODE_F6	 63
#define	KBCODE_F7	 64
#define	KBCODE_F8	 65
#define	KBCODE_F9	 66
#define	KBCODE_F10	 67
#define	KBCODE_F11	 68
#define	KBCODE_F12	 69
// leftmost keys
#define	KBCODE_ESCAPE	 41
#define	KBCODE_TAB	 43
#define	KBCODE_CAP_LOCK	 57
#define	KBCODE_L_SHIFT	225
#define	KBCODE_L_CTRL	224
#define	KBCODE_L_ALT	226 // trapped by Win32
// alphanumeric row 1
#define	KBCODE_1	 30
#define	KBCODE_2	 31
#define	KBCODE_3	 32
#define	KBCODE_4	 33
#define	KBCODE_5	 34
#define	KBCODE_6	 35
#define	KBCODE_7	 36
#define	KBCODE_8	 37
#define	KBCODE_9	 38
#define	KBCODE_0	 39
#define	KBCODE_CHR1_1	 45
#define	KBCODE_CHR1_2	 46
// alphanumeric row 2
#define	KBCODE_Q	 20
#define	KBCODE_W	 26
#define	KBCODE_E	  8
#define	KBCODE_R	 21
#define	KBCODE_T	 23
#define	KBCODE_Y	 28
#define	KBCODE_U	 24
#define	KBCODE_I	 12
#define	KBCODE_O	 18
#define	KBCODE_P	 19
#define	KBCODE_CHR2_1	 47
#define	KBCODE_CHR2_2	 48
// alphanumeric row 3
#define	KBCODE_A	  4
#define	KBCODE_S	 22
#define	KBCODE_D	  7
#define	KBCODE_F	  9
#define	KBCODE_G	 10
#define	KBCODE_H	 11
#define	KBCODE_J	 13
#define	KBCODE_K	 14
#define	KBCODE_L	 15
#define	KBCODE_CHR3_1	 51
#define	KBCODE_CHR3_2	 52
#define	KBCODE_CHR3_3	 49
// alphanumeric row 4
#define	KBCODE_Z	 29
#define	KBCODE_X	 27
#define	KBCODE_C	  6
#define	KBCODE_V	 25
#define	KBCODE_B	  5
#define	KBCODE_N	 17
#define	KBCODE_M	 16
#define	KBCODE_CHR4_1	 54
#define	KBCODE_CHR4_2	 55
#define	KBCODE_CHR4_3	 56
#define	KBCODE_CHR4_4	 53
#define	KBCODE_CHR4_5	100
// rightmost keys
#define	KBCODE_SPACE	 44
#define	KBCODE_BKSPACE	 42
#define	KBCODE_ENTER	 40
#define	KBCODE_R_SHIFT	229
#define	KBCODE_R_CTRL	228
#define	KBCODE_R_ALT	230 // trapped by Win32
// extended keys
#define	KBCODE_PRINT	 70 // trapped by Win32
#define	KBCODE_SCR_LOCK	 71
#define	KBCODE_HOLD	 72
#define	KBCODE_INSERT	 73
#define	KBCODE_DELETE	 76
#define	KBCODE_HOME	 74
#define	KBCODE_END	 77
#define	KBCODE_PRIOR	 75
#define	KBCODE_NEXT	 78
#define	KBCODE_UP	 82
#define	KBCODE_DOWN	 81
#define	KBCODE_LEFT	 80
#define	KBCODE_RIGHT	 79
#define	KBCODE_NUM_LOCK	 83
// numeric keypad
#define	KBCODE_X_7	 95
#define	KBCODE_X_8	 96
#define	KBCODE_X_9	 97
#define	KBCODE_X_4	 92
#define	KBCODE_X_5	 93
#define	KBCODE_X_6	 94
#define	KBCODE_X_1	 89
#define	KBCODE_X_2	 90
#define	KBCODE_X_3	 91
#define	KBCODE_X_0	 98
#define	KBCODE_X_DOT	 99
#define	KBCODE_X_ENTER	 88
#define	KBCODE_X_ADD	 87
#define	KBCODE_X_SUB	 86
#define	KBCODE_X_MUL	 85
#define	KBCODE_X_DIV	 84

const BYTE kbd_k2j[]= // these keys can simulate a joystick
	{ KBCODE_UP, KBCODE_DOWN, KBCODE_LEFT, KBCODE_RIGHT, KBCODE_Z, KBCODE_X, KBCODE_C, KBCODE_V };

unsigned char kbd_map[256]; // key-to-key translation map

// general engine functions and procedures -------------------------- //

int session_user(int k); // handle the user's commands; 0 OK, !0 ERROR. Must be defined later on!
#ifndef CONSOLE_DEBUGGER
	void session_debug_show(void);
	int session_debug_user(int k); // debug logic is a bit different: 0 UNKNOWN COMMAND, !0 OK
	int debug_xlat(int k); // translate debug keys into codes. Must be defined later on!
#endif
INLINE void audio_playframe(int q,AUDIO_UNIT *ao); // handle the sound filtering; is defined in CPCEC-RT.H!

void session_please(void) // stop activity for a short while
{
	if (!session_wait)
	{
		if (session_audio)
			SDL_PauseAudioDevice(session_audio,1);
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

SDL_Joystick *session_ji=NULL;
SDL_Window *session_hwnd=NULL;
SDL_Surface *session_dib=NULL;

// extremely tiny graphical user interface: SDL2 provides no widgets! //

SDL_Rect session_ideal;
#define SESSION_UI_HEIGHT 14 // DEBUG_LENGTH_Z
SDL_Surface *session_ui_alloc(int w,int h) { return ((w+=2)*h)?SDL_CreateRGBSurface(0,w*8,h*SESSION_UI_HEIGHT,32,0xFF0000,0x00FF00,0x0000FF,0):NULL; }
void session_ui_free(SDL_Surface *s) { if (s) SDL_FreeSurface(s); }

int session_ui_base_x,session_ui_base_y,session_ui_size_x,session_ui_size_y; // used to calculate mouse clicks relative to widget

void session_border(SDL_Surface *s,int x,int y,int q) // draw surface with an optional border
{
	SDL_Surface *t=SDL_GetWindowSurface(session_hwnd);
	SDL_Rect tgt,tmp;
	tgt.x=x*8; tgt.w=s->w;
	tgt.y=y*SESSION_UI_HEIGHT; tgt.h=s->h;
	SDL_SetSurfaceBlendMode(s,SDL_BLENDMODE_NONE);
	SDL_BlitSurface(s,NULL,t,&tgt);
	if (q)
	{
		// top border
		tmp.x=tgt.x-2; tmp.y=tgt.y-2;
		tmp.w=tgt.w+4; tmp.h=2; SDL_FillRect(t,&tmp,0x00C0C0C0);
		// bottom border
		tmp.y=tgt.y+tgt.h;
		SDL_FillRect(t,&tmp,0x00404040);
		// left border
		tmp.y=tgt.y-1;
		tmp.w=2; tmp.h=tgt.h+2; SDL_FillRect(t,&tmp,0x00808080);
		// right border
		tmp.x=tgt.x+tgt.w;
		SDL_FillRect(t,&tmp,0x00808080);
	}
}
void session_centre(SDL_Surface *s,int q) // draw surface in centre with an optional border
{
	SDL_Surface *t=SDL_GetWindowSurface(session_hwnd);
	session_ui_size_x=s->w/8; session_ui_size_y=s->h/SESSION_UI_HEIGHT;
	session_border(s,session_ui_base_x=(t->w-s->w)/16,session_ui_base_y=(t->h-s->h)/(2*SESSION_UI_HEIGHT),q);
}

void session_blitit(SDL_Surface *s,int ox,int oy)
{
	SDL_Surface *surface=SDL_GetWindowSurface(session_hwnd); int xx,yy; // calculate window area
	if ((xx=surface->w)>0&&(yy=surface->h)>0) // don't redraw invalid windows!
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
		session_ideal.x=(surface->w-(session_ideal.w=xx))/2; session_ideal.y=(surface->h-(session_ideal.h=yy))/2;
		{
			SDL_Rect src;
			src.x=ox; src.w=VIDEO_PIXELS_X;
			src.y=oy; src.h=VIDEO_PIXELS_Y;
			if (SDL_GetWindowFlags(session_hwnd)&SDL_WINDOW_FULLSCREEN_DESKTOP)
				SDL_BlitScaled(s,&src,SDL_GetWindowSurface(session_hwnd),&session_ideal);
			else
				SDL_BlitSurface(s,&src,SDL_GetWindowSurface(session_hwnd),&session_ideal);
		}
	}
}
void session_redraw(BYTE q)
{
	#ifndef CONSOLE_DEBUGGER
	if (session_signal&SESSION_SIGNAL_DEBUG)
		session_blitit(session_dbg_dib,0,0);
	else
	#endif
		session_blitit(session_dib,VIDEO_OFFSET_X,VIDEO_OFFSET_Y);
	if (q)
		SDL_UpdateWindowSurface(session_hwnd);
}
#define session_clrscr() SDL_FillRect(SDL_GetWindowSurface(session_hwnd),NULL,0)
void session_togglefullscreen(void)
{
	SDL_SetWindowFullscreen(session_hwnd,SDL_GetWindowFlags(session_hwnd)&SDL_WINDOW_FULLSCREEN_DESKTOP?0*SDL_WINDOW_BORDERLESS:SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void session_ui_init(void) { memset(kbd_bit,0,sizeof(kbd_bit)); session_please(); }
void session_ui_exit(void) { session_redraw(1); }

int session_ui_clickx,session_ui_clicky; // mouse X+Y, when the "key" is -1
int session_ui_char,session_ui_shift; // ASCII+Shift of the latest keystroke
int session_ui_exchange(void) // update window and wait for a keystroke
{
	session_ui_char=0; SDL_Event event;
	SDL_UpdateWindowSurface(session_hwnd);
	for (;;)
	{
		SDL_WaitEvent(&event);
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				if (event.window.event==SDL_WINDOWEVENT_EXPOSED)
					return 0; // dummy command, force full redraw
				break;
			case SDL_MOUSEBUTTONDOWN:
				session_ui_clickx=event.button.x/8-session_ui_base_x,session_ui_clicky=event.button.y/SESSION_UI_HEIGHT-session_ui_base_y;
				return -1;
			case SDL_MOUSEWHEEL:
				{
					int i=event.wheel.y;
					if (!i) return 0;
					if (event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED) i=-i;
					return i>0?KBCODE_PRIOR:KBCODE_NEXT;
				}
			case SDL_TEXTINPUT:
				session_ui_char=event.text.text[0];
				if (session_ui_char&128) // UTF-8?
					session_ui_char=128+(session_ui_char&1)*64+(event.text.text[1]&63);
				return 0;
			case SDL_KEYDOWN:
				session_ui_shift=!!(event.key.keysym.mod&KMOD_SHIFT);
				return event.key.keysym.mod&KMOD_ALT?0:event.key.keysym.scancode;
			case SDL_QUIT:
				return KBCODE_ESCAPE;
		}
	}
}
void session_ui_printglyph(SDL_Surface *t,char z,int x,int y) // print one glyph
{
	BYTE const *r=&onscreen_chrs[((z-32)&127)*onscreen_size];
	VIDEO_UNIT q0,q1;
	if (z&128)
		q0=VIDEO1(0),q1=VIDEO1(-1);
	else
		q1=VIDEO1(0),q0=VIDEO1(-1);
	BYTE yy=0;
	while (yy<(SESSION_UI_HEIGHT-onscreen_size)/2)
	{
		VIDEO_UNIT *p=(VIDEO_UNIT *)&((BYTE *)t->pixels)[(y*SESSION_UI_HEIGHT+yy)*t->pitch]; p=&p[x*8];
		for (int xx=0;xx<8;++xx)
			*p++=q0;
		++yy;
	}
	while (yy<(SESSION_UI_HEIGHT-onscreen_size)/2+onscreen_size)
	{
		VIDEO_UNIT *p=(VIDEO_UNIT *)&((BYTE *)t->pixels)[(y*SESSION_UI_HEIGHT+yy)*t->pitch]; p=&p[x*8];
		BYTE rr=*r++; rr|=rr>>1; // normal, not thin or bold
		for (int xx=0;xx<8;++xx)
			*p++=(rr&(128>>xx))?q1:q0;
		++yy;
	}
	while (yy<SESSION_UI_HEIGHT)
	{
		VIDEO_UNIT *p=(VIDEO_UNIT *)&((BYTE *)t->pixels)[(y*SESSION_UI_HEIGHT+yy)*t->pitch]; p=&p[x*8];
		for (int xx=0;xx<8;++xx)
			*p++=q0;
		++yy;
	}
}
int session_ui_printasciz(SDL_Surface *t,char *s,int x,int y,int m,int q) // print a text at a location with an optional length limit
{
	BYTE z,l=0,k=strlen(s);
	session_ui_printglyph(t,q?160:32,x++,y);
	while (m>0?l<m:*s)
	{
		if (!(z=*s))
			z=' '; // pad with spaces
		else
		{
			++s;
			if (m>0&&k>m&&l+2>=m) // string is too long
				z='.'; // ".."
		}
		if (q) z^=128;
		session_ui_printglyph(t,z,x,y);
		++x;
		++l;
	}
	session_ui_printglyph(t,q?160:32,x,y);
	return l+2;
}

BYTE session_ui_menudata[1<<12],session_ui_menusize; // encoded menu data
SDL_Surface *session_ui_icon=NULL;

void session_ui_menu(void) // show the menu and set session_event accordingly
{
	if (!session_ui_menusize)
		return;
	session_ui_init();
	SDL_Surface *hmenu,*hitem=NULL;
	int menuxs[64],events[64]; // more than 64 menus, or 64 items in a menu? overkill!!!
	int menu=0,menus=0,menuz=-1,item=0,items=0,itemz=-1,itemx=0,itemw=0;
	char *zz,*z;
	hmenu=session_ui_alloc(session_ui_menusize-2,1);
	//session_ui_printasciz(hmenu,"",0,0,VIDEO_PIXELS_X/8-4,0);//SDL_FillRect(,NULL,VIDEO1(0xFFFFFF));
	int done,ox=(session_ideal.x+(session_ideal.w-VIDEO_PIXELS_X)/2)/8,
		oy=(session_ideal.y+(session_ideal.h-VIDEO_PIXELS_Y)/2)/SESSION_UI_HEIGHT;
	session_ui_base_x=ox+1; session_ui_base_y=oy+1;
	session_event=0x8000;
	while (session_event==0x8000) // empty menu items must be ignored
	{
		done=0;
		while (!done) // redraw menus and items as required, then obey the user
		{
			if (menuz!=menu)
			{
				menuz=menu;
				itemz=-1;
				item=items=0;
				int menux=menus=0;
				BYTE *m=session_ui_menudata;
				while (*m) // scan menu data for menus
				{
					if (menus==menu)
					{
						itemx=menux;
						menux+=session_ui_printasciz(hmenu,m,menux,0,-1,1);
					}
					else
						menux+=session_ui_printasciz(hmenu,m,menux,0,-1,0);
					int i=0,j=0,k=0;
					while (*m++) // skip menu name
						++j;
					while (*m++|*m++) // skip menu items
					{
						++k; i=0;
						while (*m++) // skip item name
							++i;
						if (j<i)
							j=i;
					}
					if (menus==menu)
						itemw=j,items=k;
					menuxs[menus++]=menux;
				}
				session_redraw(0); // build background, delay refresh
				session_border(hmenu,1+ox,1+oy,1);
				session_ui_free(hitem);
				hitem=session_ui_alloc(itemw,items);
			}
			if (itemz!=item)
			{
				itemz=item;
				int i=0;
				BYTE *m=session_ui_menudata;
				while (*m) // scan menu data for items
				{
					while (*m++) // skip menu name
						;
					int j,k=0;
					if (i==menu)
						zz=m;
					while (j=*m++<<8,j+=*m++,j)
					{
						if (i==menu)
						{
							if (item==k)
								z=&m[-2],session_event=j;
							session_ui_printasciz(hitem,m,0,k,itemw,item==k);
							events[k++]=j;
						}
						while (*m++) // skip item name
							;
					}
					++i;
				}
				session_border(hitem,itemx+1+ox,2+oy,1);
			}
			switch (session_ui_exchange())
			{
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
				case KBCODE_PRIOR:
				case KBCODE_LEFT:
					if (--menu<0)
						menu=menus-1;
					break;
				case KBCODE_NEXT:
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
					//if (session_event!=0x8000) // gap?
						done=1;
					break;
				case -1: // mouse click
					if (!session_ui_clicky&&session_ui_clickx>=0&&session_ui_clickx<session_ui_menusize) // select menu?
					{
						menu=menus; while (session_ui_clickx<menuxs[menu-1])
							--menu;
					}
					else if (session_ui_clicky>0&&session_ui_clicky<=items&&session_ui_clickx>=itemx&&session_ui_clickx<itemx+itemw+2) // select item?
						session_event=events[session_ui_clicky-1],done=1;
					else // quit?
						session_event=0,done=1;
					break;
				default:
					if (session_ui_char>=32&&session_ui_char<=128)
						for (int o=lcase(session_ui_char),n=items;n;--n)
						{
							++z,++z; // skip ID
							while (*z++)
								;
							if (++item>=items)
								item=0,z=zz;
							if (lcase(z[4])==o)
								break;
						}
					break;
			}
		}
	}
	session_ui_free(hmenu);
	session_ui_free(hitem);
	session_ui_exit();
	if (done<0)
		session_event=0;
	else
		session_shift=!!(session_event&0x4000),session_event&=0xBFFF; // cfr.shift
	return;
}

int session_ui_text(char *s,char *t,char q) // see session_message
{
	if (!s||!t)
		return -1;
	session_ui_init();
	int i,j,textw=0,texth=1;
	unsigned char *m=t;
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
	if (textw>VIDEO_PIXELS_X/8-4)
		textw=VIDEO_PIXELS_X/8-4;
	if (i)
		++texth;
	if (q)
		textw+=q=6; // 6: four characters, plus one extra char per side
	SDL_Surface *htext=session_ui_alloc(textw,texth);
	session_ui_printasciz(htext,t,0,0,textw,1);
	i=j=0;
	m=session_parmtr;
	while (*s) // render text proper
	{
		int k=(*m++=*s++);
		++j;
		if (k=='\n')
		{
			m[-1]=j=0;
			session_ui_printasciz(htext,m=session_parmtr,q,++i,textw-q,0);
		}
		else if (k=='\t')
		{
			m[-1]=' ';
			while (j&7)
				++j,*m++=' ';
		}
	}
	if (m!=session_parmtr)
	{
		*m=0;
		session_ui_printasciz(htext,m=session_parmtr,q,++i,textw-q,0);
	}
	if (q)
	{
		SDL_Rect r;
		r.x=0; r.w=q*8; r.y=SESSION_UI_HEIGHT; r.h=(texth-1)*SESSION_UI_HEIGHT;
		SDL_FillRect(htext,&r,0x00C0C0C0); // bright grey background
		r.x=q*4-16; r.y=SESSION_UI_HEIGHT*2; r.w=r.h=32;
		SDL_BlitSurface(session_ui_icon,NULL,htext,&r); // the icon!
	}
	session_centre(htext,1);//SDL_BlitSurface(htext,NULL,SDL_GetWindowSurface(session_hwnd),NULL);
	for (;;)
		switch (session_ui_exchange())
		{
			case -1: // mouse click
				if (session_ui_clickx>=0&&session_ui_clickx<session_ui_size_x&&session_ui_clicky>=0&&session_ui_clicky<session_ui_size_y)
					break;
			case KBCODE_ESCAPE:
			case KBCODE_SPACE:
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				session_ui_free(htext);
				session_ui_exit();
				return 0;
		}
}

int session_ui_list(int item,char *s,char *t,void x(void),int q) // see session_list
{
	int i,items=0,listw=0,listh,listz=0;
	char *m=t,*z=NULL;
	while (*m++)
		++listw;
	m=s;
	while (*m)
	{
		++items; i=0;
		while (*m++)
			++i;
		if (listw<i)
			listw=i;
	}
	if (!items)
		return -1;
	session_ui_init();
	if (listw>VIDEO_PIXELS_X/8-4)
		listw=VIDEO_PIXELS_X/8-4;
	listh=items+1<VIDEO_PIXELS_Y/SESSION_UI_HEIGHT-2?items+1:VIDEO_PIXELS_Y/SESSION_UI_HEIGHT-2;
	SDL_Surface *hlist=session_ui_alloc(listw,listh);
	session_ui_printasciz(hlist,t,0,0,listw,1);
	int done=0;
	while (!done)
	{
		if (listz>item)
			if ((listz=item)<0)
				listz=0; // allow index -1
		if (listz<item-listh+2)
			listz=item-listh+2;
		m=s; i=0;
		while (*m&&i<listz+listh-1)
		{
			if (i>=listz)
			{
				if (i==item)
					z=m;
				session_ui_printasciz(hlist,m,0,1+i-listz,listw,i==item);
			}
			if (*m) while (*m++)
				;
			++i;
		}
		session_centre(hlist,1);
		switch (session_ui_exchange())
		{
			case KBCODE_PRIOR:
			case KBCODE_LEFT:
				item-=listh-2;
			case KBCODE_UP:
				if (--item<0)
			case KBCODE_HOME:
					item=0;
				break;
			case KBCODE_NEXT:
			case KBCODE_RIGHT:
				item+=listh-2;
			case KBCODE_DOWN:
				if (++item>=items)
			case KBCODE_END:
					item=items-1;
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
					x();
				break;
			case -1: // mouse click
				if (session_ui_clickx<0||session_ui_clickx>=session_ui_size_x)
					item=-1,done=1; // quit!
				else if (session_ui_clicky<1) // scroll up?
				{
					if ((item-=listh-2)<0)
						item=0;
				}
				else if (session_ui_clicky>=session_ui_size_y) // scroll down?
				{
					if ((item+=listh-2)>=items)
						item=items-1;
				}
				else // select an item?
				{
					int button=listz+session_ui_clicky-1;
					if (q||item==button) // `q` (single click, f.e. "YES"/"NO") or same item? (=double click)
						item=button,done=1;
					else // different item!
					{
						item=button;
						if (item<0)
							item=0;
						else if (item>=items)
							item=items-1;
					}
				}
				break;
			case KBCODE_ESCAPE:
			case KBCODE_F10:
				item=-1;
				//break;
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				done=1;
				break;
			default:
				if (session_ui_char>=32&&session_ui_char<=128)
				{
					int itemz=item,n=items;
					for (int o=lcase(session_ui_char);n;--n)
					{
						if (item<0)
							item=0,z=s;
						else
						{
							++item;
							while (*z++)
								;
						}
						if (!(*z))
							item=0,z=s;
						if (lcase(*z)==o)
							break;
					}
					if (!n)
						item=itemz; // allow rewinding to -1
				}
				break;
		}
	}
	if (item>=0)
		strcpy(session_parmtr,z);
	session_ui_free(hlist);
	session_ui_exit();
	return item;
}

int session_ui_input(char *s,char *t) // see session_input
{
	int i,j,first,textw=VIDEO_PIXELS_X/8-4;
	if ((i=j=first=strlen(strcpy(session_parmtr,s)))>=textw)
		return -1; // error!
	session_ui_init();
	SDL_Surface *htext=session_ui_alloc(textw,2);
	session_ui_printasciz(htext,t,0,0,textw,1);
	int done=0;
	while (!done)
	{
		if (first)
		{
			char *m=session_parmtr; while (*m) *m^=128,++m; // inverse video for all text
			session_ui_printasciz(htext,session_parmtr,0,1,textw,0);
			m=session_parmtr; while (*m) *m^=128,++m; // restore video
		}
		else if (i<j)
		{
			session_parmtr[i]^=128; // inverse video
			session_ui_printasciz(htext,session_parmtr,0,1,textw,0);
			session_parmtr[i]^=128; // restore video
		}
		else
		{
			session_parmtr[i]=160; session_parmtr[i+1]=0; // inverse space
			session_ui_printasciz(htext,session_parmtr,0,1,textw,0);
			session_parmtr[i]=0; // end of string
		}
		session_centre(htext,1);
		switch (session_ui_exchange())
		{
			case KBCODE_LEFT:
				if (first||--i<0)
			case KBCODE_HOME:
					i=0;
				first=0;
				break;
			case KBCODE_RIGHT:
				if (first||++i>j)
			case KBCODE_END:
					i=j;
				first=0;
				break;
			case -1: // mouse click
				if (session_ui_clickx<0||session_ui_clickx>=session_ui_size_x||session_ui_clicky<0||session_ui_clicky>=session_ui_size_y)
					j=-1,done=1; // quit!
				else if (session_ui_clicky==1)
				{
					first=0;
					if ((i=session_ui_clickx-1)>j)
						i=j;
					else if (i<0)
						i=0;
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
				if (first)
					*session_parmtr=i=j=0;
				else if (i>0)
				{
					memmove(&session_parmtr[i-1],&session_parmtr[i],j-i+1);
					--i; --j;
				}
				first=0;
				break;
			case KBCODE_DELETE:
				if (first)
					*session_parmtr=i=j=0;
				if (i<j)
				{
					memmove(&session_parmtr[i],&session_parmtr[i+1],j-i+1);
					--j;
				}
				first=0;
				break;
			default:
				if (session_ui_char>=32&&session_ui_char<=128&&j<textw-1)
				{
					if (first)
					{
						*session_parmtr=session_ui_char;
						session_parmtr[i=j=1]=0;
					}
					else
					{
						memmove(&session_parmtr[i+1],&session_parmtr[i],j-i+1);
						session_parmtr[i]=session_ui_char;
						++i; ++j;
					}
					first=0;
				}
				break;
		}
	}
	if (j>=0)
		strcpy(s,session_parmtr);
	session_ui_free(htext);
	session_ui_exit();
	return j;
}

int multiglobbing(char *w,char *t,int q); // multi-pattern globbing; must be defined later on!
int sortedinsert(char *t,int z,char *s); // ordered list inserting; must be defined later on!
int sortedsearch(char *t,int z,char *s); // ordered list searching; must be defined later on!
int session_ui_fileflags;
char *session_ui_filedialog_sanitizepath(char *s) // restore PATHCHAR at the end of a path name
{
	if (s&&*s)
	{
		char *t=s;
		while (*t)
			++t;
		if (t[-1]!=PATHCHAR)
			*t=PATHCHAR,t[1]=0;
	}
	return s;
}
void session_ui_filedialog_tabkey(void)
{
	char *l={"Read/Write\000Read-Only\000\000"};
	int i=session_ui_list(session_ui_fileflags&1,l,"File access",NULL,0);
	if (i>=0)
		session_ui_fileflags=i;
}
int session_ui_filedialog_stat(char *s) // -1 = not exist, 0 = file, 1 = directory
{
	if (!s||!*s) return -1; // not even a valid path!
	#ifdef _WIN32
	int i=GetFileAttributes(s); return i<0?-1:i&FILE_ATTRIBUTE_DIRECTORY?1:0;
	#else
	struct stat a; return stat(s,&a)<0?-1:S_ISDIR(a.st_mode)?1:0;
	#endif
}
#ifdef _WIN32
int session_ui_drives=0; char session_ui_drive[]="::\\";
#endif
int session_ui_filedialog(char *r,char *s,char *t,int q,int f) // see session_filedialog; q here means TAB is available or not, and f means the starting TAB value (q) or whether we're reading (!q)
{
	int i; char *m,*n,basepath[8<<9],pastname[STRMAX],basefind[STRMAX]; // realpath() is an exception, see below
	session_ui_fileflags=f;
	if (m=strrchr(r,PATHCHAR))
		i=m[1],m[1]=0,strcpy(basepath,r),m[1]=i,strcpy(pastname,&m[1]);
	else
		*basepath=0,strcpy(pastname,r);
	for (;;)
	{
		i=0; m=session_scratch;

		#ifdef _WIN32
		if (session_ui_filedialog_stat(basepath)<1) // ensure that basepath exists and is a valid directory
		{
			GetCurrentDirectory(STRMAX,basepath); // fall back to current directory
			session_ui_filedialog_sanitizepath(basepath);
		}
		char *half=m;
		for (session_ui_drive[0]='A';session_ui_drive[0]<='Z';++session_ui_drive[0])
			if (session_ui_drives&(1<<(session_ui_drive[0]-'A')))
				++i,m=&half[sortedinsert(half,m-half,session_ui_drive)];
		if (basepath[3]) // not root directory?
		{
			*m++='.';*m++='.'; *m++=PATHCHAR; *m++=0; // always before the directories!
		}
		half=m;
		WIN32_FIND_DATA wfd; HANDLE h; strcpy(basefind,basepath); strcat(basefind,"*");
		if ((h=FindFirstFile(basefind,&wfd))!=INVALID_HANDLE_VALUE)
		{
			do
				if (!(wfd.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN)&&(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) // reject invisible directories
					if (strcmp(n=wfd.cFileName,".")&&strcmp(n=wfd.cFileName,"..")) // add directory name with the separator at the end
						++i,m=&half[sortedinsert(half,m-half,session_ui_filedialog_sanitizepath(n))];
			while (FindNextFile(h,&wfd)&&m-(char *)session_scratch<sizeof(session_scratch)-STRMAX);
			FindClose(h);
		}
		half=m;
		if ((h=FindFirstFile(basefind,&wfd))!=INVALID_HANDLE_VALUE)
		{
			do
				if (!(wfd.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN)&&!(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) // reject invisible files
					if (multiglobbing(s,n=wfd.cFileName,1)) // add file name
						++i,m=&half[sortedinsert(half,m-half,n)];
			while (FindNextFile(h,&wfd)&&m-(char *)session_scratch<sizeof(session_scratch)-STRMAX);
			FindClose(h);
		}
		#else
		DIR *d; struct dirent *e;
		if (session_ui_filedialog_stat(basepath)<1) // ensure that basepath exists and is a valid directory
		{
			if (getcwd(basepath,STRMAX)) // fall back to current directory
				session_ui_filedialog_sanitizepath(basepath);
		}
		if (basepath[1]) // not root directory?
		{
			*m++='.';*m++='.'; *m++=PATHCHAR; *m++=0; // always before the directories!
		}
		char *half=m;
		if (d=opendir(basepath))
		{
			while ((e=readdir(d))&&m-(char *)session_scratch<sizeof(session_scratch)-STRMAX)
				if (n=e->d_name,n[0]!='.') // reject ".*"
					if (session_ui_filedialog_stat(strcat(strcpy(basefind,basepath),n))>0) // add directory name with the separator at the end
						++i,m=&half[sortedinsert(half,m-half,session_ui_filedialog_sanitizepath(n))];
			closedir(d);
		}
		half=m;
		if (d=opendir(basepath))
		{
			while ((e=readdir(d))&&m-(char *)session_scratch<sizeof(session_scratch)-STRMAX)
				if (n=e->d_name,n[0]!='.'&&multiglobbing(s,n,1)) // reject ".*"
					if (!session_ui_filedialog_stat(strcat(strcpy(basefind,basepath),n))) // add file name
						++i,m=&half[sortedinsert(half,m-half,n)];
			closedir(d);
		}
		#endif

		if (!q&&!f)
			++i,memcpy(m,"*NEW*\000",7); // point at "*" by default on SAVE
		else
			*m=0,half=session_scratch,i=sortedsearch(half,m-half,pastname); // look for past name, if any

		if (session_ui_list(i,session_scratch,t,q?session_ui_filedialog_tabkey:NULL,0)<0)
			return 0; // user closed the dialog!

		m=session_parmtr;
		while (*m)
			++m;
		if (m[-1]==PATHCHAR) // the user chose a directory
		{
			strcat(strcpy(session_scratch,basepath),session_parmtr);
			#ifdef _WIN32
			if (session_parmtr[1]==':')
				*pastname=0,strcpy(basepath,session_parmtr);
			else
			{
				if (!strcmp(session_parmtr,strcpy(pastname,"..\\")))
				{
					m=strrchr(basepath,PATHCHAR);
					while(m&&m>basepath&&*--m!=PATHCHAR)
						;
					if (m)
						strcpy(pastname,&m[1]); // allow returning to previous directory
					else
						*pastname=0;
				}
				GetFullPathName(session_scratch,STRMAX,basepath,&m);
			}
			#else
			if (!strcmp(session_parmtr,strcpy(pastname,"../")))
			{
				m=strrchr(basepath,PATHCHAR);
				while(m&&m>basepath&&*--m!=PATHCHAR)
					;
				if (m)
					strcpy(pastname,&m[1]); // allow returning to previous directory
				else
					*pastname=0;
			}
			!realpath(session_scratch,basepath); // realpath() requires the target to be at least 4096 bytes long in Linux 4.10.0-38-generic, Linux 4.15.0-96-generic...!
			#endif
			session_ui_filedialog_sanitizepath(basepath);
		}
		else // the user chose a file
		{
			if (*session_parmtr=='*') // the user wants to create a file
			{
				*session_parmtr=0;
				if (session_ui_input(session_parmtr,t)>0)
				{
					if (multiglobbing(s,session_parmtr,1)!=1) // not the first extension?
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
			if (q||f||session_ui_filedialog_stat(session_parmtr)<0)
				return 1; // the user succesfully chose a file, either extant for reading or new for writing!
			if (session_ui_list(0,"YES\000NO\000\000","Overwrite?",NULL,1)==0)
				return strcpy(session_parmtr,session_scratch),1; // ...otherwise session_parmtr would hold "YES"!
		}
	}
}

// create, handle and destroy session ------------------------------- //

INLINE char* session_create(char *s) // create video+audio devices and set menu; 0 OK, !0 ERROR
{
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_EVENTS|SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK)<0)
		return (char *)SDL_GetError();
	if (!(session_hwnd=SDL_CreateWindow(caption_version,SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,0*SDL_WINDOW_BORDERLESS)))
		return SDL_Quit(),(char *)SDL_GetError();
	if (!(session_dib=SDL_CreateRGBSurface(0,VIDEO_LENGTH_X,VIDEO_LENGTH_Y,32,0xFF0000,0x00FF00,0x0000FF,0)))
		return SDL_Quit(),(char *)SDL_GetError();
	SDL_SetSurfaceBlendMode(session_dib,SDL_BLENDMODE_NONE);
	video_frame=session_dib->pixels;

	#ifndef CONSOLE_DEBUGGER
		session_dbg_dib=SDL_CreateRGBSurface(0,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,32,0xFF0000,0x00FF00,0x0000FF,0);
		SDL_SetSurfaceBlendMode(session_dbg_dib,SDL_BLENDMODE_NONE);
		debug_frame=session_dbg_dib->pixels;
	#endif

	if (session_stick)
	{
		int i=SDL_NumJoysticks();
		//printf("Detected %i joystick[s]: ",i);
		while (--i>=0&&!(/*printf("Joystick #%i = '%s'. ",i,SDL_JoystickNameForIndex(i)),*/session_ji=SDL_JoystickOpen(i))) // scan joysticks until we run out or one is OK
			; // unlike Win32, SDL lists the joysticks from last to first
		session_stick=i>=0;
		//printf(session_stick?"Joystick enabled!\n":"No joystick!");
	}
	if (session_audio)
	{
		SDL_AudioSpec spec;
		SDL_zero(spec);
		spec.freq=AUDIO_PLAYBACK;
		spec.format=AUDIO_BITDEPTH>8?AUDIO_S16SYS:AUDIO_U8;
		spec.channels=AUDIO_STEREO+1;
		spec.samples=4096; // safe value?
		if (session_audio=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0))
			audio_frame=audio_buffer;
	}
	session_timer=SDL_GetTicks();

	session_ui_icon=SDL_CreateRGBSurfaceFrom(session_icon32xx16,32,32,16,32*2,0xF00,0xF0,0xF,0xF000);
	#ifdef _WIN32
	long int z;
	for (session_ui_drive[0]='A';session_ui_drive[0]<='Z';++session_ui_drive[0]) // scan session_ui_drive just once
		if (GetDriveType(session_ui_drive)>1&&GetDiskFreeSpace(session_ui_drive,&z,&z,&z,&z))
			session_ui_drives|=1<<(session_ui_drive[0]-'A');
	#else
	SDL_SetWindowIcon(session_hwnd,session_ui_icon); // SDL already handles WIN32 icons alone!
	#endif

	// translate menu data into custom format
	BYTE *t=session_ui_menudata; session_ui_menusize=0;
	while (*s)
	{
		// scan and generate menu header
		while ((*t++=*s++)!='\n')
			++session_ui_menusize;
		session_ui_menusize+=2;
		t[-1]=0;
		// first scan: maximum item width
		char *r=s; int mxx=0;
		for (;;)
		{
			if (*r=='=')
				while (*r++!='\n')
					;
			else if (*r=='0')
			{
				while (*r++!=' ')
					;
				int n=0;
				while (*r++>=' ')
					++n;
				if (mxx<n)
					mxx=n;
				--r;
				while (*r++!='\n')
					;
			}
			else
				break;
		}
		// second scan: generate items
		for (;;)
			if (*s=='=')
			{
				*t++=0x80; *t++=0;*t++=0; // fake DROP code!
				while (*s++!='\n')
					;
			}
			else if (*s=='0')
			{
				int i=strtol(s,&s,0); // allow either hex or decimal
				*t++=i>>8; *t++=i; *t++=' ';*t++=' ';
				++s; int h=-2; // spaces between body and shortcut
				while ((i=(*t++=*s++))!='\n')
					if (i=='\t') // keyboard shortcut?
					{
						--t;
						for (int n=h;n<mxx;++n)
							*t++=' ';
					}
					else if (i=='_') // string shortcut?
						--t;
					else
						++h;
				t[-1]=0;
			}
			else
				break;
		*t++=0;*t++=0;
	}
	*t=0;
	SDL_StartTextInput();
	session_please();
	return NULL;
}

void session_menuinfo(void); // set the current menu flags. Must be defined later on!
INLINE int session_listen(void) // handle all pending messages; 0 OK, !0 EXIT
{
	static int config_signal=0; // catch DEBUG and PAUSE signals
	if (config_signal!=session_signal)
		config_signal=session_signal,session_dirtymenu=1;
	if (session_dirtymenu)
		session_dirtymenu=0,session_menuinfo();
	#ifndef CONSOLE_DEBUGGER
	if (session_signal)
	#else
	if (session_signal&~SESSION_SIGNAL_DEBUG)
	#endif
	{
		#ifndef CONSOLE_DEBUGGER
		if (session_signal&SESSION_SIGNAL_DEBUG)
		{
			if (*debug_buffer==128)
				session_please(),session_debug_show();
			if (*debug_buffer)//!=0
				session_redraw(1),*debug_buffer=0;
		}
		else
		#endif
		if (!session_paused) // set the caption just once
		{
			session_please();
			sprintf(session_tmpstr,"%s | %s | PAUSED",caption_version,session_info);
			SDL_SetWindowTitle(session_hwnd,session_tmpstr);
			session_paused=1;
		}
		SDL_WaitEvent(NULL);
	}
	int q=0; SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				if (event.window.event==SDL_WINDOWEVENT_EXPOSED)
					session_clrscr(),session_redraw(1);//SDL_UpdateWindowSurface(session_hwnd); // clear and redraw
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button==SDL_BUTTON_RIGHT)
					session_event=0x8080; // show menu
				break;
			#ifndef CONSOLE_DEBUGGER
			case SDL_TEXTINPUT:
				if (session_signal&SESSION_SIGNAL_DEBUG) // only relevant for the debugger, see below
					if (event.text.text[0]>32) // exclude SPACE and non-visible codes!
						if ((session_event=event.text.text[0])&128) // UTF-8?
							session_event=128+(session_event&31)*64+(event.text.text[1]&63);
				break;
			#endif
			case SDL_KEYDOWN: //if (event.key.state==SDL_PRESSED)
				{
					if (event.key.keysym.mod&KMOD_ALT)
					{
						if (event.key.keysym.sym==SDLK_RETURN)
							session_togglefullscreen();
						break;
					}
					session_shift=!!(event.key.keysym.mod&KMOD_SHIFT);
					if (event.key.keysym.sym==SDLK_F10)
					{
						session_event=0x8080; // show menu
						break;
					}
					#ifndef CONSOLE_DEBUGGER
					if (session_signal&SESSION_SIGNAL_DEBUG)
						session_event=debug_xlat(event.key.keysym.scancode);
					#endif
					int k=session_key_n_joy(event.key.keysym.scancode);
					if (k<128)
					{
						#ifndef CONSOLE_DEBUGGER
						if (!(session_signal&SESSION_SIGNAL_DEBUG))
						#endif
							kbd_bit_set(k);
					}
					else if (!session_event) // special key
						session_event=(k-((event.key.keysym.mod&KMOD_CTRL)?128:0))<<8;
				}
				break;
			case SDL_KEYUP: //if (event.key.state==SDL_RELEASED)
				{
					int k=session_key_n_joy(event.key.keysym.scancode);
					if (k<128)
						kbd_bit_res(k);
				}
				break;
			case SDL_JOYAXISMOTION: // 10922 ~ 32768/3
				if (event.jaxis.axis&1)
					session_joybits=(session_joybits&~(1+2))+(event.jaxis.value<-10922?1:event.jaxis.value>=19022?2:0);
				else
					session_joybits=(session_joybits&~(4+8))+(event.jaxis.value<-10922?4:event.jaxis.value>=19022?8:0);
				break;
			case SDL_JOYBUTTONDOWN:
				session_joybits|=16<<event.jbutton.button;
				break;
			case SDL_JOYBUTTONUP:
				session_joybits&=~(16<<event.jbutton.button);
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				memset(kbd_bit,0,sizeof(kbd_bit)); // loss of focus: no keys!
				break;
			case SDL_DROPFILE:
				strcpy(session_parmtr,event.drop.file);
				SDL_free(event.drop.file);
				session_event=0x8000;
				break;
			case SDL_QUIT:
				q=1;
				break;
		}
		if (session_event==0x8080)
			session_ui_menu();
		if (session_event==0x3F00)
			q=1;
		else if (session_event)
		{
			#ifndef CONSOLE_DEBUGGER
			if (!((session_signal&SESSION_SIGNAL_DEBUG)&&session_debug_user(session_event)))
			#endif
				q|=session_user(session_event);
			session_event=0;
		}
	}
	if (session_dirtymenu)
		session_dirtymenu=0,session_menuinfo();
	return q;
}

INLINE void session_writewave(AUDIO_UNIT *t); // save the current sample frame. Must be defined later on!
INLINE void session_render(void) // update video, audio and timers
{
	int i,j;
	static int performance_t=-9999,performance_f=0,performance_b=0; ++performance_f;
	if (!video_framecount) // do we need to hurry up?
	{
		if ((video_interlaces=!video_interlaces)||!video_interlaced)
			++performance_b,session_redraw(1);
		if (session_stick&&!session_key2joy) // do we need to check the joystick?
			for (i=0;i<sizeof(kbd_joy);++i)
			{
				int k=kbd_joy[i];
				if (session_joybits&(1<<i))
					kbd_bit_set(k); // key is down
				else
					kbd_bit_res(k); // key is up
			}
	}

	if (!audio_disabled)
		if (audio_filter) // audio interpolation: sample averaging
			audio_playframe(audio_filter,audio_buffer);
	if (session_wavefile) // record audio output, if required
		session_writewave(audio_frame);

	i=SDL_GetTicks();
	if (session_wait||session_fast)
		session_timer=i; // ensure that the next frame can be valid!
	else
	{
		j=1000/VIDEO_PLAYBACK-(i-session_timer);
		if (j>0)
			SDL_Delay(j>1000/VIDEO_PLAYBACK?1+1000/VIDEO_PLAYBACK:j);
		else if (j<0&&!session_recording)
			video_framecount=-2; // automatic frameskip!
		session_timer+=1000/VIDEO_PLAYBACK;
	}

	#ifdef SDL2_DOUBLE_QUEUE // audio buffer workaround (f.e. SlITaz v5)
		#define AUDIO_N_FRAMEZ (AUDIO_N_FRAMES*2)
	#else
		#define AUDIO_N_FRAMEZ AUDIO_N_FRAMES
	#endif
	if (session_audio)
	{
		static BYTE s=1;
		if (s!=audio_disabled)
			if (s=audio_disabled) // silent mode needs cleanup
				memset(audio_buffer,AUDIO_ZERO,sizeof(audio_buffer));
		j=SDL_GetQueuedAudioSize(session_audio);
		if (j<=sizeof(audio_buffer)*AUDIO_N_FRAMEZ) // buffer full?
		{
			/*audio_disabled&=~8*/; // buffer is ready!
			SDL_QueueAudio(session_audio,audio_buffer,sizeof(audio_buffer));
			if (j<=sizeof(audio_buffer)*AUDIO_N_FRAMEZ/2) // buffer not full enough?
				i+=500/VIDEO_PLAYBACK; // force speedup by mangling the timer!
		}
		else
			/*audio_disabled|=8*/; // buffer is full, no need to write the next frame!
	}

	if (session_wait) // resume activity after a pause
	{
		if (session_audio)
			SDL_PauseAudioDevice(session_audio,0);
		session_wait=0;
	}

	if (i>(performance_t+1000)) // performance percentage
	{
		if (performance_t)
		{
			sprintf(session_tmpstr,"%s | %s | %g%% CPU %g%% SDL",caption_version,session_info,performance_f*100.0/VIDEO_PLAYBACK,performance_b*100.0/VIDEO_PLAYBACK);
			SDL_SetWindowTitle(session_hwnd,session_tmpstr);
		}
		performance_t=i,performance_f=performance_b=session_paused=0;
	}
}

INLINE void session_byebye(void) // delete video+audio devices
{
	if (session_ji)
		SDL_JoystickClose(session_ji);
	SDL_StopTextInput();
	SDL_DestroyWindow(session_hwnd);
	#ifndef _WIN32
	SDL_FreeSurface(session_ui_icon);
	#endif
	SDL_FreeSurface(session_dib);
	#ifndef CONSOLE_DEBUGGER
	SDL_FreeSurface(session_dbg_dib);
	#endif
	SDL_Quit();
}

// auxiliary functions ---------------------------------------------- //

void session_detectpath(char *s) // detects session path
{
	if (s=strrchr(strcpy(session_path,s),PATHCHAR))
		s[1]=0; // keep separator
	else
		*session_path=0; // no path
}
char *session_configfile(void) // returns path to configuration file
{
	return strcat(strcpy(session_parmtr,session_path),
	#ifdef _WIN32
	my_caption ".ini"
	#else
	"." my_caption "rc"
	#endif
	);
}

void session_writebitmap(FILE *f) // write current bitmap into a BMP file
{
	static BYTE r[VIDEO_PIXELS_X*3];
	for (int i=VIDEO_OFFSET_Y+VIDEO_PIXELS_Y-1;i>=VIDEO_OFFSET_Y;--i)
	{
		BYTE *s=(BYTE *)&video_frame[(VIDEO_OFFSET_X+i*VIDEO_LENGTH_X)],*t=r;
		for (int j=0;j<VIDEO_PIXELS_X;++j) // turn ARGB (32 bits) into RGB (24 bits)
		{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			*t++=s[3]; // copy B
			*t++=s[2]; // copy G
			*t++=s[1]; // copy R
			s+=4; // skip A
#else
			*t++=*s++; // copy B
			*t++=*s++; // copy G
			*t++=*s++; // copy R
			s++; // skip A
#endif
		}
		fwrite(r,1,VIDEO_PIXELS_X*3,f);
	}
}

// menu item functions ---------------------------------------------- //

void session_menucheck(int id,int q) // set the state of option `id` as `q`
{
	BYTE *m=session_ui_menudata; int j;
	while (*m) // scan menu data for items
	{
		while (*m++) // skip menu name
			;
		while (j=*m++<<8,j+=*m++)
		{
			if (j==id)
				*m=q?'*':' ';
			while (*m++) // skip item name
				;
		}
	}
}
void session_menuradio(int id,int a,int z) // set the option `id` in the range `a-z`
{
	BYTE *m=session_ui_menudata; int j;
	while (*m) // scan menu data for items
	{
		while (*m++) // skip menu name
			;
		while (j=*m++<<8,j+=*m++)
		{
			if (j>=a&&j<=z)
				*m=j==id?'>':' ';
			while (*m++) // skip item name
				;
		}
	}
}

// message box ------------------------------------------------------ //

int session_message(char *s,char *t) { return session_ui_text(s,t,0); } // show multi-lined text `s` under caption `t`
int session_aboutme(char *s,char *t) { return session_ui_text(s,t,1); } // special case: "About.."

// input dialog ----------------------------------------------------- //

int session_input(char *s,char *t) { return session_ui_input(s,t); } // `s` is the target string (empty or not), `t` is the caption; returns -1 on error or LENGTH on success

// list dialog ------------------------------------------------------ //

int session_list(int i,char *s,char *t) { return session_ui_list(i,s,t,NULL,0); } // `s` is a list of ASCIZ entries, `i` is the default chosen item, `t` is the caption; returns -1 on error or 0..n-1 on success

// file dialog ------------------------------------------------------ //

#define session_filedialog_readonly (session_ui_fileflags&1)
#define session_filedialog_readonly0() (session_ui_fileflags&=~1)
#define session_filedialog_readonly1() (session_ui_fileflags|=1)
char *session_newfile(char *r,char *s,char *t) // "Create File" | ...and returns NULL on failure, or a string on success.
	{ return session_ui_filedialog(r,s,t,0,0)?session_parmtr:NULL; }
char *session_getfile(char *r,char *s,char *t) // "Open a File" | lists files in path `r` matching pattern `s` under caption `t`, etc.
	{ return session_ui_filedialog(r,s,t,0,1)?session_parmtr:NULL; }
char *session_getfilereadonly(char *r,char *s,char *t,int q) // "Open a File" with Read Only option | lists files in path `r` matching pattern `s` under caption `t`; `q` is the default Read Only value, etc.
	{ return session_ui_filedialog(r,s,t,1,q)?session_parmtr:NULL; }

// OS-dependant composite funcions ---------------------------------- //

//#define mgetc(x) (*(x))
//#define mputc(x,y) (*(x)=(y))
// 'i' = lil-endian (Intel), 'm' = big-endian (Motorola)

#if SDL_BYTEORDER == SDL_BIG_ENDIAN

int  mgetii(unsigned char *x) { return (x[1]<<8)+*x; }
void mputii(unsigned char *x,int y) { x[1]=y>>8; *x=y; }
int  mgetiiii(unsigned char *x) { return (x[3]<<24)+(x[2]<<16)+(x[1]<<8)+*x; }
void mputiiii(unsigned char *x,int y) { x[3]=y>>24; x[2]=y>>16; x[1]=y>>8; *x=y; }
#define mgetmm(x) (*(WORD*)(x))
#define mgetmmmm(x) (*(DWORD*)(x))
#define mputmm(x,y) ((*(WORD*)(x))=(y))
#define mputmmmm(x,y) ((*(DWORD*)(x))=(y))

#define equalsmm(x,i) (*(WORD*)(x)==(i))
#define equalsmmmm(x,i) (*(DWORD*)(x)==(i))
int equalsii(unsigned char *x,unsigned int i) { return (x[1]<<8)+*x==i; }
int equalsiiii(unsigned char *x,unsigned int i) { return (x[3]<<24)+(x[2]<<16)+(x[1]<<8)+*x==i; }

int fgetii(FILE *f) { int i=fgetc(f); return i+(fgetc(f)<<8); } // common lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { fputc(i,f); return fputc(i>>8,f); } // common lil-endian 16-bit fputc()
int fgetiiii(FILE *f) { int i=fgetc(f); i+=fgetc(f)<<8; i+=fgetc(f)<<16; return i+(fgetc(f)<<24); } // common lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { fputc(i,f); fputc(i>>8,f); fputc(i>>16,f); return fputc(i>>24,f); } // common lil-endian 32-bit fputc()
int fgetmm(FILE *f) { int i=0; return (fread(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fgetc()
int fputmm(int i,FILE *f) { return (fwrite(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fputc()
int fgetmmmm(FILE *f) { int i=0; return (fread(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fgetc()
int fputmmmm(int i,FILE *f) { return (fwrite(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fputc()

#else

#define mgetii(x) (*(WORD*)(x))
#define mgetiiii(x) (*(DWORD*)(x))
#define mputii(x,y) ((*(WORD*)(x))=(y))
#define mputiiii(x,y) ((*(DWORD*)(x))=(y))
int  mgetmm(unsigned char *x) { return (*x<<8)+x[1]; }
void mputmm(unsigned char *x,int y) { *x=y>>8; x[1]=y; }
int  mgetmmmm(unsigned char *x) { return (*x<<24)+(x[1]<<16)+(x[2]<<8)+x[3]; }
void mputmmmm(unsigned char *x,int y) { *x=y>>24; x[1]=y>>16; x[2]=y>>8; x[3]=y; }

#define equalsii(x,i) (*(WORD*)(x)==(i))
#define equalsiiii(x,i) (*(DWORD*)(x)==(i))
int equalsmm(unsigned char *x,unsigned int i) { return (*x<<8)+x[1]==i; }
int equalsmmmm(unsigned char *x,unsigned int i) { return (*x<<24)+(x[1]<<16)+(x[2]<<8)+x[3]==i; }

int fgetii(FILE *f) { int i=0; return (fread(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { return (fwrite(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fputc()
int fgetiiii(FILE *f) { int i=0; return (fread(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { return (fwrite(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fputc()
int fgetmm(FILE *f) { int i=fgetc(f)<<8; return i+fgetc(f); } // common big-endian 16-bit fgetc()
int fputmm(int i,FILE *f) { fputc(i>>8,f); return fputc(i,f); } // common big-endian 16-bit fputc()
int fgetmmmm(FILE *f) { int i=fgetc(f)<<24; i+=fgetc(f)<<16; i+=fgetc(f)<<8; return i+fgetc(f); } // common big-endian 32-bit fgetc()
int fputmmmm(int i,FILE *f) { fputc(i>>24,f); fputc(i>>16,f); fputc(i>>8,f); return fputc(i,f); } // common big-endian 32-bit fputc()

#endif

#define BOOTSTRAP

// ====================================== END OF SDL 2.0+ DEFINITIONS //
