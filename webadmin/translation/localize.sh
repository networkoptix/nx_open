#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

for lang_dir in ../translations/*
do
    lang_dir=${lang_dir%*/}
    LANG=${lang_dir/..\/translations\//}

    echo "../static/lang_$LANG/views/"

    mkdir ../static/lang_$LANG
    mkdir ../static/lang_$LANG/views/

    echo "Copy default views - with default language"
    cp -rf ../static/views/* ../static/lang_$LANG/views/

    echo "Overwrite them with localized sources"
    cp -rf $lang_dir/views/* ../static/lang_$LANG/views/ || true


    mkdir ../static/lang_$LANG/web_common/
    mkdir ../static/lang_$LANG/web_common/views/
    echo "Copy web_common default views - with default language"
    cp -rf ../static/web_common/views/* ../static/lang_$LANG/web_common/views/

    echo "Overwrite them with localized sources"
    cp -rf $lang_dir/web_common/views/* ../static/lang_$LANG/web_common/views/ || true

done

echo "Generate language.json"
python generate_languages_json.py