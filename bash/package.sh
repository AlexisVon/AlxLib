#!/bin/bash
# Copyright (c) 2026 AlexisVon

# package.sh - package AlxLib (wipes build/ and rebuilds by default)

set -e

# Project root
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# Read the version out of CMakeLists.txt
ALXLIB_VERSION=$(grep -E "set\(ALXLIB_VERSION_(MAJOR|MINOR|PATCH)" ${ROOT_DIR}/CMakeLists.txt | awk '{print $2}' | paste -sd. -)

# Package name (-g<short sha>, plus -dirty when tracked files carry uncommitted changes)
VERSION_STRING="${ALXLIB_VERSION}"
GIT_SUFFIX=""
if GIT_SHA=$(git -C "${ROOT_DIR}" rev-parse --short HEAD 2>/dev/null); then
    GIT_SUFFIX="-g${GIT_SHA}"
    if [ -n "$(git -C "${ROOT_DIR}" status --porcelain --untracked-files=no)" ]; then
        GIT_SUFFIX="${GIT_SUFFIX}-dirty"
    fi
else
    echo "*** warning: cannot read the git sha, package name carries none"
fi
PACKAGE_NAME="AlxLib-${VERSION_STRING}-linux-x64${GIT_SUFFIX}"

echo "=== Packaging ${PACKAGE_NAME} ==="

cd ${ROOT_DIR}

# Clear old products: build/ is rebuilt from scratch (the version is a cache variable; keeping it would pin the previous generation)
# bin/ only loses library files -- it is the output directory shared by build/ and build-test/, and packaging must not wipe out the test binaries
rm -rf build ${PACKAGE_NAME} ${PACKAGE_NAME}.tar.gz ${PACKAGE_NAME}.debug ${PACKAGE_NAME}.debug.tar.gz
rm -f bin/libalx*

# Build
echo "Building..."
cmake -B build \
    -DALXBASE_STATIC=ON \
    -DALXCORE_STATIC=ON \
    -DALXSCPT_STATIC=ON \
    -DALXCOMM_STATIC=ON \
    -DALXLIB_STATIC=ON \
    -DALXCOMM_TLS=ON \
    -DALXLIB_SPLIT_DEBUG=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF
cmake --build build -j7

# Create the directory layout
mkdir -p ${PACKAGE_NAME}/{include,bin,lib}

# Copy headers
echo "Copying headers..."
cp -r include/alxbase ${PACKAGE_NAME}/include/
cp -r include/alxcore ${PACKAGE_NAME}/include/
cp -r include/alxscpt ${PACKAGE_NAME}/include/
cp -r include/alxcomm ${PACKAGE_NAME}/include/

# Copy shared libraries (keeping the symlinks)
echo "Copying shared libraries..."
cp -a bin/libalx*.so* ${PACKAGE_NAME}/bin/
rm -f ${PACKAGE_NAME}/bin/*.debug

# Copy debug info (packaged separately)
echo "Copying debug info..."
DEBUG_NAME="${PACKAGE_NAME}.debug"
mkdir -p ${DEBUG_NAME}/debug
cp bin/*.debug ${DEBUG_NAME}/debug/ 2>/dev/null || true

# Copy static libraries
echo "Copying static libraries..."
cp bin/libalx*.a ${PACKAGE_NAME}/lib/

# Copy docs (issue/feature/todo/verify are internal ledgers and stay out of the package)
echo "Copying docs..."
cp README.md ${PACKAGE_NAME}/ 2>/dev/null || true
cp LICENSE ${PACKAGE_NAME}/ 2>/dev/null || true
cp -r doc ${PACKAGE_NAME}/doc 2>/dev/null || true
rm -f ${PACKAGE_NAME}/doc/issue.md ${PACKAGE_NAME}/doc/feature.md \
      ${PACKAGE_NAME}/doc/todo.md ${PACKAGE_NAME}/doc/verify.md

# Copy the third-party notices (seven full texts + the manifest)
echo "Copying third-party notices..."
cp -r licenses ${PACKAGE_NAME}/ 2>/dev/null || true
cp THIRD-PARTY.md ${PACKAGE_NAME}/ 2>/dev/null || true

# Create the archives
echo "Creating archive..."
tar -czf ${PACKAGE_NAME}.tar.gz ${PACKAGE_NAME}
tar -czf ${DEBUG_NAME}.tar.gz ${DEBUG_NAME}

# Clean up
rm -rf ${PACKAGE_NAME} ${DEBUG_NAME}

echo "=== Done ==="
echo "  Main:   ./${PACKAGE_NAME}.tar.gz"
echo "  Debug:  ./${DEBUG_NAME}.tar.gz"
