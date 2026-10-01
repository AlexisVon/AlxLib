#!/bin/bash
# Copyright (c) 2026 AlexisVon

# test.sh - test the AlxLib modules

set -e

# Project root
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# Parse the arguments
FORCE_REBUILD=0
while getopts "f" opt; do
    case $opt in
        f) FORCE_REBUILD=1 ;;
    esac
done

echo "=== AlxLib Test Suite ==="

cd ${ROOT_DIR}

# Reconfigure when -f is given, or when the build directory does not exist
if [ $FORCE_REBUILD -eq 1 ] || [ ! -d "build" ]; then
    rm -rf build
    cmake -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF 2>&1 | tail -1
fi

# Build
echo "Building..."
cmake --build build -j7 2>&1 | tail -1

# Run the tests
echo "Running tests..."
cd build && ctest --output-on-failure && cd ..

echo -e "\n=== All tests passed ==="
