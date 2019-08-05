#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=cloud_connect_test_util

function stage()
{
    stage_cpp
}

main $@
