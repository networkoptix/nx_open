#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=discovery_service

function stage()
{
    stage_cpp
}

main $@
