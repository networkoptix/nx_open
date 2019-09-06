#!/bin/bash
## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"

# Use Ninja for Linux and Cygwin, if it is available on PATH.
if which ninja >/dev/null
then
    GEN_OPTIONS=( -GNinja ) #< Generate for Ninja and gcc; Ninja uses all available CPU cores.
    BUILD_OPTIONS=()
else
    GEN_OPTIONS=() #< Generate for GNU make and gcc.
    BUILD_OPTIONS=( -- -j ) #< Use all available CPU cores.
fi

case "$(uname -s)" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        if [[ $(which cmake) == /usr/bin/* ]] # Cygwin's cmake is on PATH.
        then
            echo "WARNING: In Cygwin/MinGW, gcc instead of MSVC may work, but is not supported."
            echo ""
        else # Assuming Windows-native cmake is on PATH.
            GEN_OPTIONS=( -Ax64 -Tv140,host=x64 ) #< Generate for Visual Studio 2015 compiler.
            BASE_DIR=$(cygpath -w "$BASE_DIR") #< Windows-native cmake requires Windows path.
            BUILD_OPTIONS=()
        fi
        ;;
esac

(set -x #< Log each command.
    rm -rf "$BUILD_DIR/"
)

for SAMPLE_DIR in "$BASE_DIR/samples"/*
do
    SOURCE_DIR="$SAMPLE_DIR/src"
    SAMPLE=$(basename "$SAMPLE_DIR")
    (set -x #< Log each command.
        mkdir -p "$BUILD_DIR/$SAMPLE"
        cd "$BUILD_DIR/$SAMPLE"

        cmake "$SOURCE_DIR" `# allow empty array #` ${GEN_OPTIONS[@]+"${GEN_OPTIONS[@]}"} "$@"
        cmake --build . `# allow empty array #` ${BUILD_OPTIONS[@]+"${BUILD_OPTIONS[@]}"}
    )
    ARTIFACT=$(find "$BUILD_DIR" -name "$SAMPLE.dll" -o -name "lib$SAMPLE.so")
    if [ ! -f "$ARTIFACT" ]
    then
        echo "ERROR: Failed to build plugin $SAMPLE."
        exit 64
    fi
    echo ""
    echo "Plugin built:"
    echo "$ARTIFACT"
    echo ""
done

echo "Samples built successfully, see the binaries in $BUILD_DIR"
