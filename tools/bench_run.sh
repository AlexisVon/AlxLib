#!/bin/bash
# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# Build and run one of the tools/*_bench.cpp benchmarks
# Usage: ./bench_run.sh <name> [-f] [-- bench_args...]
#        e.g. ./bench_run.sh ajson
#        CXX=g++ STD=17 OPT=3 ./bench_run.sh avarsolid -f
#   -f   Clean-rebuild library before compiling the bench
#
# Comparing two revisions: export the other one with `git archive <rev> | tar -x -C <dir>`
# and run this same script inside that copy — each tree builds and links its own bin/.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"

NAME="$1" || true
shift || true
if [ -z "$NAME" ] || [ ! -f "$SCRIPT_DIR/${NAME}_bench.cpp" ]; then
    echo "usage: $0 <name> [-f] [-- bench_args...]"
    echo "available: $(cd "$SCRIPT_DIR" && ls *_bench.cpp 2>/dev/null | sed 's/_bench\.cpp//' | tr '\n' ' ')"
    exit 1
fi

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
if $FORCE_CLEAN || [ ! -f "$PROJ_DIR/bin/libalxbase.so" ]; then
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
echo "=== Building ${NAME}_bench ==="
GIT_COMMIT=$(git -C "$PROJ_DIR" rev-parse --short HEAD 2>/dev/null || echo "?")
${CXX:-g++} -std=c++${STD:-17} -fPIC -Wall -g -O${OPT:-3} -march=native \
    -I"$PROJ_DIR/include" -I"$PROJ_DIR/include/alxbase" -I"$PROJ_DIR/include/alxcore" -I"$PROJ_DIR/include/alxscpt" \
    -o "$PROJ_DIR/bin/${NAME}_bench" "$SCRIPT_DIR/${NAME}_bench.cpp" \
    -DGIT_COMMIT=\"$GIT_COMMIT\" \
    -L"$PROJ_DIR/bin" -lalxscpt -lalxcore -lalxbase '-Wl,-rpath,$ORIGIN'

# --- Run ---
# The settle delay is not cosmetic: a concurrent build or probe moves these
# numbers by more than the differences being looked for.
echo "=== Running ${NAME}_bench ==="
sleep 5
exec "$PROJ_DIR/bin/${NAME}_bench" "${BENCH_ARGS[@]}"
