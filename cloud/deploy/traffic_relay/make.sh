#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=traffic_relay

function stage()
{
    stage_cpp
}

main $@
