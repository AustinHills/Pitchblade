#!/bin/bash

# Pitchblade Linux Packager
# Gathers built artifacts and scripts into a distributable folder

set -e

BUILD_TYPE="Debug" # Default to Debug as per configure script, change to Release for prod
BUILD_DIR="build/plugin/Pitchblade_artefacts/${BUILD_TYPE}"

DIST_DIR="dist_linux"

echo "Packaging for ${BUILD_TYPE}..."

# 1. Prepare Dist Directory
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/Standalone"
mkdir -p "$DIST_DIR/VST3"
mkdir -p "$DIST_DIR/assets"
mkdir -p "$DIST_DIR/scripts"

# 2. Copy Scripts
echo "Copying Scripts..."
cp scripts/install_linux.sh "$DIST_DIR/"
cp scripts/uninstall_linux.sh "$DIST_DIR/"
cp scripts/Pitchblade.desktop "$DIST_DIR/scripts/"
chmod +x "$DIST_DIR/install_linux.sh"
chmod +x "$DIST_DIR/uninstall_linux.sh"

# 3. Copy Assets
echo "Copying Assets..."
# Assuming assets are in plugin/assets mostly, but installer script looks for specific ones
# We'll copy from the source assets
if [ -d "plugin/assets" ]; then
    cp plugin/assets/pb_logo.png "$DIST_DIR/assets/" 2>/dev/null || true
    cp plugin/assets/pb_logo_512.png "$DIST_DIR/assets/" 2>/dev/null || true
fi

# 4. Copy Binaries
echo "Copying Binaries from $BUILD_DIR..."

# Standalone
if [ -f "${BUILD_DIR}/Standalone/Pitchblade" ]; then
    cp "${BUILD_DIR}/Standalone/Pitchblade" "$DIST_DIR/Standalone/"
else
    echo "ERROR: Standalone binary not found at ${BUILD_DIR}/Standalone/Pitchblade"
    echo "Did you run ./configure_linux.sh?"
    exit 1
fi

# VST3
if [ -d "${BUILD_DIR}/VST3/Pitchblade.vst3" ]; then
    cp -r "${BUILD_DIR}/VST3/Pitchblade.vst3" "$DIST_DIR/VST3/"
else
    echo "WARNING: VST3 bundle not found. Skipping."
fi

# 5. Create Archive
echo "Creating Archive..."
tar -czvf Pitchblade_Linux.tar.gz -C "$DIST_DIR" .

echo "----------------------------------------"
echo "Package Created: Pitchblade_Linux.tar.gz"
echo "You can distribute this file."
echo "----------------------------------------"
