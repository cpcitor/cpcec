 //  ####  ######    ####  #######   ####  ------------------------- //
//  ##  ##  ##  ##  ##  ##  ##   #  ##  ##  CPCEC, plain text Amstrad //
// ##       ##  ## ##       ## #   ##       CPC emulator written in C //
// ##       #####  ##       ####   ##       as a postgraduate project //
// ##       ##     ##       ## #   ##       by Cesar Nicolas-Gonzalez //
//  ##  ##  ##      ##  ##  ##   #  ##  ##  since 2018-12-01 till now //
 //  ####  ####      ####  #######   ####  ------------------------- //

// Emscripten/WebAssembly platform support for browser execution.
// This file provides the necessary adaptations to run CPCEC in a web browser
// using Emscripten's SDL2 port and the main loop callback mechanism.

// START OF EMSCRIPTEN DEFINITIONS ================================== //

#ifndef __EMSCRIPTEN__
#error "This file should only be included when compiling with Emscripten"
#endif

#include <emscripten.h>
#include <emscripten/html5.h>

// Emscripten main loop state
static int emscripten_running = 1;
static int emscripten_initialized = 0;

// Forward declarations for main loop
void emscripten_main_loop(void);
int emscripten_init(int argc, char *argv[]);
void emscripten_cleanup(void);

// Browser-specific file system helpers
INLINE int emscripten_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

// Persistent storage sync (for save states, config, etc.)
INLINE void emscripten_sync_fs(void)
{
    #ifdef EMSCRIPTEN_PERSIST
    EM_ASM(
        if (typeof FS !== 'undefined' && FS.syncfs) {
            FS.syncfs(false, function(err) {
                if (err) console.error('FS sync error:', err);
            });
        }
    );
    #endif
}

// Load a file from URL into the virtual filesystem
INLINE int emscripten_wget_file(const char *url, const char *path)
{
    return emscripten_wget(url, path);
}

// Check if we're running in a secure context (for audio autoplay, etc.)
INLINE int emscripten_is_secure_context(void)
{
    return EM_ASM_INT({
        return window.isSecureContext ? 1 : 0;
    });
}

// Request user interaction for audio unlock (browsers require this)
INLINE void emscripten_request_audio_unlock(void)
{
    EM_ASM({
        if (typeof SDL2 !== 'undefined' && SDL2.audioContext) {
            if (SDL2.audioContext.state === 'suspended') {
                document.addEventListener('click', function resumeAudio() {
                    SDL2.audioContext.resume();
                    document.removeEventListener('click', resumeAudio);
                }, { once: true });
                document.addEventListener('keydown', function resumeAudioKey() {
                    SDL2.audioContext.resume();
                    document.removeEventListener('keydown', resumeAudioKey);
                }, { once: true });
            }
        }
    });
}

// Get canvas size from browser
INLINE void emscripten_get_canvas_size(int *width, int *height)
{
    *width = EM_ASM_INT({ return Module.canvas.width; });
    *height = EM_ASM_INT({ return Module.canvas.height; });
}

// Set canvas size
INLINE void emscripten_set_canvas_size_em(int width, int height)
{
    EM_ASM({
        Module.canvas.width = $0;
        Module.canvas.height = $1;
    }, width, height);
}

// Enter/exit fullscreen
INLINE void emscripten_toggle_fullscreen(void)
{
    EmscriptenFullscreenChangeEvent fsce;
    emscripten_get_fullscreen_status(&fsce);
    if (fsce.isFullscreen) {
        emscripten_exit_fullscreen();
    } else {
        EmscriptenFullscreenStrategy strategy = {
            .scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT,
            .canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF,
            .filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT,
        };
        emscripten_request_fullscreen_strategy("#canvas", 1, &strategy);
    }
}

// JavaScript interop: Show message to user
INLINE void emscripten_show_message(const char *title, const char *message)
{
    EM_ASM({
        var title = UTF8ToString($0);
        var msg = UTF8ToString($1);
        if (typeof Module.showMessage === 'function') {
            Module.showMessage(title, msg);
        } else {
            alert(title + '\n\n' + msg);
        }
    }, title, message);
}

// JavaScript interop: Open file dialog
INLINE void emscripten_open_file_dialog(const char *accept, void (*callback)(const char*, int))
{
    EM_ASM({
        var accept = UTF8ToString($0);
        var input = document.createElement('input');
        input.type = 'file';
        input.accept = accept;
        input.onchange = function(e) {
            var file = e.target.files[0];
            if (file) {
                var reader = new FileReader();
                reader.onload = function(event) {
                    var data = new Uint8Array(event.target.result);
                    var filename = '/tmp/' + file.name;
                    try {
                        FS.writeFile(filename, data);
                        // Call the C callback
                        if (typeof Module.onFileLoaded === 'function') {
                            Module.onFileLoaded(filename, data.length);
                        }
                    } catch (err) {
                        console.error('Failed to write file:', err);
                    }
                };
                reader.readAsArrayBuffer(file);
            }
        };
        input.click();
    }, accept);
}

// JavaScript interop: Download file from emulator
INLINE void emscripten_download_file(const char *filename, const void *data, int size)
{
    EM_ASM({
        var filename = UTF8ToString($0);
        var data = HEAPU8.subarray($1, $1 + $2);
        var blob = new Blob([data], { type: 'application/octet-stream' });
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = filename;
        a.click();
        URL.revokeObjectURL(url);
    }, filename, data, size);
}

