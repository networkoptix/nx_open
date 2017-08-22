#!/bin/bash
set -e
CURRENTDIR=$(pwd)
PARENTDIR=$(dirname -- "$CURRENTDIR")
CROWDIN_OPERATION=$1
DIRECTORY=language_pack

rm -rf $DIRECTORY || true
mkdir $DIRECTORY

echo "Copy views"
mkdir $DIRECTORY/views
cp -rf ../app/views/*  $DIRECTORY/views

echo "Copy language.json"
cp -rf ../app/language.json  $DIRECTORY/


echo "Copy web_common"
mkdir $DIRECTORY/web_common
mkdir $DIRECTORY/web_common/views
cp -rf ../app/web_common/views/*  $DIRECTORY/web_common/views || true
cp -rf ../app/web_common/commonLanguage.json  $DIRECTORY/web_common || true
