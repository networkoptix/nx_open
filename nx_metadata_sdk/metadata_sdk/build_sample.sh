#!/bin/bash
set -e #< Exit on error.

PLUGIN_NAME="stub_metadata_plugin"

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"

SOURCE_DIR="$BASE_DIR/samples/$PLUGIN_NAME"

case "$(uname -s)" in #< Check for OS: Windows with Cygwin or MinGW (Git Bash), or Linux.
    CYGWIN*|MINGW*)
        if [[ $(which cmake) == /usr/bin/* ]]
        then
            # Using cmake included with Cygwin or MinGW (Git Bash).
            echo ""
            echo "WARNING: Using cmake and gcc included with Cygwin or MinGW (Git Bash) may work,"
            echo "but was not tested, and thus is not officially supported."
            echo "It is recommended to use Windows native cmake and MSVC from such environments:"
            echo "just put Windows cmake on PATH (check the command \"which cmake\")."
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

set -x #< Log each command.

rm -rf "$BUILD_DIR/"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# All script args are passed to cmake. ATTENTION: Use full paths in these args due to "cd".
cmake "$SOURCE_DIR" "${GEN_OPTIONS[@]}" "$@"
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
