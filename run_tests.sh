#!/bin/bash
TEST_BRANCH=$1
branch=$(hg branch)
current_changeset=$(hg id -i)
nx_vms_dir=$(pwd)
ci_dir=$nx_vms_dir/.test
devtools_dir=../devtools
latest_success_changeset=
run_tests=true
script_name=$(basename "$0")

if [ -n "$TEST_BRANCH" ]; then
    if [ -f "$ci_dir/$branch" ]; then 
        latest_success_changeset=$(cat $nx_vms_dir/.test/$branch)
        if [ $current_changeset == $latest_success_changeset ]; then run_tests=false; fi
    fi
fi

if [ $run_tests != "true" ]; then
    echo "This changeset has been already tested." $ERRORLEVEL
    exit 0
fi

if [ -n "$TEST_BRANCH" ]; then
    echo "[${script_name}] Called with devtools brach "$TEST_BRANCH" specified. $devtools_dir is switching to this branch with --clean!"
    mkdir -p "$devtools_dir"
    cd "$devtools_dir"
    hg pull || hg clone ssh://hg@hdw.mx/devtools .
    ERRORLEVEL=$?
    if [[ $ERRORLEVEL -ne 0 ]]; then echo $ERRORLEVEL && exit $ERRORLEVEL; fi
    hg up "$TEST_BRANCH" --clean
    ERRORLEVEL=$?
    if [[ $ERRORLEVEL -ne 0 ]]; then echo $ERRORLEVEL && exit $ERRORLEVEL; fi
    cp "${nx_vms_dir}/run_test_conf.py" testing/testconf_local.py
    ERRORLEVEL=$?
    if [[ $ERRORLEVEL -ne 0 ]]; then echo $ERRORLEVEL && exit $ERRORLEVEL; fi
else
    if [ ! -d "$devtools_dir" ]; then
        echo ERROR: Directory "'$devtools_dir'" does\'t exist!
        exit 1
    fi
    cd $devtools_dir 
fi


cd testing
python auto.py -t -o -p $nx_vms_dir
ERRORLEVEL=$?
echo "Tests Finished with code " $ERRORLEVEL
if [[ $ERRORLEVEL -ne 0 ]]; then echo $ERRORLEVEL && exit $ERRORLEVEL; fi

if [ -n "$TEST_BRANCH" ]; then
    mkdir -p $nx_vms_dir/.test
    echo $current_changeset > $nx_vms_dir/.test/$branch
fi
