#!/bin/bash

. ../environment
. ../common.sh

SRC_DIR=../../nx_cloud/cloud_tests/monitoring
MAKE_SH=$SRC_DIR/make.sh

MODULE=monitoring_simple
VERSION=$($MAKE_SH version)
BUILD_ARGS=(--build-arg CACHE_DATE=$(date +%s) --build-arg VERSION=${VERSION})

function build()
{
    $MAKE_SH build
}

function stage()
{
    rm -rf stage; mkdir stage

    cp $SRC_DIR/dist/monitoring_simple-$VERSION-py3-none-any.whl stage
}

main $@