// Status bar update
INLINE void emscripten_update_status(const char *status)
{
    EM_ASM({
        var status = UTF8ToString($0);
        if (typeof Module.setStatus === 'function') {
            Module.setStatus(status);
        }
        var elem = document.getElementById('status');
        if (elem) elem.textContent = status;
    }, status);
}

// Performance timing
INLINE double emscripten_get_now_ms(void)
{
    return emscripten_get_now();
}

// Drag and drop support
static void emscripten_setup_dragdrop(void)
{
    EM_ASM({
        var canvas = Module.canvas;
        canvas.addEventListener('dragover', function(e) {
            e.preventDefault();
            e.stopPropagation();
        });
        canvas.addEventListener('drop', function(e) {
            e.preventDefault();
            e.stopPropagation();
            var file = e.dataTransfer.files[0];
            if (file) {
                var reader = new FileReader();
                reader.onload = function(event) {
                    var data = new Uint8Array(event.target.result);
                    var filename = '/tmp/' + file.name;
                    try {
                        FS.writeFile(filename, data);
                        if (typeof Module.onFileDrop === 'function') {
                            Module.onFileDrop(filename);
                        }
                    } catch (err) {
                        console.error('Failed to handle dropped file:', err);
                    }
                };
                reader.readAsArrayBuffer(file);
            }
        });
    });
}

// Gamepad/controller support via HTML5 Gamepad API
INLINE int emscripten_get_gamepad_state(int *buttons, int *axes)
{
    return EM_ASM_INT({
        var gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
        for (var i = 0; i < gamepads.length; i++) {
            var gp = gamepads[i];
            if (gp && gp.connected) {
                // Pack button states
                var btnState = 0;
                for (var b = 0; b < Math.min(gp.buttons.length, 16); b++) {
                    if (gp.buttons[b].pressed) btnState |= (1 << b);
                }
                HEAP32[$0 >> 2] = btnState;
                
                // Pack axis values (-128 to 127)
                var axisVal = 0;
                if (gp.axes.length >= 2) {
                    var ax = Math.round(gp.axes[0] * 127);
                    var ay = Math.round(gp.axes[1] * 127);
                    axisVal = ((ay & 0xFF) << 8) | (ax & 0xFF);
                }
                HEAP32[$1 >> 2] = axisVal;
                return 1;
            }
        }
        return 0;
    }, buttons, axes);
}

// Touch controls support
static int emscripten_touch_x = -1, emscripten_touch_y = -1;
static int emscripten_touch_active = 0;

static EM_BOOL emscripten_touch_callback(int eventType, const EmscriptenTouchEvent *e, void *userData)
{
    if (e->numTouches > 0) {
        EmscriptenTouchPoint touch = e->touches[0];
        emscripten_touch_x = touch.targetX;
        emscripten_touch_y = touch.targetY;
        emscripten_touch_active = (eventType != EMSCRIPTEN_EVENT_TOUCHEND && 
                                    eventType != EMSCRIPTEN_EVENT_TOUCHCANCEL);
    } else {
        emscripten_touch_active = 0;
    }
    return EM_TRUE;
}

static void emscripten_setup_touch(void)
{
    emscripten_set_touchstart_callback("#canvas", NULL, EM_TRUE, emscripten_touch_callback);
    emscripten_set_touchend_callback("#canvas", NULL, EM_TRUE, emscripten_touch_callback);
    emscripten_set_touchmove_callback("#canvas", NULL, EM_TRUE, emscripten_touch_callback);
    emscripten_set_touchcancel_callback("#canvas", NULL, EM_TRUE, emscripten_touch_callback);
}

// Keyboard focus management for canvas
static void emscripten_setup_keyboard(void)
{
    EM_ASM({
        var canvas = Module.canvas;
        canvas.tabIndex = 1; // Make canvas focusable
        canvas.style.outline = 'none'; // Remove focus outline
        canvas.addEventListener('click', function() {
            canvas.focus();
        });
        // Prevent default for game keys
        document.addEventListener('keydown', function(e) {
            if (document.activeElement === canvas) {
                // Prevent browser shortcuts for common game keys
                if ([32, 37, 38, 39, 40, 9].indexOf(e.keyCode) !== -1) {
                    e.preventDefault();
                }
            }
        });
    });
}

// Initialize Emscripten-specific features
INLINE void emscripten_platform_init(void)
{
    emscripten_setup_keyboard();
    emscripten_setup_touch();
    emscripten_setup_dragdrop();
    emscripten_request_audio_unlock();
    
    // Set initial status
    emscripten_update_status("Ready");
}

// Cleanup
INLINE void emscripten_platform_cleanup(void)
{
    emscripten_sync_fs();
}

// Export functions for JavaScript
EMSCRIPTEN_KEEPALIVE
void em_load_file(const char *path)
{
    // This will be called from JavaScript when a file is loaded
    // The actual implementation depends on the emulator's file loading mechanism
    strcpy(session_parmtr, path);
    session_event = 0x8000; // Signal file load event
}

EMSCRIPTEN_KEEPALIVE
void em_reset(void)
{
    session_event = 0x8500; // Reset event (adjust based on actual menu codes)
}

EMSCRIPTEN_KEEPALIVE
void em_pause(void)
{
    session_event = 0x0F00; // Pause event
}

EMSCRIPTEN_KEEPALIVE
void em_set_speed(int fast)
{
    session_fast = fast ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int em_is_running(void)
{
    return emscripten_running;
}

// ====================================== END OF EMSCRIPTEN DEFINITIONS //
