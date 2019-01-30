#!/bin/bash
set -e #< Exit on error.
set -u #< Prohibit undefined variables.

PLUGIN_NAME="stub_analytics_plugin"

# Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.
BUILD_DIR="$BASE_DIR-build"

SOURCE_DIR="$BASE_DIR/samples/$PLUGIN_NAME"

if (($# > 0)) && [[ $1 == "--no-tests" ]]
then
    shift
    NO_TESTS=1
else
    NO_TESTS=0
fi

# Use Ninja for Linux and Cygwin, if it is available on PATH.
if which ninja >/dev/null
then
    GEN_OPTIONS=( -GNinja ) #< Generate for Ninja and gcc. Use all available CPU cores.
else
    GEN_OPTIONS=() #< Generate for GNU make and gcc. ATTENTION: Uses 1 CPU core only.
fi

case "$(uname -s)" in #< Check if running in Windows from Cygwin/MinGW.
    CYGWIN*|MINGW*)
        if [[ $(which cmake) == /usr/bin/* ]] # Cygwin's cmake is on PATH.
        then
            echo "WARNING: In Cygwin/MinGW, gcc instead of MSVC may work, but is not supported.\n"
        else # Assuming Windows-native cmake is on PATH.
            GEN_OPTIONS=( -Ax64 -Tv140,host=x64 ) #< Generate for Visual Studio 2015 compiler.
            SOURCE_DIR=$(cygpath -w "$SOURCE_DIR") #< Windows-native cmake requires Windows path.
        fi
        ;;
esac

(set -x #< Log each command.
    rm -rf "$BUILD_DIR/"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake "$SOURCE_DIR" ${GEN_OPTIONS[@]+"${GEN_OPTIONS[@]}"} "$@"
    cmake --build .
)
cd "$BUILD_DIR" #< Restore the required current directory after exiting the subshell.

ARTIFACT=$(find "$BUILD_DIR" -name "*$PLUGIN_NAME.dll" -o -name "*$PLUGIN_NAME.so")
if [ ! -f "$ARTIFACT" ]
then
    echo "ERROR: Failed to build plugin."
    exit 42
fi

echo ""
if [[ $NO_TESTS == 1 ]]
then
    echo "NOTE: Unit tests were not run."
else
    (set -x #< Log each command.
        ctest --output-on-failure -C Debug
    )
fi

echo ""
echo "Plugin built:"
echo "$ARTIFACT"
