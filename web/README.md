# CPCEC Web - Amstrad CPC Emulator in Browser

This directory contains the WebAssembly build of CPCEC, allowing you to run the Amstrad CPC emulator directly in a web browser.

## Prerequisites

### 1. Install Emscripten SDK

```bash
# Clone the SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Download and install the latest SDK
./emsdk install latest

# Activate the SDK
./emsdk activate latest

# Add to your shell (add to .bashrc/.zshrc for permanent setup)
source ./emsdk_env.sh
```

### 2. ROM Files

You need the following ROM files in the project root directory:
- `cpc464.rom` - CPC 464 firmware
- `cpc664.rom` - CPC 664 firmware  
- `cpc6128.rom` - CPC 6128 firmware
- `cpcplus.rom` - CPC Plus firmware
- `cpcados.rom` - AMSDOS ROM

> **Note**: Amstrad has kindly given permission for distribution of their firmware ROMs for emulation purposes. You can find these legally from various CPC emulator sites.

## Building

### Using the build script (recommended)

```bash
# From the project root directory
./build-web.sh          # Build release version
./build-web.sh debug    # Build with debug symbols
./build-web.sh small    # Build size-optimized version
./build-web.sh clean    # Clean build artifacts
./build-web.sh serve    # Build and start local server
./build-web.sh check    # Check prerequisites
```

### Using Make

```bash
# Build
make -f Makefile.emscripten

# Clean
make -f Makefile.emscripten clean

# Start server
make -f Makefile.emscripten serve
```

## Testing

After building, start a local web server:

```bash
./build-web.sh serve
# or
cd web && python3 -m http.server 8080
```

Then open http://localhost:8080 in your browser.

> **Important**: The emulator must be served from a web server (not opened directly as a file) due to browser security restrictions.

## Output Files

After building, you'll find these files in the `web/` directory:

- `index.html` - Main web page with UI
- `cpcec.js` - Emscripten-generated JavaScript
- `cpcec.wasm` - WebAssembly binary
- `cpcec.data` - Preloaded ROM files

## Browser Support

The emulator should work in modern browsers:
- Chrome/Chromium 70+
- Firefox 65+
- Safari 14+
- Edge 79+

### Mobile Support

Basic touch controls are provided for mobile devices, though a physical keyboard is recommended for the best experience.

## Features

### Supported in Web Version
- Full CPC 464/664/6128 emulation
- CPC Plus ASIC emulation
- Disk image loading (.DSK)
- Tape loading (.CDT)
- Snapshot loading (.SNA)
- Keyboard input
- Joystick/gamepad support
- Audio emulation
- Fullscreen mode

### Drag and Drop
You can drag and drop disk images, tapes, or snapshots directly onto the emulator canvas.

### Keyboard Shortcuts

| Key | Function |
|-----|----------|
| F1-F10 | CPC function keys F0-F9 |
| F11 | Cycle color palette |
| F12 | Screenshot |
| Pause | Pause emulation |
| Alt+Enter | Toggle fullscreen |
| Arrow keys | Joystick directions |
| Z/X | Fire buttons |
| Escape | Menu |

## JavaScript API

The emulator exposes functions that can be called from JavaScript:

```javascript
// Load a file
Module._em_load_file(pathPtr);

// Reset the emulator
Module._em_reset();

// Toggle pause
Module._em_pause();

// Set speed (0=normal, 1=fast)
Module._em_set_speed(1);

// Check if running
Module._em_is_running();
```

## Troubleshooting

### "Failed to load BIOS ROM files"
- Make sure all ROM files are present in the project root before building
- The ROMs are embedded in cpcec.data during build

### No sound
- Click anywhere on the page to enable audio (browser autoplay policy)
- Check that your browser supports Web Audio API

### Slow performance
- Try the release build instead of debug
- Disable browser developer tools
- Close other resource-intensive tabs

### Black screen
- Check browser console for errors
- Ensure WebGL is supported and enabled

## Credits

- **CPCEC** by CNGSOFT - Original emulator
- **WebAssembly port** - Adaptation for browser execution
- **Emscripten** - Compiler toolchain

## License

This project is licensed under the GNU General Public License v3.0 - see the [GPL.txt](../gpl.txt) file for details.
