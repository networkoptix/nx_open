#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=base
VERSION=1.10

function stage()
{
    true
}

main $@
