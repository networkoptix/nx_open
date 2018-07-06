#!/bin/bash
set -e #< Exit on error.
set -x #< Log each command.

PLUGIN_NAME="stub_metadata_plugin"

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"

rm -rf "$BUILD_DIR/"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

GEN_OPTIONS=""
SOURCE_DIR="$BASE_DIR/samples/$PLUGIN_NAME"
case "$(uname -s)" in
    CYGWIN*)
        if [[ $(which cmake) != /usr/bin/* ]] #< Using Windows native cmake rather than Cygwin.
        then
            GEN_OPTIONS="-Ax64" #< Generate for Visual Studio and x64.
            SOURCE_DIR=$(cygpath -w "$SOURCE_DIR") #< Windows cmake requires Windows path.
        fi
        ;;
    *)
        GEN_OPTIONS="-GNinja"
        ;;
esac

# Works for any cmake: Linux, Cygwin, and Windows native.
# All script args are passed to cmake. ATTENTION: Use full paths in these args due to "cd".
cmake "$SOURCE_DIR" $GEN_OPTIONS "$@"
cmake --build .

{ set +x; } 2>/dev/null #< Silently turn off logging each command.
ARTIFACT=$(find "$BUILD_DIR" -name "*$PLUGIN_NAME.dll" -o -name "*$PLUGIN_NAME.so")
if [ -f "$ARTIFACT" ]
then
    echo
    echo "Plugin built successfully:"
    echo "$ARTIFACT"
else
    echo
    echo "ERROR: Failed to build plugin."
    exit 42
fi
