#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=deployment_verification_tests

function stage_cmake()
{
    true
}

main $@
