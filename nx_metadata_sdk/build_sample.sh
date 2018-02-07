#!/bin/bash
set -e #< Exit on error.
set -x #< Log each command.

# All paths here are relative to this script's dir ($0).
declare -r SDK_NAME="nx_metadata_sdk"
declare -r PLUGIN_NAME="stub_metadata_plugin"
declare -r PLUGIN_PATH="samples/$PLUGIN_NAME"
declare -r BUILD_PATH="build"

main()
{
    local -r BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to this script's dir.

    local -r BUILD_DIR="$BASE_DIR/$BUILD_PATH"
    rm -rf "$BUILD_DIR/"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    case "$(uname -s)" in
        CYGWIN*) GEN_OPTIONS="-Ax64";; #< Generate for Visual Studio if on Cygwin.
        *) GEN_OPTIONS="-GNinja";;
    esac

    # Relative path needed when using Windows CMake from Cygwin.
    local -r SOURCE_RELATIVE_PATH=$(realpath --relative-to="." "$BASE_DIR/$PLUGIN_PATH")

    cmake "$SOURCE_RELATIVE_PATH" $GEN_OPTIONS
    cmake --build .

    { set +x; } 2>/dev/null #< Silently turn off logging each command.
    sleep 1 #< Workaround from strange behavior of Windows CMake in Cygwin console.
    echo
    echo "Plugin built successfully:"
    find `# Show absolute path #` "$(pwd)" -name "*$PLUGIN_NAME.dll" -o -name "*$PLUGIN_NAME.so"
}

main "$@"
