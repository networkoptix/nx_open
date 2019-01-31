#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=provision
VERSION=1.6
REPOSITORY_PATH=/common

function stage()
{
    true
}

main $@
