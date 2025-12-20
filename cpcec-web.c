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

// ===== UI Override flags ===== //
// When set to 1, UI functions will call JavaScript instead of SDL
// The JavaScript callbacks are non-blocking - dialogs are shown but code continues
static int ui_use_web = 1;

// ===== Macro to skip original UI function definitions ===== //
// We define these to override them after include
#define CPCEC_WEB_UI_OVERRIDE 1

// ===== Global state for Emscripten main loop ===== //
static int em_initialized = 0;
static int em_quit_requested = 0;
static int dialog_active = 0;  // Flag to pause main loop during dialogs

// Forward declaration of the main loop iteration
static void em_main_loop(void);

// ===== Include the original source ===== //
// We need to modify cpcec.c behavior, so we'll define some macros first

// Redefine BOOTSTRAP to do nothing - we provide our own main()
#define CPCEC_NO_MAIN
#include "cpcec.c"
#undef CPCEC_NO_MAIN

// ===== Web UI Override Functions ===== //
// NON-BLOCKING approach: dialogs are shown but C code doesn't wait.
// - Messages: fire-and-forget (show dialog, return immediately)
// - Inputs/Lists: show dialog but return -1 (cancelled), user must use F1 menu
// This avoids Asyncify issues with emscripten_set_main_loop and SDL audio.

// Forward declaration
static void em_main_loop(void);

// ===== Dialog state for polling approach ===== //
// When a dialog needs a result, we pause emulation and poll for the answer
static int pending_dialog_type = 0;  // 0=none, 1=message, 2=input, 3=list, 4=scan, 5=file
static int pending_dialog_result = -1;
static int pending_dialog_done = 0;
static char pending_dialog_string[STRMAX+1];

// Non-blocking: just send to JS, don't wait (fire-and-forget for messages)
EM_JS(void, web_ui_show_message, (const char* message, const char* title, int isAbout), {
    var msg = UTF8ToString(message);
    var ttl = UTF8ToString(title);
    
    if (typeof Module.onShowMessage === 'function') {
        // The callback just acknowledges - we don't wait for it
        Module.onShowMessage(msg, ttl, isAbout, function() {
            // Message dismissed - nothing to do
        });
    } else {
        // Queue alert for next frame to not block
        setTimeout(function() { alert(ttl + "\n\n" + msg); }, 0);
    }
});

// Non-blocking: show input dialog, will call back when done
EM_JS(void, web_ui_show_input, (const char* title, const char* currentValue), {
    var ttl = UTF8ToString(title);
    var val = UTF8ToString(currentValue);
    
    if (typeof Module.onShowInput === 'function') {
        Module.onShowInput(ttl, val, function(result) {
            if (result !== null) {
                Module._em_dialog_complete_string(result.length, allocateUTF8(result));
            } else {
                Module._em_dialog_complete(-1);
            }
        });
    } else {
        setTimeout(function() {
            var result = prompt(ttl, val);
            if (result !== null) {
                Module._em_dialog_complete_string(result.length, allocateUTF8(result));
            } else {
                Module._em_dialog_complete(-1);
            }
        }, 0);
    }
});

// Non-blocking: show list dialog
EM_JS(void, web_ui_show_list, (int defaultItem, const char* items, const char* title), {
    var ttl = UTF8ToString(title);
    
    // Parse null-terminated list of items
    var itemList = [];
    var ptr = items;
    while (true) {
        var str = UTF8ToString(ptr);
        if (str === "") break;
        itemList.push(str);
        ptr += lengthBytesUTF8(str) + 1;
    }
    
    if (typeof Module.onShowList === 'function') {
        Module.onShowList(ttl, itemList, defaultItem, function(result) {
            Module._em_dialog_complete(result !== null ? result : -1);
        });
    } else {
        setTimeout(function() {
            var msg = itemList.map(function(item, idx) {
                return (idx + 1) + ": " + item;
            }).join("\n");
            var result = prompt(ttl + "\n\n" + msg + "\n\nEnter number:", String(defaultItem + 1));
            if (result !== null) {
                var idx = parseInt(result, 10) - 1;
                Module._em_dialog_complete(idx >= 0 && idx < itemList.length ? idx : -1);
            } else {
                Module._em_dialog_complete(-1);
            }
        }, 0);
    }
});

// Non-blocking: show key scan dialog
EM_JS(void, web_ui_show_scan, (const char* eventName), {
    var name = UTF8ToString(eventName);
    
    if (typeof Module.onShowKeyScan === 'function') {
        Module.onShowKeyScan(name, function(keyCode) {
            Module._em_dialog_complete(keyCode !== null ? keyCode : -1);
        });
    } else {
        setTimeout(function() {
            alert("Press a key for: " + name);
            var handler = function(e) {
                document.removeEventListener('keydown', handler);
                Module._em_dialog_complete(e.keyCode);
            };
            document.addEventListener('keydown', handler);
        }, 0);
    }
});

