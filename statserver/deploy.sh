#!/bin/bash
set -e -x

SRV=${SRV:-muskov@stats.networkoptix.com}

./setup.py bdist_wheel

WHLP=dist/*.whl
WHLN=$(basename $WHLP)

scp $WHLP deploy_install.sh $SRV:.
ssh $SRV ./deploy_install.sh $WHLN

rm -rf build dist *.egg-info
