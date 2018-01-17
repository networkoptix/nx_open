#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=base
VERSION=1.6

function stage()
{
    true
}

main $@
