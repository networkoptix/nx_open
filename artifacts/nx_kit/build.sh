#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

if [[ $# > 0 && ($1 == "/?" || $1 == "-h" || $1 == "--help") ]]; then
    echo "Usage: $(basename "$0") [--no-tests] [--debug] [<cmake-generation-args>...]"
    echo " --debug Compile using Debug configuration (without optimizations) instead of Release."
    exit
fi

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
declare BASE_DIR="$(readlink -f "$(dirname "$0")")" #< Absolute path to this script's dir.
declare -r BUILD_DIR="${BASE_DIR}-build"

declare NO_TESTS=0
if [[ $# > 0 && $1 == "--no-tests" ]]; then
    shift
    NO_TESTS=1
fi

declare BUILD_TYPE="Release"
if [[ $# > 0 && $1 == "--debug" ]]; then
    shift
    BUILD_TYPE=Debug
fi

declare -a GEN_OPTIONS=()
declare LIBRARY_NAME=""

case "$(uname -s)" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        # Assume that the MSVC environment is set up correctly and Ninja is installed. Also
        # specify the compiler explicitly to avoid clashes with gcc.
        GEN_OPTIONS+=( -GNinja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_C_COMPILER=cl.exe )
        BASE_DIR=$(cygpath -w "${BASE_DIR}") #< Windows-native CMake requires Windows path.
        LIBRARY_NAME="nx_kit.dll"
        ;;
    *)
        if command -v ninja &> /dev/null; then #< Use Ninja if it is available on PATH.
            GEN_OPTIONS+=( -GNinja )
        fi
        LIBRARY_NAME="libnx_kit.so"
        ;;
esac

if [[ "${BUILD_TYPE}" == "Release" ]]; then
    GEN_OPTIONS+=( -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" )
fi

(set -x #< Log each command.
    rm -rf "${BUILD_DIR}/"
)

cmake "${BASE_DIR}" -B "${BUILD_DIR}" \
    ${GEN_OPTIONS[@]+"${GEN_OPTIONS[@]}"} `#< allow empty array for bash before 4.4 #` \
    "$@"
cmake --build "${BUILD_DIR}"

if [[ ! -f "${BUILD_DIR}/${LIBRARY_NAME}" ]]; then
    echo "ERROR: Failed to build nx_kit."
    exit 64
fi

echo ""
echo "Built: ${BUILD_DIR}/${LIBRARY_NAME}"
echo ""

# Run unit tests if needed.
if (( ${NO_TESTS} == 1 )); then
    echo "NOTE: Unit tests were not run."
else
    cd "${BUILD_DIR}/unit_tests"
    PATH="${BUILD_DIR}:${PATH}"
    (set -x #< Log each command.
        ctest --output-on-failure -C "${BUILD_TYPE}"
    )
fi
echo ""

echo "Samples built successfully, see the binaries in ${BUILD_DIR}"
