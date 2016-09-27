#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR
. ../env/bin/activate

TARGET_DIR="localization"

echo "Create $TARGET_DIR"
mkdir $TARGET_DIR


echo "------------------------------"

echo "Building front_end"
pushd ../front_end
grunt setbranding:default
grunt build
popd

echo "Move front_end to destination"
mv ../front_end/dist $TARGET_DIR/static

echo "Building front_end finished"


echo "------------------------------"
echo "Building templates"

mkdir $TARGET_DIR/templates/
mkdir $TARGET_DIR/templates/src

echo "Process sources"
cp -rf ../cloud/notifications/static/templates/* $TARGET_DIR/templates/src/

echo "Run processing"

echo "Copy custom styles"
cp ../customizations/default/front_end/styles/_custom_palette.scss $TARGET_DIR/templates/src/

pushd $TARGET_DIR/templates/src
python preprocess.py
popd

echo "Templates success"

echo "------------------------------"
echo "Generate localization files"

echo "Copy branding and translation files"
pushd $TARGET_DIR
python ../generate_ts.py
cp *.ts ..
popd

echo "Generate localization files finished"

echo "------------------------------"

echo "Clean build"
echo "Clear $TARGET_DIR"
rm -rf $TARGET_DIR

echo "Done!"

# say "Cloud portal build is finished"
