#!/bin/bash
# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# Node.js encode → Python decode
# Usage: ./node2py.sh [--keep]
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE="$DIR/cache"
KEEP="${1:-}"

make -C "$DIR/../python" >/dev/null
(cd "$DIR/../nodejs" && ./make.sh) >/dev/null 2>&1
mkdir -p "$CACHE"

echo "=== node → py ==="
node "$DIR/test_node.js" encode "$CACHE/data.bin"
python3 "$DIR/test_py.py" decode "$CACHE/data.bin"

[ "$KEEP" = "--keep" ] || rm -rf "$CACHE"
echo "PASS"
