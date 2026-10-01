# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT
# See LICENSE file in the project root for full license information.

EXT_SUFFIX=$(python3 -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")

if [ "$1" = "clean" ]; then
    make clean
else
    make -j
    echo "Build complete → bin/alxbase${EXT_SUFFIX}"
fi
