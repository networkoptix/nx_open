#!/bin/bash

crowdin upload sources -b cloud --config crowdin-cloud.yaml
crowdin download -b cloud --config crowdin-cloud.yaml -l en-GB

DIRECTORY=language_pack
rm -rf $DIRECTORY || true
mkdir -p $DIRECTORY
cp -rf ../cloud_portal/translations/en_GB/*  $DIRECTORY
