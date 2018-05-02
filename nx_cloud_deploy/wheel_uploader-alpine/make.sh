#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=wheel_uploader
VERSION=3.6-alpine3.7
REPOSITORY_PATH=/devtools

function stage()
{
    true
}

main $@
