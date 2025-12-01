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
