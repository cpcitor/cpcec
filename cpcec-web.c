 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// Emscripten/WebAssembly wrapper for browser execution.
// Compile with: emcc -DSDL2 cpcec-web.c -s USE_SDL=2 ...

#ifndef __EMSCRIPTEN__
#error "This file should only be compiled with Emscripten!"
#endif

#define SDL2
#include <emscripten.h>
#include <emscripten/html5.h>

// ===== Global state for Emscripten main loop ===== //
static int em_initialized = 0;
static int em_quit_requested = 0;

// Forward declaration of the main loop iteration
static void em_main_loop(void);

// ===== Include the original source ===== //
// We need to modify cpcec.c behavior, so we'll define some macros first

// Redefine BOOTSTRAP to do nothing - we provide our own main()
#define CPCEC_NO_MAIN
#include "cpcec.c"
#undef CPCEC_NO_MAIN

// ===== Main loop for Emscripten (one frame per call) ===== //
static void em_main_loop(void)
{
    if (!em_initialized || em_quit_requested) {
        return;
    }
    
    // Check for quit request
    if (session_listen()) {
        em_quit_requested = 1;
        emscripten_cancel_main_loop();
        return;
    }
    
    int j, k;
    
    // Run emulation until end of frame
    while (!session_signal) {
        z80_main(
            UNLIKELY(video_pos_x < video_threshold) ? 0 :
            ((VIDEO_LENGTH_X + 15 - video_pos_x) >> 4) << multi_t
        );
    }
    
    // Handle end of frame
    if (session_signal & SESSION_SIGNAL_FRAME) {
        // Audio processing
        if (audio_required) {
            if (audio_pos_z < AUDIO_LENGTH_Z) 
                audio_main(TICKS_PER_FRAME);
            #ifdef PSG_PLAYCITY
            if (!playcity_disabled)
                playcity_main(audio_frame, AUDIO_LENGTH_Z);
            #endif
            #if AUDIO_CHANNELS > 1
            if (audio_surround && audio_mixmode) 
                session_surround(length(audio_stereos) - 1 - audio_mixmode);
            #endif
            audio_playframe();
        }
        
        // On-screen display
        if (video_required && onscreen_flag) {
            if (disc_disabled) {
                onscreen_text(+1, -3, "--\t--", 0);
            } else {
                if ((k = disc_phase & 2)) 
                    onscreen_char(+3, -3, disc_phase & 1 ? 128 + 'R' : 128 + 'W');
                onscreen_byte(+1, -3, disc_track[0], k && ((disc_parmtr[1] & 3) == 0));
                onscreen_byte(+4, -3, disc_track[1], k && ((disc_parmtr[1] & 3) == 1));
            }
            k = tape_disabled ? 0 : 128;
            if (tape_filesize <= 0 || tape_type < 0) {
                onscreen_text(+7, -3, tape ? "REC" : "---", k);
            } else {
                if (tape_skipping) 
                    onscreen_char(+6, -3, (tape_skipping > 0 ? '*' : '+') + k);
                j = (long long int)tape_filetell * 999 / tape_filesize;
                onscreen_char(+7, -3, '0' + j / 100 + k);
                onscreen_byte(+8, -3, j % 100, k);
            }
            if (session_stick | session_key2joy) {
                if (autorun_m) {
                    onscreen_bool(-7, -7, 3, 1, 1);
                    onscreen_bool(-7, -4, 3, 1, 1);
                    onscreen_bool(-8, -6, 1, 5, 1);
                    onscreen_bool(-4, -6, 1, 5, 1);
                } else {
                    onscreen_bool(-6, -8, 1, 2, joy_bit & (1 << 0));
                    onscreen_bool(-6, -5, 1, 2, joy_bit & (1 << 1));
                    onscreen_bool(-8, -6, 2, 1, joy_bit & (1 << 2));
                    onscreen_bool(-5, -6, 2, 1, joy_bit & (1 << 3));
                    onscreen_bool(-8, -2, 2, 1, joy_bit & (5 << 4));
                    onscreen_bool(-5, -2, 2, 1, joy_bit & (5 << 5));
                    if (video_threshold > VIDEO_LENGTH_X / 4) 
                        onscreen_bool(-6, -6, 1, 1, 0);
                }
            }
            session_status();
        }
        
        // Update session and continue
        if (!--autorun_t) autorun_next();
        dac_frame();
        if (ym3_file) ym3_write(), ym3_flush();
        tape_skipping = audio_queue = 0;
        
        if (tape_enabled) {
            if (tape_delay > 0) --tape_delay;
        } else if (tape_delay < 3) {
            ++tape_delay;
        }
        
        if (!tape_fastload) tape_song = 0, tape_loud = 1;
        else if (!tape_song) tape_loud = 1;
        else tape_loud = 0, --tape_song;
        
        tape_output = tape_type < 0 && (pio_port_c & 32);
        
        if (tape_signal) {
            tape_signal = 0;
            session_dirty = 1;
        }
        
        if (tape && tape_filetell < tape_filesize && tape_skipload && 
            !session_filmfile && !tape_disabled && tape_loud) {
            session_fast |= +2;
            audio_disabled |= +2;
        } else {
            session_fast &= ~2;
            audio_disabled &= ~2;
        }
        
        session_update();
    }
}

// ===== JavaScript-callable functions ===== //

EMSCRIPTEN_KEEPALIVE
void em_load_file(const char *path)
{
    if (path && *path) {
        strcpy(session_parmtr, path);
        any_load(session_parmtr, 1);
    }
}

