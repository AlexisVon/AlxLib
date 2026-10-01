# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT
# See LICENSE file in the project root for full license information.

if [ "$1" = "clean" ]; then
    node-gyp clean
else
    node-gyp configure build --release -j
    cp source/alxbase.d.ts bin/alxbase.d.ts 2>/dev/null
fi