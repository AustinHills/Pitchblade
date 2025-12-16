#!/bin/sh

# Pitchblade Linux Build Script (Debug)
# This script handles dependency installation and building in Debug mode.

set -e # Exit on error

echo "=========================================="
echo "Pitchblade Setup & Debug Build"
echo "=========================================="

# 1. Dependency Check & Installation
if command -v apt-get >/dev/null; then
    echo "Detected Debian/Ubuntu-based system."
    echo "Checking/Installing dependencies..."
    
    # Update package list (optional, might need sudo)
    sudo apt-get update
    
    # Determine available WebKit2Gtk version (4.1 is for Ubuntu 24.04+, 4.0 for older)
    if apt-cache show libwebkit2gtk-4.1-dev >/dev/null 2>&1; then
        WEBKIT_PKG="libwebkit2gtk-4.1-dev"
    else
        WEBKIT_PKG="libwebkit2gtk-4.0-dev"
    fi
    echo "Selected WebKit package: $WEBKIT_PKG"

    # Install dependencies
    # We use 'sudo' here, so the user might be prompted for a password.
    sudo apt-get install -y \
        cmake ninja-build build-essential \
        libasound2-dev libjack-jackd2-dev \
        libcurl4-openssl-dev libfreetype6-dev \
        libx11-dev libxcomposite-dev libxcursor-dev \
        libxext-dev libxinerama-dev libxrandr-dev \
        libxrender-dev $WEBKIT_PKG \
        libglu1-mesa-dev mesa-common-dev \
        librubberband-dev pkg-config

else
    echo "Warning: 'apt-get' not found. Skipping automatic dependency installation."
    echo "Please ensure you have the following installed:"
    echo " - CMake, Ninja (or Make), GCC/Clang"
    echo " - JUCE Dependencies (ALSA, JACK, X11, WebKit2Gtk, etc.)"
    echo " - RubberBand Library (librubberband-dev) + pkg-config"
fi

# 2. Build Configuration
echo "Cleaning previous build..."
rm -rf build
rm -f CMakeCache.txt

echo "Configuring CMake (Debug)..."
# Using Ninja if available for faster builds, otherwise Make
if command -v ninja >/dev/null; then
    GENERATOR="-G Ninja"
else
    GENERATOR=""
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug $GENERATOR

# 3. Build
echo "Building Project..."
cmake --build build --config Debug

echo "=========================================="
echo "Build Complete!"
echo "Artifacts are in the 'build' directory."
echo "=========================================="
