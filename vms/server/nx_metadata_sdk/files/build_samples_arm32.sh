#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Exit on error.
set -u #< Prohibit undefined variables.

BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to the current dir.

set -x #< Log each command.

"$BASE_DIR"/build_samples.sh \
    --no-tests \
    -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/toolchain_arm32.cmake \
    "$@"
