#!/bin/bash

BASE_DIR=$(dirname "$0")
if [[ $(uname) != "Darwin" ]]
then
    BASE_DIR=$(readlink -f "${BASE_DIR}")
fi

if [[ -z "${DISABLE_NINJA_TOOL}" ]]
then
    python3 "${BASE_DIR}/../python/ninja_tool.py" --log-output --stack-trace
fi

ninja "$@"
