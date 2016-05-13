#!/bin/bash
nx_vms_dir=`pwd`
mkdir -p ../devtools 
cd ../devtools 
hg update || hg clone ssh://hg@hdw.mx/devtools .
if [[ $? -ne 0 ]]; then exit $?; fi
cd testing
python auto.py -t -o -p $nx_vms_dir
if [[ $? -ne 0 ]]; then exit $?; fi