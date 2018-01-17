#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=provision
VERSION=1.4
REPOSITORY_PATH=/common

function stage()
{
    true
}

main $@
