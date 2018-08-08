#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=ofelia
VERSION=1.0

function stage()
{
    true
}

main $@
