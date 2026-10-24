#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_DIR="$PROJECT_ROOT/build-web"
TARGET_OUTPUT_DIR="$BUILD_DIR/KeyGuide"

PACKAGE_DIR="$BUILD_DIR/itch-web"
ZIP_DIR="$BUILD_DIR/compiled-zip"
ZIP_FILE="$ZIP_DIR/KeyGuide-web.zip"

EMSDK_ENV="$HOME/emsdk/emsdk_env.sh"

echo "========================================"
echo " Activating Emscripten"
echo "========================================"

if [[ ! -f "$EMSDK_ENV" ]]; then
    echo "Error: Emscripten environment not found:"
    echo "  $EMSDK_ENV"
    echo
    echo "Change EMSDK_ENV in this script if emsdk is installed elsewhere."
    exit 1
fi

# Activate emcc, em++, emcmake and related Emscripten tools.
source "$EMSDK_ENV" >/dev/null

echo
echo "========================================"
echo " Configuring Web build"
echo "========================================"

emcmake cmake \
    -S "$PROJECT_ROOT" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DPLATFORM=Web \
    -DCMAKE_BUILD_TYPE=Release

echo
echo "========================================"
echo " Compiling Web build"
echo "========================================"

cmake --build "$BUILD_DIR" --config Release

echo
echo "========================================"
echo " Preparing itch.io folder"
echo "========================================"

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

required_files=(
    "index.html"
    "index.js"
    "index.wasm"
)

for filename in "${required_files[@]}"; do
    source_file="$TARGET_OUTPUT_DIR/$filename"

    if [[ ! -f "$source_file" ]]; then
        echo "Error: expected build output was not found:"
        echo "  $source_file"
        exit 1
    fi

    cp "$source_file" "$PACKAGE_DIR/"
done

# The .data file is generated when preloaded assets exist.
if [[ -f "$TARGET_OUTPUT_DIR/index.data" ]]; then
    cp "$TARGET_OUTPUT_DIR/index.data" "$PACKAGE_DIR/"
fi

# Copy any source-map files when generated.
if compgen -G "$TARGET_OUTPUT_DIR/*.map" >/dev/null; then
    cp "$TARGET_OUTPUT_DIR"/*.map "$PACKAGE_DIR/"
fi

echo
echo "========================================"
echo " Creating itch.io ZIP"
echo "========================================"

mkdir -p "$ZIP_DIR"
rm -f "$ZIP_FILE"

(
    cd "$PACKAGE_DIR"

    zip -r "$ZIP_FILE" . \
        -x "*.DS_Store" \
        -x "__MACOSX/*"
)

echo
echo "========================================"
echo " Web package created successfully"
echo "========================================"
echo
echo "Output folder:"
echo "  $PACKAGE_DIR"
echo
echo "Upload this file to itch.io:"
echo "  $ZIP_FILE"
echo
