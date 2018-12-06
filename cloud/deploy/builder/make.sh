#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=builder
VERSION=1.0

function stage()
{
    true
}

main $@
