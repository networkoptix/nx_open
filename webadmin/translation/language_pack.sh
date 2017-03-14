#!/bin/bash
set -e

DIRECTORY=language_pack

rm -rf $DIRECTORY || true
mkdir $DIRECTORY

echo "Copy views"
mkdir $DIRECTORY/views
cp -rf ../app/views/*  $DIRECTORY/views

echo "Copy language.json"
cp -rf ../app/language.json  $DIRECTORY/
