#!/bin/bash -e

. ./environment
. ./common.sh

MODULES="cloud_db cloud_portal cloud_portal_nginx connection_mediator vms_gateway nxcloud_host_agent"

function run_targets()
{
    local modules=$1
    local targets=$2

    for module in $modules
    do
        echo "Running targets $targets for module $module.."
        (cd $module; ./make.sh $targets)
    done
}

function pack_all()
{
    run_targets "$MODULES" "stage pack"
}

function push_all()
{
    run_targets "$MODULES" "push"
}

function clean_all()
{
    run_targets "$MODULES" "clean"
}

main $@
