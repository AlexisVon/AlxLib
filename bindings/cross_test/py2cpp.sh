#!/bin/bash
# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# Python encode → C++ decode
# Usage: ./py2cpp.sh [--keep]
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE="$DIR/cache"
KEEP="${1:-}"
ROOT="$DIR/../.."

if [ ! -f "$ROOT/bin/libalxbase.so" ]; then mkdir -p "$ROOT/build" && cd "$ROOT/build" && cmake .. >/dev/null && make alxbase -j$(nproc --ignore=1) >/dev/null; fi
make -C "$DIR/../python" >/dev/null
mkdir -p "$CACHE"

g++ -std=c++17 -I"$ROOT/include" -I"$ROOT/include/alxbase" -O2 -o "$CACHE/test_cpp" \
    "$DIR/test_cpp.cpp" \
    -L"$ROOT/bin" -lalxbase -Wl,-rpath,"$ROOT/bin"

echo "=== py → cpp ==="
python3 "$DIR/test_py.py" encode "$CACHE/data.bin"
LD_LIBRARY_PATH="$ROOT/bin" "$CACHE/test_cpp" decode "$CACHE/data.bin"

[ "$KEEP" = "--keep" ] || rm -rf "$CACHE"
echo "PASS"
