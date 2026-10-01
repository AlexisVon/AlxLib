#!/bin/bash
# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# Run all 6 cross-language compatibility tests (3×3 matrix)
# Passes --keep to avoid recompiling between tests; cleans cache at end.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE="$DIR/cache"
PASS=0
FAIL=0

mkdir -p "$CACHE"

run_test() {
    echo
    if "$DIR/$1" --keep 2>&1; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "*** FAILED: $1 ***"
    fi
}

run_test cpp2py.sh
run_test cpp2node.sh
run_test py2cpp.sh
run_test py2node.sh
run_test node2cpp.sh
run_test node2py.sh

rm -rf "$CACHE"

echo
echo "========================================="
echo "Results: $PASS passed, $FAIL failed (of 6)"
echo "========================================="
[ $FAIL -eq 0 ] || exit 1
