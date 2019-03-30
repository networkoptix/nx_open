#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=connection_mediator

function stage()
{
    stage_cpp
}

main $@
