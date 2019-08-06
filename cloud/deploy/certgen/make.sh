#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=certgen
VERSION=1.3

function stage()
{
    true
}

main $@
