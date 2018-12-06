#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=ofelia
VERSION=1.8

function stage()
{
    true
}

main $@
