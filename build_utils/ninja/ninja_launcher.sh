#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

BASE_DIR=$(dirname "$0")
if [[ $(uname) != "Darwin" ]]
then
    BASE_DIR=$(readlink -f "${BASE_DIR}")
fi

if [[ -z "${DISABLE_NINJA_TOOL}" ]]
then
    python3 "${BASE_DIR}/ninja_tool.py" --log-output --stack-trace
    if [[ $? != 0 ]]
    then
        echo "Ninja tool exited with error."
        echo "For more details, see <build-dir>/build_logs/pre_build.log"
        exit 1
    fi
fi

ninja "$@"
