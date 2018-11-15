#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=fluentd-cloudwatch
VERSION=1.1

function stage ()
{
    true
}

main $@
