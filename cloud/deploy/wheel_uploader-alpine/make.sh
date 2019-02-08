#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=wheel_uploader
VERSION=3.7-alpine3.7
REPOSITORY_HOST=009544449203.dkr.ecr.us-west-1.amazonaws.com
REPOSITORY_PATH=/devtools

function stage()
{
    true
}

main $@