// Non-blocking: show file dialog
EM_JS(void, web_ui_show_file, (const char* path, const char* pattern, const char* title, int showReadOnly, int isOpen), {
    var p = UTF8ToString(path);
    var pat = UTF8ToString(pattern);
    var ttl = UTF8ToString(title);
    
    if (typeof Module.onShowFileDialog === 'function') {
        Module.onShowFileDialog(ttl, pat, isOpen, showReadOnly, function(result) {
            if (result !== null) {
                Module._em_dialog_complete_string(1, allocateUTF8(result));
            } else {
                Module._em_dialog_complete(0);
            }
        });
    } else {
        setTimeout(function() {
            if (isOpen) {
                var input = document.createElement('input');
                input.type = 'file';
                input.accept = pat.split(";").map(function(p) { return p.replace("*", ""); }).join(",");
                input.onchange = function(e) {
                    var file = e.target.files[0];
                    if (file) {
                        Module._em_dialog_complete_string(1, allocateUTF8(file.name));
                    } else {
                        Module._em_dialog_complete(0);
                    }
                };
                input.click();
            } else {
                var result = prompt(ttl + "\nEnter filename:", "");
                if (result) {
                    Module._em_dialog_complete_string(1, allocateUTF8(result));
                } else {
                    Module._em_dialog_complete(0);
                }
            }
        }, 0);
    }
});

// Called from JS when dialog completes (int result)
EMSCRIPTEN_KEEPALIVE
void em_dialog_complete(int result) {
    pending_dialog_result = result;
    pending_dialog_done = 1;
    // Resume emulation
    session_signal &= ~SESSION_SIGNAL_PAUSE;
}

// Called from JS when dialog completes with string (file/input)
EMSCRIPTEN_KEEPALIVE
void em_dialog_complete_string(int result, char* str) {
    pending_dialog_result = result;
    if (str && result >= 0) {
        strncpy(pending_dialog_string, str, STRMAX);
        pending_dialog_string[STRMAX] = 0;
        free(str);  // allocated by allocateUTF8
    }
    pending_dialog_done = 1;
    // Resume emulation
    session_signal &= ~SESSION_SIGNAL_PAUSE;
}

// ===== Wait for dialog completion (called in a loop) ===== //
// Returns 1 when dialog is done, 0 if still waiting
static int wait_for_dialog(void) {
    if (pending_dialog_done) {
        pending_dialog_done = 0;
        pending_dialog_type = 0;
        return 1;
    }
    // Keep emulation paused while waiting
    session_signal |= SESSION_SIGNAL_PAUSE;
    return 0;
}

// ===== Override the SDL UI functions ===== //

int session_message_web(char *s, char *t) {
    if (ui_use_web) {
        // Fire and forget - just show the message
        web_ui_show_message(s, t, 0);
        return 0;
    }
    return session_ui_text(s, t, 0);
}

int session_aboutme_web(char *s, char *t) {
    if (ui_use_web) {
        // Fire and forget - just show the message
        web_ui_show_message(s, t, 1);
        return 0;
    }
    return session_ui_text(s, t, 1);
}

int session_line_web(char *t) {
    if (ui_use_web) {
        // Start dialog and pause emulation
        pending_dialog_type = 2;
        pending_dialog_done = 0;
        pending_dialog_result = -1;
        strcpy(pending_dialog_string, session_parmtr);
        session_signal |= SESSION_SIGNAL_PAUSE;
        web_ui_show_input(t, session_parmtr);
        // Poll until done (main loop continues with paused emulation)
        while (!wait_for_dialog()) {
            emscripten_sleep(16);  // ~60fps polling
        }
        if (pending_dialog_result >= 0) {
            strcpy(session_parmtr, pending_dialog_string);
        }
        return pending_dialog_result;
    }
    return session_ui_line(t);
}

int session_list_web(int i, char *s, char *t) {
    if (ui_use_web) {
        // Start dialog and pause emulation
        pending_dialog_type = 3;
        pending_dialog_done = 0;
        pending_dialog_result = -1;
        session_signal |= SESSION_SIGNAL_PAUSE;
        web_ui_show_list(i, s, t);
        // Poll until done
        while (!wait_for_dialog()) {
            emscripten_sleep(16);
        }
        return pending_dialog_result;
    }
    return session_ui_list(i, s, t, NULL, 0);
}

