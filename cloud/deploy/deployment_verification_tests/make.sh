#!/bin/bash -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $SCRIPT_DIR/../environment
. $SCRIPT_DIR/../common.sh

MODULE=deployment_verification_tests

function build()
{
    local currentSourceDir=$1

    docker run -e USERID=$(id -u) -e GROUPID=$(id -g) --rm -v $currentSourceDir:/go -v $PWD:/bin.out golang:1.10.2-alpine3.7 sh -c 'go build -o /bin.out/deployment_verification_tests ./src; chown $USERID:$GROUPID /bin.out/deployment_verification_tests'
}

function stage_cmake()
{
    true
}

main $@
