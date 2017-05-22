#!/bin/bash
set -e
CURRENTDIR=$(pwd)
PARENTDIR=$(dirname -- "$CURRENTDIR")
CROWDIN_OPERATION=$1
DIRECTORY=language_pack
. ../env/bin/activate

rm -rf $DIRECTORY || true
mkdir $DIRECTORY

echo "Copy templates"

mkdir $DIRECTORY/templates
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

echo "Copy crowdin yaml"
cat crowdin.yaml >> $DIRECTORY/crowdin.yaml

if [[ $CROWDIN_OPERATION == 'upload' ]]; then 
    echo "Uploading to Crowdin..."
    cd $DIRECTORY 
    crowdin-cli-py upload sources
    cd ..
fi