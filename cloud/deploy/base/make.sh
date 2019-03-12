#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=base
VERSION=3.0.1

function stage()
{
    true
}

main $@
