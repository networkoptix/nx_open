#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

if [[ $# > 0 && ($1 == "/?" || $1 == "-h" || $1 == "--help") ]]; then
    cat <<EOF
Usage:
$0 [<cmake-generation-args>...]

To avoid running unit tests, set the environment variable NX_SDK_NO_TESTS=1.

To build samples using the Debug configuration, use -DCMAKE_BUILD_TYPE=Debug.

To build samples that require Qt, pass the paths to the Qt and Qt Host Conan packages using CMake
 parameters:
 -DQT_DIR=<path-to-qt-conan-package> -DQT_HOST_PATH=<path-to-qt-host-conan-package>
EOF
    exit 0
fi

BASE_DIR="$(readlink -f "$(dirname "$0")")" #< Absolute path to this script's dir.
SYSTEM_TYPE="$(uname -s)"
if [[ "${SYSTEM_TYPE}" == "CYGWIN"* || "${SYSTEM_TYPE}" == "MINGW"* ]]; then
    BASE_DIR="$(cygpath -w "${BASE_DIR}")" #< Windows-native cmake requires Windows path
fi

EXTRA_CMAKE_ARGS=( "$@" )

# Set up variables for the build system.

GEN_OPTIONS=()
BUILD_OPTIONS=()

case "${SYSTEM_TYPE}" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        # Assume that the MSVC environment is set up correctly and Ninja is installed. Also
        # specify the compiler explicitly to avoid clashes with gcc.
        GEN_OPTIONS+=( -GNinja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_C_COMPILER=cl.exe )
        ;;
    *)
        if command -v ninja >/dev/null; then
            GEN_OPTIONS=( -GNinja ) #< Generate for Ninja and gcc; Ninja uses all CPU cores.
        else
            GEN_OPTIONS=() #< Generate for GNU make and gcc.
            BUILD_OPTIONS+=( -- -j ) #< Use all available CPU cores.
        fi
        ;;
esac

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BUILD_DIR="${BASE_DIR}-build"

# Clean up the build dir and build the samples.
(set -x #< Log each command.
    rm -rf "${BUILD_DIR}/"
    cmake "${BASE_DIR}/samples" -B "${BUILD_DIR}" \
        ${GEN_OPTIONS[@]+"${GEN_OPTIONS[@]}"} `#< allow empty array #` \
        "${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}" `#< allow empty array #`
    cmake --build "${BUILD_DIR}" \
        ${BUILD_OPTIONS[@]+"${BUILD_OPTIONS[@]}"} `#< allow empty array #`
)

# Run unit tests if not disabled by setting NX_SDK_NO_TESTS=1. The default value when
# NX_SDK_NO_TESTS is not set is 0.
if [[ "${NX_SDK_NO_TESTS:-0}" != "1" ]]; then
    (set -x #< Log each command.
        cd "${BUILD_DIR}"
        ctest --output-on-failure
    )
else
    echo "NOTE: Unit tests were not run."
fi

echo ""
echo "All samples built successfully, see the binaries in ${BUILD_DIR}/samples/"
