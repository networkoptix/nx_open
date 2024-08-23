#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# TODO: Use ninja for Windows builds as well.

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

if [[ $# > 0 && ($1 == "/?" || $1 == "-h" || $1 == "--help") ]]
then
    echo "Usage: $(basename "$0") [--with-rpi-samples] [--no-qt-samples] [--debug] [<cmake-generation-args>...]"
    echo " --with-rpi-samples Build Raspberry Pi samples (use only when building for 32-bit ARM)."
    echo " --no-qt-samples Exclude samples that require the Qt library."
    echo " --debug Compile using Debug configuration (without optimizations) instead of Release."
    echo "NOTE: If --no-qt-samples is not specified, and cmake cannot find Qt, add the following"
    echo " argument to this script (will be passed to the cmake generation command):"
    echo " -DCMAKE_PREFIX_PATH=<absolute-path-to-Qt5-dir>"
    exit
fi

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"

if [[ $# > 0 && $1 == "--with-rpi-samples" ]]
then
    shift
    WITH_RPI_SAMPLES=1
else
    WITH_RPI_SAMPLES=0
fi

if [[ $# > 0 && $1 == "--no-qt-samples" ]]
then
    shift
    NO_QT_SAMPLES=1
else
    NO_QT_SAMPLES=0
fi

if [[ $# > 0 && $1 == "--debug" ]]
then
    shift
    BUILD_TYPE=Debug
else
    BUILD_TYPE=Release
fi

case "$(uname -s)" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        # Assume that the MSVC environment is set up correctly and Ninja is installed. Also
        # specify the compiler explicitly to avoid clashes with gcc.
        GEN_OPTIONS=( -GNinja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_C_COMPILER=cl.exe )
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
    if [[ $WITH_RPI_SAMPLES == 0 && $SAMPLE == rpi_* ]]
    then
        echo "ATTENTION: Skipping $SAMPLE because --with-rpi-samples is not specified."
        echo ""
        continue
    fi
    if [[ $NO_QT_SAMPLES == 1 && $SAMPLE == axis_camera_plugin ]]
    then
        echo "ATTENTION: Skipping $SAMPLE because --no-qt-samples is specified."
        echo ""
        continue
    fi
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
