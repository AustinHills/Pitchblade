#!/bin/bash

# Pitchblade Linux Uninstaller

set -e

echo "----------------------------------------"
echo "Pitchblade Linux Uninstaller"
echo "----------------------------------------"

# Detect User vs Root
if [ "$EUID" -eq 0 ]; then
  echo "Running as ROOT (System-wide Uninstall)"
  BIN_DIR="/usr/local/bin"
  VST_DIR="/usr/lib/vst3"
  APP_DIR="/usr/share/applications"
  ICON_DIR="/usr/share/icons/hicolor/512x512/apps"
else
  echo "Running as USER (Local Uninstall)"
  BIN_DIR="$HOME/.local/bin"
  VST_DIR="$HOME/.vst3"
  APP_DIR="$HOME/.local/share/applications"
  ICON_DIR="$HOME/.local/share/icons/hicolor/512x512/apps"
fi

echo "Removing files..."

# 1. Remove Standalone
if [ -f "$BIN_DIR/Pitchblade" ]; then
    rm "$BIN_DIR/Pitchblade"
    echo "Removed: $BIN_DIR/Pitchblade"
else
    echo "Not Found: $BIN_DIR/Pitchblade"
fi

# 2. Remove VST3
if [ -d "$VST_DIR/Pitchblade.vst3" ]; then
    rm -rf "$VST_DIR/Pitchblade.vst3"
    echo "Removed: $VST_DIR/Pitchblade.vst3"
else
    echo "Not Found: $VST_DIR/Pitchblade.vst3"
fi

# 3. Remove Desktop Entry
if [ -f "$APP_DIR/Pitchblade.desktop" ]; then
    rm "$APP_DIR/Pitchblade.desktop"
    echo "Removed: $APP_DIR/Pitchblade.desktop"
    
    if command -v update-desktop-database >/dev/null; then
        update-desktop-database "$APP_DIR"
    fi
fi

# 4. Remove Icon
if [ -f "$ICON_DIR/pitchblade.png" ]; then
    rm "$ICON_DIR/pitchblade.png"
    echo "Removed: $ICON_DIR/pitchblade.png"
fi

echo "----------------------------------------"
echo "Uninstallation Complete."
echo "----------------------------------------"
