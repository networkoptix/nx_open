#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=wheel_uploader
VERSION=1.0-py2
REPOSITORY_PATH=/devtools

function stage()
{
    true
}

main $@
