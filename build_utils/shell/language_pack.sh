#!/bin/bash
set -e

DIRECTORY=language_pack
BASE_DIRECTORY=../..
WEBADMIN_DIRECTORY=$DIRECTORY/webadmin
WEBADMIN_SOURCE_DIRECTORY=$BASE_DIRECTORY/webadmin
CURRENTDIR=$(pwd)
PARENTDIR=$(dirname -- "$CURRENTDIR")
CROWDIN_OPERATION=$1

rm -rf $DIRECTORY || true
TS_FILES=$(find $BASE_DIRECTORY -iname "*_en_US.ts")

mkdir -p $WEBADMIN_DIRECTORY/views
echo "project_identifier: nx-witness" >> $DIRECTORY/crowdin.yaml
echo "api_key: 42f3cd326fea96b19ad3bf2eb9009b45" >> $DIRECTORY/crowdin.yaml

echo "Copy views"
cp -rf $WEBADMIN_SOURCE_DIRECTORY/app/views/* $WEBADMIN_DIRECTORY/views

echo "Copy language.json"
cp -rf $WEBADMIN_SOURCE_DIRECTORY/app/language.json $WEBADMIN_DIRECTORY/

for f in $TS_FILES; do
    filename=$(basename "$f")
    echo $filename
    filename_no_ext="${filename%.*}"
    filename_no_suffix="${filename_no_ext::-6}"
    cp -f $f $DIRECTORY/$filename_no_suffix.ts
done

cp -f $BASE_DIRECTORY/wixsetup/maven/filter-resources/OptixTheme_en-us.wxl $DIRECTORY/OptixTheme.wxl

echo "Copy crowdin yaml"
cat crowdin.yaml >> $DIRECTORY/crowdin.yaml

if [[ $CROWDIN_OPERATION == 'upload' ]]; then
    echo "Uploading to Crowdin..."
    cd $DIRECTORY
    crowdin-cli-py upload sources
    cd ..
fi