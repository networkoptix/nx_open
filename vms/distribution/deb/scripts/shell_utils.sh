#!/bin/bash

function crash_gdb_bt() {
    BIN_PATH=$1
    CRASH_DIR=${2:-$HOME}
    FULL_VERSION=${3:-"no-version"}

    if [[ -z $BIN_PATH ]]
    then
        echo "Usage: crash_gdb_bt BINARY_PATH [CRASH_DIR]" >&2
        return 2
    fi

    if ! which gdb
    then
        echo "GDB is not available on the host." >&2
        return 0
    fi

    if pidof gdb
    then
        echo "GDB is already engaged." >&2
        return 0
    fi

    CORE_ORIG=$(dirname "$BIN_PATH")/core
    TIME=$(date +"%s")
    CORE="$CORE_ORIG.$TIME"

    if mv "$CORE_ORIG" "$CORE" 2>/dev/null
    then
        REPORT="$(basename "$BIN_PATH")_${FULL_VERSION}_$TIME.gdb-bt"

        echo "Generate crash report $CRASH_DIR/$REPORT...   BACKGROUND"
        echo "t apply all bt 25" | gdb "$BIN_PATH" "$CORE" >"/tmp/$REPORT" 2>&1 && \
            mkdir -p "$CRASH_DIR" && mv "/tmp/$REPORT" "$CRASH_DIR/$REPORT" &

        chmod -R 777 "$CRASH_DIR"
        ls "$CORE_ORIG.*" | grep -v "$CORE" | xargs rm 2>/dev/null
    fi
}

COMMAND=$1
if [[ -z $COMMAND ]]
then
    echo "Usage: shell_utils COMMAND [ARGS ...]" >&2
    exit 1
fi

shift
$COMMAND $@
