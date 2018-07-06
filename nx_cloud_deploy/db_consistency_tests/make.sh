#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=db_consistency_tests
VERSION=1.1

function stage()
{
    rm -rf stage
    cp -R ../../nx_cloud/cloud_tests/db_consistency_tests stage
}

main $@
