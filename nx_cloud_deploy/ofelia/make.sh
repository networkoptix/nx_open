#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=ofelia
VERSION=1.6

function stage()
{
    true
}

main $@
