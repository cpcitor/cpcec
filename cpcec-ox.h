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
// snippets of code upon whether the symbol "__WIN32__" exists or not.

// START OF SDL 2.0+ DEFINITIONS ==================================== //

#include <SDL2/SDL.h> // requires defining the symbol SDL_MAIN_HANDLED!

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	typedef union { unsigned short w; struct { unsigned char h,l; } b; } Z80W; // big-endian: PPC, ARM...
#else
	typedef union { unsigned short w; struct { unsigned char l,h; } b; } Z80W; // lil-endian: i86, x64...
#endif

#define strcasecmp SDL_strcasecmp
#ifdef __WIN32__
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
	#define fsetsize(f,l) ftruncate(fileno(f),(l))
#endif

#define BYTE Uint8
#define WORD Uint16
#define DWORD Uint32

#ifdef DEBUG
#define logprintf(...) (fprintf(stdout,__VA_ARGS__))
#else
#define logprintf(...)
#endif

#define MESSAGEBOX_WIDETAB "\t\t"

// general engine constants and variables --------------------------- //

#define VIDEO_DATATYPE Uint32
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
#define AUDIO_STEREO 1

VIDEO_DATATYPE *video_frame; // video frame, allocated on runtime
AUDIO_DATATYPE *audio_frame,audio_buffer[AUDIO_LENGTH_Z*(AUDIO_STEREO+1)]; // audio frame
VIDEO_DATATYPE *video_target; // pointer to current video pixel
AUDIO_DATATYPE *audio_target; // pointer to current audio sample
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
BYTE session_intzoom=0; int session_joybits=0;

FILE *session_wavefile=NULL; // audio recording is done on each session update

#ifndef DEBUG
	VIDEO_DATATYPE *debug_frame;
	BYTE debug_buffer[DEBUG_LENGTH_X*DEBUG_LENGTH_Y]; // [0] can be a valid character, 128 (new redraw required) or 0 (redraw not required)
	SDL_Surface *session_dbg_dib=NULL;
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
	session_border(s,(t->w-s->w)/16,(t->h-s->h)/(2*SESSION_UI_HEIGHT),q);
}

void session_redraw(BYTE q)
{
	SDL_Rect src;
	src.x=VIDEO_OFFSET_X; src.w=VIDEO_PIXELS_X;
	src.y=VIDEO_OFFSET_Y; src.h=VIDEO_PIXELS_Y;
	SDL_Surface *surface=SDL_GetWindowSurface(session_hwnd);
	int xx=surface->w,yy=surface->h;
	if (xx>0&&yy>0) // don't redraw invalid windows
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
		if (SDL_GetWindowFlags(session_hwnd)&SDL_WINDOW_FULLSCREEN_DESKTOP)
			SDL_BlitScaled(session_dib,&src,SDL_GetWindowSurface(session_hwnd),&session_ideal);
		else
			SDL_BlitSurface(session_dib,&src,SDL_GetWindowSurface(session_hwnd),&session_ideal);
		#ifndef DEBUG
		if (session_signal&SESSION_SIGNAL_DEBUG)
			session_centre(session_dbg_dib,0);
		#endif
		if (q)
			SDL_UpdateWindowSurface(session_hwnd);
	}
}

void session_ui_init(void) { memset(kbd_bit,0,sizeof(kbd_bit)); session_please(); }
void session_ui_exit(void) { session_redraw(1); }

