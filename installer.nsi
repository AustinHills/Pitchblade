# --- Pitchblade NSIS Installer Script ---

# 1. The name of the installer .exe this script will create
OutFile "Pitchblade_Installer.exe"

# 2. The default installation folder
InstallDir "$PROGRAMFILES64\Common Files\VST3"

# 3. Request admin privileges (required for Program Files)
RequestExecutionLevel admin

# 4. The "Yes/No" Prompt
# This function runs first when the .exe is opened
Function .onInit
    # Show a popup box with "Yes" and "No" buttons
    MessageBox MB_YESNO|MB_ICONQUESTION "Install Pitchblade?" IDYES install
    
    # If "No" is clicked, quit the installer
    Abort
    
  # If "Yes" is clicked, jump to this label and continue
  install:
FunctionEnd

# 5. The installation logic
# This is the main (and only) section
Section "Install"
    # 6. Set the output path to the user's chosen directory
    SetOutPath "$INSTDIR\Pitchblade"
    
    # 7. This is the command that packages your built plugin
    # It finds the file (relative to this script) and adds it
    # to the installer .exe.
    File /r "build\plugin\Pitchblade_artefacts\Debug\VST3\Pitchblade.vst3\Contents\x86_64-win\Pitchblade.vst3"
SectionEnd