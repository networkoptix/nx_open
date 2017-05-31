#!/bin/bash -e

. ./environment
. ./common.sh

function run_targets()
{
    local modules=$1
    local targets=$2

    for module in ${modules[@]}
    do
        echo "Running targets $targets for module $module.."
        (cd $module; ./make.sh $targets)
    done
}

function pack_all()
{
    run_targets "${MODULES[*]}" "stage pack"
}

function push_all()
{
    run_targets "${MODULES[*]}" "push"
}

function clean_all()
{
    run_targets "${MODULES[*]}" "clean"
}

main $@
