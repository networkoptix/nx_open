#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=base
VERSION=3.0.3

function stage()
{
    true
}

main $@
