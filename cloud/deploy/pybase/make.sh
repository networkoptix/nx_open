#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=pybase
VERSION=1.10

function stage()
{
    true
}

main $@
