#!/bin/bash
set -e #< Stop on first error.
set -u #< Require variable definitions.

source "build_options.conf"

export PATH="$DEV_PATH/python/bin:$DEV_PATH/python:$PATH"
export LD_LIBRARY_PATH="$DEV_PATH/python/lib"
export PYTHONPATH="$DEV_PATH/python/lib/python3.6/site-packages"

python3.6 \
    $DEV_PATH/python/bin/pyinstaller \
    -y \
    -F --hidden-import json \
    --distpath $BUILD_DIR/dist \
    --workpath $BUILD_DIR/build \
    --specpath $BUILD_DIR \
    -p $SRC_DIR/lib \
    $SRC_DIR/bin/nx_box_tool
