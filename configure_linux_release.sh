#!/bin/sh

# Pitchblade Linux Release Build Script
# Builds the plugin in Release mode (RelWithDebInfo).

# set -e # Disabled to allow retry logic

echo "=========================================="
echo "Pitchblade Release Build (Linux)"
echo "=========================================="

echo "Cleaning previous build..."
rm -rf build
rm -f CMakeCache.txt

echo "Configuring CMake (RelWithDebInfo)..."
if command -v ninja >/dev/null; then
    GENERATOR="-G Ninja"
else
    GENERATOR=""
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo $GENERATOR

echo "Building Plugin..."
# We build the 'Pitchblade' target (or Pitchblade_All if defined, usually just all is default)
if ! cmake --build build --config RelWithDebInfo; then
    echo "=========================================="
    echo "Build failed! This might be an Out-Of-Memory (OOM) error."
    echo "Retrying with single-core (low memory mode)..."
    echo "=========================================="
    cmake --build build --config RelWithDebInfo -j 1
fi

echo "=========================================="
echo "Release Build Complete!"
echo "Note: Installer generation is not currently strictly defined for Linux."
echo "You can find the plugin artifacts in 'build/plugin/Pitchblade_artefacts/'"
echo "=========================================="