int session_scan_web(char *s) {
    if (ui_use_web) {
        // Start dialog and pause emulation
        pending_dialog_type = 4;
        pending_dialog_done = 0;
        pending_dialog_result = -1;
        session_signal |= SESSION_SIGNAL_PAUSE;
        web_ui_show_scan(s);
        // Poll until done
        while (!wait_for_dialog()) {
            emscripten_sleep(16);
        }
        return pending_dialog_result;
    }
    return session_ui_scan(s);
}

char *session_getfile_web(char *r, char *s, char *t) {
    if (ui_use_web) {
        pending_dialog_type = 5;
        pending_dialog_done = 0;
        pending_dialog_result = 0;
        pending_dialog_string[0] = 0;
        session_signal |= SESSION_SIGNAL_PAUSE;
        web_ui_show_file(r ? r : "", s, t, 0, 1);
        while (!wait_for_dialog()) {
            emscripten_sleep(16);
        }
        if (pending_dialog_result > 0) {
            strcpy(session_parmtr, pending_dialog_string);
            return session_parmtr;
        }
        return NULL;
    }
    return session_ui_filedialog(r, s, t, 0, 1) ? session_parmtr : NULL;
}

char *session_newfile_web(char *r, char *s, char *t) {
    if (ui_use_web) {
        pending_dialog_type = 5;
        pending_dialog_done = 0;
        pending_dialog_result = 0;
        pending_dialog_string[0] = 0;
        session_signal |= SESSION_SIGNAL_PAUSE;
        web_ui_show_file(r ? r : "", s, t, 0, 0);
        while (!wait_for_dialog()) {
            emscripten_sleep(16);
        }
        if (pending_dialog_result > 0) {
            strcpy(session_parmtr, pending_dialog_string);
            return session_parmtr;
        }
        return NULL;
    }
    return session_ui_filedialog(r, s, t, 0, 0) ? session_parmtr : NULL;
}

char *session_getfilereadonly_web(char *r, char *s, char *t, int q) {
    if (ui_use_web) {
        pending_dialog_type = 5;
        pending_dialog_done = 0;
        pending_dialog_result = 0;
        pending_dialog_string[0] = 0;
        session_signal |= SESSION_SIGNAL_PAUSE;
        web_ui_show_file(r ? r : "", s, t, 1, q);
        while (!wait_for_dialog()) {
            emscripten_sleep(16);
        }
        if (pending_dialog_result > 0) {
            strcpy(session_parmtr, pending_dialog_string);
            return session_parmtr;
        }
        return NULL;
    }
    return session_ui_filedialog(r, s, t, 1, q) ? session_parmtr : NULL;
}

// Expose control functions to JavaScript
EMSCRIPTEN_KEEPALIVE
void em_set_web_ui(int enable) {
    ui_use_web = enable;
}

