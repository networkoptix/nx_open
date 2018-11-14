#!/bin/bash -ex

. ../environment
. ../common.sh

MODULE=cloud_modules
VERSION=1.0

function stage()
{
    local builder=009544449203.dkr.ecr.us-east-1.amazonaws.com/devtools/gobuilder:1.0

    rm -rf stage

    mkdir stage
    docker run --rm -v "$PWD"/build/pkg:/go/pkg -v "$PWD":/go/src/hdw.mx/cloud_modules -w /go/src/hdw.mx/cloud_modules $builder
    mv cloud_modules stage
}

main $@
