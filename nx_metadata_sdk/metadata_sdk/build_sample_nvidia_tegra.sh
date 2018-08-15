#!/bin/bash
set -e #< Exit on error.
set -u #< Prohibit undefined variables.

BASE_DIR=$(readlink -f "$(dirname "$0")") #< Absolute path to the current dir.

set -x #< Log each command.

"$BASE_DIR"/build_sample.sh \
    --no-tests \
    -DCMAKE_TOOLCHAIN_FILE=$BASE_DIR/nvidia_tegra_toolchain.cmake \
    "$@"
