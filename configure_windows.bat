@echo off
REM Rebuilds entire configuration. Run this the first time to setup the project and test suite.
REM USAGE: sh .\ configure_windows.sh

ECHO "Cleaning previous build..."
DEL /S /F /Q "build"
DEL  "CMakeCache.txt"

ECHO "Configuring CMake..."
cmake "-S" "." "-B" "build" "-G" "Visual Studio 17 2022" "-A" "x64" "-T" "host=x64"

ECHO "Building plugin..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake "--build" "build" --target Pitchblade_All

REM --- Check if build was successful ---
IF %ERRORLEVEL% NEQ 0 (
    ECHO "Build FAILED. Cannot create installer."
    pause
    exit /b %ERRORLEVEL%
)

ECHO "Build complete."
ECHO "--------------------------------"
ECHO "Creating installer .exe..."

REM --- This is the new part ---
REM This runs the NSIS compiler on our script
"C:\Program Files (x86)\NSIS\makensis.exe" installer.nsi

IF %ERRORLEVEL% EQU 0 (
    ECHO "SUCCESS: 'Pitchblade_Installer.exe' created in the root folder."
) ELSE (
    ECHO "ERROR: 'makensis' command failed."
    ECHO "Please ensure NSIS is installed and 'makensis.exe' is in your system's PATH."
)
    
ECHO "--------------------------------"
pause
