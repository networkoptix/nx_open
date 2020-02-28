#!/bin/bash
## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# NOTE: If cmake cannot find Qt, add the following arg to this script (will be passed to cmake):
# -DCMAKE_PREFIX_PATH=<full_path_to_Qt5_dir>

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

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

case "$(uname -s)" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        GEN_OPTIONS=( -Ax64 )
        BASE_DIR=$(cygpath -w "$BASE_DIR") #< Windows-native cmake requires Windows path.
        BUILD_OPTIONS=()
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
