#!/bin/bash
# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# Build and run script_bench
# Usage: ./script_bench_run.sh [-f] [-- bench_args...]
#        CXX=g++ STD=17 OPT=3 ./script_bench_run.sh -f
#   -f   Clean-rebuild library before compiling bench

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"

FORCE_CLEAN=false
BENCH_ARGS=()
PASSTHRU=false

for arg in "$@"; do
    if $PASSTHRU; then
        BENCH_ARGS+=("$arg")
    elif [ "$arg" = "-f" ]; then
        FORCE_CLEAN=true
    elif [ "$arg" = "--" ]; then
        PASSTHRU=true
    else
        BENCH_ARGS+=("$arg")
    fi
done

cd "$PROJ_DIR"

# --- Build library ---
if $FORCE_CLEAN || [ ! -f "$PROJ_DIR/bin/libalxscpt.so" ]; then
    if $FORCE_CLEAN; then
        echo "=== Clean build (CXX=${CXX:-g++} STD=${STD:-17} OPT=${OPT:-3}) ==="
        rm -rf build
    fi
    echo "=== Building alxlib ==="
    cmake -B build -DALXLIB_OPT_LEVEL=${OPT:-3} -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF \
        -DCMAKE_CXX_COMPILER=${CXX:-g++} \
        -DCMAKE_CXX_STANDARD=${STD:-17}
    cmake --build build -j7
fi

# --- Build bench binary ---
echo "=== Building script_bench ==="
GIT_COMMIT=$(git -C "$PROJ_DIR" rev-parse --short HEAD 2>/dev/null || echo "?")
${CXX:-g++} -std=c++${STD:-17} -fPIC -Wall -g -O${OPT:-3} -march=native \
    -I"$PROJ_DIR/include/alxbase" -I"$PROJ_DIR/include/alxcore" -I"$PROJ_DIR/include/alxscpt" \
    -o "$PROJ_DIR/bin/script_bench" "$SCRIPT_DIR/script_bench.cpp" \
    -DGIT_COMMIT=\"$GIT_COMMIT\" \
    -L"$PROJ_DIR/bin" -lalxscpt -lalxcore -lalxbase '-Wl,-rpath,$ORIGIN'

# --- Run ---
echo "=== Running script_bench ==="
sleep 5
exec "$PROJ_DIR/bin/script_bench" "${BENCH_ARGS[@]}"
