#!/bin/bash -e

. ../environment
. ../common.sh

function stage()
{
    true
}

function stage_cmake()
{
    true
}

MODULE=cloud_portal_nginx
VERSION=${CLOUD_PORTAL_VERSION:-$VERSION}

main $@