EMSCRIPTEN_KEEPALIVE
void em_reset(void)
{
    all_reset();
}

EMSCRIPTEN_KEEPALIVE
void em_pause(void)
{
    if (!(session_signal & SESSION_SIGNAL_DEBUG)) {
        session_please();
        session_signal ^= SESSION_SIGNAL_PAUSE;
    }
}

EMSCRIPTEN_KEEPALIVE
void em_set_speed(int fast)
{
    session_fast = fast ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int em_is_running(void)
{
    return em_initialized && !em_quit_requested;
}

EMSCRIPTEN_KEEPALIVE
const char* em_get_status(void)
{
    static char status[256];
    if (!em_initialized) {
        snprintf(status, sizeof(status), "initializing");
    } else if (em_quit_requested) {
        snprintf(status, sizeof(status), "stopped");
    } else if (session_signal & SESSION_SIGNAL_PAUSE) {
        snprintf(status, sizeof(status), "paused");
    } else if (session_fast) {
        snprintf(status, sizeof(status), "running (fast)");
    } else {
        snprintf(status, sizeof(status), "running");
    }
    return status;
}

// ===== Main function for Emscripten ===== //

int main(int argc, char *argv[])
{
    // Standard initialization - but we need to handle paths specially for web
    // session_prae would set session_path from argv[0], but in web context
    // we need to point to /roms/ where files are preloaded
    
    // Call session_prae with a fake path that will result in /roms/
    session_prae("/roms/cpcec");
    
    // Ensure session_path points to /roms/ for ROM loading
    strcpy(session_path, "/roms/");
    
    // Also set bios_path explicitly
    strcpy(bios_path, "/roms/");
    
    EM_ASM({
        console.log('CPCEC: session_path set to /roms/');
    });
    
    all_setup(); 
    all_reset();
    
    EM_ASM({
        console.log('CPCEC: all_setup and all_reset completed');
    });
    
    int i = 0, j, k = 0;
    
    // Process command line arguments (from URL parameters potentially)
    while (++i < argc) {
        if (argv[i][0] == '-') {
            j = 1;
            do {
                switch (argv[i][j++]) {
                    case 'c':
                        video_scanline = (BYTE)(argv[i][j++] - '0');
                        if (video_scanline < 0 || video_scanline > 7)
                            i = argc;
                        else
                            video_pageblend = video_scanline & 1, video_scanline >>= 1;
                        break;
                    case 'C':
                        video_type = (BYTE)(argv[i][j++] - '0');
                        if (video_type < 0 || video_type > 4)
                            i = argc;
                        break;
                    case 'd':
                        session_signal = SESSION_SIGNAL_DEBUG;
                        break;
                    case 'g':
                        crtc_type = (BYTE)(argv[i][j++] - '0');
                        if (crtc_type < 0 || crtc_type > 4)
                            i = argc;
                        break;
                    case 'j':
                        session_key2joy = 1;
                        break;
                    case 'J':
                        session_stick = 0;
                        break;
                    case 'o':
                        onscreen_flag = 1;
                        break;
                    case 'O':
                        onscreen_flag = 0;
                        break;
                    case 'r':
                        video_framelimit = (BYTE)(argv[i][j++]) & MAIN_FRAMESKIP_MASK;
                        break;
                    case 'R':
                        session_rhythm = 0;
                        break;
                    case 'S':
                        session_audio = 0;
                        break;
                    case 'W':
                        session_fullblit = 1;
                        break;
                    case '+':
                        session_zoomblit = 0;
                        break;
                    case '!':
                        session_softblit = 1;
                        session_softplay = 0;
                        break;
                    default:
                        break;
                }
            } while (argv[i][j]);
        } else {
            // Non-option argument: treat as file to load
            if (any_load(argv[i], 1))
                k = 1;
        }
    }
    
    if (k) all_reset();
    
    // Load the BIOS ROM - this is critical!
    EM_ASM({
        console.log('CPCEC: Loading BIOS ROM...');
    });
    
    if (bios_reload()) {
        EM_ASM({
            console.error('CPCEC: FATAL - Failed to load BIOS ROM!');
            console.error('CPCEC: Make sure ROM files are in /roms/ directory');
        });
        // Don't return - try to continue anyway for debugging
    } else {
        EM_ASM({
            console.log('CPCEC: BIOS ROM loaded successfully');
        });
    }
    
    // Load AMSDOS ROM
    amsdos_load("cpcados.rom");
    
    // Create the session (SDL window, etc.)
    char *s = session_create(session_menudata);
    if (s) {
        EM_ASM_({
            console.error('CPCEC: Cannot create session:', UTF8ToString($0));
        }, s);
        return 1;
    }
    
    // Setup keyboard
    session_kbdreset();
    session_kbdsetup(kbd_map_xlt, length(kbd_map_xlt) / 2);
    
    // Setup video/audio targets
    video_target = &video_frame[video_pos_y * VIDEO_LENGTH_X + video_pos_x];
    audio_target = audio_frame;
    video_main_xlat();
    video_xlat_clut();
    session_resize();
    
    // Mark as initialized
    audio_disabled = !session_audio;
    em_initialized = 1;
    
    EM_ASM({
        console.log('CPCEC: Emulator initialized successfully');
        if (typeof Module.setStatus === 'function') {
            Module.setStatus('Ready');
        }
    });
    
    // Start the main loop - 0 means use requestAnimationFrame (typically 60fps)
    // The second parameter (0) means don't simulate an infinite loop
    emscripten_set_main_loop(em_main_loop, 0, 0);
    
    return 0;
}

// end of cpcec-web.c
