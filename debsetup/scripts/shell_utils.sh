#!/bin/bash

function crash_gdb_bt() {
    BIN_PATH=$1
    CRASH_DIR=${2:$HOME}
    if [ ! "$BIN_PATH" ]; then
        echo "Usage: crash_gdb_bt BINARY_PATH [CRASH_DIR]" > 2
        return 2
    fi

    if [ ! $(which gdb) ]; then
        return 0 # no gdb on the host
    fi
    if [ $(pidof gdb) ]; then
        return 0 # gdb is alrady engaged
    fi

    CORE_ORIG=$(dirname $BIN_PATH)/core
    TIME=$(date +"%s")
    CORE=$CORE_ORIG.$TIME
    mv $CORE_ORIG $CORE 2>/dev/null
    if [ $? -eq 0 ]; then
        REPORT=$(basename $BIN_PATH)_$($BIN_PATH --version)_$TIME.gdb-bt

        echo Generate crash report $REPORT...        BACKGROUND
        echo "t apply all bt 25" | gdb $BIN_PATH $CORE >/tmp/$REPORT 2>&1 && \
            mkdir -p $CRASH_DIR && mv /tmp/$REPORT $CRASH_DIR/$REPORT &

        chmod 666 $CRASH_DIR/$REPORT
        ls $CORE_ORIG.* | grep -v $CORE | xargs rm 2>/dev/null
    fi
}

COMMAND=$1
if [ ! "$COMMAND" ]; then
    echo "Usage: shell_utils COMMAND [ARGS ...]" >2
    exit 1;
fi

shift
$COMMAND $@
