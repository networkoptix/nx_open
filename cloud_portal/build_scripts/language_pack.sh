#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR
. ../env/bin/activate

rm -rt language_pack || true
mkdir language_pack

echo "Copy templates"

mkdir language_pack/templates
cp -rf ../cloud/notifications/static/templates/*.html  language_pack/templates/
cp -rf ../cloud/notifications/static/templates/*.json  language_pack/templates/

echo "Copy views"
mkdir language_pack/views
cp -rf ../front_end/app/views/*  language_pack/views

echo "Copy language.json"
cp -rf ../front_end/app/language.json  language_pack/