#!/bin/bash -e

. ../environment
. ../common.sh

NXCLOUD_VERSION=$(pip search --trusted-host=la.hdw.mx --index=http://la.hdw.mx:3141/root/public/ nxcloud 2>/dev/null | sed 's/.*(\(.*\)).*/\1/g')

MODULE=nxcloud_host_agent
BUILD_ARGS=(--build-arg NXCLOUD_VERSION=${NXCLOUD_VERSION} --build-arg VERSION=${VERSION} --build-arg MODULES="${MODULES[@]}")

function stage()
{
    true
}

function stage_cmake()
{
    true
}

main $@
