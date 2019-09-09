#!/bin/bash

. ../environment
. ../common.sh

SRC_DIR=../../backend/cloud_tests/monitoring
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

function publish()
{
    build
    stage
    pack
    push
}

function publish_deps()
{
    local dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
    local img=009544449203.dkr.ecr.us-west-1.amazonaws.com/devtools/wheel_uploader:3.7.3-alpine3.10

    docker pull $img
    docker run --rm -i $img < $SRC_DIR/requirements.txt
}

main $@
