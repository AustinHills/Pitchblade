# --- Pitchblade NSIS Installer Script ---

# 1. Name of the Installer (This fixes the "Name Setup" window title)
Name "Pitchblade"

# 2. Output File Name
OutFile "Pitchblade_Installer.exe"

# 3. Default installation folder (Standard VST3 path)
InstallDir "$PROGRAMFILES64\Common Files\VST3"

# 4. Request admin privileges (Required for writing to Program Files)
RequestExecutionLevel admin

# 5. Installer Icon 
# NOTE: NSIS strictly requires an .ico file. You must convert pb_logo.png to .ico
# and save it as plugin\assets\pb_logo.ico for this to work.
Icon "plugin\assets\pb_logo.ico"

# --- License Configuration ---
LicenseData "LICENSE"
LicenseText "Please review the license agreement before installing Pitchblade. You must accept the terms of the GNU GPL v3 to continue."

# --- Wizard Pages ---
# 1. License Page: Users must click "I Agree"
Page license

# 2. Directory Page (NEW): 
# This solves the issue of immediate installation. It shows the user 
# WHERE it will install and provides an "Install" button to confirm.
Page directory

# 3. Installation Page: Performs the actual file copying
Page instfiles

# --- Installation Logic ---
Section "Install"
    # Set the destination folder to the VST3 directory
    SetOutPath "$INSTDIR"
    
    # Copy the VST3 plugin bundle
    # Note: Check if your build folder is "Release" or "RelWithDebInfo"
    File /r "build\plugin\Pitchblade_artefacts\RelWithDebInfo\VST3\Pitchblade.vst3"

    # Copy the LICENSE file to the install directory
    SetOutPath "$INSTDIR\Pitchblade.vst3\Contents\Resources" 
    File "LICENSE"
SectionEnd