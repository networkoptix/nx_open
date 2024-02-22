#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
SOURCE_DIR=$(cd "$(dirname "$0")" && pwd -P) #< NOTE: "readlink -f" does not work in MacOS.
BUILD_DIR="${SOURCE_DIR}-build"

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    mkdir -p "${BUILD_DIR}"

    set -x #< Print commands to be executed.

    # Ensure that only the currently supplied CMake arguments are in effect.
    rm -f "${BUILD_DIR}/CMakeCache.txt"

    cmake "${SOURCE_DIR}" -B "${BUILD_DIR}" -GNinja "$@"
else
    set -x #< Print commands to be executed.
fi

cmake --build "${BUILD_DIR}"
