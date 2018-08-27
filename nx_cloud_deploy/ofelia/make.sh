#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=ofelia
VERSION=1.1

function stage()
{
    true
}

main $@
