#!/bin/bash
# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# C++ encode → Python decode
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE="$DIR/cache"
ROOT="$DIR/../.."

# Build alxbase (unless it is built already)
if [ ! -f "$ROOT/bin/libalxbase.so" ]; then
    mkdir -p "$ROOT/build" && cd "$ROOT/build" && cmake .. >/dev/null && make alxbase -j$(nproc --ignore=1) >/dev/null
fi
make -C "$DIR/../python" >/dev/null 2>&1
mkdir -p "$CACHE"

g++ -std=c++17 -I"$ROOT/include" -I"$ROOT/include/alxbase" -O2 -o "$CACHE/test_cpp" \
    "$DIR/test_cpp.cpp" \
    -L"$ROOT/bin" -lalxbase -Wl,-rpath,"$ROOT/bin"

echo "=== cpp → py ==="
LD_LIBRARY_PATH="$ROOT/bin" "$CACHE/test_cpp" encode "$CACHE/data.bin"
python3 "$DIR/test_py.py" decode "$CACHE/data.bin"
echo "PASS"
