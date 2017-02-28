#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR
CROWDIN=$1
DIRECTORY=language_pack
. ../env/bin/activate

echo $CROWDIN

rm -rf $DIRECTORY || true
mkdir $DIRECTORY

echo "Copy templates"

mkdir $DIRECTORY/templates
cp -rf ../cloud/notifications/static/templates/*.html  $DIRECTORY/templates/
cp -rf ../cloud/notifications/static/templates/*.json  $DIRECTORY/templates/

echo "Copy views"
mkdir $DIRECTORY/views
cp -rf ../front_end/app/views/*  $DIRECTORY/views

echo "Copy language.json"
cp -rf ../front_end/app/language.json  $DIRECTORY/

echo "Copy crowdin yaml"
cp -rf crowdin.yaml $DIRECTORY/

if [[ $CROWDIN == 'upload' ]]; then 
    echo "Uploading to Crowdin..."
    cd $DIRECTORY 
    crowdin-cli upload sources
    cd ..
fi