#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=certgen
VERSION=1.2

function stage()
{
    true
}

main $@
