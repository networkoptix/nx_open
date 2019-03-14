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


    echo "Build statics"
    pushd ../front_end
        npm run setSkin $SKIN
        npm run build
        # Save the repository info.
        echo "Create version.txt"
        hg log -r . --repository "$2" | head -n 7 > dist/version.txt
        cat dist/version.txt
    popd


    if [ "$SKIN" = "blue" ]
    then
        echo "Move fonts and help  - only for blue"
        rm -rf $TARGET_DIR/../common || true
        mkdir -p $TARGET_DIR/../common/static
        mv ../front_end/dist/fonts $TARGET_DIR/../common/static/fonts
        cp -R ../help $TARGET_DIR/../common/static/help
    fi
    rm -rf ../front_end/dist/fonts || true

    echo "Move front_end to destination"
    mv ../front_end/dist $TARGET_DIR/$SKIN/static

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


        mkdir -p $TARGET_DIR/$SKIN/static/lang_$LANG/web_common/

        echo "Copy web_common default views - with default language"
        cp -rf $TARGET_DIR/$SKIN/static/web_common/* $TARGET_DIR/$SKIN/static/lang_$LANG/web_common/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/web_common/views/* $TARGET_DIR/$SKIN/static/lang_$LANG/web_common/ || true

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
