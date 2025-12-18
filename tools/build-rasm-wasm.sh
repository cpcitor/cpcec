#!/bin/bash
# Build RASM (Z80 Assembler) as WebAssembly
#
# RASM by Edouard BERGE (Roudoudou)
# https://github.com/EdouardBERGE/rasm
#
# MIT License

set -e

RASM_DIR="/Users/ivanduchauffour/iiivan/rasm"
OUTPUT_DIR="/Users/ivanduchauffour/iiivan/cpcec/web"

# Create output directory if needed
mkdir -p "$OUTPUT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() { echo -e "${BLUE}[*]${NC} $1"; }
print_success() { echo -e "${GREEN}[✓]${NC} $1"; }
print_error() { echo -e "${RED}[✗]${NC} $1"; }

# Check Emscripten
if ! command -v emcc &> /dev/null; then
    print_error "Emscripten not found. Please run: source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

print_status "Building RASM for WebAssembly..."

cd "$RASM_DIR"

# Compile RASM to WebAssembly (same config as pixsaur)
emcc rasm.c \
    -O2 \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=33554432 \
    -s FILESYSTEM=1 \
    -s EXPORTED_RUNTIME_METHODS='["FS","callMain"]' \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="createRASM" \
    -s ENVIRONMENT='web' \
    -DNO_3RD_PARTIES \
    -lm \
    -o "$OUTPUT_DIR/rasm.js"

print_success "RASM compiled successfully!"
echo ""
echo "Output files:"
ls -lh "$OUTPUT_DIR/rasm.js" "$OUTPUT_DIR/rasm.wasm" 2>/dev/null || true

echo ""
print_status "Usage in JavaScript:"
cat << 'EOF'

// Load RASM module
const RASM = await createRASM();

// Write source file to virtual filesystem
RASM.FS.writeFile('/source.asm', `
    org #4000
    ld a,#42
    ret
`);

// Run assembler
RASM.callMain(['/source.asm', '-o', '/output.bin']);

// Read compiled binary
const binary = RASM.FS.readFile('/output.bin');

EOF