int session_ui_char; // ASCIZ interpretation of the latest keystroke
int session_ui_exchange(void) // update window and wait for a keystroke
{
	session_ui_char=0; SDL_Event event;
	SDL_UpdateWindowSurface(session_hwnd);
	for (;;)
	{
		SDL_WaitEvent(&event);
		switch (event.type)
		{
			//case SDL_WINDOWEVENT_FOCUS_GAINED: // force full redraw
			#ifndef __WIN32__ // seemingly unavailable on Windows ???
			case SDL_WINDOWEVENT_EXPOSED:
				SDL_FillRect(SDL_GetWindowSurface(session_hwnd),NULL,0); // wipe leftovers
				session_redraw(1);//SDL_UpdateWindowSurface(session_hwnd);//
				break;
			#endif
			case SDL_TEXTINPUT:
				session_ui_char=event.text.text[0];
				if (session_ui_char&128)
					session_ui_char=128+(session_ui_char&1)*64+(event.text.text[1]&63);
				return 0;
			case SDL_KEYDOWN:
				return event.key.keysym.scancode;
			case SDL_QUIT:
				return KBCODE_ESCAPE;
		}
	}
}
void session_ui_printglyph(SDL_Surface *t,char z,int x,int y) // print one glyph
{
	BYTE const *r=&onscreen_chrs[((z-32)&127)*8];
	VIDEO_DATATYPE q0,q1;
	if (z&128)
		q0=VIDEO1(0),q1=VIDEO1(-1);
	else
		q1=VIDEO1(0),q0=VIDEO1(-1);
	BYTE yy=0;
	while (yy<(SESSION_UI_HEIGHT-8)/2)
	{
		VIDEO_DATATYPE *p=(VIDEO_DATATYPE *)&((BYTE *)t->pixels)[(y*SESSION_UI_HEIGHT+yy)*t->pitch]; p=&p[x*8];
		for (BYTE xx=0;xx<8;++xx)
			*p++=q0;
		++yy;
	}
	while (yy<(SESSION_UI_HEIGHT-8)/2+8)
	{
		VIDEO_DATATYPE *p=(VIDEO_DATATYPE *)&((BYTE *)t->pixels)[(y*SESSION_UI_HEIGHT+yy)*t->pitch]; p=&p[x*8];
		BYTE rr=*r++; rr|=rr>>1; // normal, not thin or bold
		for (BYTE xx=0;xx<8;++xx)
			*p++=(rr&(128>>xx))?q1:q0;
		++yy;
	}
	while (yy<SESSION_UI_HEIGHT)
	{
		VIDEO_DATATYPE *p=(VIDEO_DATATYPE *)&((BYTE *)t->pixels)[(y*SESSION_UI_HEIGHT+yy)*t->pitch]; p=&p[x*8];
		for (BYTE xx=0;xx<8;++xx)
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
				z='.';
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

void session_ui_menu(void) // show the menu and set session_event accordingly
{
	if (!session_ui_menusize)
		return;
	session_ui_init();
	SDL_Surface *hmenu,*hitem=NULL;
	int menu=0,menus=0,menuz=-1,item=0,items=0,itemz=-1,itemx=0,itemw=0;
	char *zz,*z;
	hmenu=session_ui_alloc(session_ui_menusize-2,1);
	//session_ui_printasciz(hmenu,"",0,0,VIDEO_PIXELS_X/8-4,0);//SDL_FillRect(,NULL,VIDEO1(0xFFFFFF));
	for (;;) // redraw menus and items as required, then obey the user
	{
		int ox=(session_ideal.x+(session_ideal.w-VIDEO_PIXELS_X)/2)/8,
			oy=(session_ideal.y+(session_ideal.h-VIDEO_PIXELS_Y)/2)/SESSION_UI_HEIGHT;
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
				++menus;
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
					}
					++k;
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
				session_event=0;
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				session_shift=!!(session_event&0x4000);
				session_event&=0xBFFF; // cfr.shift
				if (session_event!=0x8000)
				{
					session_ui_free(hmenu);
					session_ui_free(hitem);
					session_ui_exit();
					return;
				}
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

int session_ui_text(char *s,char *t) // see session_message
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
	while (*m)
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
	SDL_Surface *htext=session_ui_alloc(textw,texth);
	session_ui_printasciz(htext,t,0,0,textw,1);
	i=j=0;
	m=session_parmtr;
	while (*s)
	{
		int k=(*m++=*s++);
		++j;
		if (k=='\n')
		{
			m[-1]=j=0;
			session_ui_printasciz(htext,m=session_parmtr,0,++i,textw,0);
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
		session_ui_printasciz(htext,m=session_parmtr,0,++i,textw,0);
	}
	session_centre(htext,1);//SDL_BlitSurface(htext,NULL,SDL_GetWindowSurface(session_hwnd),NULL);
	for (;;)
	{
		switch (session_ui_exchange())
		{
			case KBCODE_ESCAPE:
			case KBCODE_SPACE:
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				session_ui_free(htext);
				session_ui_exit();
				return 0;
		}
	}
}

int session_ui_list(int item,char *s,char *t,void x(void)) // see session_list
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
	for (;;)
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
				if (x)
					x();
				break;
			case KBCODE_ESCAPE:
			case KBCODE_F10:
				item=-1;
				z=NULL;
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				if (item>=0)
					strcpy(session_parmtr,z);
				session_ui_free(hlist);
				session_ui_exit();
				return item;
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
}

int session_ui_input(char *s,char *t) // see session_input
{
	int i,j,first,textw=VIDEO_PIXELS_X/8-4;
	if ((i=j=first=strlen(strcpy(session_parmtr,s)))>=textw)
		return -1; // error!
	session_ui_init();
	SDL_Surface *htext=session_ui_alloc(textw,2);
	session_ui_printasciz(htext,t,0,0,textw,1);
	for (;;)
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
			case KBCODE_ESCAPE:
				j=-1;
			case KBCODE_X_ENTER:
			case KBCODE_ENTER:
				if (j>=0)
					strcpy(s,session_parmtr);
				session_ui_free(htext);
				session_ui_exit();
				return j;
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
	int i=session_ui_list(session_ui_fileflags&1,l,"File access",NULL);
	if (i>=0)
		session_ui_fileflags=i;
}
int session_ui_filedialog_stat(char *s) // -1 = not exist, 0 = file, 1 = directory
{
	if (!s||!*s) return -1; // not even a valid path!
	#ifdef __WIN32__
	int i=GetFileAttributes(s); return i<0?-1:i&FILE_ATTRIBUTE_DIRECTORY?1:0;
	#else
	struct stat a; return stat(s,&a)<0?-1:S_ISDIR(a.st_mode)?1:0;
	#endif
}
int session_ui_filedialog(char *r,char *s,char *t,int q,int f) // see session_filedialog; q here means TAB is available or not, and f means the starting TAB value (q) or whether we're reading (!q)
{
	int i; char *m,*n,basepath[8<<9],pastname[STRMAX],basefind[STRMAX]; // realpath() is an exception, see below
	session_ui_fileflags=f;
	if (m=strrchr(r,PATHCHAR))
		i=m[1],m[1]=0,strcpy(basepath,r),m[1]=i,strcpy(pastname,&m[1]);
	else
		*basepath=0,strcpy(pastname,r);
	#ifdef __WIN32__
	long int w32drives=0,z; char drives[]="::\\"; ;
	for (drives[0]='A';drives[0]<='Z';++drives[0]) // scan drives just once
		if (GetDriveType(drives)>1&&GetDiskFreeSpace(drives,&z,&z,&z,&z))
			w32drives|=1<<(drives[0]-'A');
	#endif
	for (;;)
	{
		i=0; m=session_scratch;

		#ifdef __WIN32__
		if (session_ui_filedialog_stat(basepath)<1) // ensure that basepath exists and is a valid directory
		{
			GetCurrentDirectory(STRMAX,basepath); // fall back to current directory
			session_ui_filedialog_sanitizepath(basepath);
		}
		char *half=m;
		for (drives[0]='A';drives[0]<='Z';++drives[0])
			if (w32drives&(1<<(drives[0]-'A')))
				++i,m=&half[sortedinsert(half,m-half,drives)];
		*m++=*m++='.'; *m++=PATHCHAR; *m++=0; // always before the directories!
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
			getcwd(basepath,STRMAX); // fall back to current directory
			session_ui_filedialog_sanitizepath(basepath);
		}
		*m++=*m++='.'; *m++=PATHCHAR; *m++=0; // always before the directories!
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

		if (session_ui_list(i,session_scratch,t,q?session_ui_filedialog_tabkey:NULL)<0)
			return 0; // user closed the dialog!

		m=session_parmtr;
		while (*m)
			++m;
		if (m[-1]==PATHCHAR) // the user chose a directory
		{
			strcat(strcpy(session_scratch,basepath),session_parmtr);
			#ifdef __WIN32__
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
			realpath(session_scratch,basepath); // realpath() requires the target to be at least 4096 bytes long in Linux 4.10.0-38-generic, Linux 4.15.0-96-generic...!
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
					m=session_parmtr; n=s; // append default extension
					while (*m) // go to end of string
						++m;
					while (*n!='*') // look for asterisks
						++n;
					while (*n=='*') // skip all asterisks
						++n;
					while (*n&&*n!=';') // copy till end of extension
						*m++=*n++;
					*m=0;
				}
				else
					return 0; // failure, quit!
			}
			strcat(strcpy(session_scratch,basepath),session_parmtr); // build full name
			strcpy(session_parmtr,session_scratch); // copy to target, but keep the source...
			if (q||f||session_ui_filedialog_stat(session_parmtr)<0)
				return 1; // the user succesfully chose a file, either extant for reading or new for writing!
			if (session_ui_list(0,"YES\000NO\000\000","Overwrite?",NULL)==0)
				return strcpy(session_parmtr,session_scratch),1; // ...otherwise session_parmtr would hold "YES"!
		}
	}
}

