#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

TARGET_DIR="../cloud/static"
CUSTOMIZATION=$1
if [ -z "$CUSTOMIZATION" ]
then
    CUSTOMIZATION="default"
fi

dir=../customizations/$CUSTOMIZATION/

    echo "============================================================"
    echo "Building customization: $CUSTOMIZATION $dir"
    rm -rf $TARGET_DIR/$CUSTOMIZATION || true
    mkdir -p $TARGET_DIR/$CUSTOMIZATION/source

    echo "------------------------------"
    echo "Copy config"
    cp ../cloud/cloud_portal.yaml $TARGET_DIR/$CUSTOMIZATION/


    echo "------------------------------"
    echo "Building front_end"

    echo "Copy custom styles"
    mkdir -p ../front_end/app/styles/custom
    cp -rf $dir/front_end/styles/* ../front_end/app/styles/custom

    echo "Build statics"
    pushd ../front_end
    grunt setbranding:$CUSTOMIZATION
    grunt build
    popd

    cp -rf $dir/front_end/styles/* ../front_end/app/styles/custom

    if [ "$CUSTOMIZATION" = "default" ]
    then
        echo "Move fonts  - only for default"
        rm -rf $TARGET_DIR/common || true
        mkdir -p $TARGET_DIR/common/static
        mv ../front_end/dist/fonts $TARGET_DIR/common/static/fonts
    fi

    echo "Move front_end to destination"
    mv ../front_end/dist $TARGET_DIR/$CUSTOMIZATION/source/static

    echo "Overwrite images"
    cp -rf $dir/front_end/images/* $TARGET_DIR/$CUSTOMIZATION/source/static/images

    echo "Overwrite static"
    cp -rf $dir/front_end/views/* $TARGET_DIR/$CUSTOMIZATION/source/static/views || true

    echo "Building front_end finished"


    echo "------------------------------"
    echo "Building templates - for each language"

    mkdir -p $TARGET_DIR/$CUSTOMIZATION/source/templates/
    cp -rf $dir/templates/* $TARGET_DIR/$CUSTOMIZATION/source/templates

    for lang_dir in ../translations/*/
    do
        lang_dir=${lang_dir%*/}
        LANG=${lang_dir/..\/translations\//}

        echo "$TARGET_DIR/$CUSTOMIZATION/source/templates/lang_$LANG"

        mkdir -p $TARGET_DIR/$CUSTOMIZATION/source/templates/lang_$LANG/src

        echo "Copy template sources - with default language"
        cp -rf ../cloud/notifications/static/templates/* $TARGET_DIR/$CUSTOMIZATION/source/templates/lang_$LANG/src/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/templates/* $TARGET_DIR/$CUSTOMIZATION/source/templates/lang_$LANG/src/ || true

        echo "Copy custom styles"
        cp $dir/front_end/styles/_custom_palette.scss $TARGET_DIR/$CUSTOMIZATION/source/templates/lang_$LANG/src/

        pushd $TARGET_DIR/$CUSTOMIZATION/source/templates/lang_$LANG/src
        python preprocess.py
        popd

        echo "Clean sources"
        rm -rf $TARGET_DIR/$CUSTOMIZATION/source/templates/lang_$LANG/src
    done
    echo "Templates success"

    echo "------------------------------"
    echo "Localization - portal"

    for lang_dir in ../translations/*/
    do
        lang_dir=${lang_dir%*/}
        LANG=${lang_dir/..\/translations\//}

        echo "$TARGET_DIR/$CUSTOMIZATION/source/static/lang_$LANG/views/"

        mkdir -p $TARGET_DIR/$CUSTOMIZATION/source/static/lang_$LANG/views/

        echo "Copy default views - with default language"
        cp -rf $TARGET_DIR/$CUSTOMIZATION/source/static/views/* $TARGET_DIR/$CUSTOMIZATION/source/static/lang_$LANG/views/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/views/* $TARGET_DIR/$CUSTOMIZATION/source/static/lang_$LANG/views/ || true


        mkdir -p $TARGET_DIR/$CUSTOMIZATION/source/static/lang_$LANG/web_common/views/

        echo "Copy web_common default views - with default language"
        cp -rf $TARGET_DIR/$CUSTOMIZATION/source/static/web_common/views/* $TARGET_DIR/$CUSTOMIZATION/source/static/lang_$LANG/web_common/views/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/web_common/views/* $TARGET_DIR/$CUSTOMIZATION/source/static/lang_$LANG/web_common/views/ || true

        echo "Generate language.json"
        pushd $TARGET_DIR/$CUSTOMIZATION/source
        python ../../../../build_scripts/generate_language_json.py $LANG
        popd

    done

    pushd $TARGET_DIR/$CUSTOMIZATION/source
    python ../../../../build_scripts/generate_all_languages_json.py
    popd

    rm -rf $TARGET_DIR/$CUSTOMIZATION/source/static/views
    echo "Localization success"

    echo "copy sources to root"
    cp -rf $TARGET_DIR/$CUSTOMIZATION/source/* $TARGET_DIR/$CUSTOMIZATION/

echo "$CUSTOMIZATION Done"

# say "Cloud portal build is finished"
