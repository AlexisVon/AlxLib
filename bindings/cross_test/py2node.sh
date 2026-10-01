#!/bin/bash
# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# Python encode → Node.js decode
# Usage: ./py2node.sh [--keep]
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE="$DIR/cache"
KEEP="${1:-}"

make -C "$DIR/../python" >/dev/null
(cd "$DIR/../nodejs" && ./make.sh) >/dev/null 2>&1
mkdir -p "$CACHE"

echo "=== py → node ==="
python3 "$DIR/test_py.py" encode "$CACHE/data.bin"
node "$DIR/test_node.js" decode "$CACHE/data.bin"

[ "$KEEP" = "--keep" ] || rm -rf "$CACHE"
echo "PASS"
