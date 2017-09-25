#!/bin/bash

. ../environment
. ../common.sh

MODULE=monitoring_simple
VERSION=1.7
BUILD_ARGS=(--build-arg CACHE_DATE=$(date +%s) --build-arg VERSION=${VERSION})

function stage()
{
    true
}

main $@
