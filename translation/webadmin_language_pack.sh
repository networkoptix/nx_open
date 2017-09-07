#!/bin/bash

crowdin upload sources -b vms_3.1 --config crowdin-vms.yaml
crowdin download -b vms_3.1 --config crowdin-vms.yaml -l en-GB

DIRECTORY=webadmin_language_pack
rm -rf $DIRECTORY || true
mkdir -p $DIRECTORY
cp -rf ../webadmin/translations/en_GB/*  $DIRECTORY
