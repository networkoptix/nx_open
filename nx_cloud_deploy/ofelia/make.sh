#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=ofelia
VERSION=1.2

function stage()
{
    true
}

main $@
