#!/bin/bash
set -e
CURRENTDIR=$(pwd)
PARENTDIR=$(dirname -- "$CURRENTDIR")
CROWDIN_OPERATION=$1
DIRECTORY=language_pack

rm -rf $DIRECTORY || true

echo "Copy templates"

mkdir -p $DIRECTORY/templates
echo "project_identifier: cloud-portal" >> $DIRECTORY/crowdin.yaml
echo "api_key: faf881fc7b7d8dc757eb069e53cba4b1" >> $DIRECTORY/crowdin.yaml
cp -rf ../cloud/notifications/static/templates/*.html  $DIRECTORY/templates/
cp -rf ../cloud/notifications/static/templates/*.txt  $DIRECTORY/templates/
cp -rf ../cloud/notifications/static/templates/*.json  $DIRECTORY/templates/

echo "Copy views"
mkdir $DIRECTORY/views
cp -rf ../front_end/app/views/*  $DIRECTORY/views

echo "Copy language.json"
cp -rf ../front_end/app/language.json  $DIRECTORY/


echo "Copy web_common"
mkdir -p $DIRECTORY/web_common/views
cp -rf ../../webadmin/app/web_common/views/*  $DIRECTORY/web_common/views || true
cp -rf ../../webadmin/app/web_common/commonLanguage.json  $DIRECTORY/web_common || true

echo "Copy crowdin yaml"
cat crowdin.yaml >> $DIRECTORY/crowdin.yaml

if [[ $CROWDIN_OPERATION == 'upload' ]]; then 
    echo "Uploading to Crowdin..."
    cd $DIRECTORY 
    crowdin-cli-py upload sources
    cd ..
fi
