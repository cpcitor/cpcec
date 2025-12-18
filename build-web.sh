#!/bin/bash
# CPCEC WebAssembly Build Script
# This script builds CPCEC for running in web browsers using Emscripten
#
# Prerequisites:
#   - Emscripten SDK installed and activated
#   - ROM files present in the project directory
#
# Usage:
#   ./build-web.sh         # Build the emulator
#   ./build-web.sh clean   # Clean build artifacts
#   ./build-web.sh serve   # Build and start local server
#   ./build-web.sh debug   # Build with debug symbols

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project settings
PROJECT="cpcec"
OUTPUT_DIR="web"
SOURCES="cpcec-web.c"

# ROM files needed for the emulator
ROMS="cpc464.rom cpc664.rom cpc6128.rom cpcplus.rom cpcados.rom"

# Print with color
print_status() {
    echo -e "${BLUE}[*]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

# Check Emscripten installation
check_emscripten() {
    print_status "Checking Emscripten installation..."
    
    if ! command -v emcc &> /dev/null; then
        print_error "emcc not found!"
        echo ""
        echo "Please install and activate Emscripten SDK:"
        echo "  1. git clone https://github.com/emscripten-core/emsdk.git"
        echo "  2. cd emsdk"
        echo "  3. ./emsdk install latest"
        echo "  4. ./emsdk activate latest"
        echo "  5. source ./emsdk_env.sh"
        echo ""
        exit 1
    fi
    
    print_success "Emscripten found: $(emcc --version | head -n1)"
}

# Check ROM files
check_roms() {
    print_status "Checking ROM files..."
    local missing=0
    
    for rom in $ROMS; do
        if [ -f "$rom" ]; then
            print_success "Found: $rom"
        else
            print_warning "Missing: $rom"
            missing=1
        fi
    done
    
    if [ $missing -eq 1 ]; then
        echo ""
        print_warning "Some ROM files are missing. The emulator needs these files to work."
        print_warning "You can obtain them legally from Amstrad's official sources."
        echo ""
    fi
}

# Create output directory
prepare() {
    print_status "Preparing build environment..."
    mkdir -p "$OUTPUT_DIR"
    print_success "Output directory ready: $OUTPUT_DIR/"
}

# Build flags
get_common_flags() {
    echo "-DSDL2 -DEMSCRIPTEN"
    echo "-fno-strict-aliasing"
    echo "-Wno-unused-variable -Wno-unused-but-set-variable"
}

get_emscripten_flags() {
    echo "-s USE_SDL=2"
    echo "-s ALLOW_MEMORY_GROWTH=1"
    echo "-s INITIAL_MEMORY=67108864"
    echo "-s STACK_SIZE=1048576"
    echo "-s ASYNCIFY=1"
    echo "-s ASYNCIFY_STACK_SIZE=65536"
    echo "-s FORCE_FILESYSTEM=1"
    echo "-s EXPORTED_RUNTIME_METHODS=['allocateUTF8','UTF8ToString','ccall','cwrap','FS']"
    echo "-s EXPORTED_FUNCTIONS=['_main','_em_load_file','_em_reset','_em_pause','_em_set_speed','_em_is_running','_em_get_status','_em_key_press','_em_key_release','_em_press_fn','_em_release_fn','_malloc','_free']"
    echo "-s MODULARIZE=0"
    echo "-s ENVIRONMENT='web'"
    echo "-s EXIT_RUNTIME=0"
    echo "-s MIN_WEBGL_VERSION=1"
    echo "-s MAX_WEBGL_VERSION=2"
}

get_preload_flags() {
    local flags=""
    for rom in $ROMS; do
        if [ -f "$rom" ]; then
            flags="$flags --preload-file $rom@/roms/$rom"
        fi
    done
    echo "$flags"
}

# Build the project
build() {
    local build_type="${1:-release}"
    
    print_status "Building CPCEC for WebAssembly ($build_type)..."
    
    local cflags=$(get_common_flags)
    local emflags=$(get_emscripten_flags)
    local preload=$(get_preload_flags)
    
    case "$build_type" in
        debug)
            cflags="$cflags -g -DDEBUG -O0"
            emflags="$emflags -s ASSERTIONS=2 -s SAFE_HEAP=1"
            ;;
        release)
            cflags="$cflags -O3 -DNDEBUG"
            ;;
        small)
            cflags="$cflags -Os -DNDEBUG"
            emflags="$emflags --closure 1"
            ;;
    esac
    
    echo ""
    print_status "Compiler: emcc"
    print_status "Sources: $SOURCES"
    print_status "Output: $OUTPUT_DIR/$PROJECT.js"
    echo ""
    
    # Run the compiler
    emcc $cflags $emflags $preload -o "$OUTPUT_DIR/$PROJECT.js" $SOURCES
    
    if [ $? -eq 0 ]; then
        print_success "Build successful!"
        echo ""
        echo "Output files:"
        ls -lah "$OUTPUT_DIR/$PROJECT".*
        echo ""
        print_status "To test: ./build-web.sh serve"
    else
        print_error "Build failed!"
        exit 1
    fi
}

# Clean build artifacts
clean() {
    print_status "Cleaning build artifacts..."
    rm -f "$OUTPUT_DIR/$PROJECT.js"
    rm -f "$OUTPUT_DIR/$PROJECT.wasm"
    rm -f "$OUTPUT_DIR/$PROJECT.data"
    rm -f "$OUTPUT_DIR/$PROJECT.js.mem"
    rm -f "$OUTPUT_DIR/$PROJECT.worker.js"
    print_success "Clean complete"
}

# Start local server
serve() {
    print_status "Starting local web server..."
    echo ""
    print_success "Server running at: http://localhost:8080"
    print_status "Press Ctrl+C to stop"
    echo ""
    
    cd "$OUTPUT_DIR"
    python3 -m http.server 8080
}

# Show help
show_help() {
    echo "CPCEC WebAssembly Build Script"
    echo "==============================="
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  (none)    Build release version"
    echo "  debug     Build with debug symbols"
    echo "  release   Build optimized release"
    echo "  small     Build size-optimized version"
    echo "  clean     Remove build artifacts"
    echo "  serve     Start local web server"
    echo "  check     Check prerequisites"
    echo "  help      Show this help"
    echo ""
    echo "Examples:"
    echo "  $0              # Build release version"
    echo "  $0 debug        # Build debug version"
    echo "  $0 serve        # Build and start server"
    echo ""
}

# Main entry point
case "${1:-build}" in
    build|release)
        check_emscripten
        check_roms
        prepare
        build release
        ;;
    debug)
        check_emscripten
        check_roms
        prepare
        build debug
        ;;
    small)
        check_emscripten
        check_roms
        prepare
        build small
        ;;
    clean)
        clean
        ;;
    serve)
        if [ ! -f "$OUTPUT_DIR/$PROJECT.js" ]; then
            check_emscripten
            check_roms
            prepare
            build release
        fi
        serve
        ;;
    check)
        check_emscripten
        check_roms
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        print_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
