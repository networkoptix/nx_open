#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=cloud_db

function stage()
{
    stage_cpp
}

main $@
