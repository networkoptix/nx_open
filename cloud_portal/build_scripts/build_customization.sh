#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR
. ../env/bin/activate

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
    mkdir $TARGET_DIR/$CUSTOMIZATION

    echo "------------------------------"
    echo "Copy config"
    cp $dir/cloud_portal.yaml $TARGET_DIR/$CUSTOMIZATION/


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

    echo "Move front_end to destination"
    mv ../front_end/dist $TARGET_DIR/$CUSTOMIZATION/static

    echo "Overwrite images"
    cp -rf $dir/front_end/images/* $TARGET_DIR/$CUSTOMIZATION/static/images

    echo "Overwrite static"
    cp -rf $dir/front_end/views/* $TARGET_DIR/$CUSTOMIZATION/static/views || true

    echo "Building front_end finished"


    echo "------------------------------"
    echo "Building templates - for each language"

    mkdir $TARGET_DIR/$CUSTOMIZATION/templates/
    cp -rf $dir/templates/* $TARGET_DIR/$CUSTOMIZATION/templates

    for lang_dir in ../translations/*/
    do
        lang_dir=${lang_dir%*/}
        LANG=${lang_dir/..\/translations\//}

        echo "$TARGET_DIR/$CUSTOMIZATION/templates/lang_$LANG"

        mkdir $TARGET_DIR/$CUSTOMIZATION/templates/lang_$LANG
        mkdir $TARGET_DIR/$CUSTOMIZATION/templates/lang_$LANG/src

        echo "Copy template sources - with default language"
        cp -rf ../cloud/notifications/static/templates/* $TARGET_DIR/$CUSTOMIZATION/templates/lang_$LANG/src/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/templates/* $TARGET_DIR/$CUSTOMIZATION/templates/lang_$LANG/src/ || true

        echo "Copy custom styles"
        cp $dir/front_end/styles/_custom_palette.scss $TARGET_DIR/$CUSTOMIZATION/templates/lang_$LANG/src/

        pushd $TARGET_DIR/$CUSTOMIZATION/templates/lang_$LANG/src
        python preprocess.py
        popd

        echo "Clean sources"
        rm -rf $TARGET_DIR/$CUSTOMIZATION/templates/src
    done
    echo "Templates success"

    echo "------------------------------"
    echo "Localization - portal"

    for lang_dir in ../translations/*/
    do
        lang_dir=${lang_dir%*/}
        LANG=${lang_dir/..\/translations\//}

        echo "$TARGET_DIR/$CUSTOMIZATION/static/lang_$LANG/views/"

        mkdir $TARGET_DIR/$CUSTOMIZATION/static/lang_$LANG
        mkdir $TARGET_DIR/$CUSTOMIZATION/static/lang_$LANG/views/

        echo "Copy default views - with default language"
        cp -rf $TARGET_DIR/$CUSTOMIZATION/static/views/* $TARGET_DIR/$CUSTOMIZATION/static/lang_$LANG/views/

        echo "Overwrite them with localized sources"
        cp -rf $lang_dir/views/* $TARGET_DIR/$CUSTOMIZATION/static/lang_$LANG/views/ || true
    done

    echo "Localization success"


    echo "------------------------------"
    echo "Branding"
    cp $dir/branding.ts $TARGET_DIR/$CUSTOMIZATION
    pushd $TARGET_DIR/$CUSTOMIZATION
    python ../../../build_scripts/generate_languages_json.py
    python ../../../build_scripts/branding.py
    rm -rf *.ts
    popd
    echo "Branding success"


echo "$CUSTOMIZATION Done"

# say "Cloud portal build is finished"
