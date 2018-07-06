#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=fluentd-cloudwatch
VERSION=1.2

function stage ()
{
    true
}

main $@
