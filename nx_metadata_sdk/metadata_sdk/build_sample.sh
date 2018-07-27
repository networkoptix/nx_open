#!/bin/bash
set -e #< Exit on error.

PLUGIN_NAME="stub_metadata_plugin"

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"

SOURCE_DIR="$BASE_DIR/samples/$PLUGIN_NAME"

case "$(uname -s)" in #< Check OS.
    CYGWIN*|MINGW*)
        if [[ $(which cmake) == /usr/bin/* ]]
        then
            echo "WARNING: In Cygwin/MinGW, gcc instead of MSVC may work, but is not supported."
            echo ""
            GEN_OPTIONS=() #< Generate for GNU make and gcc.
        else
            # Using Windows native cmake.
            GEN_OPTIONS=( -Ax64 -Thost=x64 -G "Visual Studio 14 2015" )
            SOURCE_DIR=$(cygpath -w "$SOURCE_DIR") #< Windows cmake requires Windows path.
        fi
        ;;
    *) # Assuming Linux.
        GEN_OPTIONS=( -G Ninja )
        ;;
esac

(set -x #< Log each command.
    rm -rf "$BUILD_DIR/"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # All script args are passed to cmake. ATTENTION: Use full paths in these args due to "cd".
    cmake "$SOURCE_DIR" "${GEN_OPTIONS[@]}" "$@"
    cmake --build .
)
cd "$BUILD_DIR"
echo ""

ARTIFACT=$(find "$BUILD_DIR" -name "*$PLUGIN_NAME.dll" -o -name "*$PLUGIN_NAME.so")
if [ ! -f "$ARTIFACT" ]
then
    echo "ERROR: Failed to build plugin."
    exit 42
fi

(set -x #< Log each command.
    ctest --output-on-failure -C Debug
)

echo ""
echo "SUCCESS: All tests passed; plugin built:"
echo "$ARTIFACT"
