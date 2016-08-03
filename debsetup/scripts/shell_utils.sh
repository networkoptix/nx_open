#!/bin/bash

function crash_gdb_bt() {
    BIN_PATH=$1

    if [! $1 ]; then
        echo "Usage: craqsh_gdb_bt BINARY_PATH" > 2
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
        echo "t apply all bt 25" | gdb $BIN_PATH $CORE 2&>1 >/tmp/$REPORT && \
            mv /tmp/$REPORT ~/$REPORT &

        ls $CORE_ORIG.* | grep -v $CORE | xargs rm 2>/dev/null
    fi
}

if [! $1]; then
    echo "Usage: shell_utils COMMAND [ARGS ...]" >2
    exit 1;
fi

COMMAND=$1
shift
$COMMAND $@
