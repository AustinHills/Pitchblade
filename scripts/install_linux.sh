#!/bin/bash

# Pitchblade Universal Linux Installer
# Installs Standalone App and VST3 Plugin

set -e

echo "----------------------------------------"
echo "Pitchblade Linux Installer"
echo "----------------------------------------"

# Detect User vs Root
if [ "$EUID" -eq 0 ]; then
  echo "Running as ROOT (System-wide Install)"
  IS_ROOT=1
  BIN_DIR="/usr/local/bin"
  VST_DIR="/usr/lib/vst3"
  APP_DIR="/usr/share/applications"
  ICON_DIR="/usr/share/icons/hicolor/512x512/apps"
else
  echo "Running as USER (Local Install)"
  IS_ROOT=0
  BIN_DIR="$HOME/.local/bin"
  VST_DIR="$HOME/.vst3"
  APP_DIR="$HOME/.local/share/applications"
  ICON_DIR="$HOME/.local/share/icons/hicolor/512x512/apps"
fi

# Ensure Directories Exist
mkdir -p "$BIN_DIR"
mkdir -p "$VST_DIR"
mkdir -p "$APP_DIR"
mkdir -p "$ICON_DIR"

echo "Installing to:"
echo "  Binaries: $BIN_DIR"
echo "  VST3:     $VST_DIR"

# 1. Install Standalone
if [ -f "./Standalone/Pitchblade" ]; then
    echo "Installing Standalone Application..."
    cp "./Standalone/Pitchblade" "$BIN_DIR/Pitchblade"
    chmod +x "$BIN_DIR/Pitchblade"
else
    echo "WARNING: ./Standalone/Pitchblade binary not found in current folder."
fi

# 2. Install VST3
if [ -d "./VST3/Pitchblade.vst3" ]; then
    echo "Installing VST3 Plugin..."
    rm -rf "$VST_DIR/Pitchblade.vst3" # Remove old version
    cp -r "./VST3/Pitchblade.vst3" "$VST_DIR/"
else
    echo "WARNING: ./VST3/Pitchblade.vst3 bundle not found in current folder."
fi

# 3. Install Icon
if [ -f "./assets/pb_logo_512.png" ]; then
    echo "Installing Icon..."
    cp "./assets/pb_logo_512.png" "$ICON_DIR/pitchblade.png"
elif [ -f "./assets/pb_logo.png" ]; then
     # Fallback if specific size missing
    cp "./assets/pb_logo.png" "$ICON_DIR/pitchblade.png"
fi

# 4. Install Desktop Shortcut
if [ -f "./scripts/Pitchblade.desktop" ]; then
    echo "Installing Desktop Entry..."
    cp "./scripts/Pitchblade.desktop" "$APP_DIR/Pitchblade.desktop"
    
    # Update Exec path in .desktop file if User install
    if [ "$IS_ROOT" -eq 0 ]; then
        sed -i "s|Exec=/usr/local/bin/Pitchblade|Exec=$HOME/.local/bin/Pitchblade|g" "$APP_DIR/Pitchblade.desktop"
    fi
    
    # Refresh database
    if command -v update-desktop-database >/dev/null; then
        update-desktop-database "$APP_DIR"
    fi
fi

echo "----------------------------------------"
echo "Installation Complete!"
echo "You can now launch Pitchblade from your application menu or terminal."
echo "VST3 plugin installed to: $VST_DIR"
echo "----------------------------------------"
