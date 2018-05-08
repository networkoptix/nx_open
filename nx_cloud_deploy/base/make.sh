#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=base
VERSION=2.01

function stage()
{
    true
}

main $@
