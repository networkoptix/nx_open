#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=provision
VERSION=1.3
REPOSITORY_PATH=/common

function stage()
{
    true
}

main $@
