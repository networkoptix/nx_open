#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=balancer
VERSION=3.1.4

function stage()
{
    true
}

main $@
