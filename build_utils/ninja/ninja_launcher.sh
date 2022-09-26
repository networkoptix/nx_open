#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

BASE_DIR=$(dirname "$0")
if [[ $(uname) != "Darwin" ]]
then
    BASE_DIR=$(readlink -f "${BASE_DIR}")
fi

if [[ -z "${DISABLE_NINJA_TOOL}" ]]
then
    LOG_FILE_NAME="$(pwd)/build_logs/pre_build.log"
    python3 "${BASE_DIR}/ninja_tool.py" --log-output "${LOG_FILE_NAME}" --stack-trace
    if [[ $? != 0 ]]
    then
        echo "Ninja tool exited with error."
        echo "For more details, see ${LOG_FILE_NAME}"
        exit 1
    fi
fi

ninja "$@"