// create, handle and destroy session ------------------------------- //

INLINE char* session_create(char *s) // create video+audio devices and set menu; 0 OK, !0 ERROR
{
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK)<0)
		return (char *)SDL_GetError();
	if (!(session_hwnd=SDL_CreateWindow(caption_version,SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,VIDEO_PIXELS_X,VIDEO_PIXELS_Y,0*SDL_WINDOW_BORDERLESS)))
		return SDL_Quit(),(char *)SDL_GetError();
	if (!(session_dib=SDL_CreateRGBSurface(0,VIDEO_LENGTH_X,VIDEO_LENGTH_Y,32,0xFF0000,0x00FF00,0x0000FF,0)))
		return SDL_Quit(),(char *)SDL_GetError();
	SDL_SetSurfaceBlendMode(session_dib,SDL_BLENDMODE_NONE);
	video_frame=session_dib->pixels;

	#ifndef DEBUG
		session_dbg_dib=SDL_CreateRGBSurface(0,DEBUG_LENGTH_X*8,DEBUG_LENGTH_Y*DEBUG_LENGTH_Z,32,0xFF0000,0x00FF00,0x0000FF,0);
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

	// translate menu data into custom format
	BYTE *t=session_ui_menudata; session_ui_menusize=0;
	while (*s)
	{
		// menu header
		while ((*t++=*s++)!='\n')
			++session_ui_menusize;
		session_ui_menusize+=2;
		t[-1]=0;
		// items
		for (;;)
			if (*s=='=')
			{
				*t++=0x80; *t++=*t++=0; // fake DROP code!
				while (*s++!='\n')
					;
			}
			else if (*s=='0')
			{
				int i=strtol(s,&s,0); // allow either hex or decimal
				*t++=i>>8; *t++=i; *t++=*t++=' ';
				++s; int h=0;
				while ((i=(*t++=*s++))!='\n')
					if (i==9)
					{
						h=1;
						t[-1]=' ';
						*t++='(';
					}
					else if (i=='_')
						--t;
				if (h)
					t++[-1]=')';
				t[-1]=0;
			}
			else
				break;
		*t++=*t++=0;
	}
	*t=0;
	SDL_StartTextInput();
	session_please();
	return NULL;
}

