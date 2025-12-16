#!/bin/sh

# Pitchblade Linux Test Script
# Builds and runs unit tests.

set -e

echo "=========================================="
echo "Pitchblade Unit Tests"
echo "=========================================="

# Ensure build directory exists
if [ ! -d "build" ]; then
    echo "Error: 'build' directory not found."
    echo "Please run ./configure_linux.sh first to configure the project."
    exit 1
fi

echo "Building Tests..."
cmake --build build --target runTests

echo "Running Tests..."
cd build
ctest --output-on-failure
cd ..

echo "=========================================="
echo "Tests Complete!"
echo "=========================================="
