#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

if [[ $# > 0 && ($1 == "/?" || $1 == "-h" || $1 == "--help") ]]
then
    echo "Usage: $(basename "$0") [--release] [<cmake-generation-args>...]"
    echo " --release Compile using Release configuration (with optimizations) instead of Debug."
    exit
fi

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"

if [[ $# > 0 && $1 == "--release" ]]
then
    shift
    BUILD_TYPE=Release
else
    BUILD_TYPE=Debug
fi

case "$(uname -s)" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        GEN_OPTIONS=( -Ax64 )
        BASE_DIR=$(cygpath -w "$BASE_DIR") #< Windows-native cmake requires Windows path.
        BUILD_OPTIONS=()
        if [[ $BUILD_TYPE == Release ]]
        then
            BUILD_OPTIONS+=( --config $BUILD_TYPE )
        fi
        ;;
    *) # Assume Linux; use Ninja if it is available on PATH.
        if which ninja >/dev/null
        then
            GEN_OPTIONS=( -GNinja ) #< Generate for Ninja and gcc; Ninja uses all CPU cores.
            BUILD_OPTIONS=()
        else
            GEN_OPTIONS=() #< Generate for GNU make and gcc.
            BUILD_OPTIONS=( -- -j ) #< Use all available CPU cores.
        fi
        if [[ $BUILD_TYPE == Release ]]
        then
            GEN_OPTIONS+=( -DCMAKE_BUILD_TYPE=$BUILD_TYPE )
        fi
        ;;
esac

(set -x #< Log each command.
    rm -rf "$BUILD_DIR/"
)

for SOURCE_DIR in "$BASE_DIR/samples"/*
do
    SAMPLE=$(basename "$SOURCE_DIR")
    (set -x #< Log each command.
        mkdir -p "$BUILD_DIR/$SAMPLE"
        cd "$BUILD_DIR/$SAMPLE"

        cmake "$SOURCE_DIR/src" `# allow empty array #` ${GEN_OPTIONS[@]+"${GEN_OPTIONS[@]}"} "$@"
        cmake --build . `# allow empty array #` ${BUILD_OPTIONS[@]+"${BUILD_OPTIONS[@]}"}
    )
    ARTIFACT=$(find "$BUILD_DIR" -name "$SAMPLE.dll" -o -name "lib$SAMPLE.so")
    if [ ! -f "$ARTIFACT" ]
    then
        echo "ERROR: Failed to build plugin $SAMPLE."
        exit 64
    fi
    echo ""
    echo "Plugin built: $ARTIFACT"
    echo ""
done

echo "Samples built successfully, see the binaries in $BUILD_DIR"