void session_togglefullscreen(void)
{
	SDL_SetWindowFullscreen(session_hwnd,SDL_GetWindowFlags(session_hwnd)&SDL_WINDOW_FULLSCREEN_DESKTOP?0*SDL_WINDOW_BORDERLESS:SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void session_menuinfo(void); // set the current menu flags. Must be defined later on!
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
			//case SDL_WINDOWEVENT_FOCUS_GAINED: // force full redraw
			#ifndef __WIN32__ // seemingly unavailable on Windows ???
			case SDL_WINDOWEVENT_EXPOSED:
				SDL_FillRect(SDL_GetWindowSurface(session_hwnd),NULL,0); // wipe leftovers
				SDL_UpdateWindowSurface(session_hwnd);//session_redraw(1);//
				break;
			#endif
			#ifndef DEBUG
			case SDL_TEXTINPUT:
				if (session_signal&SESSION_SIGNAL_DEBUG) // only relevant for the debugger, see below
					if ((session_event=event.text.text[0])&128)
						session_event=128+(session_event&31)*64+(event.text.text[1]&63);
				break;
			#endif
			case SDL_KEYDOWN: //if (event.key.state==SDL_PRESSED)
				{
					session_shift=!!(event.key.keysym.mod&KMOD_SHIFT);
					if (event.key.keysym.mod&KMOD_ALT&&event.key.keysym.sym==SDLK_RETURN)
					{
						session_togglefullscreen();
						break;
					}
					if (event.key.keysym.sym==SDLK_F10)
					{
						session_event=0x8080; // show menu
						break;
					}
					#ifndef DEBUG
					if (session_signal&SESSION_SIGNAL_DEBUG)
						switch (event.key.keysym.sym)
						{
							case SDLK_LEFT: session_event=8; break;
							case SDLK_RIGHT: session_event=9; break;
							case SDLK_DOWN: session_event=10; break;
							case SDLK_UP: session_event=11; break;
							case SDLK_RETURN: session_event=13; break;
							case SDLK_ESCAPE: session_event=27; break;
							case SDLK_HOME: session_event=28; break;
							case SDLK_END: session_event=29; break;
							case SDLK_PAGEDOWN: session_event=30; break;
							case SDLK_PAGEUP: session_event=31; break;
							case SDLK_SPACE: if (session_shift) session_event=160; break; // special case unlike WM_CHAR
							case SDLK_BACKSPACE: session_event=session_shift?9:8; break;
							case SDLK_TAB: session_event=session_shift?7:12; break;
							default: session_event=0; break;
						}
					#endif
					int k=session_key_n_joy(event.key.keysym.scancode);
					if (k<128)
					{
						#ifndef DEBUG
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
					session_shift=!!(event.key.keysym.mod&KMOD_SHIFT);
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
			#ifndef DEBUG
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

INLINE void session_writewave(AUDIO_DATATYPE *t); // save the current sample frame. Must be defined later on!
INLINE void session_render(void) // update video, audio and timers
{
	int i,j;
	static int performance_t=0,performance_f=0,performance_b=0; ++performance_f;
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
		else if (j<0)
			video_framecount=-2; // automatic frameskip!
		session_timer+=1000/VIDEO_PLAYBACK;
	}

	if (session_audio)
	{
		static BYTE s=1;
		if (s!=audio_disabled)
			if (s=audio_disabled) // silent mode needs cleanup
				memset(audio_buffer,AUDIO_ZERO,sizeof(audio_buffer));
		j=SDL_GetQueuedAudioSize(session_audio);
		if (j<=sizeof(audio_buffer)*AUDIO_N_FRAMES) // buffer full?
		{
			SDL_QueueAudio(session_audio,audio_buffer,sizeof(audio_buffer));
			if (j<=sizeof(audio_buffer)*AUDIO_N_FRAMES/2) // buffer not full enough?
				i+=500/VIDEO_PLAYBACK; // force speedup by mangling the timer!
		}
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
	SDL_FreeSurface(session_dib);
	SDL_DestroyWindow(session_hwnd);
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
	return strcat(strcpy(session_parmtr,session_path),MY_CAPTION ".INI");
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
			s++; // skip A
			*t++=*s++; // copy R
			*t++=*s++; // copy G
			*t++=*s++; // copy B
#else
			*t++=*s++; // copy B
			*t++=*s++; // copy R
			*t++=*s++; // copy G
			s++; // skip A
#endif
		}
		fwrite(r,1,VIDEO_PIXELS_X*3,f);
	}
}

// menu item functions ---------------------------------------------- //

void session_menucheck(int id,int q) // set option `id`state as `q`
{
	BYTE *m=session_ui_menudata;
	while (*m) // scan menu data for items
	{
		while (*m++) // skip menu name
			;
		int j;
		while (j=*m++<<8,j+=*m++,j)
		{
			if (j==id)
				*m=q?'*':' ';
			while (*m++) // skip item name
				;
		}
	}
}
void session_menuradio(int id,int a,int z) // select option `id` in the range `a-z`
{
	BYTE *m=session_ui_menudata;
	while (*m) // scan menu data for items
	{
		while (*m++) // skip menu name
			;
		int j;
		while (j=*m++<<8,j+=*m++,j)
		{
			if (j>=a&&j<=z)
				*m=j==id?'>':' ';
			while (*m++) // skip item name
				;
		}
	}
}

// message box ------------------------------------------------------ //

int session_message(char *s,char *t) { return session_ui_text(s,t); } // show multi-lined text `s` under caption `t`
int session_aboutme(char *s,char *t) { return session_ui_text(s,t); } // special case: "About.."

// input dialog ----------------------------------------------------- //

int session_input(char *s,char *t) { return session_ui_input(s,t); } // `s` is the target string (empty or not), `t` is the caption; returns -1 on error or LENGTH on success

// list dialog ------------------------------------------------------ //

int session_list(int i,char *s,char *t) { return session_ui_list(i,s,t,NULL); } // `s` is a list of ASCIZ entries, `i` is the default chosen item, `t` is the caption; returns -1 on error or 0..n-1 on success

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

// 'i' = lil-endian (Intel), 'm' = big-endian (Motorola)

#if SDL_BYTEORDER == SDL_BIG_ENDIAN

//#define mgetc(x) (*(x))
//#define mputc(x,y) (*(x)=(y))
int  mgetii(unsigned char *x) { return (x[1]<<8)+*x; }
void mputii(unsigned char *x,int y) { x[1]=y>>8; *x=y; }
int  mgetiiii(unsigned char *x) { return (x[3]<<24)+(x[2]<<16)+(x[1]<<8)+*x; }
void mputiiii(unsigned char *x,int y) { x[3]=y>>24; x[2]=y>>16; x[1]=y>>8; *x=y; }
#define mgetmm(x) (*(WORD*)(x))
#define mgetmmmm(x) (*(DWORD*)(x))
#define mputmm(x,y) ((*(WORD*)(x))=(y))
#define mputmmmm(x,y) ((*(DWORD*)(x))=(y))

int fgetii(FILE *f) { int i=fgetc(f); return i+(fgetc(f)<<8); } // common lil-endian 16-bit fgetc()
int fputii(int i,FILE *f) { fputc(i,f); return fputc(i>>8,f); } // common lil-endian 16-bit fputc()
int fgetiiii(FILE *f) { int i=fgetc(f); i+=fgetc(f)<<8; i+=fgetc(f)<<16; return i+(fgetc(f)<<24); } // common lil-endian 32-bit fgetc()
int fputiiii(int i,FILE *f) { fputc(i,f); fputc(i>>8,f); fputc(i>>16,f); return fputc(i>>24,f); } // common lil-endian 32-bit fputc()
int fgetmm(FILE *f) { int i=0; return (fread(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fgetc()
int fputmm(int i,FILE *f) { return (fwrite(&i,1,2,f)!=2)?EOF:i; } // native lil-endian 16-bit fputc()
int fgetmmmm(FILE *f) { int i=0; return (fread(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fgetc()
int fputmmmm(int i,FILE *f) { return (fwrite(&i,1,4,f)!=4)?EOF:i; } // native lil-endian 32-bit fputc()

#else

//#define mgetc(x) (*(x))
//#define mputc(x,y) (*(x)=(y))
#define mgetii(x) (*(WORD*)(x))
#define mgetiiii(x) (*(DWORD*)(x))
#define mputii(x,y) ((*(WORD*)(x))=(y))
#define mputiiii(x,y) ((*(DWORD*)(x))=(y))
int  mgetmm(unsigned char *x) { return (*x<<8)+x[1]; }
void mputmm(unsigned char *x,int y) { *x=y>>8; x[1]=y; }
int  mgetmmmm(unsigned char *x) { return (*x<<24)+(x[1]<<16)+(x[2]<<8)+x[3]; }
void mputmmmm(unsigned char *x,int y) { *x=y>>24; x[1]=y>>16; x[2]=y>>8; x[3]=y; }

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
