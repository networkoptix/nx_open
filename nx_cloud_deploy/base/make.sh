#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=base
VERSION=1.7

function stage()
{
    true
}

main $@
