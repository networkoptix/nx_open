#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR
. ../env/bin/activate

TARGET_DIR="../cloud/static"

echo "Clear $TARGET_DIR"
rm -rf $TARGET_DIR
echo "Create $TARGET_DIR"
mkdir $TARGET_DIR

echo "Iterate all customizations"
for dir in ../customizations/*/
do
    dir=${dir%*/}
    CUSTOMIZATION=${dir/..\/customizations\//}

    echo "============================================================"
    echo "Building customization: $CUSTOMIZATION $dir"
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
    echo "Building templates"

    mkdir $TARGET_DIR/$CUSTOMIZATION/templates/
    mkdir $TARGET_DIR/$CUSTOMIZATION/templates/src

    echo "Process sources"
    cp -rf ../cloud/notifications/static/templates/* $TARGET_DIR/$CUSTOMIZATION/templates/src/

    echo "Copy custom styles"
    cp $dir/front_end/styles/_custom_palette.scss $TARGET_DIR/$CUSTOMIZATION/templates/src/

    echo "Run processing"
    pushd $TARGET_DIR/$CUSTOMIZATION/templates/src
    python preprocess.py
    popd

    echo "Clean sources"
    rm -rf $TARGET_DIR/$CUSTOMIZATION/templates/src

    echo "Copy additional templates files"
    cp -rf $dir/templates/* $TARGET_DIR/$CUSTOMIZATION/templates

    echo "Templates success"

    echo "------------------------------"
    echo "Localization"

    echo "Generate blank translation files"
    pushd $TARGET_DIR/$CUSTOMIZATION
    python ../../../build_scripts/generate_ts.py
    popd

    echo "Copy branding and translation files"
    cp -f $dir/*.ts $TARGET_DIR/$CUSTOMIZATION

    pushd $TARGET_DIR/$CUSTOMIZATION
    echo "Customizing and localizing"
    python ../../../build_scripts/localize.py
    popd

    echo "clean branding files"
    rm -rf $TARGET_DIR/$CUSTOMIZATION/*.ts

    echo "Localization success"
done

echo "Done!"

# say "Cloud portal build is finished"
