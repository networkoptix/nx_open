#!/bin/bash

. ../environment
. ../common.sh

MODULE=monitoring_simple
VERSION=1.3
monitoring_simple_ARGS="--build-arg CACHE_DATE=$(date +%s) --build-arg VERSION=${VERSION}"

function stage()
{
    true
}

main $@
