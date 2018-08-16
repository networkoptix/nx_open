#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=certgen
VERSION=1.0

function stage()
{
    true
}

main $@
