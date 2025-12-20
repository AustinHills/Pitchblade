# --- Pitchblade NSIS Installer Script ---
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

# 1. General Configuration
Name "Pitchblade"
OutFile "Pitchblade_Installer.exe"
InstallDir "$PROGRAMFILES64\Pitchblade" # Default Standalone Path
RequestExecutionLevel admin

# Variables
Var VST3_DIR
Var STARTMENU_FOLDER
Var DO_RESTART

# 2. MUI Settings
!define MUI_ICON "plugin\assets\pb_logo.ico"
!define MUI_UNICON "plugin\assets\pb_logo.ico"
!define MUI_ABORTWARNING

# 3. Pages

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_COMPONENTS

# --- Standalone Directory Page ---
# Only show if Standalone is selected
!define MUI_PAGE_CUSTOMFUNCTION_PRE DirectoryStandalonePre
!insertmacro MUI_PAGE_DIRECTORY

# --- VST3 Directory Page ---
# We reuse the directory page but point it to a different variable
!define MUI_PAGE_CUSTOMFUNCTION_PRE DirectoryVST3Pre
!define MUI_DIRECTORYPAGE_VARIABLE $VST3_DIR
!define MUI_DIRECTORYPAGE_TEXT_TOP "Select the folder where you want to install the VST3 plugin."
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "VST3 Install Location"
!insertmacro MUI_PAGE_DIRECTORY

# --- Start Menu Page ---
!define MUI_PAGE_CUSTOMFUNCTION_PRE StartMenuPre
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Pitchblade" 
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
!insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

# 4. Languages
!insertmacro MUI_LANGUAGE "English"

# 5. Initialization
# 5. Initialization
Function .onInit
    # Initialize variables
    StrCpy $VST3_DIR "$COMMONFILES64\VST3"

    # 1. Check for previous Standalone InstallDir
    ReadRegStr $0 HKCU "Software\Pitchblade" "InstallDir"
    ${If} $0 != ""
        StrCpy $INSTDIR $0
    ${EndIf}

    # 2. Check for previous VST3 InstallDir
    ReadRegStr $0 HKCU "Software\Pitchblade" "VST3Dir"
    ${If} $0 != ""
        StrCpy $VST3_DIR $0
    ${EndIf}

    # 3. Check for specific restart flag /R
    # Used by auto-updater to restart app after silent install
    ${GetParameters} $R0
    ${GetOptions} $R0 "/R" $R1
    ${If} ${Errors}
        # Flag not present
        StrCpy $DO_RESTART "false"
    ${Else}
        StrCpy $DO_RESTART "true"
    ${EndIf}
FunctionEnd

Function .onInstSuccess
    ${If} $DO_RESTART == "true"
        Exec "$INSTDIR\Pitchblade.exe"
    ${EndIf}
FunctionEnd

# 7. Installer Sections

# --- Standalone Application ---
Section "Standalone Application" SecStandalone
    SetOutPath "$INSTDIR"
    
    # [PERSISTENCE] Save the installation path for future upgrades
    WriteRegStr HKCU "Software\Pitchblade" "InstallDir" $INSTDIR
    
    # Copy the Standalone Executable
    File "build\plugin\Pitchblade_artefacts\RelWithDebInfo\Standalone\Pitchblade.exe"
    
    # Create Uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    # Start Menu Shortcuts
    !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
        CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
        CreateShortcut "$SMPROGRAMS\$STARTMENU_FOLDER\Pitchblade.lnk" "$INSTDIR\Pitchblade.exe"
        CreateShortcut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

# --- VST3 Plugin ---
Section "VST3 Plugin" SecVST3
    SetOutPath "$VST3_DIR"
    
    # [PERSISTENCE] Save the installation path for future upgrades
    WriteRegStr HKCU "Software\Pitchblade" "VST3Dir" $VST3_DIR
    
    # Clean old bundle in the TARGET directory (custom or default)
    # Use logic to avoid deleting common files if the path is weird, but generally standard
    RMDir /r "$VST3_DIR\Pitchblade.vst3"
    
    # Copy new bundle
    # Note: We must create the .vst3 folder specifically
    SetOutPath "$VST3_DIR\Pitchblade.vst3"
    File /r "build\plugin\Pitchblade_artefacts\RelWithDebInfo\VST3\Pitchblade.vst3\*.*"
    
    # Copy License to VST3 bundle resources
    SetOutPath "$VST3_DIR\Pitchblade.vst3\Contents\Resources"
    File "LICENSE"
SectionEnd

# 8. Descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStandalone} "The standalone Pitchblade application. Defaults to Program Files."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "The VST3 plugin. Defaults to Common Files\VST3, but can be customized."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

# 9. Page Callbacks (Moved here to ensure Section IDs are defined)

Function DirectoryStandalonePre
    ${IfNot} ${SectionIsSelected} ${SecStandalone}
        Abort
    ${EndIf}
FunctionEnd

Function DirectoryVST3Pre
    ${IfNot} ${SectionIsSelected} ${SecVST3}
        Abort
    ${EndIf}
FunctionEnd

Function StartMenuPre
    ${IfNot} ${SectionIsSelected} ${SecStandalone}
        Abort
    ${EndIf}
FunctionEnd

# 10. Uninstaller
Section "Uninstall"
    # Remove Standalone
    Delete "$INSTDIR\Pitchblade.exe"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR" 
    
    # Remove Shortcuts
    !insertmacro MUI_STARTMENU_GETFOLDER Application $STARTMENU_FOLDER
    Delete "$SMPROGRAMS\$STARTMENU_FOLDER\Pitchblade.lnk"
    Delete "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk"
    RMDir "$SMPROGRAMS\$STARTMENU_FOLDER"
    
    # Remove VST3 (Optional? Usually good to ask, but for now we remove what we installed)
    # Note: We are very specific to avoid deleting other things
    RMDir /r "$COMMONFILES64\VST3\Pitchblade.vst3"
SectionEnd