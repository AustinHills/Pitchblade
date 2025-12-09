# --- Pitchblade NSIS Installer Script ---

# 1. Name of the installer executable
OutFile "Pitchblade_Installer.exe"

# 2. Default installation folder (Standard VST3 path)
InstallDir "$PROGRAMFILES64\Common Files\VST3"

# 3. Request admin privileges
RequestExecutionLevel admin

# --- NEW: License Configuration ---
# This reads the text from the LICENSE file you created in the root directory.
LicenseData "LICENSE"
LicenseText "Please review the license agreement before installing Pitchblade. You must accept the terms of the GNU GPL v3 to continue."

# --- NEW: Wizard Pages ---
# 1. License Page: Shows the license and requires "I Agree" to continue
Page license

# 2. Installation Page: Shows the progress bar while copying files
Page instfiles

# --- Installation Logic ---
Section "Install"
    # Set the destination folder to the VST3 directory
    SetOutPath "$INSTDIR"
    
    # Copy the VST3 plugin bundle
    # Note: Ensure "RelWithDebInfo" matches your actual build output folder (e.g., might be "Release")
    File /r "build\plugin\Pitchblade_artefacts\RelWithDebInfo\VST3\Pitchblade.vst3"

    # GOOD PRACTICE: Copy the LICENSE file to the install directory so the user has a copy
    SetOutPath "$INSTDIR\Pitchblade.vst3\Contents\Resources" 
    File "LICENSE"
SectionEnd