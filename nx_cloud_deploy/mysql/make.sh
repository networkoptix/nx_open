#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=mysql
VERSION=5.7

function stage()
{
    true
}

main $@
