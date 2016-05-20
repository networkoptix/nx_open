#!/bin/bash
branch=$(hg branch)
current_changeset=$(hg id -i)
nx_vms_dir=$(pwd)
latest_success_changeset=
run_tests=true

if [ -f $nx_vms_dir/.test/$branch ]; then 
    latest_success_changeset=$(cat $nx_vms_dir/.test/$branch)
    if [ $current_changeset == $latest_success_changeset ]; then run_tests=false; fi
fi

if [ $run_tests == "true" ]; then
    mkdir -p ../devtools 
    cd ../devtools 
    hg pul || hg clone ssh://hg@hdw.mx/devtools .
    hg up stable_tests
    ERRORLEVEL=$?
    if [[ $ERRORLEVEL -ne 0 ]]; then echo $ERRORLEVEL && exit $ERRORLEVEL; fi
    cd testing
    python auto.py -t -o -p $nx_vms_dir
    ERRORLEVEL=$?
    echo "Tests Finished with code " $ERRORLEVEL
    if [[ $ERRORLEVEL -ne 0 ]]; then echo $ERRORLEVEL && exit $ERRORLEVEL; fi
    mkdir -p $nx_vms_dir/.test
    echo $current_changeset > $nx_vms_dir/.test/$branch
else
    echo "This changeset has been already tested." $ERRORLEVEL
    exit 0
fi