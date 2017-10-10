#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=provision
VERSION=1.1
REPOSITORY_PATH=/common

function stage()
{
    true
}

main $@
