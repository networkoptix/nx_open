#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

TARGET_DIR="../cloud/static/_source"
SKIN=$1
if [ -z "$SKIN" ]
then
    SKIN="blue"
fi

dir=../skins/$SKIN

    echo "============================================================"
    echo "Building SKIN: $SKIN $dir"
    rm -rf $TARGET_DIR/$SKIN || true
    mkdir -p $TARGET_DIR/$SKIN


    echo "------------------------------"
    echo "Building front_end"

    echo "Copy custom styles"
    mkdir -p ../front_end/app/styles/custom
    cp -rf $dir/front_end/styles/* ../front_end/app/styles/custom

    echo "Build statics"
    pushd ../front_end
    grunt setskin:$SKIN
    grunt build
    popd

    cp -rf $dir/front_end/styles/* ../front_end/app/styles/custom

    if [ "$SKIN" = "blue" ]
    then
        echo "Move fonts  - only for blue"
        rm -rf $TARGET_DIR/../common || true
        mkdir -p $TARGET_DIR/../common/static
        mv ../front_end/dist/fonts $TARGET_DIR/../common/static/fonts
    fi
    rm -rf ../front_end/dist/fonts || true

    echo "Move front_end to destination"
    mv ../front_end/dist $TARGET_DIR/$SKIN/static

    echo "Overwrite images"
    cp -rf $dir/front_end/images/* $TARGET_DIR/$SKIN/static/images

    echo "Overwrite static"
    cp -rf $dir/front_end/views/* $TARGET_DIR/$SKIN/static/views || true

    echo "Building front_end finished"


    echo "------------------------------"
    echo "Building templates - for each language"

    mkdir -p $TARGET_DIR/$SKIN/templates/
    cp -rf $dir/templates/* $TARGET_DIR/$SKIN/templates || true

    for lang_dir in ../translations/*/
    do
        lang_dir=${lang_dir%*/}
        LANG=${lang_dir/..\/translations\//}

        echo "$TARGET_DIR/$SKIN/templates/lang_$LANG"

        mkdir -p $TARGET_DIR/$SKIN/templates/lang_$LANG/src

        echo "Copy template sources - with default language"
        cp -rf ../cloud/notifications/static/templates/* $TARGET_DIR/$SKIN/templates/lang_$LANG/src/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/templates/* $TARGET_DIR/$SKIN/templates/lang_$LANG/src/ || true

        echo "Copy custom styles"
        cp $dir/front_end/styles/_custom_palette.scss $TARGET_DIR/$SKIN/templates/lang_$LANG/src/

        pushd $TARGET_DIR/$SKIN/templates/lang_$LANG/src
        python preprocess.py
        popd

        echo "Clean sources"
        rm -rf $TARGET_DIR/$SKIN/templates/lang_$LANG/src
    done
    echo "Templates success"

    echo "------------------------------"
    echo "Localization - portal"

    for lang_dir in ../translations/*/
    do
        lang_dir=${lang_dir%*/}
        LANG=${lang_dir/..\/translations\//}

        echo "$TARGET_DIR/$SKIN/static/lang_$LANG/views/"

        mkdir -p $TARGET_DIR/$SKIN/static/lang_$LANG/views/

        echo "Copy default views - with default language"
        cp -rf $TARGET_DIR/$SKIN/static/views/* $TARGET_DIR/$SKIN/static/lang_$LANG/views/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/views/* $TARGET_DIR/$SKIN/static/lang_$LANG/views/ || true


        mkdir -p $TARGET_DIR/$SKIN/static/lang_$LANG/web_common/views/

        echo "Copy web_common default views - with default language"
        cp -rf $TARGET_DIR/$SKIN/static/web_common/views/* $TARGET_DIR/$SKIN/static/lang_$LANG/web_common/views/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/web_common/views/* $TARGET_DIR/$SKIN/static/lang_$LANG/web_common/views/ || true

        echo "Generate language.json"
        pushd $TARGET_DIR/$SKIN
        python ../../../../build_scripts/generate_language_json.py $LANG
        popd

    done

    pushd $TARGET_DIR/$SKIN
    python ../../../../build_scripts/generate_all_languages_json.py
    popd

    rm -rf $TARGET_DIR/$SKIN/static/views
    echo "Localization success"

echo "$SKIN Done"

# say "Cloud portal build is finished"
