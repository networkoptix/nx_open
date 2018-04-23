#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CLEANUP=False

if [[ "$PWD" != */nx_vms/cloud_portal* ]]
then
    CLEANUP=True
    echo "Cleaning current directory"
    rm -rf *

    echo "Copying source files"
    cp -pr $DIR/.. cloud_portal
    mkdir -p webadmin/app/web_common && cp -pr $DIR/../../webadmin/app/web_common/* "$_"
    mkdir -p webadmin/app/styles && cp -pr $DIR/../../webadmin/app/styles/* "$_"
    cp $DIR/../../webadmin/package.json webadmin

    cd cloud_portal
    echo "pip install requirements"
    virtualenv env
    . ./env/bin/activate
    pip install -r build_scripts/requirements.txt


    echo "npm install"
    echo $PWD
    pushd front_end
        npm install
    popd

    pushd ../webadmin
        npm install
    popd

    cd build_scripts
else
    cd $DIR
    . ../env/bin/activate
fi

TARGET_DIR="../cloud/static"

echo "Clear $TARGET_DIR"
rm -rf $TARGET_DIR
echo "Create $TARGET_DIR"
mkdir -p $TARGET_DIR


echo "Iterate all customizations"
for dir in ../customizations/*/
do
    dir=${dir%*/}
    CUSTOMIZATION=${dir/..\/customizations\//}
    ./build_customization.sh $CUSTOMIZATION $DIR
done

echo "Done!"

if $CLEANUP
then
    echo "Clean build dir"
    cd ..
    for dir in $(ls $PWD)
    do
        dir=${dir%*/}
        [[ "$dir" = "cloud" ]] && continue
        rm -rf ./$dir
    done

    rm -rf ../webadmin
    echo $PWD
fi

echo "Cloud portal build is finished"
# say "Cloud portal build is finished"
