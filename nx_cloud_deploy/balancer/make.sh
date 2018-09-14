#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=balancer
VERSION=3.1.3

function stage()
{
    true
}

main $@
