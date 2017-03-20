#!/bin/bash
set -e

DIRECTORY=language_pack
WEBADMIN_DIRECTORY=$DIRECTORY/webadmin
CURRENTDIR=$(pwd)
PARENTDIR=$(dirname -- "$CURRENTDIR")
CROWDIN_OPERATION=$1

rm -rf $DIRECTORY || true
mkdir -p $WEBADMIN_DIRECTORY/views
echo "project_identifier: nx-witness" >> $DIRECTORY/crowdin.yaml
echo "api_key: 42f3cd326fea96b19ad3bf2eb9009b45" >> $DIRECTORY/crowdin.yaml


echo "Copy views"
cp -rf ../app/views/* $WEBADMIN_DIRECTORY/views

echo "Copy language.json"
cp -rf ../app/language.json $WEBADMIN_DIRECTORY/

echo "Copy crowdin yaml"
cat crowdin.yaml >> $DIRECTORY/crowdin.yaml

if [[ $CROWDIN_OPERATION == 'upload' ]]; then
    echo "Uploading to Crowdin..."
    cd $DIRECTORY
    crowdin-cli-py upload sources
    cd ..
fi