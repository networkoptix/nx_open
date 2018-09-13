#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=certgen
VERSION=1.1

function stage()
{
    true
}

main $@