// ===== Main loop for Emscripten (one frame per call) ===== //
static void em_main_loop(void)
{
    // Skip main loop while a dialog is active (Asyncify is suspended)
    if (!em_initialized || em_quit_requested || dialog_active) {
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

// ===== Debug Panel API ===== //
// Expose Z80 registers and memory for the React debug panel

EMSCRIPTEN_KEEPALIVE
int em_get_reg_af(void) { return z80_af.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_bc(void) { return z80_bc.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_de(void) { return z80_de.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_hl(void) { return z80_hl.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_af2(void) { return z80_af2.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_bc2(void) { return z80_bc2.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_de2(void) { return z80_de2.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_hl2(void) { return z80_hl2.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_ix(void) { return z80_ix.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_iy(void) { return z80_iy.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_sp(void) { return z80_sp.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_pc(void) { return z80_pc.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_ir(void) { return z80_ir.w; }

EMSCRIPTEN_KEEPALIVE
int em_get_reg_iff(void) { return z80_iff.w; }

// Read a byte from memory (PEEK)
EMSCRIPTEN_KEEPALIVE
int em_peek(int address) {
    return PEEK(address & 0xFFFF);
}

// Read multiple bytes from memory into a buffer
// Returns pointer to static buffer with the data
EMSCRIPTEN_KEEPALIVE
unsigned char* em_peek_range(int address, int length) {
    static unsigned char buffer[256];
    if (length > 256) length = 256;
    for (int i = 0; i < length; i++) {
        buffer[i] = PEEK((address + i) & 0xFFFF);
    }
    return buffer;
}

// Get stack contents (up to 16 entries)
EMSCRIPTEN_KEEPALIVE
unsigned char* em_get_stack(int count) {
    static unsigned char buffer[32];
    if (count > 16) count = 16;
    for (int i = 0; i < count * 2; i++) {
        buffer[i] = PEEK((z80_sp.w + i) & 0xFFFF);
    }
    return buffer;
}

// Check if emulator is paused
EMSCRIPTEN_KEEPALIVE
int em_is_paused(void) {
    return (session_signal & (SESSION_SIGNAL_PAUSE | SESSION_SIGNAL_DEBUG)) != 0;
}

// Step one instruction (for debugger)
EMSCRIPTEN_KEEPALIVE
void em_step(void) {
    if (session_signal & SESSION_SIGNAL_PAUSE) {
        // Execute one Z80 instruction
        z80_main(1);
    }
}

// Step over: execute until next instruction (skip CALLs)
EMSCRIPTEN_KEEPALIVE
void em_step_over(void) {
    if (session_signal & SESSION_SIGNAL_PAUSE) {
        int opcode = PEEK(z80_pc.w);
        // Check if current instruction is a CALL (CD, C4, CC, D4, DC, E4, EC, F4, FC)
        // or RST (C7, CF, D7, DF, E7, EF, F7, FF)
        int is_call = (opcode == 0xCD) ||
                      (opcode == 0xC4) || (opcode == 0xCC) ||
                      (opcode == 0xD4) || (opcode == 0xDC) ||
                      (opcode == 0xE4) || (opcode == 0xEC) ||
                      (opcode == 0xF4) || (opcode == 0xFC) ||
                      ((opcode & 0xC7) == 0xC7); // RST instructions
        
        if (is_call) {
            // Get instruction length to find return address
            int next_pc;
            if ((opcode & 0xC7) == 0xC7) {
                // RST: 1 byte
                next_pc = (z80_pc.w + 1) & 0xFFFF;
            } else {
                // CALL: 3 bytes
                next_pc = (z80_pc.w + 3) & 0xFFFF;
            }
            // Execute until we return to next_pc or hit a limit
            int limit = 100000;
            while (z80_pc.w != next_pc && limit-- > 0) {
                z80_main(1);
            }
        } else {
            // Regular step
            z80_main(1);
        }
    }
}

// Run/Continue execution
EMSCRIPTEN_KEEPALIVE
void em_run(void) {
    if (session_signal & SESSION_SIGNAL_PAUSE) {
        session_signal &= ~SESSION_SIGNAL_PAUSE;
    }
}

// Run to specific address (breakpoint)
EMSCRIPTEN_KEEPALIVE
void em_run_to(int address) {
    if (session_signal & SESSION_SIGNAL_PAUSE) {
        address &= 0xFFFF;
        int limit = 1000000;
        while (z80_pc.w != address && limit-- > 0) {
            z80_main(1);
        }
    }
}

// ===== CPC Key simulation for keyboards without numpad ===== //
// CPC keyboard matrix codes for function keys (from cpcec.c header)
// F0=0x0F, F1=0x0D, F2=0x0E, F3=0x05, F4=0x14, F5=0x0C, F6=0x04
// F7=0x0A, F8=0x0B, F9=0x03, F.=0x07

EMSCRIPTEN_KEEPALIVE
void em_key_press(int cpc_code)
{
    // Press a CPC key by its matrix code
    if (cpc_code >= 0 && cpc_code < 128) {
        kbd_bit_set(cpc_code);
    }
}

EMSCRIPTEN_KEEPALIVE
void em_key_release(int cpc_code)
{
    // Release a CPC key by its matrix code
    if (cpc_code >= 0 && cpc_code < 128) {
        kbd_bit_res(cpc_code);
    }
}

// Helper to press a CPC function key by number (0-9)
EMSCRIPTEN_KEEPALIVE
void em_press_fn(int fn_num)
{
    // CPC function key matrix codes
    static const int fn_codes[] = {
        0x0F,  // F0
        0x0D,  // F1
        0x0E,  // F2
        0x05,  // F3
        0x14,  // F4
        0x0C,  // F5
        0x04,  // F6
        0x0A,  // F7
        0x0B,  // F8
        0x03   // F9
    };
    if (fn_num >= 0 && fn_num <= 9) {
        kbd_bit_set(fn_codes[fn_num]);
    }
}

EMSCRIPTEN_KEEPALIVE
void em_release_fn(int fn_num)
{
    static const int fn_codes[] = {
        0x0F, 0x0D, 0x0E, 0x05, 0x14, 0x0C, 0x04, 0x0A, 0x0B, 0x03
    };
    if (fn_num >= 0 && fn_num <= 9) {
        kbd_bit_res(fn_codes[fn_num]);
    }
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
